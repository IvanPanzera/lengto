#define _CRT_SECURE_NO_WARNINGS
#include "portable.h"
#include <objbase.h>
#include <compressapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static wchar_t temporary_directory[32768],temporary_library[32768];
/* Keep the extracted DLL immutable until it has been unloaded. */
static HANDLE library_file=INVALID_HANDLE_VALUE;
static const void *resource_bytes(int id,DWORD *size) {
    HMODULE exe=GetModuleHandleW(NULL);HRSRC resource=FindResourceW(exe,MAKEINTRESOURCEW(id),RT_RCDATA);HGLOBAL loaded;
    if(!resource) return NULL;*size=SizeofResource(exe,resource);loaded=LoadResource(exe,resource);
    if(!loaded || !*size) return NULL;return LockResource(loaded);
}
static void *unpack_resource(int id,SIZE_T *size) {
    DWORD packed_size;const void *packed=resource_bytes(id,&packed_size);void *data=NULL;SIZE_T required=0,actual=0;DECOMPRESSOR_HANDLE handle=NULL;
    if(!packed || !CreateDecompressor(COMPRESS_ALGORITHM_LZMS,NULL,&handle)) return NULL;
    if(Decompress(handle,packed,packed_size,NULL,0,&required) || GetLastError()!=ERROR_INSUFFICIENT_BUFFER || !required || required>64*1024*1024) goto done;
    data=malloc(required);if(!data) { SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done; }
    if(!Decompress(handle,packed,packed_size,data,required,&actual) || actual!=required) { free(data);data=NULL;goto done; }*size=actual;
done:{ DWORD error=GetLastError();CloseDecompressor(handle);SetLastError(error); }return data;
}
void portable_pdfium_cleanup(void) {
    if(library_file!=INVALID_HANDLE_VALUE) { CloseHandle(library_file);library_file=INVALID_HANDLE_VALUE; }
    if(*temporary_library) DeleteFileW(temporary_library);
    if(*temporary_directory) RemoveDirectoryW(temporary_directory);
    temporary_library[0]=temporary_directory[0]=0;
}
HMODULE portable_load_pdfium(void) {
    DWORD written=0,error,count;SIZE_T size=0;void *data=NULL;wchar_t root[32768],token[40];GUID id;HMODULE module;
    data=unpack_resource(RESOURCE_PDFIUM,&size);
    if(!data) { SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);return NULL; }
    count=GetTempPathW(32768,root);if(!count || count>32768-80) { SetLastError(ERROR_BAD_PATHNAME);goto fail; }
    if(FAILED(CoCreateGuid(&id)) || !StringFromGUID2(&id,token,40)) { SetLastError(ERROR_GEN_FAILURE);goto fail; }
    swprintf(temporary_directory,32768,L"%lslengto-%ls",root,token);
    if(!CreateDirectoryW(temporary_directory,NULL)) { temporary_directory[0]=0;goto fail; }
    swprintf(temporary_library,32768,L"%ls\\pdfium.dll",temporary_directory);
    library_file=CreateFileW(temporary_library,GENERIC_WRITE|GENERIC_READ,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if(library_file==INVALID_HANDLE_VALUE) goto fail;
    if(!WriteFile(library_file,data,(DWORD)size,&written,NULL) || written!=size) goto fail;
    free(data);data=NULL;
    CloseHandle(library_file);
    library_file=CreateFileW(temporary_library,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY,NULL);
    if(library_file==INVALID_HANDLE_VALUE) goto fail;
    module=LoadLibraryExW(temporary_library,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(module) return module;
fail:
    error=GetLastError();free(data);portable_pdfium_cleanup();SetLastError(error ? error : ERROR_WRITE_FAULT);return NULL;
}
wchar_t *portable_text(int id) {
    SIZE_T size=0;char *data=unpack_resource(id,&size);wchar_t *text;int count;
    if(!data || size>0x7fffffff) { free(data);return NULL; }
    count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data,(int)size,NULL,0);if(!count) { free(data);return NULL; }
    text=malloc(((size_t)count+1)*sizeof(*text));if(!text) { free(data);return NULL; }
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data,(int)size,text,count)) { free(data);free(text);return NULL; }
    free(data);text[count]=0;return text;
}
