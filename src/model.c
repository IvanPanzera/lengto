#define _CRT_SECURE_NO_WARNINGS
#include "model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <limits.h>


void project_init(Project *p, int pages) {
    memset(p, 0, sizeof(*p));
    p->next_cal = p->next_meas = 1;
    p->page_count = pages;
    p->display_unit = UNIT_M;
    p->precision_exp = -3;
}
double point_distance(Point a, Point b) { return hypot(b.x-a.x, b.y-a.y); }
int point_valid(Point a) { return isfinite(a.x) && isfinite(a.y) && a.x >= 0 && a.y >= 0 && a.x <= 1e9 && a.y <= 1e9; }
int parse_positive(const wchar_t *text, double *value) {
    wchar_t buf[128], *end;
    size_t n = wcslen(text), i;
    if (!n || n >= 128) return 0;
    for (i = 0; i <= n; ++i) buf[i] = text[i] == L',' ? L'.' : text[i];
    *value = wcstod(buf, &end);
    if (end == buf) return 0;
    while (iswspace(*end)) ++end;
    return !*end && isfinite(*value) && *value > 0 && *value <= 1e30;
}
typedef struct { const wchar_t *symbol, *description; double metres; } UnitInfo;
static const UnitInfo units[UNIT_COUNT] = {
    {L"mm",L"Millimetre",1e-3}, {L"cm",L"Centimetre",1e-2}, {L"m",L"Metre",1},
    {L"Å",L"Angstrom",1e-10}, {L"nm",L"Nanometre",1e-9}, {L"µm",L"Micrometre",1e-6},
    {L"dm",L"Decimetre",1e-1}, {L"dam",L"Decametre",1e1}, {L"hm",L"Hectometre",1e2},
    {L"km",L"Kilometre",1e3}, {L"Mm",L"Megametre",1e6}, {L"Gm",L"Gigametre",1e9},
    {L"AU",L"Astronomical unit",149597870700.0}, {L"Tm",L"Terametre",1e12},
    {L"Pm",L"Petametre",1e15}, {L"ly",L"Light-year",9460730472580800.0}
};
const wchar_t *unit_name(Unit u) {
    return u>=0 && u<UNIT_COUNT ? units[u].symbol : L"?";
}
const wchar_t *unit_description(Unit u) { return u>=0 && u<UNIT_COUNT ? units[u].description : L"?"; }
double unit_metres(Unit u) { return u>=0 && u<UNIT_COUNT ? units[u].metres : NAN; }
Unit unit_order(int index) {
    static const Unit order[]={UNIT_ANGSTROM,UNIT_NM,UNIT_UM,UNIT_MM,UNIT_CM,UNIT_DM,UNIT_M,UNIT_DAM,UNIT_HM,UNIT_KM,UNIT_MEGAM,UNIT_GIGAM,UNIT_AU,UNIT_TERAM,UNIT_PETAM,UNIT_LY};
    return index>=0 && index<UNIT_COUNT ? order[index] : UNIT_M;
}
double measured_metres(const Project *p,const Measurement *m) {
    const Calibration *c=get_cal(p,m->calibration);
    return c ? measured_length(p,m)*unit_metres(c->unit) : NAN;
}
double rounded_metres(double value,int exponent) {
    double step=pow(10.0,exponent),ratio=value/step;
    /* Beyond 2^52, rounding cannot add precision to an IEEE double. */
    return fabs(ratio)<4503599627370496.0 ? round(ratio)*step : value;
}
void format_length(double metres,Unit unit,int exponent,wchar_t *text,size_t cap) {
    double value=rounded_metres(metres,exponent)/unit_metres(unit);
    double step=pow(10.0,exponent)/unit_metres(unit);
    int decimals=(int)fmax(0,ceil(-log10(step)-1e-10));
    if(!isfinite(value)) { swprintf(text,cap,L"—");return; }
    if(value==0) value=0;
    if(decimals<=10 && fabs(value)<1e12) swprintf(text,cap,L"%.*f %ls",decimals,value,unit_name(unit));
    else swprintf(text,cap,L"%.14g %ls",value,unit_name(unit));
}
const Calibration *get_cal(const Project *p, int id) {
    int i;
    for (i=0; i<p->nc; ++i) if (p->cal[i].id == id) return &p->cal[i];
    return NULL;
}
Calibration *find_cal(Project *p, int id) { return (Calibration *)get_cal(p,id); }
static int valid_name(const wchar_t *name) {
    size_t i, n=wcsnlen(name,P2L_NAME);
    if (!n || n >= P2L_NAME) return 0;
    for (i=0;i<n;++i) if (name[i] < 32) return 0;
    return 1;
}
static int segment_valid(Point a, Point b) {
    return point_valid(a) && point_valid(b) && point_distance(a,b) > 1e-6;
}
int add_cal(Project *p, int page, const wchar_t *name, Point a, Point b, double known, Unit unit) {
    Calibration *c;
    if (p->nc >= P2L_MAX_CAL || p->next_cal == INT_MAX || page < 0 || page >= p->page_count ||
        !valid_name(name) || !segment_valid(a,b) || !isfinite(known) || known <= 0 || known > 1e30 || unit < 0 || unit >= UNIT_COUNT) return 0;
    c = &p->cal[p->nc++];
    c->id=p->next_cal++; c->page=page; c->a=a; c->b=b; c->known=known; c->unit=unit;
    wcscpy(c->name,name);
    return c->id;
}
int add_meas(Project *p, int page, int cal, const wchar_t *name, Point a, Point b) {
    Measurement *m;
    const Calibration *c=get_cal(p,cal);
    if (p->nm >= P2L_MAX_MEAS || p->next_meas == INT_MAX || !c || page < 0 || page >= p->page_count || !valid_name(name) || !segment_valid(a,b)) return 0;
    m=&p->meas[p->nm++]; m->id=p->next_meas++; m->page=page; m->calibration=cal; m->a=a; m->b=b;
    wcscpy(m->name,name);
    return m->id;
}
double measured_length(const Project *p, const Measurement *m) {
    const Calibration *c=get_cal(p,m->calibration);
    if (!c || m->page < 0 || m->page >= p->page_count) return NAN;
    return point_distance(m->a,m->b) * (c->known / point_distance(c->a,c->b));
}
void remove_meas(Project *p, int index) {
    if (index < 0 || index >= p->nm) return;
    memmove(&p->meas[index],&p->meas[index+1],(p->nm-index-1)*sizeof(Measurement)); --p->nm;
}
int project_valid(const Project *p) {
    int i,j,maxc=0,maxm=0;
    if (p->page_count<1 || p->nc<0 || p->nc>P2L_MAX_CAL || p->nm<0 || p->nm>P2L_MAX_MEAS) return 0;
    if(p->display_unit<0 || p->display_unit>=UNIT_COUNT || p->precision_exp<-10 || p->precision_exp>16) return 0;
    for(i=0;i<p->nc;++i) {
        const Calibration *c=&p->cal[i];
        if(c->id<1 || c->page<0 || c->page>=p->page_count || !valid_name(c->name) ||
           !segment_valid(c->a,c->b) || !isfinite(c->known) || c->known<=0 || c->known>1e30 || c->unit<0 || c->unit>=UNIT_COUNT) return 0;
        for(j=0;j<i;++j) if(p->cal[j].id==c->id) return 0;
        if(c->id>maxc) maxc=c->id;
    }
    for(i=0;i<p->nm;++i) {
        const Measurement *m=&p->meas[i]; const Calibration *c=get_cal(p,m->calibration);
        if(m->id<1 || !c || m->page<0 || m->page>=p->page_count || !valid_name(m->name) || !segment_valid(m->a,m->b) || !isfinite(measured_length(p,m))) return 0;
        for(j=0;j<i;++j) if(p->meas[j].id==m->id) return 0;
        if(m->id>maxm) maxm=m->id;
    }
    return p->next_cal>maxc && p->next_meas>maxm;
}
