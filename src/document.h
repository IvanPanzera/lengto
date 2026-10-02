#ifndef P2L_DOCUMENT_H
#define P2L_DOCUMENT_H
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include "model.h"

typedef struct {
    wchar_t path[P2L_PATH];
    int pdf, pages, page, clipboard;
    IStream *source_stream;
    IWICBitmapDecoder *decoder;
    IWICFormatConverter *sampling;
    void *pdf_doc, *mapped;
    HANDLE file, mapping;
    HBITMAP bitmap;
    unsigned char *pixels;
    int width,height;
    double logical_width,logical_height;
} Document;

int imaging_init(void);
void imaging_shutdown(void);
int document_open(Document *d,const wchar_t *path,wchar_t *error,size_t cap);
int document_open_image(Document *d,const wchar_t *path,wchar_t *error,size_t cap);
int document_from_memory(Document *d,const void *data,size_t size,wchar_t *error,size_t cap);
int document_from_dib(Document *d,const void *data,size_t size,wchar_t *error,size_t cap);
int document_paste(Document *d,HWND owner,wchar_t *error,size_t cap);
unsigned char *document_png(Document *d,size_t *size);
int document_page(Document *d,int page,wchar_t *error,size_t cap);
void document_close(Document *d);
int bitmap_save_png(HBITMAP bitmap,const wchar_t *path);
/* Original raster / 144 dpi PDF, RGB over white, excluding lengto annotations. */
int document_rgb(Document *d,int x,int y,int width,int height,unsigned char *rgb);
typedef enum { EXPORT_TIFF, EXPORT_JPEG, EXPORT_PNG, EXPORT_PDF } ExportFormat;
typedef struct ExportJob ExportJob;
ExportJob *export_begin(const wchar_t *path,ExportFormat format);
/* bitmap must be the opaque, top-down, 32-bit DIB returned by render_export. */
int export_append(ExportJob *job,HBITMAP bitmap,double page_width_pt,double page_height_pt);
int export_finish(ExportJob *job);
void export_abort(ExportJob *job);
const wchar_t *export_error(void);

#endif
