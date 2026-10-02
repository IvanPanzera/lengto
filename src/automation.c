#define _CRT_SECURE_NO_WARNINGS
#include "automation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>

int drawing_export(Document *doc,const Project *project,const wchar_t *path,ExportFormat format,int all_pages,wchar_t *error,size_t cap) {
    ExportJob *job;int original=doc->page,start=all_pages ? 0 : original,end=all_pages ? doc->pages : original+1,i,ok=1;
    if(!doc->bitmap) { swprintf(error,cap,L"Open a document first.");return 0; }
    if(!_wcsicmp(path,doc->path)) { swprintf(error,cap,L"Choose an output file different from the original.");return 0; }
    if(all_pages && doc->pages>1 && format!=EXPORT_TIFF && format!=EXPORT_PDF) { swprintf(error,cap,L"Choose TIFF or PDF to export all pages.");return 0; }
    job=export_begin(path,format);if(!job) { swprintf(error,cap,L"%ls",export_error());return 0; }
    for(i=start;i<end && ok;++i) {
        HBITMAP bitmap;if(i!=doc->page && !document_page(doc,i,error,cap)) { ok=0;break; }
        bitmap=render_export(doc,project);ok=bitmap && export_append(job,bitmap,doc->logical_width/2,doc->logical_height/2);if(bitmap) DeleteObject(bitmap);
        if(!ok) swprintf(error,cap,L"%ls",export_error());
    }
    if(ok) { ok=export_finish(job);if(!ok) swprintf(error,cap,L"%ls",export_error()); }else export_abort(job);
    if(doc->page!=original) { wchar_t restore[512];if(!document_page(doc,original,restore,512)) { swprintf(error,cap,L"%ls",restore);return 0; } }
    return ok;
}
static int integer(const Json *j,const char *key,int low,int high,int *result) {
    double n;if(!json_number(j,key,&n) || n<low || n>high || n!=floor(n)) return 0;*result=(int)n;return 1;
}
static int unit(const Json *j,Unit *u) { wchar_t s[40];int i;if(!json_wide(j,"unit",s,40)) return 0;if(!wcscmp(s,L"UA")) { *u=UNIT_AU;return 1; }for(i=0;i<UNIT_COUNT;++i) if(!wcscmp(s,unit_name((Unit)i))) { *u=(Unit)i;return 1; }return 0; }
static int point(const Json *j,Document *d,const char *xkey,const char *ykey,Point *p) {
    int x,y;if(!integer(j,xkey,0,(int)ceil(d->logical_width)-1,&x) || !integer(j,ykey,0,(int)ceil(d->logical_height)-1,&y)) return 0;p->x=x+0.5;p->y=y+0.5;return 1;
}
static int name(const Json *j,wchar_t *s) { size_t i;if(!json_wide(j,"name",s,P2L_NAME) || !*s) return 0;for(i=0;s[i];++i) if(s[i]<32) return 0;return 1; }
static int measurement_index(Project *p,int id) { int i;for(i=0;i<p->nm;++i) if(p->meas[i].id==id) return i;return -1; }
static char *error_result(const wchar_t *message) {
    Text out={0};char *q=json_quote(message);if(!q) return _strdup("{\"ok\":false,\"error\":\"Not enough memory\"}");text_add(&out,"{\"ok\":false,\"error\":%s}",q);free(q);return out.data;
}
static void measurement_result(Text *out,const Project *p,const Measurement *m) {
    char *q=json_quote(unit_name(p->display_unit));double metres=measured_metres(p,m);
    text_add(out,"{\"id\":%d,\"metres\":%.17g,\"value\":%.17g,\"rounded\":%.17g,\"unit\":%s}",m->id,metres,metres/unit_metres(p->display_unit),rounded_metres(metres,p->precision_exp)/unit_metres(p->display_unit),q ? q : "null");free(q);
}
char *automation_execute(Automation *a,const char *request) {
    Json j;Text result={0},out={0};Document *d=a->document;Project *p=a->project;wchar_t error[1024]=L"Invalid command or parameters.";int ok=0;
    a->changes=0;if(!json_parse(&j,request)) return error_result(L"Invalid JSON request.");
    if(json_is(&j,"op","info")) {
        char *q=json_quote(unit_name(p->display_unit));text_add(&result,"{\"loaded\":%s,\"width\":%d,\"height\":%d,\"page\":%d,\"pages\":%d,\"measurements\":%d,\"calibrated\":%s,\"unit\":%s,\"precision\":%d}",d->bitmap ? "true" : "false",(int)ceil(d->logical_width),(int)ceil(d->logical_height),d->page+1,d->pages,p->nm,p->nc ? "true" : "false",q ? q : "null",p->precision_exp);free(q);ok=1;
    } else if(json_is(&j,"op","open") || json_is(&j,"op","open_image")) {
        wchar_t path[P2L_PATH],full[P2L_PATH];Document *next;DWORD n;
        if(!json_wide(&j,"path",path,P2L_PATH)) goto done;n=GetFullPathNameW(path,P2L_PATH,full,NULL);if(!n || n>=P2L_PATH) goto done;
        next=calloc(1,sizeof(*next));if(!next) goto done;ok=json_is(&j,"op","open_image") ? document_open_image(next,full,error,1024) : document_open(next,full,error,1024);
        if(ok) { Unit u=p->display_unit;int precision=p->precision_exp;document_close(d);*d=*next;project_init(p,d->pages);p->display_unit=u;p->precision_exp=precision;a->dirty=d->clipboard;a->changes=AUTO_OPEN|AUTO_VIEW;a->output[0]=0; }
        else document_close(next);free(next);
    } else if(json_is(&j,"op","close")) {
        Unit u=p->display_unit;int precision=p->precision_exp;document_close(d);project_init(p,1);p->display_unit=u;p->precision_exp=precision;a->dirty=0;a->changes=AUTO_OPEN|AUTO_VIEW;a->output[0]=0;ok=1;
    } else if(json_is(&j,"op","settings")) {
        Unit u;int precision;if(!unit(&j,&u) || !integer(&j,"precision",-10,16,&precision)) goto done;p->display_unit=u;p->precision_exp=precision;a->changes=AUTO_CHANGED;a->dirty=d->bitmap && p->nm ? 1 : a->dirty;ok=1;
    } else if(!d->bitmap) { wcscpy(error,L"Open a document first."); }
    else if(json_is(&j,"op","page")) {
        int page;if(!integer(&j,"page",1,d->pages,&page)) goto done;ok=document_page(d,page-1,error,1024);if(ok) a->changes=AUTO_VIEW;
    } else if(json_is(&j,"op","calibrate")) {
        Point first,last;Unit u;double value;
        if(!point(&j,d,"ax","ay",&first) || !point(&j,d,"bx","by",&last) || point_distance(first,last)<1 || !unit(&j,&u) || !json_number(&j,"value",&value) || value<=0 || value>1e30) goto done;
        if(!p->nc) ok=add_cal(p,d->page,L"Calibration",first,last,value,u)!=0;
        else { Calibration *c=&p->cal[0];c->page=d->page;c->a=first;c->b=last;c->known=value;c->unit=u;ok=1; }if(ok) { a->dirty=1;a->changes=AUTO_CHANGED; }
    } else if(json_is(&j,"op","measure") || json_is(&j,"op","edit_measure")) {
        Point first,last;wchar_t label[P2L_NAME];int index=-1,id;
        if(!point(&j,d,"ax","ay",&first) || !point(&j,d,"bx","by",&last) || point_distance(first,last)<1 || !name(&j,label)) goto done;
        if(!p->nc) { wcscpy(error,L"Define a calibration first.");goto done; }
        if(json_is(&j,"op","edit_measure")) {
            if(!integer(&j,"id",1,0x7fffffff,&id) || (index=measurement_index(p,id))<0 || p->meas[index].page!=d->page) goto done;
            p->meas[index].a=first;p->meas[index].b=last;wcscpy(p->meas[index].name,label);ok=1;
        } else { id=add_meas(p,d->page,p->cal[0].id,label,first,last);ok=id!=0;if(ok) index=p->nm-1;else wcscpy(error,L"The limit of 4096 measurements has been reached."); }
        if(ok) { a->dirty=1;a->changes=AUTO_CHANGED;measurement_result(&result,p,&p->meas[index]); }
    } else if(json_is(&j,"op","delete")) {
        int id,index;if(!integer(&j,"id",1,0x7fffffff,&id) || (index=measurement_index(p,id))<0) goto done;remove_meas(p,index);a->dirty=1;a->changes=AUTO_CHANGED;ok=1;
    } else if(json_is(&j,"op","pixels")) {
        int x,y,w,h;size_t i,count;unsigned char *rgb;
        if(!integer(&j,"x",0,(int)ceil(d->logical_width)-1,&x) || !integer(&j,"y",0,(int)ceil(d->logical_height)-1,&y) || !integer(&j,"width",1,65536,&w) || !integer(&j,"height",1,65536,&h) || (double)w*h>65536) goto done;
        count=(size_t)w*h;rgb=malloc(count*3);if(!rgb) goto done;ok=document_rgb(d,x,y,w,h,rgb);
        if(ok) {
            static const char digits[]="0123456789abcdef";char *hex=malloc(count*6+1);
            if(!hex) ok=0;else { for(i=0;i<count*3;++i) { hex[i*2]=digits[rgb[i]>>4];hex[i*2+1]=digits[rgb[i]&15]; }hex[count*6]=0;text_add(&result,"{\"rgb\":\"%s\"}",hex);free(hex); }
        }else wcscpy(error,L"Cannot read the requested pixels.");free(rgb);
    } else if(json_is(&j,"op","export")) {
        wchar_t path[P2L_PATH],full[P2L_PATH];int format,all;DWORD n;
        if(!json_wide(&j,"path",path,P2L_PATH) || !integer(&j,"format",0,3,&format) || !integer(&j,"all_pages",0,1,&all)) goto done;
        n=GetFullPathNameW(path,P2L_PATH,full,NULL);if(!n || n>=P2L_PATH) goto done;
        ok=drawing_export(d,p,full,(ExportFormat)format,all,error,1024);
        if(ok) { int i;a->dirty=0;if(!all) { for(i=0;i<p->nm;++i) if(p->meas[i].page!=d->page) a->dirty=1;if(p->nc && p->cal[0].page!=d->page) a->dirty=1; }a->changes=AUTO_EXPORT;wcscpy(a->output,full);a->format=(ExportFormat)format;a->all_pages=all; }
    }
done:
    json_free(&j);if(!ok || result.failed) { text_free(&result);return error_result(error); }
    text_add(&out,"{\"ok\":true,\"result\":%s}",result.data ? result.data : "null");text_free(&result);if(out.failed) { text_free(&out);return error_result(L"Not enough memory."); }return out.data;
}
int automation_stdio(void) {
    Automation *a=calloc(1,sizeof(*a));char *line=malloc(1024*1024);int status=0;
    if(!a || !line) { free(a);free(line);return 1; }a->document=calloc(1,sizeof(Document));a->project=calloc(1,sizeof(Project));
    if(!a->document || !a->project) { free(a->document);free(a->project);free(a);free(line);return 1; }
    project_init(a->project,1);_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);setvbuf(stdout,NULL,_IONBF,0);
    while(fgets(line,1024*1024,stdin)) { char *result;if(!strchr(line,'\n')) { status=1;break; }result=automation_execute(a,line);if(!result) { status=1;break; }fputs(result,stdout);fputc('\n',stdout);free(result); }
    document_close(a->document);free(a->document);free(a->project);free(a);free(line);return status;
}
