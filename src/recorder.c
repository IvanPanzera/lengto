#define _CRT_SECURE_NO_WARNINGS
#include "recorder.h"
#include "portable.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
void recorder_reset(Recorder *r) { text_free(&r->body);memset(r,0,sizeof(*r)); }
static int recording(Recorder *r) { return r->active && !r->body.failed && !*r->error; }
static char *quoted(Recorder *r,const wchar_t *s) { char *q=json_quote(s);if(!q) r->body.failed=1;return q; }
void recorder_open(Recorder *r,Document *document) {
    char *q;if(!recording(r)) return;
    if(document->clipboard) {
        static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t size,i;unsigned char *png=document_png(document,&size);const char *indent=++r->opens==1 ? "        " : "    ";
        if(!png) { wcscpy(r->error,L"Cannot include the clipboard image in the recording.");return; }
        if(r->opens==1) text_add(&r->body,"    if input_file:\n        app.open(input_file)\n    else:\n");
        text_add(&r->body,"%s# Keep the pasted source independent of the clipboard.\n%sapp.open_image(base64.b64decode(\n",indent,indent);
        for(i=0;i<size && !r->body.failed;) {
            char line[65];int n=0;while(i<size && n<64) {
                size_t remaining=size-i;unsigned a=png[i++],b=remaining>1 ? png[i++] : 0,c=remaining>2 ? png[i++] : 0;
                line[n++]=alphabet[a>>2];line[n++]=alphabet[((a&3)<<4)|(b>>4)];line[n++]=remaining>1 ? alphabet[((b&15)<<2)|(c>>6)] : '=';line[n++]=remaining>2 ? alphabet[c&63] : '=';
            }line[n]=0;text_add(&r->body,"%s    \"%s\"\n",indent,line);
        }
        text_add(&r->body,"%s))\n",indent);free(png);return;
    }
    q=quoted(r,document->path);if(!q) return;
    text_add(&r->body,"    app.open(%s%s)\n",++r->opens==1 ? "input_file or " : "",q);free(q);
}
void recorder_page(Recorder *r,int page) { if(recording(r)) text_add(&r->body,"    app.page(%d)\n",page+1); }
void recorder_close(Recorder *r) { if(recording(r)) text_add(&r->body,"    app.close()\n"); }
void recorder_settings(Recorder *r,const Project *p) {
    char *q;if(!recording(r)) return;q=quoted(r,unit_name(p->display_unit));if(q) { text_add(&r->body,"    app.settings(unit=%s, precision=%d)\n",q,p->precision_exp);free(q); }
}
static int record_point(Recorder *r,Document *d,Point p) {
    unsigned char rgb[3];int x=(int)floor(p.x),y=(int)floor(p.y);
    if(!document_rgb(d,x,y,1,1,rgb)) { wcscpy(r->error,L"Recording stopped: cannot read the original color of an endpoint.");return 0; }
    text_add(&r->body,"Point(absolute=(%d, %d), relative=(Fraction(%d, %d), Fraction(%d, %d)), rgb=(%u, %u, %u))",x,y,x,(int)ceil(d->logical_width),y,(int)ceil(d->logical_height),rgb[0],rgb[1],rgb[2]);return !r->body.failed;
}
static int recording_page(Recorder *r,Document *d,int page) {
    if(d->page==page) return 1;
    if(!document_page(d,page,r->error,512)) return 0;recorder_page(r,page);return 1;
}
void recorder_calibration(Recorder *r,Document *d,const Calibration *c) {
    int original=d->page;char *q;if(!recording(r) || !recording_page(r,d,c->page)) return;
    q=quoted(r,unit_name(c->unit));if(!q) return;text_add(&r->body,"    app.calibrate(\n        ");
    if(record_point(r,d,c->a)) { text_add(&r->body,",\n        ");if(record_point(r,d,c->b)) text_add(&r->body,",\n        value=%.17g, unit=%s)\n",c->known,q); }free(q);
    if(d->page!=original) { wchar_t error[512];if(document_page(d,original,error,512)) recorder_page(r,original);else wcscpy(r->error,error); }
}
void recorder_measurement(Recorder *r,Document *d,const Measurement *m,int edit) {
    int original=d->page;char *q;if(!recording(r) || !recording_page(r,d,m->page)) return;
    q=quoted(r,m->name);if(!q) return;
    if(edit) text_add(&r->body,"    m_%d = app.edit_measurement(m_%d,\n        ",m->id,m->id);
    else text_add(&r->body,"    m_%d = app.measure(\n        ",m->id);
    if(record_point(r,d,m->a)) { text_add(&r->body,",\n        ");if(record_point(r,d,m->b)) text_add(&r->body,",\n        name=%s)\n",q); }free(q);
    if(d->page!=original) { wchar_t error[512];if(document_page(d,original,error,512)) recorder_page(r,original);else wcscpy(r->error,error); }
}
void recorder_delete(Recorder *r,int id) { if(recording(r)) text_add(&r->body,"    app.delete_measurement(m_%d)\n",id); }
void recorder_export(Recorder *r,const wchar_t *path,int all_pages) {
    char *q;if(!recording(r)) return;q=quoted(r,path);if(!q) return;
    if(++r->exports==1) text_add(&r->body,"    app.export(_output_path(output_file, input_file, %s), all_pages=%s)\n",q,all_pages ? "True" : "False");
    else text_add(&r->body,"    app.export(%s, all_pages=%s)\n",q,all_pages ? "True" : "False");free(q);
}
int recorder_start(Recorder *r,Document *d,const Project *p,int relative) {
    int i,original=d->page;recorder_reset(r);r->active=1;r->relative=relative;recorder_settings(r,p);
    if(d->bitmap) {
        recorder_open(r,d);
        /* Opening a file starts on page one. Keep the snapshot's context explicit. */
        recorder_page(r,original);
        if(p->nc) recorder_calibration(r,d,&p->cal[0]);
        for(i=0;i<p->nm && recording(r);++i) recorder_measurement(r,d,&p->meas[i],0);
    }
    return recording(r);
}
int recorder_save(Recorder *r,const wchar_t *path) {
    wchar_t *api=NULL,*temp=NULL;char *utf=NULL;FILE *f=NULL;int n,ok=0;Text script={0};
    if(!recording(r)) return 0;api=portable_text(RESOURCE_PYTHON);if(!api) goto done;
    n=WideCharToMultiByte(CP_UTF8,0,api,-1,NULL,0,NULL,NULL);utf=malloc(n);if(!utf) goto done;WideCharToMultiByte(CP_UTF8,0,api,-1,utf,n,NULL,NULL);
    text_add(&script,"# -*- coding: utf-8 -*-\n# Macro lengto 0.6 - Python 3.8+, no additional packages.\n# Zero-based pixel indices, top-left origin. RGB from the source raster.\n# Batch example: python macro.py --input drawing.tif --output measured.pdf\n# Change COORDINATES or use --coordinates absolute / relative.\nCOORDINATES = \"%s\"\n\ndef run(app, input_file=None, output_file=None):\n",r->relative ? "relative" : "absolute");
    text_add(&script,"%s\n\n# --- Python support: you can leave this section unchanged ---\n%s\n\nif __name__ == \"__main__\":\n    _main(run)\n",r->body.len ? r->body.data : "    pass\n",utf);
    if(script.failed) goto done;temp=malloc((wcslen(path)+48)*sizeof(wchar_t));if(!temp) goto done;swprintf(temp,wcslen(path)+48,L"%ls.%lu.tmp",path,GetCurrentProcessId());
    f=_wfopen(temp,L"wb");if(!f) goto done;ok=fwrite(script.data,1,script.len,f)==script.len;if(fflush(f)) ok=0;
    if(ok && !FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(f)))) ok=0;if(fclose(f)) ok=0;f=NULL;
    if(ok) ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
done:if(f) fclose(f);if(temp && !ok) DeleteFileW(temp);free(temp);free(utf);free(api);text_free(&script);
    if(ok) recorder_reset(r);return ok;
}
