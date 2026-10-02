#define _CRT_SECURE_NO_WARNINGS
#include "document.h"
#include "portable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <limits.h>
#include <io.h>
#include <shellapi.h>

/* Only the C ABI is used. The supplied PDFium build has no external runtime. */
typedef void *PDF_DOC;
typedef void *PDF_PAGE;
typedef void *PDF_BITMAP;
typedef struct PdfWrite {
    int version;
    int (*write)(struct PdfWrite *,const void *,unsigned long);
    FILE *file;
} PdfWrite;
typedef struct {
    HMODULE dll;
    void (__stdcall *init)(void);
    void (__stdcall *destroy)(void);
    PDF_DOC (__stdcall *load)(const void *,size_t,const char *);
    void (__stdcall *close)(PDF_DOC);
    int (__stdcall *count)(PDF_DOC);
    PDF_PAGE (__stdcall *page)(PDF_DOC,int);
    void (__stdcall *close_page)(PDF_PAGE);
    float (__stdcall *width)(PDF_PAGE);
    float (__stdcall *height)(PDF_PAGE);
    PDF_BITMAP (__stdcall *bitmap)(int,int,int,void *,int);
    void (__stdcall *free_bitmap)(PDF_BITMAP);
    void (__stdcall *render)(PDF_BITMAP,PDF_PAGE,int,int,int,int,int,int);
    PDF_DOC (__stdcall *create)(void);
    PDF_PAGE (__stdcall *new_page)(PDF_DOC,int,double,double);
    void *(__stdcall *image_object)(PDF_DOC);
    int (__stdcall *set_bitmap)(PDF_PAGE *,int,void *,PDF_BITMAP);
    void (__stdcall *transform)(void *,double,double,double,double,double,double);
    int (__stdcall *insert)(PDF_PAGE,void *);
    int (__stdcall *generate)(PDF_PAGE);
    void (__stdcall *destroy_object)(void *);
    int (__stdcall *save)(PDF_DOC,PdfWrite *,unsigned long);
} PdfApi;
static PdfApi pdf;
static IWICImagingFactory *factory;
/* One decoded page, max. 96 MiB. Coordinates always refer to the source. */
#define MAX_PREVIEW_PIXELS (24.0*1024.0*1024.0)

