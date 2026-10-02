#ifndef P2L_MODEL_H
#define P2L_MODEL_H

#include <windows.h>
#include <stddef.h>

#define P2L_MAX_CAL 1
#define P2L_MAX_MEAS 4096
#define P2L_NAME 80
#define P2L_PATH 32768

typedef struct { double x, y; } Point;
typedef enum { UNIT_MM, UNIT_CM, UNIT_M, UNIT_ANGSTROM, UNIT_NM, UNIT_UM,
    UNIT_DM, UNIT_DAM, UNIT_HM, UNIT_KM, UNIT_MEGAM, UNIT_GIGAM, UNIT_AU,
    UNIT_TERAM, UNIT_PETAM, UNIT_LY, UNIT_COUNT } Unit;
typedef struct {
    int id, page;
    wchar_t name[P2L_NAME];
    Point a, b;
    double known;
    Unit unit;
} Calibration;
typedef struct {
    int id, page, calibration;
    wchar_t name[P2L_NAME];
    Point a, b;
} Measurement;
typedef struct {
    Calibration cal[P2L_MAX_CAL];
    Measurement meas[P2L_MAX_MEAS];
    int nc, nm, next_cal, next_meas, page_count;
    Unit display_unit;
    int precision_exp;
} Project;

void project_init(Project *p, int pages);
double point_distance(Point a, Point b);
int point_valid(Point a);
int parse_positive(const wchar_t *text, double *value);
const wchar_t *unit_name(Unit u);
const wchar_t *unit_description(Unit u);
double unit_metres(Unit u);
Unit unit_order(int index);
double measured_metres(const Project *p, const Measurement *m);
double rounded_metres(double value, int exponent);
void format_length(double metres, Unit unit, int exponent, wchar_t *text, size_t cap);
Calibration *find_cal(Project *p, int id);
const Calibration *get_cal(const Project *p, int id);
int add_cal(Project *p, int page, const wchar_t *name, Point a, Point b, double known, Unit unit);
int add_meas(Project *p, int page, int cal, const wchar_t *name, Point a, Point b);
double measured_length(const Project *p, const Measurement *m);
void remove_meas(Project *p, int index);
int project_valid(const Project *p);

#endif
