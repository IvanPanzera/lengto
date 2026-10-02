#ifndef LENGTO_RECORDER_H
#define LENGTO_RECORDER_H
#include "automation.h"
typedef struct { Text body;int active,relative,opens,exports;wchar_t error[512]; } Recorder;
void recorder_reset(Recorder *r);
int recorder_start(Recorder *r,Document *d,const Project *p,int relative);
int recorder_save(Recorder *r,const wchar_t *path);
void recorder_open(Recorder *r,Document *document);
void recorder_page(Recorder *r,int page);
void recorder_close(Recorder *r);
void recorder_settings(Recorder *r,const Project *p);
void recorder_calibration(Recorder *r,Document *d,const Calibration *c);
void recorder_measurement(Recorder *r,Document *d,const Measurement *m,int edit);
void recorder_delete(Recorder *r,int id);
void recorder_export(Recorder *r,const wchar_t *path,int all_pages);
#endif
