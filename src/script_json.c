#define _CRT_SECURE_NO_WARNINGS
#include "script_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
void text_add(Text *b,const char *format,...) {
    va_list args,copy;int n;size_t need;char *next;if(b->failed) return;
    va_start(args,format);va_copy(copy,args);n=_vscprintf(format,copy);va_end(copy);
    if(n<0 || b->len+(size_t)n>64*1024*1024) { b->failed=1;va_end(args);return; }
    need=b->len+(size_t)n+1;
    if(need>b->cap) { size_t cap=max(need,max((size_t)256,b->cap*2));next=realloc(b->data,cap);if(!next) { b->failed=1;va_end(args);return; }b->data=next;b->cap=cap; }
    vsnprintf(b->data+b->len,b->cap-b->len,format,args);b->len+=(size_t)n;va_end(args);
}
void text_free(Text *b) { free(b->data);memset(b,0,sizeof(*b)); }
char *json_quote(const wchar_t *s) {
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,NULL,0,NULL,NULL);char *utf;Text out={0};unsigned char *p;
    if(!n) return NULL;utf=malloc(n);if(!utf) return NULL;WideCharToMultiByte(CP_UTF8,0,s,-1,utf,n,NULL,NULL);text_add(&out,"\"");
    for(p=(unsigned char *)utf;*p;++p) {
        if(*p=='"' || *p=='\\') text_add(&out,"\\%c",*p);
        else if(*p<32) text_add(&out,"\\u%04x",*p);else text_add(&out,"%c",*p);
    }text_add(&out,"\"");free(utf);if(out.failed) { text_free(&out);return NULL; }return out.data;
}
static void space(const char **p) { while(**p==' ' || **p=='\t' || **p=='\r' || **p=='\n') ++*p; }
static int hex4(const char **p,unsigned *value) {
    int i;*value=0;for(i=0;i<4;++i) { unsigned char c=(unsigned char)*(*p)++;int v=c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : -1;if(v<0) return 0;*value=(*value<<4)|(unsigned)v; }return 1;
}
static char *string(const char **p) {
    Text b={0};if(*(*p)++!='"') return NULL;
    while(**p && **p!='"') {
        unsigned char c=(unsigned char)*(*p)++;if(c<32) goto fail;
        if(c!='\\') { text_add(&b,"%c",c);continue; }
        c=(unsigned char)*(*p)++;
        if(c=='"' || c=='\\' || c=='/') text_add(&b,"%c",c);
        else if(c=='b') text_add(&b,"\b");else if(c=='f') text_add(&b,"\f");else if(c=='n') text_add(&b,"\n");else if(c=='r') text_add(&b,"\r");else if(c=='t') text_add(&b,"\t");
        else if(c=='u') {
            unsigned a,z;wchar_t w[2];int count=1,n;char utf[8];if(strlen(*p)<4 || !hex4(p,&a) || !a) goto fail;w[0]=(wchar_t)a;
            if(a>=0xd800 && a<=0xdbff) { if(strlen(*p)<6 || (*p)[0]!='\\' || (*p)[1]!='u') goto fail;*p+=2;if(!hex4(p,&z) || z<0xdc00 || z>0xdfff) goto fail;w[1]=(wchar_t)z;count=2; }
            else if(a>=0xdc00 && a<=0xdfff) goto fail;
            n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,count,utf,8,NULL,NULL);if(!n) goto fail;text_add(&b,"%.*s",n,utf);
        } else goto fail;
        if(b.failed) goto fail;
    }
    if(**p!='"') goto fail;++*p;if(!b.data) text_add(&b,"");
    if(b.failed || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b.data,-1,NULL,0)) goto fail;return b.data;
fail:text_free(&b);return NULL;
}
void json_free(Json *j) { int i;for(i=0;i<j->count;++i) free(j->fields[i].value);memset(j,0,sizeof(*j)); }
const JsonField *json_field(const Json *j,const char *key) { int i;for(i=0;i<j->count;++i) if(!strcmp(j->fields[i].key,key)) return &j->fields[i];return NULL; }
int json_parse(Json *j,const char *p) {
    memset(j,0,sizeof(*j));space(&p);if(*p++!='{') return 0;space(&p);
    if(*p=='}') { ++p;space(&p);return !*p; }
    for(;;) {
        char *key;JsonField *f;if(j->count==32 || *p!='"') goto fail;key=string(&p);if(!key) goto fail;
        if(strlen(key)>=48 || json_field(j,key)) { free(key);goto fail; }f=&j->fields[j->count++];strcpy(f->key,key);free(key);space(&p);if(*p++!=':') goto fail;space(&p);
        if(*p=='"') { f->type=J_STRING;f->value=string(&p);if(!f->value) goto fail; }
        else if(!strncmp(p,"true",4)) { f->type=J_BOOL;f->number=1;p+=4; }
        else if(!strncmp(p,"false",5)) { f->type=J_BOOL;p+=5; }
        else if(!strncmp(p,"null",4)) { f->type=J_NULL;p+=4; }
        else {
            const char *start=p;char *end;f->type=J_NUMBER;if(*p=='-') ++p;
            if(*p=='0') ++p;else { if(*p<'1' || *p>'9') goto fail;while(isdigit((unsigned char)*p)) ++p; }
            if(*p=='.') { ++p;if(!isdigit((unsigned char)*p)) goto fail;while(isdigit((unsigned char)*p)) ++p; }
            if(*p=='e' || *p=='E') { ++p;if(*p=='+' || *p=='-') ++p;if(!isdigit((unsigned char)*p)) goto fail;while(isdigit((unsigned char)*p)) ++p; }
            f->number=strtod(start,&end);if(end!=p || !isfinite(f->number)) goto fail;
        }
        space(&p);if(*p=='}') { ++p;space(&p);if(*p) goto fail;return 1; }if(*p++!=',') goto fail;space(&p);
    }
fail:json_free(j);return 0;
}
int json_number(const Json *j,const char *key,double *value) { const JsonField *f=json_field(j,key);if(!f || f->type!=J_NUMBER) return 0;*value=f->number;return 1; }
int json_wide(const Json *j,const char *key,wchar_t *value,size_t cap) {
    const JsonField *f=json_field(j,key);if(!f || f->type!=J_STRING) return 0;return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,f->value,-1,value,(int)cap)>0;
}
int json_is(const Json *j,const char *key,const char *value) { const JsonField *f=json_field(j,key);return f && f->type==J_STRING && !strcmp(f->value,value); }
