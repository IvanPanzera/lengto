#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <compressapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build-only utility. Buffer mode stores the original length in the stream. */
int wmain(int argc,wchar_t **argv) {
    FILE *input=NULL,*output=NULL;unsigned char *raw=NULL,*packed=NULL,*check=NULL;
    COMPRESSOR_HANDLE compressor=NULL;DECOMPRESSOR_HANDLE decompressor=NULL;
    SIZE_T size,capacity=0,used=0,decoded=0;long length;DWORD algorithm;int ok=0;
    if(argc!=4) { fwprintf(stderr,L"Usage: pack-resource algorithm input output\n");return 2; }
    algorithm=(DWORD)wcstoul(argv[1],NULL,10);input=_wfopen(argv[2],L"rb");if(!input) goto done;
    if(fseek(input,0,SEEK_END) || (length=ftell(input))<=0 || fseek(input,0,SEEK_SET)) goto done;size=(SIZE_T)length;
    raw=malloc(size);check=malloc(size);if(!raw || !check || fread(raw,1,size,input)!=size) goto done;
    if(!CreateCompressor(algorithm,NULL,&compressor)) goto done;
    if(Compress(compressor,raw,size,NULL,0,&capacity) || GetLastError()!=ERROR_INSUFFICIENT_BUFFER) goto done;
    packed=malloc(capacity);if(!packed || !Compress(compressor,raw,size,packed,capacity,&used)) goto done;
    if(!CreateDecompressor(algorithm,NULL,&decompressor) || !Decompress(decompressor,packed,used,check,size,&decoded) || decoded!=size || memcmp(raw,check,size)) goto done;
    output=_wfopen(argv[3],L"wb");if(!output || fwrite(packed,1,used,output)!=used) goto done;
    if(fclose(output)) { output=NULL;goto done; }output=NULL;ok=1;
    wprintf(L"Resource: %zu -> %zu bytes (%.1f%%)\n",size,used,100.0*used/size);
done:
    if(input) fclose(input);if(output) fclose(output);if(compressor) CloseCompressor(compressor);if(decompressor) CloseDecompressor(decompressor);
    free(raw);free(packed);free(check);if(!ok) { fwprintf(stderr,L"Resource compression failed (%lu).\n",GetLastError());DeleteFileW(argv[3]); }return ok ? 0 : 1;
}
