#ifndef LENGTO_PORTABLE_H
#define LENGTO_PORTABLE_H
#include <windows.h>
#define RESOURCE_PDFIUM 101
#define RESOURCE_LICENSES 102
#define RESOURCE_GUIDE 103
#define RESOURCE_PYTHON 104
HMODULE portable_load_pdfium(void);
void portable_pdfium_cleanup(void);
/* Caller frees the returned UTF-16 text. */
wchar_t *portable_text(int id);
#endif
