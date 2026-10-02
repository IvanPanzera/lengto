#ifndef LENGTO_AUTOMATION_H
#define LENGTO_AUTOMATION_H
#include "render.h"
#include "script_json.h"
enum { AUTO_CHANGED=1,AUTO_VIEW=2,AUTO_OPEN=4,AUTO_EXPORT=8 };
typedef struct {
    Document *document;Project *project;int changes,dirty;
    wchar_t output[P2L_PATH];ExportFormat format;int all_pages;
} Automation;
char *automation_execute(Automation *a,const char *request);
int automation_stdio(void);
int drawing_export(Document *doc,const Project *project,const wchar_t *path,ExportFormat format,int all_pages,wchar_t *error,size_t cap);
#endif
