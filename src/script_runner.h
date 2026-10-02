#ifndef LENGTO_SCRIPT_RUNNER_H
#define LENGTO_SCRIPT_RUNNER_H
#include <windows.h>
#define WM_SCRIPT_REQUEST (WM_APP+41)
typedef struct ScriptRunner ScriptRunner;
ScriptRunner *runner_start(HWND owner,const wchar_t *python,const wchar_t *script,wchar_t *error,size_t cap);
void runner_cancel(ScriptRunner *r);
int runner_cancelled(ScriptRunner *r);
int runner_done(ScriptRunner *r,DWORD *exit_code);
wchar_t *runner_log(ScriptRunner *r);
void runner_free(ScriptRunner *r);
#endif