int imaging_init(void) {
    return SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory));
}
static int pdf_init(void) {
    if(pdf.dll) return 1;
    pdf.dll=portable_load_pdfium();
    if(!pdf.dll) return 0;
#define RESOLVE(field,name) do { *(FARPROC *)&pdf.field=GetProcAddress(pdf.dll,name); if(!pdf.field) goto fail; } while(0)
    RESOLVE(init,"FPDF_InitLibrary"); RESOLVE(destroy,"FPDF_DestroyLibrary");
    RESOLVE(load,"FPDF_LoadMemDocument64"); RESOLVE(close,"FPDF_CloseDocument");
    RESOLVE(count,"FPDF_GetPageCount"); RESOLVE(page,"FPDF_LoadPage");
    RESOLVE(close_page,"FPDF_ClosePage"); RESOLVE(width,"FPDF_GetPageWidthF"); RESOLVE(height,"FPDF_GetPageHeightF");
    RESOLVE(bitmap,"FPDFBitmap_CreateEx"); RESOLVE(free_bitmap,"FPDFBitmap_Destroy"); RESOLVE(render,"FPDF_RenderPageBitmap");
    RESOLVE(create,"FPDF_CreateNewDocument");RESOLVE(new_page,"FPDFPage_New");RESOLVE(image_object,"FPDFPageObj_NewImageObj");
    RESOLVE(set_bitmap,"FPDFImageObj_SetBitmap");RESOLVE(transform,"FPDFPageObj_Transform");RESOLVE(insert,"FPDFPage_InsertObject");
    RESOLVE(generate,"FPDFPage_GenerateContent");RESOLVE(destroy_object,"FPDFPageObj_Destroy");RESOLVE(save,"FPDF_SaveAsCopy");
#undef RESOLVE
    pdf.init();return 1;
fail:
    FreeLibrary(pdf.dll);memset(&pdf,0,sizeof(pdf));portable_pdfium_cleanup();SetLastError(ERROR_PROC_NOT_FOUND);return 0;
}
void imaging_shutdown(void) {
    if(factory) { IWICImagingFactory_Release(factory);factory=NULL; }
    if(pdf.dll) { pdf.destroy();FreeLibrary(pdf.dll);memset(&pdf,0,sizeof(pdf)); }
    portable_pdfium_cleanup();
}
void document_close(Document *d) {
    if(d->bitmap) DeleteObject(d->bitmap);
    if(d->sampling) IWICFormatConverter_Release(d->sampling);
    if(d->decoder) IWICBitmapDecoder_Release(d->decoder);
    if(d->source_stream) IStream_Release(d->source_stream);
    if(d->pdf_doc) pdf.close(d->pdf_doc);
    if(d->mapped) UnmapViewOfFile(d->mapped);
    if(d->mapping) CloseHandle(d->mapping);
    if(d->file && d->file!=INVALID_HANDLE_VALUE) CloseHandle(d->file);
    memset(d,0,sizeof(*d));
}
static HBITMAP allocate_bitmap(int w,int h,unsigned char **pixels) {
    BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;
    bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    return CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,(void **)pixels,NULL,0);
}
int document_page(Document *d,int page,wchar_t *error,size_t cap) {
    IWICBitmapFrameDecode *frame=NULL;IWICFormatConverter *convert=NULL;IWICBitmapScaler *scaler=NULL;
    IWICBitmapSource *source=NULL; PDF_PAGE pg=NULL;PDF_BITMAP pb=NULL;
    HBITMAP bm=NULL; unsigned char *pixels=NULL; UINT sw=0,sh=0; int w,h,ok=0;
    double lw,lh,scale=1;
    if(page<0 || page>=d->pages) return 0;
    if(d->pdf) {
        pg=pdf.page(d->pdf_doc,page);if(!pg) goto cleanup;
        /* Fixed 144 dpi reference space; downsampling never changes calibration. */
        lw=(double)pdf.width(pg)*2.0;lh=(double)pdf.height(pg)*2.0;
    } else {
        if(FAILED(IWICBitmapDecoder_GetFrame(d->decoder,(UINT)page,&frame))) goto cleanup;
        if(FAILED(IWICBitmapFrameDecode_GetSize(frame,&sw,&sh))) goto cleanup;
        lw=sw;lh=sh;
    }
    if(!isfinite(lw) || !isfinite(lh) || lw<1 || lh<1 || lw>1e7 || lh>1e7) goto cleanup;
    if(lw*lh>MAX_PREVIEW_PIXELS) scale=sqrt(MAX_PREVIEW_PIXELS/(lw*lh));
    if(lw*scale>32768) scale=32768/lw;
    if(lh*scale>32768) scale=32768/lh;
    w=(int)floor(lw*scale);h=(int)floor(lh*scale);if(w<1) w=1;if(h<1) h=1;
    bm=allocate_bitmap(w,h,&pixels);if(!bm) goto cleanup;
    if(d->pdf) {
        memset(pixels,255,(size_t)w*h*4);
        pb=pdf.bitmap(w,h,4,pixels,w*4);if(!pb) goto cleanup;
        pdf.render(pb,pg,0,0,w,h,0,1); /* annotations; no scripting or forms */
    } else {
        source=(IWICBitmapSource *)frame;
        if((UINT)w!=sw || (UINT)h!=sh) {
            if(FAILED(IWICImagingFactory_CreateBitmapScaler(factory,&scaler))) goto cleanup;
            if(FAILED(IWICBitmapScaler_Initialize(scaler,source,w,h,WICBitmapInterpolationModeFant))) goto cleanup;
            source=(IWICBitmapSource *)scaler;
        }
        if(FAILED(IWICImagingFactory_CreateFormatConverter(factory,&convert))) goto cleanup;
        if(FAILED(IWICFormatConverter_Initialize(convert,source,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom))) goto cleanup;
        if(FAILED(IWICFormatConverter_CopyPixels(convert,NULL,w*4,w*h*4,pixels))) goto cleanup;
        /* GDI uses opaque BGR: composite transparency on a white drawing sheet. */
        { size_t i,count=(size_t)w*h;for(i=0;i<count;++i) {
            unsigned char *v=pixels+4*i;unsigned a=v[3];int k;
            for(k=0;k<3;++k) v[k]=(unsigned char)((v[k]*a+255*(255-a)+127)/255);
            v[3]=255;
        } }
    }
    if(d->bitmap) DeleteObject(d->bitmap);
    if(d->sampling) { IWICFormatConverter_Release(d->sampling);d->sampling=NULL; }
    d->bitmap=bm;bm=NULL;d->pixels=pixels;d->width=w;d->height=h;
    d->logical_width=lw;d->logical_height=lh;d->page=page;ok=1;
cleanup:
    if(pb) pdf.free_bitmap(pb);
    if(pg) pdf.close_page(pg);
    if(convert) IWICFormatConverter_Release(convert);
    if(scaler) IWICBitmapScaler_Release(scaler);
    if(frame) IWICBitmapFrameDecode_Release(frame);
    if(bm) DeleteObject(bm);
    if(!ok) swprintf(error,cap,L"Cannot read this page. The file may be damaged or too large.");
    return ok;
}
int document_rgb(Document *d,int x,int y,int width,int height,unsigned char *rgb) {
    unsigned char *bgra;size_t i,count;int ok=0;IWICBitmapFrameDecode *frame=NULL;PDF_PAGE page=NULL;PDF_BITMAP bitmap=NULL;
    if(!d->bitmap || x<0 || y<0 || width<1 || height<1 || (double)x+width>ceil(d->logical_width) || (double)y+height>ceil(d->logical_height) || (double)width*height>65536) return 0;
    count=(size_t)width*height;bgra=malloc(count*4);if(!bgra) return 0;
    if(d->pdf) {
        page=pdf.page(d->pdf_doc,d->page);if(!page) goto done;memset(bgra,255,count*4);
        bitmap=pdf.bitmap(width,height,4,bgra,width*4);if(!bitmap) goto done;
        pdf.render(bitmap,page,-x,-y,(int)ceil(d->logical_width),(int)ceil(d->logical_height),0,1);ok=1;
    } else {
        WICRect rect={x,y,width,height};
        if(!d->sampling) {
            if(FAILED(IWICBitmapDecoder_GetFrame(d->decoder,d->page,&frame)) || FAILED(IWICImagingFactory_CreateFormatConverter(factory,&d->sampling))) goto done;
            if(FAILED(IWICFormatConverter_Initialize(d->sampling,(IWICBitmapSource *)frame,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom))) { IWICFormatConverter_Release(d->sampling);d->sampling=NULL;goto done; }
        }
        ok=SUCCEEDED(IWICFormatConverter_CopyPixels(d->sampling,&rect,width*4,(UINT)(count*4),bgra));
    }
    if(ok) for(i=0;i<count;++i) { unsigned char *p=bgra+i*4;unsigned a=p[3];int k;for(k=0;k<3;++k) rgb[i*3+k]=(unsigned char)((p[2-k]*a+255*(255-a)+127)/255); }
done:if(frame) IWICBitmapFrameDecode_Release(frame);if(bitmap) pdf.free_bitmap(bitmap);if(page) pdf.close_page(page);free(bgra);return ok;
}
int document_open(Document *d,const wchar_t *path,wchar_t *error,size_t cap) {
    const wchar_t *ext=wcsrchr(path,L'.');UINT count=0;
    memset(d,0,sizeof(*d));
    if(wcslen(path)>=P2L_PATH) return 0;
    wcscpy(d->path,path);d->pdf=ext && !_wcsicmp(ext,L".pdf");
    if(d->pdf) {
        LARGE_INTEGER size;
        if(!pdf_init()) { swprintf(error,cap,L"Cannot start the embedded PDF reader (Windows error %lu). Check access to the temporary folder.",GetLastError());return 0; }
        d->file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(d->file==INVALID_HANDLE_VALUE || !GetFileSizeEx(d->file,&size) || size.QuadPart<=0 || (unsigned long long)size.QuadPart>SIZE_MAX) goto fail;
        d->mapping=CreateFileMappingW(d->file,NULL,PAGE_READONLY,0,0,NULL);if(!d->mapping) goto fail;
        d->mapped=MapViewOfFile(d->mapping,FILE_MAP_READ,0,0,0);if(!d->mapped) goto fail;
        d->pdf_doc=pdf.load(d->mapped,(size_t)size.QuadPart,NULL);if(!d->pdf_doc) goto fail;
        d->pages=pdf.count(d->pdf_doc);
    } else {
        if(!ext || (_wcsicmp(ext,L".tif") && _wcsicmp(ext,L".tiff") && _wcsicmp(ext,L".png") && _wcsicmp(ext,L".jpg") && _wcsicmp(ext,L".jpeg"))) {
            swprintf(error,cap,L"Unsupported format. Open a TIFF, PNG, JPG or PDF file.");return 0;
        }
        if(FAILED(IWICImagingFactory_CreateDecoderFromFilename(factory,path,NULL,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d->decoder))) goto fail;
        if(FAILED(IWICBitmapDecoder_GetFrameCount(d->decoder,&count))) goto fail;
        d->pages=(int)count;
    }
    if(d->pages<1 || d->pages>100000) goto fail;
    if(!document_page(d,0,error,cap)) { document_close(d);return 0; }
    return 1;
fail:
    swprintf(error,cap,L"Cannot open the document. Check that it is readable and, for PDF files, not password protected.");
    document_close(d);return 0;
}
int document_from_memory(Document *d,const void *data,size_t size,wchar_t *error,size_t cap) {
    HGLOBAL memory;void *copy;memset(d,0,sizeof(*d));
    if(!data || !size || size>0xffffffffu) goto fail;
    memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory) goto fail;
    copy=GlobalLock(memory);if(!copy) { GlobalFree(memory);goto fail; }memcpy(copy,data,size);GlobalUnlock(memory);
    if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&d->source_stream))) { GlobalFree(memory);goto fail; }
    if(FAILED(IWICImagingFactory_CreateDecoderFromStream(factory,d->source_stream,NULL,WICDecodeMetadataCacheOnDemand,&d->decoder))) goto fail;
    d->pages=1;d->clipboard=1;wcscpy(d->path,L"Clipboard image");
    if(document_page(d,0,error,cap)) return 1;
