#define _CRT_SECURE_NO_WARNINGS
#include "script_runner.h"
#include "script_json.h"
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <process.h>
struct ScriptRunner {
    HWND owner;HANDLE process,job,output,input,errors,reader,logger;CRITICAL_SECTION lock;
    char log[8192];size_t log_size;volatile LONG cancelled;int ended;
};
static void log_text(ScriptRunner *r,const char *text,size_t count) {
    EnterCriticalSection(&r->lock);if(count>=sizeof(r->log)) { text+=count-sizeof(r->log)+1;count=sizeof(r->log)-1; }
    if(r->log_size+count>=sizeof(r->log)) { size_t remove=r->log_size+count-sizeof(r->log)+1;memmove(r->log,r->log+remove,r->log_size-remove);r->log_size-=remove; }
    memcpy(r->log+r->log_size,text,count);r->log_size+=count;r->log[r->log_size]=0;LeaveCriticalSection(&r->lock);
}
static unsigned __stdcall read_errors(void *arg) { ScriptRunner *r=arg;char b[2048];DWORD n;while(ReadFile(r->errors,b,sizeof(b),&n,NULL) && n) log_text(r,b,n);return 0; }
static unsigned __stdcall read_commands(void *arg) {
    ScriptRunner *r=arg;char b[4096];DWORD n,i;Text line={0};
    while(!r->cancelled && ReadFile(r->output,b,sizeof(b),&n,NULL) && n) for(i=0;i<n && !r->cancelled;++i) {
        if(b[i]=='\n') {
            char *reply=(char *)SendMessageW(r->owner,WM_SCRIPT_REQUEST,0,(LPARAM)(line.data ? line.data : ""));DWORD sent;size_t size;
            if(!reply) goto done;size=strlen(reply);if(!WriteFile(r->input,reply,(DWORD)size,&sent,NULL) || sent!=size || !WriteFile(r->input,"\n",1,&sent,NULL)) { free(reply);goto done; }free(reply);line.len=0;if(line.data) line.data[0]=0;
        } else {
            if(!b[i] || line.len>=1024*1024) { log_text(r,"Script request is invalid or too long.\n",strlen("Script request is invalid or too long.\n"));runner_cancel(r);goto done; }
            text_add(&line,"%c",b[i]);if(line.failed) { runner_cancel(r);goto done; }
        }
    }
done:text_free(&line);return 0;
}
static wchar_t *environment(void) {
    wchar_t *original=GetEnvironmentStringsW(),*p,*result,*out,exe[32768];size_t length=0;if(!original) return NULL;
    for(p=original;*p;p+=wcslen(p)+1) length+=wcslen(p)+1;GetModuleFileNameW(NULL,exe,32768);
    result=calloc(length+wcslen(exe)+128,sizeof(wchar_t));if(!result) { FreeEnvironmentStringsW(original);return NULL; }out=result;
    for(p=original;*p;p+=wcslen(p)+1) if(_wcsnicmp(p,L"LENGTO_HOSTED=",14) && _wcsnicmp(p,L"LENGTO_EXE=",11) && _wcsnicmp(p,L"PYTHONIOENCODING=",17) && _wcsnicmp(p,L"PYTHONUTF8=",11)) { size_t n=wcslen(p)+1;wmemcpy(out,p,n);out+=n; }
    wcscpy(out,L"LENGTO_HOSTED=1");out+=wcslen(out)+1;swprintf(out,wcslen(exe)+16,L"LENGTO_EXE=%ls",exe);out+=wcslen(out)+1;
    wcscpy(out,L"PYTHONIOENCODING=utf-8");out+=wcslen(out)+1;wcscpy(out,L"PYTHONUTF8=1");out+=wcslen(out)+1;*out=0;FreeEnvironmentStringsW(original);return result;
}
ScriptRunner *runner_start(HWND owner,const wchar_t *python,const wchar_t *script,wchar_t *error,size_t cap) {
    ScriptRunner *r=calloc(1,sizeof(*r));SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};STARTUPINFOW start={sizeof(start)};PROCESS_INFORMATION process={0};
    HANDLE child_input=NULL,child_output=NULL,child_errors=NULL;wchar_t *env=NULL,*cmd=NULL;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};const wchar_t *base=wcsrchr(python,L'\\');DWORD code;
    if(!r) return NULL;r->owner=owner;InitializeCriticalSection(&r->lock);
    if(!CreatePipe(&child_input,&r->input,&sa,0) || !CreatePipe(&r->output,&child_output,&sa,0) || !CreatePipe(&r->errors,&child_errors,&sa,0)) goto fail;
    SetHandleInformation(r->input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(r->output,HANDLE_FLAG_INHERIT,0);SetHandleInformation(r->errors,HANDLE_FLAG_INHERIT,0);
    r->job=CreateJobObjectW(NULL,NULL);limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!r->job || !SetInformationJobObject(r->job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) goto fail;
    env=environment();cmd=malloc((wcslen(python)+wcslen(script)+64)*sizeof(wchar_t));if(!env || !cmd) goto fail;
    swprintf(cmd,wcslen(python)+wcslen(script)+64,L"\"%ls\" %ls-X utf8 -u \"%ls\"",python,!_wcsicmp(base ? base+1 : python,L"py.exe") ? L"-3 " : L"",script);
    start.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;start.hStdInput=child_input;start.hStdOutput=child_output;start.hStdError=child_errors;
    if(!CreateProcessW(python,cmd,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|CREATE_SUSPENDED,env,NULL,&start,&process)) goto fail;
    r->process=process.hProcess;
    if(!AssignProcessToJobObject(r->job,r->process)) { TerminateProcess(r->process,1);goto fail; }
    CloseHandle(child_input);child_input=NULL;CloseHandle(child_output);child_output=NULL;CloseHandle(child_errors);child_errors=NULL;
    r->reader=(HANDLE)_beginthreadex(NULL,0,read_commands,r,0,NULL);r->logger=(HANDLE)_beginthreadex(NULL,0,read_errors,r,0,NULL);
    if(!r->reader || !r->logger) { TerminateJobObject(r->job,1);goto fail; }
    ResumeThread(process.hThread);CloseHandle(process.hThread);free(env);free(cmd);return r;
fail:
    code=GetLastError();if(process.hThread) CloseHandle(process.hThread);if(child_input) CloseHandle(child_input);if(child_output) CloseHandle(child_output);if(child_errors) CloseHandle(child_errors);
    if(r->job) TerminateJobObject(r->job,1);if(r->reader) WaitForSingleObject(r->reader,INFINITE);if(r->logger) WaitForSingleObject(r->logger,INFINITE);
    swprintf(error,cap,L"Cannot start Python (Windows error %lu). Select Python 3.8 or later.",code);free(env);free(cmd);runner_free(r);return NULL;
}
void runner_cancel(ScriptRunner *r) { if(r) { InterlockedExchange(&r->cancelled,1);if(r->job) TerminateJobObject(r->job,1); } }
int runner_cancelled(ScriptRunner *r) { return r && r->cancelled; }
int runner_done(ScriptRunner *r,DWORD *exit_code) {
    if(!r || WaitForSingleObject(r->process,0)!=WAIT_OBJECT_0) return 0;
    if(!r->ended) { r->ended=1;TerminateJobObject(r->job,0); }
    if(WaitForSingleObject(r->reader,0)!=WAIT_OBJECT_0 || WaitForSingleObject(r->logger,0)!=WAIT_OBJECT_0) return 0;
    GetExitCodeProcess(r->process,exit_code);return 1;
}
wchar_t *runner_log(ScriptRunner *r) {
    int n;wchar_t *s;EnterCriticalSection(&r->lock);n=MultiByteToWideChar(CP_UTF8,0,r->log,-1,NULL,0);s=malloc((size_t)max(n,1)*sizeof(wchar_t));if(s) { if(n) MultiByteToWideChar(CP_UTF8,0,r->log,-1,s,n);else *s=0; }LeaveCriticalSection(&r->lock);return s;
}
void runner_free(ScriptRunner *r) {
    if(!r) return;
    if(r->reader) CloseHandle(r->reader);if(r->logger) CloseHandle(r->logger);if(r->input) CloseHandle(r->input);if(r->output) CloseHandle(r->output);if(r->errors) CloseHandle(r->errors);if(r->process) CloseHandle(r->process);if(r->job) CloseHandle(r->job);DeleteCriticalSection(&r->lock);free(r);
}
