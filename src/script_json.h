#ifndef LENGTO_SCRIPT_JSON_H
#define LENGTO_SCRIPT_JSON_H
#include <windows.h>
#include <stddef.h>
typedef struct { char *data;size_t len,cap;int failed; } Text;
void text_add(Text *b,const char *format,...);
void text_free(Text *b);
char *json_quote(const wchar_t *s);
enum { J_STRING=1,J_NUMBER,J_BOOL,J_NULL };
typedef struct { char key[48];int type;char *value;double number; } JsonField;
typedef struct { JsonField fields[32];int count; } Json;
int json_parse(Json *j,const char *input);
void json_free(Json *j);
const JsonField *json_field(const Json *j,const char *key);
int json_number(const Json *j,const char *key,double *value);
int json_wide(const Json *j,const char *key,wchar_t *value,size_t cap);
int json_is(const Json *j,const char *key,const char *value);
#endif
