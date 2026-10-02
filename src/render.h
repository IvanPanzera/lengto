#ifndef LENGTO_RENDER_H
#define LENGTO_RENDER_H
#include "document.h"
typedef struct {
    double sx,sy,ox,oy;
    int width,height,selected,active;
    HFONT font;
} View;
POINT view_point(const View *v,Point p);
void render_segment(HDC dc,const View *v,Point a,Point b,COLORREF color,int reference,int selected,const wchar_t *label);
void render_drawing(HDC dc,const Document *d,const Project *p,const View *v);
int hit_measurement(const Project *p,int page,const View *v,int x,int y);
HBITMAP render_export(const Document *d,const Project *p);
#endif