fail:document_close(d);swprintf(error,cap,L"Cannot read this clipboard image.");return 0;
}
int document_open_image(Document *d,const wchar_t *path,wchar_t *error,size_t cap) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    LARGE_INTEGER size;DWORD read=0;unsigned char *data=NULL;int ok=0;
    if(file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=0xffffffffu) {
        data=malloc((size_t)size.QuadPart);
        if(data && ReadFile(file,data,(DWORD)size.QuadPart,&read,NULL) && read==(DWORD)size.QuadPart) ok=document_from_memory(d,data,read,error,cap);
    }
    if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);free(data);
    if(!ok) swprintf(error,cap,L"Cannot read the embedded image.");return ok;
}
int document_from_dib(Document *d,const void *data,size_t size,wchar_t *error,size_t cap) {
    BITMAPINFOHEADER header;BITMAPFILEHEADER file={0};uint64_t offset,bytes,stride,height,palette;unsigned char *bmp;int ok;
    if(!data || size<sizeof(header) || size>0xffffffffu-sizeof(file)) goto fail;memcpy(&header,data,sizeof(header));
    if((header.biSize!=40 && header.biSize!=52 && header.biSize!=56 && header.biSize!=108 && header.biSize!=124) || header.biSize>size || header.biPlanes!=1 || header.biWidth<=0 || header.biHeight==0 || header.biHeight==LONG_MIN) goto fail;
    if(header.biBitCount!=1 && header.biBitCount!=4 && header.biBitCount!=8 && header.biBitCount!=16 && header.biBitCount!=24 && header.biBitCount!=32) goto fail;
    if(header.biCompression!=BI_RGB && header.biCompression!=BI_BITFIELDS) goto fail;
    if(header.biCompression==BI_BITFIELDS && header.biBitCount!=16 && header.biBitCount!=32) goto fail;
    palette=header.biClrUsed ? header.biClrUsed : header.biBitCount<=8 ? (uint64_t)1<<header.biBitCount : 0;
    if(header.biBitCount<=8 && palette>((uint64_t)1<<header.biBitCount)) goto fail;
    offset=header.biSize+palette*4+(header.biSize==40 && header.biCompression==BI_BITFIELDS ? 12 : 0);
    height=header.biHeight<0 ? -(int64_t)header.biHeight : header.biHeight;stride=(((uint64_t)header.biWidth*header.biBitCount+31)/32)*4;bytes=stride*height;
    if(offset>size || bytes>size-offset) goto fail;
    file.bfType=0x4d42;file.bfSize=(DWORD)(sizeof(file)+size);file.bfOffBits=(DWORD)(sizeof(file)+offset);
    bmp=malloc(file.bfSize);if(!bmp) goto fail;memcpy(bmp,&file,sizeof(file));memcpy(bmp+sizeof(file),data,size);
    ok=document_from_memory(d,bmp,file.bfSize,error,cap);free(bmp);return ok;
fail:swprintf(error,cap,L"The clipboard bitmap is incomplete or unsupported.");return 0;
}
static IStream *source_png(IWICBitmapSource *source) {
    IStream *stream=NULL;IWICBitmapEncoder *encoder=NULL;IWICBitmapFrameEncode *frame=NULL;UINT w,h;WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;int ok=0;
    if(FAILED(IWICBitmapSource_GetSize(source,&w,&h)) || FAILED(CreateStreamOnHGlobal(NULL,TRUE,&stream))) goto done;
    if(FAILED(IWICImagingFactory_CreateEncoder(factory,&GUID_ContainerFormatPng,NULL,&encoder)) || FAILED(IWICBitmapEncoder_Initialize(encoder,stream,WICBitmapEncoderNoCache))) goto done;
    if(FAILED(IWICBitmapEncoder_CreateNewFrame(encoder,&frame,NULL)) || FAILED(IWICBitmapFrameEncode_Initialize(frame,NULL))) goto done;
    if(FAILED(IWICBitmapFrameEncode_SetSize(frame,w,h)) || FAILED(IWICBitmapFrameEncode_SetPixelFormat(frame,&format)) || FAILED(IWICBitmapFrameEncode_WriteSource(frame,source,NULL))) goto done;
    ok=SUCCEEDED(IWICBitmapFrameEncode_Commit(frame)) && SUCCEEDED(IWICBitmapEncoder_Commit(encoder));
done:if(frame) IWICBitmapFrameEncode_Release(frame);if(encoder) IWICBitmapEncoder_Release(encoder);if(!ok && stream) { IStream_Release(stream);stream=NULL; }return stream;
}
static unsigned char *stream_bytes(IStream *stream,size_t *size) {
    STATSTG stat={0};HGLOBAL memory;void *source;unsigned char *copy=NULL;*size=0;
    if(FAILED(IStream_Stat(stream,&stat,STATFLAG_NONAME)) || !stat.cbSize.QuadPart || stat.cbSize.QuadPart>SIZE_MAX || FAILED(GetHGlobalFromStream(stream,&memory))) return NULL;
    source=GlobalLock(memory);if(!source) return NULL;copy=malloc((size_t)stat.cbSize.QuadPart);
    if(copy) { *size=(size_t)stat.cbSize.QuadPart;memcpy(copy,source,*size); }GlobalUnlock(memory);return copy;
}
unsigned char *document_png(Document *d,size_t *size) {
    IWICBitmapFrameDecode *frame=NULL;IStream *stream=NULL;unsigned char *data=NULL;*size=0;
    if(d->decoder && SUCCEEDED(IWICBitmapDecoder_GetFrame(d->decoder,0,&frame))) stream=source_png((IWICBitmapSource *)frame);
    if(stream) { data=stream_bytes(stream,size);IStream_Release(stream); }if(frame) IWICBitmapFrameDecode_Release(frame);return data;
}
int document_paste(Document *d,HWND owner,wchar_t *error,size_t cap) {
    UINT formats[]={RegisterClipboardFormatW(L"PNG"),CF_DIBV5,CF_DIB};int i,ok=0;
    if(!OpenClipboard(owner)) { swprintf(error,cap,L"The clipboard is busy. Try pasting again.");return 0; }
    for(i=0;i<3 && !ok;++i) if(IsClipboardFormatAvailable(formats[i])) {
        HGLOBAL memory=GetClipboardData(formats[i]);SIZE_T size=memory ? GlobalSize(memory) : 0;void *data=memory ? GlobalLock(memory) : NULL;
        if(data) { ok=i==0 ? document_from_memory(d,data,size,error,cap) : document_from_dib(d,data,size,error,cap);GlobalUnlock(memory); }
    }
    if(!ok && IsClipboardFormatAvailable(CF_BITMAP)) {
        HBITMAP bitmap=GetClipboardData(CF_BITMAP);IWICBitmap *source=NULL;IStream *stream=NULL;
        if(bitmap && SUCCEEDED(IWICImagingFactory_CreateBitmapFromHBITMAP(factory,bitmap,NULL,WICBitmapIgnoreAlpha,&source))) stream=source_png((IWICBitmapSource *)source);
        if(stream) { size_t size;unsigned char *data=stream_bytes(stream,&size);if(data) ok=document_from_memory(d,data,size,error,cap);free(data);IStream_Release(stream); }
        if(source) IWICBitmap_Release(source);
    }
    if(!ok && IsClipboardFormatAvailable(CF_HDROP)) {
        wchar_t path[P2L_PATH];HDROP files=GetClipboardData(CF_HDROP);UINT count=files ? DragQueryFileW(files,0xffffffff,NULL,0) : 0;
        for(i=0;i<(int)count && !ok;++i) if(DragQueryFileW(files,i,NULL,0)<P2L_PATH && DragQueryFileW(files,i,path,P2L_PATH)) ok=document_open(d,path,error,cap);
    }
    CloseClipboard();if(!ok) swprintf(error,cap,L"Copy an image or a supported image/PDF file, then paste it here.");return ok;
}
int bitmap_save_png(HBITMAP bitmap,const wchar_t *path) {
    IWICBitmap *b=NULL;IWICStream *s=NULL;IWICBitmapEncoder *e=NULL;IWICBitmapFrameEncode *f=NULL;
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGR;UINT w,h;int ok=0;
    if(FAILED(IWICImagingFactory_CreateBitmapFromHBITMAP(factory,bitmap,NULL,WICBitmapIgnoreAlpha,&b))) goto done;
    if(FAILED(IWICBitmap_GetSize(b,&w,&h))) goto done;
    if(FAILED(IWICImagingFactory_CreateStream(factory,&s))) goto done;
    if(FAILED(IWICStream_InitializeFromFilename(s,path,GENERIC_WRITE))) goto done;
    if(FAILED(IWICImagingFactory_CreateEncoder(factory,&GUID_ContainerFormatPng,NULL,&e))) goto done;
    if(FAILED(IWICBitmapEncoder_Initialize(e,(IStream *)s,WICBitmapEncoderNoCache))) goto done;
    if(FAILED(IWICBitmapEncoder_CreateNewFrame(e,&f,NULL))) goto done;
    if(FAILED(IWICBitmapFrameEncode_Initialize(f,NULL))) goto done;
    if(FAILED(IWICBitmapFrameEncode_SetSize(f,w,h))) goto done;
    if(FAILED(IWICBitmapFrameEncode_SetPixelFormat(f,&format))) goto done;
    if(FAILED(IWICBitmapFrameEncode_WriteSource(f,(IWICBitmapSource *)b,NULL))) goto done;
    if(FAILED(IWICBitmapFrameEncode_Commit(f))) goto done;
    ok=SUCCEEDED(IWICBitmapEncoder_Commit(e));
done:
    if(f) IWICBitmapFrameEncode_Release(f);if(e) IWICBitmapEncoder_Release(e);
    if(s) IWICStream_Release(s);if(b) IWICBitmap_Release(b);return ok;
}

