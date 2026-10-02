#define _CRT_SECURE_NO_WARNINGS
#include "render.h"
#include <math.h>
#include <wchar.h>
static const COLORREF colors[]={RGB(0,119,126),RGB(185,89,47),RGB(114,78,159),RGB(39,99,166),RGB(150,69,110),RGB(98,125,59)};
static COLORREF color_for(int id) { return colors[(id-1)%6]; }
POINT view_point(const View *v,Point p) {
    POINT s={(LONG)lround(fmax(-1e8,fmin(1e8,p.x*v->sx+v->ox))),(LONG)lround(fmax(-1e8,fmin(1e8,p.y*v->sy+v->oy)))};return s;
}
static RECT label_rect(HDC dc,const View *v,POINT a,POINT b,const wchar_t *label) {
    SIZE size;RECT r;int x,y;
    GetTextExtentPoint32W(dc,label,(int)wcslen(label),&size);
    x=(a.x+b.x)/2-size.cx/2;y=(a.y+b.y)/2-size.cy-8;
    x=max(5,min(v->width-size.cx-7,x));y=max(4,min(v->height-size.cy-7,y));
    r.left=x-4;r.top=y-2;r.right=x+size.cx+4;r.bottom=y+size.cy+2;return r;
}
void render_segment(HDC dc,const View *v,Point a,Point b,COLORREF color,int reference,int selected,const wchar_t *label) {
    POINT s=view_point(v,a),t=view_point(v,b);HPEN pen;HGDIOBJ old,brush;RECT r;
    if(max(s.x,t.x)<-10 || min(s.x,t.x)>v->width+10 || max(s.y,t.y)<-10 || min(s.y,t.y)>v->height+10) return;
    pen=CreatePen(reference ? PS_DASH : PS_SOLID,reference ? 1 : selected ? 3 : 2,color);
    old=SelectObject(dc,pen);brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
    MoveToEx(dc,s.x,s.y,NULL);LineTo(dc,t.x,t.y);
    if(selected) { Ellipse(dc,s.x-4,s.y-4,s.x+5,s.y+5);Ellipse(dc,t.x-4,t.y-4,t.x+5,t.y+5); }
    else { MoveToEx(dc,s.x-3,s.y-3,NULL);LineTo(dc,s.x+4,s.y+4);MoveToEx(dc,t.x-3,t.y-3,NULL);LineTo(dc,t.x+4,t.y+4); }
    SelectObject(dc,brush);SelectObject(dc,old);DeleteObject(pen);
    if(label && *label) {
        SelectObject(dc,v->font);r=label_rect(dc,v,s,t,label);
        FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
        TextOutW(dc,r.left+4,r.top+2,label,(int)wcslen(label));
    }
}
static void render_bitmap(HDC dc,const Document *d,const View *v) {
    double px=v->sx*d->logical_width/d->width,py=v->sy*d->logical_height/d->height;
    int x0,x1,y0,y1;
    if(px<=0 || py<=0 || v->width<=0 || v->height<=0) return;
    x0=(int)fmax(0,fmin(d->width,floor(-v->ox/px)));x1=(int)fmax(0,fmin(d->width,ceil((v->width-v->ox)/px)));
    y0=(int)fmax(0,fmin(d->height,floor(-v->oy/py)));y1=(int)fmax(0,fmin(d->height,ceil((v->height-v->oy)/py)));
    if(x1<=x0 || y1<=y0) return;
    if(px>=16 || py>=16) {
        /* At extreme zoom draw only visible pixel cells. No enormous GDI blit,
           off-screen bitmap or destination coordinate can overflow here. */
        int x,y;HBRUSH brush=(HBRUSH)GetStockObject(DC_BRUSH);COLORREF previous=GetDCBrushColor(dc);
        for(y=y0;y<y1;++y) for(x=x0;x<x1;++x) {
            const unsigned char *c=d->pixels+((size_t)y*d->width+x)*4;
            RECT r={(LONG)lround(fmax(0,fmin(v->width,v->ox+x*px))),
                    (LONG)lround(fmax(0,fmin(v->height,v->oy+y*py))),
                    (LONG)lround(fmax(0,fmin(v->width,v->ox+(x+1)*px))),
                    (LONG)lround(fmax(0,fmin(v->height,v->oy+(y+1)*py)))};
            SetDCBrushColor(dc,RGB(c[2],c[1],c[0]));FillRect(dc,&r,brush);
        }SetDCBrushColor(dc,previous);
    } else {
        HDC source=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(source,d->bitmap);
        int left=(int)lround(v->ox+x0*px),top=(int)lround(v->oy+y0*py);
        int right=(int)lround(v->ox+x1*px),bottom=(int)lround(v->oy+y1*py);
        SetStretchBltMode(dc,px>=1 && py>=1 ? COLORONCOLOR : HALFTONE);SetBrushOrgEx(dc,0,0,NULL);
        StretchBlt(dc,left,top,right-left,bottom-top,source,x0,y0,x1-x0,y1-y0,SRCCOPY);
        SelectObject(source,old);DeleteDC(source);
    }
}
void render_drawing(HDC dc,const Document *d,const Project *p,const View *v) {
    int i;wchar_t label[256];
    if(!d->bitmap) return;render_bitmap(dc,d,v);
    for(i=0;i<p->nc;++i) { const Calibration *c=&p->cal[i];if(c->page!=d->page) continue;
        swprintf(label,256,L"%ls · %.10g %ls",c->name,c->known,unit_name(c->unit));
        render_segment(dc,v,c->a,c->b,color_for(c->id),1,0,label);
    }
    for(i=0;i<p->nm;++i) { const Measurement *m=&p->meas[i];if(m->page!=d->page) continue;
        format_length(measured_metres(p,m),p->display_unit,p->precision_exp,label,256);
        render_segment(dc,v,m->a,m->b,color_for(m->calibration),0,i==v->selected,label);
    }
}
int hit_measurement(const Project *p,int page,const View *v,int x,int y) {
    int i,best=-1;double nearest=8;HDC dc=CreateCompatibleDC(NULL);HGDIOBJ old=SelectObject(dc,v->font);
    for(i=p->nm-1;i>=0;--i) {
        const Measurement *m=&p->meas[i];POINT a,b;double dx,dy,l,t,dist;wchar_t label[256];RECT r;POINT pt={x,y};
        if(m->page!=page) continue;a=view_point(v,m->a);b=view_point(v,m->b);
        if(max(a.x,b.x)<-10 || min(a.x,b.x)>v->width+10 || max(a.y,b.y)<-10 || min(a.y,b.y)>v->height+10) continue;
        dx=b.x-a.x;dy=b.y-a.y;l=dx*dx+dy*dy;t=l>0 ? ((x-a.x)*dx+(y-a.y)*dy)/l : 0;t=fmax(0,fmin(1,t));
        dist=hypot(x-a.x-t*dx,y-a.y-t*dy);
        format_length(measured_metres(p,m),p->display_unit,p->precision_exp,label,256);r=label_rect(dc,v,a,b,label);
        if(PtInRect(&r,pt)) { best=i;break; }if(dist<nearest) { nearest=dist;best=i; }
    }
    SelectObject(dc,old);DeleteDC(dc);return best;
}
HBITMAP render_export(const Document *d,const Project *p) {
    BITMAPINFO bi={0};HBITMAP bitmap;HDC dc;HGDIOBJ old;unsigned char *pixels;
    HFONT font;View v={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=d->width;bi.bmiHeader.biHeight=-d->height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void **)&pixels,NULL,0);if(!bitmap) return NULL;
    dc=CreateCompatibleDC(NULL);old=SelectObject(dc,bitmap);
    font=CreateFontW(-max(14,min(48,d->width/100)),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    v.sx=d->width/d->logical_width;v.sy=d->height/d->logical_height;v.width=d->width;v.height=d->height;v.selected=-1;v.font=font;
    render_drawing(dc,d,p,&v);SelectObject(dc,old);DeleteDC(dc);DeleteObject(font);return bitmap;
}
