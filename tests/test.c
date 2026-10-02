#define _CRT_SECURE_NO_WARNINGS
#include "../src/model.h"
#include "../src/document.h"
#include "../src/render.h"
#include "../src/portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include <string.h>

static int checks,failures;
#define CHECK(x) do { ++checks;if(!(x)) { ++failures;printf("FAIL line %d: %s\n",__LINE__,#x); } } while(0)
static int close_to(double a,double b) { return fabs(a-b)<1e-9; }
static void test_model(void) {
    Project *p=malloc(sizeof(*p));int c;double value;project_init(p,2);
    CHECK(!add_cal(p,0,L"Zero",(Point){1,1},(Point){1,1},1,UNIT_M));
    CHECK(!add_cal(p,0,L"NaN",(Point){0,0},(Point){1,1},NAN,UNIT_M));
    CHECK(!add_cal(p,0,L"Negative",(Point){0,0},(Point){1,1},-1,UNIT_M));
    CHECK(!add_cal(p,0,L"Invalid\tname",(Point){0,0},(Point){1,1},1,UNIT_M));
    c=add_cal(p,0,L"Calibration é Ω",(Point){0.5,0.5},(Point){1000.5,0.5},5,UNIT_M);CHECK(c>0);
    CHECK(!add_cal(p,0,L"Seconda",(Point){0,0},(Point){100,0},10,UNIT_CM));
    CHECK(!add_cal(p,1,L"Other page",(Point){0,0},(Point){100,0},10,UNIT_CM));CHECK(p->nc==1);
    CHECK(add_meas(p,0,c,L"Wall",(Point){0.5,0.5},(Point){640.5,0.5}));
    CHECK(add_meas(p,1,c,L"Other page",(Point){0.5,0.5},(Point){300.5,400.5}));
    CHECK(close_to(measured_metres(p,&p->meas[0]),3.2));CHECK(close_to(measured_metres(p,&p->meas[1]),2.5));
    find_cal(p,c)->known=10;CHECK(close_to(measured_metres(p,&p->meas[0]),6.4));CHECK(close_to(measured_metres(p,&p->meas[1]),5));
    find_cal(p,c)->unit=UNIT_CM;CHECK(close_to(measured_metres(p,&p->meas[0]),0.064));CHECK(close_to(measured_metres(p,&p->meas[1]),0.05));
    find_cal(p,c)->b.x=500.5;find_cal(p,c)->page=1;
    CHECK(close_to(measured_metres(p,&p->meas[0]),0.128));CHECK(close_to(measured_metres(p,&p->meas[1]),0.1));
    CHECK(!add_meas(p,-1,c,L"Negative page",(Point){0,0},(Point){3,4}));CHECK(!add_meas(p,2,c,L"Missing page",(Point){0,0},(Point){3,4}));
    CHECK(!add_meas(p,0,999,L"Missing reference",(Point){0,0},(Point){3,4}));CHECK(!add_meas(p,0,c,L"Same point",(Point){1,1},(Point){1,1}));
    CHECK(parse_positive(L" 2,75 ",&value) && close_to(value,2.75));CHECK(parse_positive(L"2.75",&value) && close_to(value,2.75));
    CHECK(!parse_positive(L"NaN",&value));CHECK(!parse_positive(L"0",&value));CHECK(!parse_positive(L"-3",&value));CHECK(!parse_positive(L"1,2.3",&value));CHECK(!parse_positive(L"3m",&value));
    CHECK(project_valid(p));p->meas[0].calibration=999;CHECK(!project_valid(p));p->meas[0].calibration=c;
    p->meas[0].page=2;CHECK(!project_valid(p));p->meas[0].page=0;
    remove_meas(p,0);CHECK(p->nm==1 && p->meas[0].page==1 && project_valid(p));remove_meas(p,0);CHECK(p->nm==0 && p->nc==1);free(p);
}
static void test_formats(const wchar_t *fixtures) {
    const wchar_t *files[]={L"drawing.png",L"drawing.jpg",L"drawing.tif",L"drawing.tiff",L"drawing-multipage.tif",L"drawing.pdf",L"drawing é Ω.png"};
    int i;wchar_t path[P2L_PATH],error[512];Document *d=calloc(1,sizeof(*d));
    for(i=0;i<7;++i) {
        swprintf(path,P2L_PATH,L"%ls\\%ls",fixtures,files[i]);
        CHECK(document_open(d,path,error,512));
        CHECK(d->width==1200 && d->height==900);
        CHECK(close_to(d->logical_width,1200) && close_to(d->logical_height,900));
        CHECK(d->pixels && d->pixels[0]>245 && d->pixels[1]>245 && d->pixels[2]>245);
        if(i==4 || i==5) {
            CHECK(d->pages==2);CHECK(document_page(d,1,error,512));CHECK(d->width==640 && d->height==480);
            CHECK(abs(d->pixels[0]-200)<=1 && abs(d->pixels[1]-120)<=1 && abs(d->pixels[2]-30)<=1);
            CHECK(!document_page(d,2,error,512));CHECK(d->page==1 && d->bitmap!=NULL);
            CHECK(document_page(d,0,error,512));CHECK(d->width==1200);
        } else CHECK(d->pages==1);
        document_close(d);
    }
    swprintf(path,P2L_PATH,L"%ls\\alpha.png",fixtures);CHECK(document_open(d,path,error,512));
    CHECK(d->pixels[0]==255 && d->pixels[1]==255 && d->pixels[2]==255);
    CHECK(d->pixels[(10*20+10)*4]==127 && d->pixels[(10*20+10)*4+2]==255);document_close(d);
    swprintf(path,P2L_PATH,L"%ls\\large.png",fixtures);CHECK(document_open(d,path,error,512));
    CHECK(d->logical_width==8000 && d->logical_height==4000);CHECK((double)d->width*d->height<=24.0*1024*1024);document_close(d);
    swprintf(path,P2L_PATH,L"%ls\\damaged.pdf",fixtures);CHECK(!document_open(d,path,error,512));document_close(d);
    swprintf(path,P2L_PATH,L"%ls\\damaged.png",fixtures);CHECK(!document_open(d,path,error,512));document_close(d);
    free(d);
}
static void test_units(void) {
    int i,id;double previous=0;wchar_t text[256];Project *p=malloc(sizeof(*p));project_init(p,1);
    for(i=0;i<UNIT_COUNT;++i) { Unit u=unit_order(i);CHECK(unit_metres(u)>previous);previous=unit_metres(u);CHECK(wcslen(unit_name(u))>0 && wcslen(unit_description(u))>0); }
    CHECK(unit_metres(UNIT_ANGSTROM)==1e-10);CHECK(unit_metres(UNIT_LY)==299792458.0*365.25*86400.0);
    for(i=-10;i<=16;++i) { double step=pow(10.0,i);CHECK(fabs(rounded_metres(step*1.26,i)/step-1)<1e-12);CHECK(fabs(rounded_metres(step*1.76,i)/step-2)<1e-12); }
    format_length(1.23456,UNIT_M,-3,text,256);CHECK(!wcscmp(text,L"1.235 m"));
    format_length(1.23456,UNIT_MM,-3,text,256);CHECK(!wcscmp(text,L"1235 mm"));
    format_length(1.23456,UNIT_CM,-3,text,256);CHECK(!wcscmp(text,L"123.5 cm"));
    format_length(4e-10,UNIT_ANGSTROM,-10,text,256);CHECK(!wcscmp(text,L"4 Å"));
    format_length(0.004,UNIT_M,-2,text,256);CHECK(!wcscmp(text,L"0.00 m"));
    format_length(unit_metres(UNIT_LY),UNIT_LY,-3,text,256);CHECK(!wcscmp(text,L"1 ly"));
    id=add_cal(p,0,L"Microscopy",(Point){0.5,0.5},(Point){100.5,0.5},250,UNIT_ANGSTROM);
    CHECK(id && add_meas(p,0,id,L"Detail",(Point){0.5,0.5},(Point){50.5,0.5}));CHECK(fabs(measured_metres(p,&p->meas[0])-1.25e-8)<1e-20);
    p->display_unit=UNIT_NM;p->precision_exp=-10;CHECK(project_valid(p));
    p->precision_exp=-11;CHECK(!project_valid(p));p->precision_exp=17;CHECK(!project_valid(p));p->precision_exp=16;p->display_unit=UNIT_COUNT;CHECK(!project_valid(p));free(p);
}
static void test_zoom(void) {
    BITMAPINFO info={0};Document *d=calloc(1,sizeof(*d));Project *p=malloc(sizeof(*p));View v={0};
    HDC dc=CreateCompatibleDC(NULL);HBITMAP canvas;HGDIOBJ old;unsigned char *pixels;int i;
    const double scales[]={0.5,1,8,15.9,16,40,160,1200,1000000};
    project_init(p,1);info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=2;info.bmiHeader.biHeight=-2;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    d->bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&d->pixels,NULL,0);CHECK(d->bitmap!=NULL);d->width=d->height=2;d->logical_width=d->logical_height=2;
    /* Red, green / blue, white: expose clipping, orientation and vanished pixels. */
    { const unsigned char source[]={0,0,255,255,0,255,0,255,255,0,0,255,255,255,255,255};memcpy(d->pixels,source,sizeof(source)); }
    info.bmiHeader.biWidth=160;info.bmiHeader.biHeight=-100;canvas=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);CHECK(canvas!=NULL);old=SelectObject(dc,canvas);v.width=160;v.height=100;v.selected=-1;
    for(i=0;i<(int)(sizeof(scales)/sizeof(scales[0]));++i) {
        double scale=scales[i];int x=81,y=51;v.sx=v.sy=scale;v.ox=80;v.oy=50;memset(pixels,70,160*100*4);
        if(scale<2) { v.ox=79;v.oy=49;x=79;y=49; }
        render_drawing(dc,d,p,&v);GdiFlush();
        if(scale<1) { const unsigned char *cell=pixels+((size_t)y*160+x)*4;CHECK(cell[0]>=120 && cell[0]<=140 && cell[1]>=120 && cell[1]<=140 && cell[2]>=120 && cell[2]<=140); }
        else CHECK(pixels[((size_t)y*160+x)*4+2]>200);
    }
    v.sx=v.sy=1200;v.ox=-1800;v.oy=-600;render_drawing(dc,d,p,&v);GdiFlush();CHECK(pixels[(50*160+80)*4+1]==255 && pixels[(50*160+80)*4+2]==0);
    v.ox=-600;v.oy=-1800;render_drawing(dc,d,p,&v);GdiFlush();CHECK(pixels[(50*160+80)*4]==255 && pixels[(50*160+80)*4+2]==0);
    v.ox=-1200;v.oy=-600;render_drawing(dc,d,p,&v);GdiFlush();CHECK(pixels[1]==255 && pixels[2]==0);
    v.ox=200;v.oy=200;memset(pixels,70,160*100*4);render_drawing(dc,d,p,&v);GdiFlush();CHECK(pixels[0]==70 && pixels[2]==70);
    /* A reduced preview pixel can span much more than the GDI coordinate limit. */
    d->logical_width=d->logical_height=20000;v.ox=-6000000;v.oy=-6000000;render_drawing(dc,d,p,&v);GdiFlush();CHECK(pixels[2]==255 && pixels[1]==0);
    SelectObject(dc,old);DeleteObject(canvas);DeleteDC(dc);DeleteObject(d->bitmap);free(d);free(p);
}
static int colored_at(const Document *d,int x,int y) {
    const unsigned char *px=d->pixels+((size_t)y*d->width+x)*4;
    return px[0]>80 && px[1]>80 && px[2]<60;
}
static void test_embedded_pdf(const wchar_t *fixtures,const wchar_t *output) {
    wchar_t *licenses=portable_text(RESOURCE_LICENSES),*guide=portable_text(RESOURCE_GUIDE),path[P2L_PATH],error[512];
    Document *d=calloc(1,sizeof(*d));Project *p=malloc(sizeof(*p));HBITMAP bitmap;ExportJob *job;
    CHECK(licenses && wcsstr(licenses,L"pdfium.txt") && wcsstr(licenses,L"libjpeg_turbo") && wcslen(licenses)>100000);
    CHECK(guide && wcsstr(guide,L"0.6") && wcsstr(guide,L"temporary") && wcsstr(guide,L"find_pixels") && wcsstr(guide,L"\r\n\r\n"));free(licenses);free(guide);
    /* The PDF engine has not been initialized by a PDF input before this test. */
    swprintf(path,P2L_PATH,L"%ls\\drawing.png",fixtures);CHECK(document_open(d,path,error,512));project_init(p,1);bitmap=render_export(d,p);CHECK(bitmap!=NULL);
    swprintf(path,P2L_PATH,L"%ls\\from raster é Ω.pdf",output);job=export_begin(path,EXPORT_PDF);CHECK(job!=NULL);
    if(job) { CHECK(export_append(job,bitmap,600,450));CHECK(export_finish(job));document_close(d);CHECK(document_open(d,path,error,512));CHECK(d->width==1200 && d->height==900); }
    document_close(d);
    swprintf(path,P2L_PATH,L"%ls\\missing-lengto-folder\\output.pdf",output);job=export_begin(path,EXPORT_PDF);CHECK(job!=NULL);
    if(job) { CHECK(export_append(job,bitmap,600,450));CHECK(!export_finish(job));CHECK(wcsstr(export_error(),L"folder")!=NULL); }
    /* A locked existing PDF must survive a failed replacement. */
    swprintf(path,P2L_PATH,L"%ls\\from raster é Ω.pdf",output);
    { HANDLE lock=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(lock!=INVALID_HANDLE_VALUE);
      job=export_begin(path,EXPORT_PDF);CHECK(job!=NULL);if(job) { CHECK(export_append(job,bitmap,600,450));CHECK(!export_finish(job));CHECK(wcsstr(export_error(),L"other programs")!=NULL); }if(lock!=INVALID_HANDLE_VALUE) CloseHandle(lock);
    }
    CHECK(document_open(d,path,error,512));CHECK(d->pages==1);document_close(d);DeleteObject(bitmap);free(d);free(p);
}
static void test_exports(const wchar_t *fixtures,const wchar_t *output) {
    Document *d=calloc(1,sizeof(*d)),*read=calloc(1,sizeof(*read));Project *p=malloc(sizeof(*p));
    const wchar_t *extensions[]={L"tif",L"jpg",L"png",L"pdf"};wchar_t path[P2L_PATH],source[P2L_PATH],error[512];int i,c;HBITMAP rendered;ExportJob *job;FILE *f;char buffer[16]={0};
    swprintf(source,P2L_PATH,L"%ls\\drawing.pdf",fixtures);CHECK(document_open(d,source,error,512));project_init(p,2);
    c=add_cal(p,0,L"Scale",(Point){100,130},(Point){600,130},5,UNIT_M);CHECK(c>0);
    CHECK(add_meas(p,0,c,L"Exported measurement",(Point){200,500},(Point){600,500}));rendered=render_export(d,p);CHECK(rendered!=NULL);
    for(i=0;i<4;++i) {
        swprintf(path,P2L_PATH,L"%ls\\export.%ls",output,extensions[i]);job=export_begin(path,(ExportFormat)i);CHECK(job!=NULL);if(!job) continue;
        CHECK(export_append(job,rendered,600,450));CHECK(export_finish(job));CHECK(document_open(read,path,error,512));if(!read->bitmap) continue;
        CHECK(read->width==1200 && read->height==900 && read->pages==1);CHECK(colored_at(read,400,500));
        /* Top and bottom are asymmetric: catch upside-down PDF image matrices. */
        CHECK(read->pixels[((size_t)180*read->width+300)*4]<100);CHECK(read->pixels[((size_t)500*read->width+1000)*4]>245);document_close(read);
    }
    for(i=0;i<4;i+=3) {
        HBITMAP second;swprintf(path,P2L_PATH,L"%ls\\multipage.%ls",output,extensions[i]);job=export_begin(path,(ExportFormat)i);CHECK(job!=NULL);
        CHECK(export_append(job,rendered,600,450));CHECK(document_page(d,1,error,512));second=render_export(d,p);CHECK(second!=NULL);CHECK(export_append(job,second,320,240));DeleteObject(second);
        CHECK(export_finish(job));CHECK(document_open(read,path,error,512));CHECK(read->pages==2);CHECK(document_page(read,1,error,512));CHECK(read->width==640 && read->height==480);
        CHECK(abs(read->pixels[0]-200)<=1 && abs(read->pixels[1]-120)<=1 && abs(read->pixels[2]-30)<=1);document_close(read);CHECK(document_page(d,0,error,512));
    }
    swprintf(path,P2L_PATH,L"%ls\\export-preserve.png",output);f=_wfopen(path,L"wb");fwrite("preserve",1,8,f);fclose(f);
    job=export_begin(path,EXPORT_PNG);CHECK(job!=NULL);CHECK(!export_finish(job));f=_wfopen(path,L"rb");fread(buffer,1,8,f);fclose(f);CHECK(!strcmp(buffer,"preserve"));
    job=export_begin(path,EXPORT_PDF);CHECK(job!=NULL);export_abort(job);f=_wfopen(path,L"rb");fread(buffer,1,8,f);fclose(f);CHECK(!strcmp(buffer,"preserve"));
    DeleteObject(rendered);document_close(d);document_close(read);free(d);free(read);free(p);
}
static void test_clipboard_images(const wchar_t *fixtures) {
    Document *d=calloc(1,sizeof(*d)),*copy=calloc(1,sizeof(*copy));wchar_t path[P2L_PATH],error[512];unsigned char rgb[12],*png;size_t size;
    struct { BITMAPINFOHEADER header;unsigned char pixels[16]; } dib={0};
    dib.header.biSize=sizeof(BITMAPINFOHEADER);dib.header.biWidth=2;dib.header.biHeight=2;dib.header.biPlanes=1;dib.header.biBitCount=24;
    /* Bottom-up padded rows: blue/white, then red/green. */
    { const unsigned char pixels[]={255,0,0,255,255,255,0,0,0,0,255,0,255,0,0,0};memcpy(dib.pixels,pixels,sizeof(pixels)); }
    CHECK(document_from_dib(d,&dib,sizeof(dib),error,512));CHECK(d->clipboard && d->pages==1 && d->width==2 && d->height==2);
    CHECK(document_rgb(d,0,0,2,2,rgb));CHECK(rgb[0]==255 && rgb[1]==0 && rgb[2]==0 && rgb[3]==0 && rgb[4]==255 && rgb[8]==255 && rgb[9]==255);
    png=document_png(d,&size);CHECK(png && size>8);CHECK(document_from_memory(copy,png,size,error,512));free(png);CHECK(document_rgb(copy,1,0,1,1,rgb) && rgb[1]==255 && !rgb[0]);document_close(copy);document_close(d);
    dib.header.biHeight=-2;CHECK(document_from_dib(d,&dib,sizeof(dib),error,512));CHECK(document_rgb(d,0,0,1,1,rgb) && rgb[2]==255 && !rgb[0]);document_close(d);
    CHECK(!document_from_dib(d,&dib,sizeof(dib)-1,error,512));dib.header.biWidth=LONG_MAX;CHECK(!document_from_dib(d,&dib,sizeof(dib),error,512));dib.header.biWidth=2;dib.header.biHeight=LONG_MIN;CHECK(!document_from_dib(d,&dib,sizeof(dib),error,512));
    { struct { BITMAPINFOHEADER header;unsigned char pixel[4]; } opaque={0};
      opaque.header.biSize=40;opaque.header.biWidth=opaque.header.biHeight=1;opaque.header.biPlanes=1;opaque.header.biBitCount=32;opaque.pixel[2]=255;
      CHECK(document_from_dib(d,&opaque,sizeof(opaque),error,512));CHECK(document_rgb(d,0,0,1,1,rgb) && rgb[0]==255 && rgb[1]==0);document_close(d);
    }
    { struct { BITMAPV5HEADER header;unsigned char pixel[4]; } alpha={0};
      alpha.header.bV5Size=sizeof(BITMAPV5HEADER);alpha.header.bV5Width=1;alpha.header.bV5Height=-1;alpha.header.bV5Planes=1;alpha.header.bV5BitCount=32;alpha.header.bV5Compression=BI_BITFIELDS;
      alpha.header.bV5RedMask=0xff0000;alpha.header.bV5GreenMask=0xff00;alpha.header.bV5BlueMask=0xff;alpha.header.bV5AlphaMask=0xff000000;alpha.header.bV5CSType=LCS_sRGB;alpha.pixel[2]=255;alpha.pixel[3]=128;
      CHECK(document_from_dib(d,&alpha,sizeof(alpha),error,512));CHECK(document_rgb(d,0,0,1,1,rgb) && rgb[0]==255 && rgb[1]==127 && rgb[2]==127);document_close(d);
    }
    { struct { BITMAPINFOHEADER header;RGBQUAD palette[2];unsigned char row[4]; } palette={0};
      palette.header.biSize=40;palette.header.biWidth=2;palette.header.biHeight=1;palette.header.biPlanes=1;palette.header.biBitCount=1;palette.palette[1].rgbRed=255;palette.row[0]=0x40;
      CHECK(document_from_dib(d,&palette,sizeof(palette),error,512));CHECK(document_rgb(d,0,0,2,1,rgb) && !rgb[0] && rgb[3]==255 && !rgb[4]);document_close(d);
    }
    swprintf(path,P2L_PATH,L"%ls\\alpha.png",fixtures);CHECK(document_open_image(d,path,error,512));CHECK(d->clipboard && document_rgb(d,10,10,1,1,rgb) && rgb[0]==255 && rgb[1]==127);document_close(d);
    CHECK(!document_from_memory(d,"not an image",12,error,512));CHECK(!d->bitmap && !d->decoder && !d->source_stream);free(copy);free(d);
}
int wmain(int argc,wchar_t **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc!=3) return 2;
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);CHECK(imaging_init());
    test_model();test_embedded_pdf(argv[1],argv[2]);test_formats(argv[1]);test_units();test_exports(argv[1],argv[2]);test_zoom();test_clipboard_images(argv[1]);imaging_shutdown();CoUninitialize();
    printf("%d checks, %d failures\n",checks,failures);return failures ? 1 : 0;
}