struct ExportJob {
    wchar_t target[P2L_PATH],temp[P2L_PATH];
    ExportFormat format;
    IWICStream *stream;
    IWICBitmapEncoder *encoder;
    PDF_DOC document;
    int pages,failed;
};
static wchar_t last_export_error[1024];
const wchar_t *export_error(void) { return *last_export_error ? last_export_error : L"Export failed. Any previous output file has been preserved."; }
static void export_problem(const wchar_t *stage,DWORD code) {
    wchar_t detail[512]=L"";
    if(code) FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),detail,512,NULL);
    if(*detail) swprintf(last_export_error,1024,L"%ls\n\n%ls",stage,detail);else swprintf(last_export_error,1024,L"%ls",stage);
}
static void export_release(ExportJob *job) {
    if(job->encoder) { IWICBitmapEncoder_Release(job->encoder);job->encoder=NULL; }
    if(job->stream) { IWICStream_Release(job->stream);job->stream=NULL; }
    if(job->document) { pdf.close(job->document);job->document=NULL; }
}
ExportJob *export_begin(const wchar_t *path,ExportFormat format) {
    ExportJob *job=calloc(1,sizeof(*job));const GUID *container;
    last_export_error[0]=0;
    if(!job) { export_problem(L"Not enough memory to create the output file.",0);return NULL; }
    if(wcslen(path)>P2L_PATH-48 || format<EXPORT_TIFF || format>EXPORT_PDF) goto fail;
    wcscpy(job->target,path);swprintf(job->temp,P2L_PATH,L"%ls.%lu.export.tmp",path,GetCurrentProcessId());job->format=format;
    if(format==EXPORT_PDF) {
        if(!pdf_init()) { export_problem(L"Cannot start the embedded PDF engine. Check access to the temporary folder.",GetLastError());goto fail; }
        job->document=pdf.create();if(!job->document) { export_problem(L"The PDF engine could not create a new document.",0);goto fail; }
    } else {
        container=format==EXPORT_TIFF ? &GUID_ContainerFormatTiff : format==EXPORT_JPEG ? &GUID_ContainerFormatJpeg : &GUID_ContainerFormatPng;
        if(FAILED(IWICImagingFactory_CreateStream(factory,&job->stream))) goto fail;
        if(FAILED(IWICStream_InitializeFromFilename(job->stream,job->temp,GENERIC_WRITE))) goto fail;
        if(FAILED(IWICImagingFactory_CreateEncoder(factory,container,NULL,&job->encoder))) goto fail;
        if(FAILED(IWICBitmapEncoder_Initialize(job->encoder,(IStream *)job->stream,WICBitmapEncoderNoCache))) goto fail;
    }
    return job;
fail:if(!*last_export_error) export_problem(L"Cannot create the output file. Check the path and folder permissions.",GetLastError());export_abort(job);return NULL;
}
int export_append(ExportJob *job,HBITMAP bitmap,double page_width_pt,double page_height_pt) {
    IWICBitmap *source=NULL;IWICBitmapFrameEncode *frame=NULL;IPropertyBag2 *options=NULL;
    WICPixelFormatGUID pixel_format=GUID_WICPixelFormat24bppBGR;UINT w,h;int ok=0;
    if(!job || job->failed) return 0;
    if(job->format==EXPORT_PDF) {
        DIBSECTION section;PDF_PAGE page=NULL;PDF_BITMAP pb=NULL;void *object=NULL;
        /* GetObject normalizes biHeight; the caller supplies a top-down DIB. */
        if(GetObjectW(bitmap,sizeof(section),&section)!=sizeof(section) || !section.dsBm.bmBits || section.dsBm.bmBitsPixel!=32) goto pdf_done;
        page=pdf.new_page(job->document,job->pages,page_width_pt,page_height_pt);if(!page) goto pdf_done;
        /* Opaque BGRx avoids accidental transparency after GDI drawing. */
        pb=pdf.bitmap(section.dsBm.bmWidth,section.dsBm.bmHeight,3,section.dsBm.bmBits,section.dsBm.bmWidthBytes);if(!pb) goto pdf_done;
        object=pdf.image_object(job->document);if(!object) goto pdf_done;
        if(!pdf.set_bitmap(NULL,0,object,pb)) goto pdf_done;
        pdf.transform(object,page_width_pt,0,0,page_height_pt,0,0);
        if(!pdf.insert(page,object)) goto pdf_done;object=NULL;
        ok=pdf.generate(page);
pdf_done:
        if(!ok) export_problem(L"Cannot convert the image into a PDF page.",0);
        if(object) pdf.destroy_object(object);if(pb) pdf.free_bitmap(pb);if(page) pdf.close_page(page);
    } else {
        if(job->pages && job->format!=EXPORT_TIFF) goto done;
        if(FAILED(IWICImagingFactory_CreateBitmapFromHBITMAP(factory,bitmap,NULL,WICBitmapIgnoreAlpha,&source))) goto done;
        if(FAILED(IWICBitmap_GetSize(source,&w,&h))) goto done;
        if(FAILED(IWICBitmapEncoder_CreateNewFrame(job->encoder,&frame,&options))) goto done;
        if(options) {
            PROPBAG2 prop={0};VARIANT value;VariantInit(&value);
            if(job->format==EXPORT_JPEG) { prop.pstrName=L"ImageQuality";value.vt=VT_R4;value.fltVal=0.95f; }
            else if(job->format==EXPORT_TIFF) { prop.pstrName=L"TiffCompressionMethod";value.vt=VT_UI1;value.bVal=WICTiffCompressionZIP; }
            if(prop.pstrName && FAILED(IPropertyBag2_Write(options,1,&prop,&value))) goto done;
        }
        if(FAILED(IWICBitmapFrameEncode_Initialize(frame,options))) goto done;
        if(FAILED(IWICBitmapFrameEncode_SetSize(frame,w,h))) goto done;
        if(FAILED(IWICBitmapFrameEncode_SetResolution(frame,144,144))) goto done;
        if(FAILED(IWICBitmapFrameEncode_SetPixelFormat(frame,&pixel_format))) goto done;
        if(FAILED(IWICBitmapFrameEncode_WriteSource(frame,(IWICBitmapSource *)source,NULL))) goto done;
        ok=SUCCEEDED(IWICBitmapFrameEncode_Commit(frame));
    }
done:
    if(options) IPropertyBag2_Release(options);if(frame) IWICBitmapFrameEncode_Release(frame);if(source) IWICBitmap_Release(source);
    if(ok) ++job->pages;else job->failed=1;return ok;
}
static int pdf_write(PdfWrite *writer,const void *data,unsigned long size) { return fwrite(data,1,size,writer->file)==size; }
int export_finish(ExportJob *job) {
    int ok;if(!job) return 0;ok=!job->failed && job->pages>0;
    if(ok && job->format==EXPORT_PDF) {
        PdfWrite writer={1,pdf_write,NULL};writer.file=_wfopen(job->temp,L"wb");
        if(!writer.file) { unsigned long code=0;_get_doserrno(&code);export_problem(L"Cannot write the PDF to the selected folder.",(DWORD)code);ok=0; }
        else { ok=pdf.save(job->document,&writer,2) && !ferror(writer.file);if(fflush(writer.file)) ok=0;
            if(ok && !FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(writer.file)))) ok=0;
            if(fclose(writer.file)) ok=0;
            if(!ok) export_problem(L"PDF writing did not finish. Check available space and folder write permissions.",0);
        }
    } else if(ok) ok=SUCCEEDED(IWICBitmapEncoder_Commit(job->encoder));
    export_release(job);
    if(ok) { ok=MoveFileExW(job->temp,job->target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
        if(!ok) export_problem(L"The file was created but could not replace the destination. Close it in other programs and try again.",GetLastError());
    }
    if(!ok) DeleteFileW(job->temp);free(job);return ok;
}
void export_abort(ExportJob *job) { if(job) { export_release(job);if(*job->temp) DeleteFileW(job->temp);free(job); } }
