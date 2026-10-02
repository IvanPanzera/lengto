#define _CRT_SECURE_NO_WARNINGS
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "render.h"
#include "portable.h"
#include "recorder.h"
#include "script_runner.h"
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

enum { CMD_OPEN=100,CMD_SAVE,CMD_SAVE_AS,CMD_EXPORT,CMD_CLOSE,CMD_CALIBRATE,CMD_MEASURE,CMD_SETTINGS,CMD_FIT,CMD_HELP,CMD_ABOUT,CMD_RECORD,CMD_STOP_RECORD,CMD_RUN_SCRIPT,CMD_GITHUB,CMD_BRAND,CMD_PASTE };
enum { IDI_LENGTO=1,IDI_SQUADRETTA=2 };
enum { MODE_IDLE,MODE_CALIBRATE,MODE_MEASURE,MODE_REPLACE_MEASURE };
enum { D_CALIBRATE,D_SETTINGS,D_EDIT_MEASURE,D_EXPORT,D_ABOUT,D_GUIDE,D_RECORD };
enum { F_SELECT=201,F_NAME,F_VALUE,F_UNIT,F_SCOPE,F_REDRAW,F_HINT };
typedef struct {
    HWND main,canvas,page_edit;HMENU menu;HINSTANCE instance;HFONT font,font_small;HBRUSH background;HICON menu_icon;
    Document document;Project *project;View view;
    wchar_t output_path[P2L_PATH];ExportFormat output_format;int output_all;
    int dirty,active,selected,mode,drawing,press,panning,replace_cal,replace_measure,test_mode;
    Point first,hover;Calibration pending_cal;POINT down;double down_ox,down_oy;
    Recorder recorder;ScriptRunner *runner;wchar_t python[P2L_PATH];int closing,editing_page;DWORD script_exit;
} App;
typedef struct {
    int kind,done,accepted,redraw,cal_id,precision,page,scope;Unit unit;ExportFormat format;
    Calibration cal;wchar_t name[P2L_NAME];HWND window,select,name_edit,value,unit_combo,scope_combo;
} Dialog;
static App app;
static void refresh(void);
static void command(int id);
static int save_output(int save_as);
static int open_path(const wchar_t *path);
static int show_dialog(Dialog *d);
static void recording_check(void);
static void end_page_entry(int apply);
static const wchar_t github_url[]=L"https://github.com/IvanPanzera/lengto";
static void open_github(void) {
    if(!app.test_mode && (INT_PTR)ShellExecuteW(app.main,L"open",github_url,NULL,NULL,SW_SHOWNORMAL)<=32)
        MessageBoxW(app.main,L"Could not open the browser. Visit https://github.com/IvanPanzera/lengto",L"lengto",MB_OK|MB_ICONINFORMATION);
}
static void message(const wchar_t *text) { if(app.test_mode) fwprintf(stderr,L"%ls\n",text);else MessageBoxW(app.main,text,L"lengto",MB_OK|MB_ICONINFORMATION); }
static void changed(void) { app.dirty=1;recording_check();refresh(); }
static const wchar_t *file_name(const wchar_t *path) { const wchar_t *s=wcsrchr(path,L'\\');return s ? s+1 : path; }
static const wchar_t *extension(const wchar_t *path) { const wchar_t *s=wcsrchr(file_name(path),L'.');return s ? s : L""; }
static HWND control(HWND parent,const wchar_t *type,const wchar_t *text,DWORD style,int id,int x,int y,int w,int h) {
    HWND c=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,(HMENU)(INT_PTR)id,app.instance,NULL);
    SendMessageW(c,WM_SETFONT,(WPARAM)app.font,TRUE);return c;
}
static int file_dialog(int save,wchar_t *path,const wchar_t *filter,const wchar_t *ext,const wchar_t *title) {
    OPENFILENAMEW f={0};f.lStructSize=sizeof(f);f.hwndOwner=app.main;f.lpstrFile=path;f.nMaxFile=P2L_PATH;f.lpstrFilter=filter;f.lpstrDefExt=ext;f.lpstrTitle=title;
    f.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);return save ? GetSaveFileNameW(&f) : GetOpenFileNameW(&f);
}
static const wchar_t drawings_filter[]=L"Images and PDF\0*.tif;*.tiff;*.png;*.jpg;*.jpeg;*.pdf\0\0";
static void choose_active(void) {
    app.active=app.project->nc ? app.project->cal[0].id : 0;
}
static void refresh(void) {
    wchar_t title[512];size_t i;int commands[]={CMD_SAVE,CMD_SAVE_AS,CMD_EXPORT,CMD_CLOSE,CMD_CALIBRATE,CMD_MEASURE,CMD_FIT};
    if(!app.main) return;if(app.editing_page && (app.runner || app.document.pages<2)) end_page_entry(0);
    choose_active();swprintf(title,512,L"lengto%ls%ls%ls%ls",app.document.bitmap ? L" — " : L"",file_name(app.document.path),app.dirty ? L" *" : L"",app.recorder.active ? L" [REC]" : app.runner ? L" [Script]" : L"");SetWindowTextW(app.main,title);
    for(i=0;i<sizeof(commands)/sizeof(commands[0]);++i) EnableMenuItem(app.menu,commands[i],MF_BYCOMMAND|(app.document.bitmap && !app.runner ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(app.menu,CMD_OPEN,MF_BYCOMMAND|(app.runner ? MF_GRAYED : MF_ENABLED));EnableMenuItem(app.menu,CMD_SETTINGS,MF_BYCOMMAND|(app.runner ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(app.menu,CMD_PASTE,MF_BYCOMMAND|(app.runner ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(app.menu,CMD_RECORD,MF_BYCOMMAND|(app.recorder.active || app.runner ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(app.menu,CMD_STOP_RECORD,MF_BYCOMMAND|(app.recorder.active ? MF_ENABLED : MF_GRAYED));EnableMenuItem(app.menu,CMD_RUN_SCRIPT,MF_BYCOMMAND|(app.recorder.active || app.runner ? MF_GRAYED : MF_ENABLED));
    CheckMenuItem(app.menu,CMD_CALIBRATE,MF_BYCOMMAND|(app.mode==MODE_CALIBRATE ? MF_CHECKED : MF_UNCHECKED));CheckMenuItem(app.menu,CMD_MEASURE,MF_BYCOMMAND|(app.mode==MODE_MEASURE ? MF_CHECKED : MF_UNCHECKED));
    app.view.active=app.active;app.view.selected=app.selected;InvalidateRect(app.main,NULL,FALSE);InvalidateRect(app.canvas,NULL,FALSE);DrawMenuBar(app.main);
}
static void fit_image(void) {
    if(!app.document.bitmap) return;app.view.sx=app.view.sy=fmax(0.001,fmin((app.view.width-40)/app.document.logical_width,(app.view.height-40)/app.document.logical_height));
    app.view.ox=(app.view.width-app.document.logical_width*app.view.sx)/2;app.view.oy=(app.view.height-app.document.logical_height*app.view.sy)/2;InvalidateRect(app.canvas,NULL,FALSE);
}
static Point image_point(int x,int y,int clamp) {
    Point p={(x-app.view.ox)/app.view.sx,(y-app.view.oy)/app.view.sy};if(clamp) { p.x=fmax(0,fmin(app.document.logical_width,p.x));p.y=fmax(0,fmin(app.document.logical_height,p.y)); }return p;
}
static Point snapped_point(int x,int y) {
    Point p=image_point(x,y,1);
    p.x=fmin(floor(p.x)+0.5,fmax(0.5,ceil(app.document.logical_width)-0.5));
    p.y=fmin(floor(p.y)+0.5,fmax(0.5,ceil(app.document.logical_height)-0.5));return p;
}
static Point constrain(Point p) {
    if(app.drawing && (GetKeyState(VK_SHIFT)&0x8000)) { if(fabs(p.x-app.first.x)>fabs(p.y-app.first.y)) p.y=app.first.y;else p.x=app.first.x; }return p;
}
static void cancel_action(void) {
    app.drawing=0;app.mode=MODE_IDLE;app.replace_cal=0;app.replace_measure=-1;app.press=app.panning=0;if(GetCapture()==app.canvas) ReleaseCapture();refresh();
}
static void set_mode(int mode) { cancel_action();app.mode=mode;refresh();SetFocus(app.canvas); }
static void fill_units(HWND combo,Unit selected) {
    SendMessageW(combo,CB_SETDROPPEDWIDTH,320,0);
    int i;wchar_t text[100];for(i=0;i<UNIT_COUNT;++i) { Unit u=unit_order(i);int row;swprintf(text,100,L"%ls — %ls",unit_name(u),unit_description(u));row=(int)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)text);SendMessageW(combo,CB_SETITEMDATA,row,u);if(u==selected) SendMessageW(combo,CB_SETCURSEL,row,0); }
}
static void select_unit(HWND combo,Unit u) { int i,n=(int)SendMessageW(combo,CB_GETCOUNT,0,0);for(i=0;i<n;++i) if(SendMessageW(combo,CB_GETITEMDATA,i,0)==u) SendMessageW(combo,CB_SETCURSEL,i,0); }
static Unit read_unit(HWND combo) { return (Unit)SendMessageW(combo,CB_GETITEMDATA,SendMessageW(combo,CB_GETCURSEL,0,0),0); }
static void load_cal_fields(Dialog *d) {
    const Calibration *c=get_cal(app.project,d->cal_id);wchar_t text[160];if(c) d->cal=*c;
    else { memset(&d->cal,0,sizeof(d->cal));d->cal.page=app.document.page;d->cal.known=1;d->cal.unit=app.project->display_unit;wcscpy(d->cal.name,L"Calibration"); }
    swprintf(text,160,L"%.15g",d->cal.known);SetWindowTextW(d->value,text);select_unit(d->unit_combo,d->cal.unit);EnableWindow(GetDlgItem(d->window,F_REDRAW),c!=NULL);
}
static void dialog_error(HWND hwnd,const wchar_t *text) { if(app.test_mode) message(text);else MessageBoxW(hwnd,text,L"lengto",MB_OK|MB_ICONINFORMATION); }
static int valid_dialog_name(HWND edit,wchar_t *name) { size_t n;GetWindowTextW(edit,name,P2L_NAME);n=wcslen(name);while(n && name[n-1]==L' ') name[--n]=0;return n && wcscspn(name,L"\t\r\n")==n; }
static LRESULT CALLBACK dialog_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    Dialog *d=(Dialog *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_CREATE) {
        int buttons=174,i;wchar_t text[180];d=((CREATESTRUCTW *)lp)->lpCreateParams;d->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)d);
        if(d->kind==D_CALIBRATE) {
            control(hwnd,L"STATIC",L"Real length",0,0,20,20,194,20);control(hwnd,L"STATIC",L"Unit",0,0,234,20,206,20);
            d->value=control(hwnd,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,F_VALUE,20,46,194,27);SendMessageW(d->value,EM_SETLIMITTEXT,100,0);
            d->unit_combo=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,F_UNIT,234,46,206,270);fill_units(d->unit_combo,UNIT_M);
            buttons=105;control(hwnd,L"BUTTON",L"Segment…",WS_TABSTOP,F_REDRAW,20,buttons,120,30);load_cal_fields(d);
        } else if(d->kind==D_SETTINGS) {
            control(hwnd,L"STATIC",L"Measurement unit",0,0,20,20,420,20);d->unit_combo=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,F_UNIT,20,46,420,300);fill_units(d->unit_combo,d->unit);
            control(hwnd,L"STATIC",L"Precision",0,0,20,91,420,20);d->select=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,F_SELECT,20,117,420,300);
            for(i=-10;i<=16;++i) { const wchar_t *note=i==-10 ? L"  ·  1 Å" : i==-9 ? L"  ·  1 nm" : i==-6 ? L"  ·  1 µm" : i==-3 ? L"  ·  1 mm" : i==-2 ? L"  ·  1 cm" : i==0 ? L"  ·  1 m" : i==3 ? L"  ·  1 km" : L"";swprintf(text,180,L"10^%d m%ls",i,note);SendMessageW(d->select,CB_ADDSTRING,0,(LPARAM)text); }
            SendMessageW(d->select,CB_SETCURSEL,d->precision+10,0);
        } else if(d->kind==D_EDIT_MEASURE) {
            control(hwnd,L"STATIC",L"Measurement name",0,0,20,20,420,20);d->name_edit=control(hwnd,L"EDIT",d->name,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,F_NAME,20,46,420,27);SendMessageW(d->name_edit,EM_SETLIMITTEXT,P2L_NAME-1,0);
            buttons=105;control(hwnd,L"BUTTON",L"Segment…",WS_TABSTOP,F_REDRAW,20,buttons,120,30);
        } else if(d->kind==D_EXPORT) {
            const wchar_t *formats[]={L"TIFF (.tif)",L"JPEG (.jpg)",L"PNG (.png)",L"PDF (.pdf)"};control(hwnd,L"STATIC",L"Format",0,0,20,20,420,20);d->select=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,F_SELECT,20,46,420,180);
            for(i=0;i<4;++i) SendMessageW(d->select,CB_ADDSTRING,0,(LPARAM)formats[i]);SendMessageW(d->select,CB_SETCURSEL,d->format,0);
            control(hwnd,L"STATIC",L"Pages",0,0,20,91,420,20);d->scope_combo=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,F_SCOPE,20,117,420,120);
            SendMessageW(d->scope_combo,CB_ADDSTRING,0,(LPARAM)L"Current page");SendMessageW(d->scope_combo,CB_ADDSTRING,0,(LPARAM)L"All pages (TIFF / PDF)");SendMessageW(d->scope_combo,CB_SETCURSEL,d->scope,0);EnableWindow(d->scope_combo,app.document.pages>1 && (d->format==EXPORT_TIFF || d->format==EXPORT_PDF));
        } else if(d->kind==D_RECORD) {
            control(hwnd,L"STATIC",L"Coordinates",0,0,20,20,420,20);d->select=control(hwnd,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,F_SELECT,20,46,420,120);
            SendMessageW(d->select,CB_ADDSTRING,0,(LPARAM)L"Relative (fractions of width and height)");SendMessageW(d->select,CB_ADDSTRING,0,(LPARAM)L"Absolute (pixel indices)");SendMessageW(d->select,CB_SETCURSEL,0,0);buttons=105;
        } else if(d->kind==D_ABOUT || d->kind==D_GUIDE) {
            wchar_t *body=portable_text(d->kind==D_ABOUT ? RESOURCE_LICENSES : RESOURCE_GUIDE);HWND edit;int top=d->kind==D_ABOUT ? 110 : 20;
            if(d->kind==D_ABOUT) control(hwnd,L"STATIC",L"lengto 0.6 · Windows x64\nPortable application in a single executable.\n\nIncluded components and licenses:",0,0,20,20,420,82);
            edit=control(hwnd,L"EDIT",L"",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_TABSTOP,F_HINT,20,top,420,416-top);
            SendMessageW(edit,EM_SETLIMITTEXT,500000,0);SetWindowTextW(edit,body ? body : L"Text unavailable.");free(body);d->name_edit=edit;buttons=430;
        }
        if(d->kind==D_GUIDE) control(hwnd,L"BUTTON",L"GitHub",WS_TABSTOP,CMD_GITHUB,20,buttons,100,30);
        control(hwnd,L"BUTTON",L"Confirm",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,342,buttons,98,30);return 0;
    }
    if(!d) return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_COMMAND:
        if(LOWORD(wp)==CMD_GITHUB) { open_github();return 0; }
        if(LOWORD(wp)==F_SELECT && HIWORD(wp)==CBN_SELCHANGE && d->kind==D_EXPORT) {
            int format=(int)SendMessageW(d->select,CB_GETCURSEL,0,0);int multi=app.document.pages>1 && (format==EXPORT_PDF || format==EXPORT_TIFF);EnableWindow(d->scope_combo,multi);if(!multi) SendMessageW(d->scope_combo,CB_SETCURSEL,0,0);
        }
        if(LOWORD(wp)==IDCANCEL) { d->done=1;return 0; }
        if(LOWORD(wp)==IDOK || LOWORD(wp)==F_REDRAW) {
            wchar_t text[128];d->redraw=LOWORD(wp)==F_REDRAW;
            if(d->kind==D_CALIBRATE) {
                GetWindowTextW(d->value,text,128);if(!parse_positive(text,&d->cal.known)) { dialog_error(hwnd,L"Enter a positive length. A decimal point or comma is accepted.");return 0; }
                d->cal.unit=read_unit(d->unit_combo);if(!d->cal_id) d->redraw=1;
            } else if(d->kind==D_SETTINGS) { d->unit=read_unit(d->unit_combo);d->precision=(int)SendMessageW(d->select,CB_GETCURSEL,0,0)-10; }
            else if(d->kind==D_RECORD) d->scope=SendMessageW(d->select,CB_GETCURSEL,0,0)==0;
            else if(d->kind==D_EDIT_MEASURE) {
                if(!valid_dialog_name(d->name_edit,d->name)) { dialog_error(hwnd,L"Enter a measurement name.");return 0; }
            } else if(d->kind==D_EXPORT) { d->format=(ExportFormat)SendMessageW(d->select,CB_GETCURSEL,0,0);d->scope=(int)SendMessageW(d->scope_combo,CB_GETCURSEL,0,0); }
            d->accepted=d->done=1;return 0;
        }break;
    case WM_CLOSE:d->done=1;return 0;
    case WM_CTLCOLORSTATIC:SetBkMode((HDC)wp,TRANSPARENT);SetTextColor((HDC)wp,RGB(48,51,55));return (LRESULT)app.background;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static int dialog_height(int kind) { return kind==D_ABOUT || kind==D_GUIDE ? 506 : kind==D_SETTINGS || kind==D_EXPORT ? 250 : 181; }
static int show_dialog(Dialog *d) {
    const wchar_t *titles[]={L"Calibrate",L"Settings",L"Edit measurement",L"Export",L"About",L"Guide",L"Start recording"};RECT parent;HWND hwnd;MSG msg;
    int quit=0,height=dialog_height(d->kind);
    GetWindowRect(app.main,&parent);EnableWindow(app.main,FALSE);hwnd=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,L"lengto.dialog",titles[d->kind],WS_POPUP|WS_CAPTION|WS_SYSMENU,parent.left+(parent.right-parent.left-476)/2,parent.top+(parent.bottom-parent.top-height)/2,476,height,app.main,NULL,app.instance,d);
    if(!hwnd) { EnableWindow(app.main,TRUE);return 0; }ShowWindow(hwnd,SW_SHOW);SetFocus(d->kind==D_SETTINGS ? d->unit_combo : d->value ? d->value : d->select ? d->select : d->name_edit);
    while(!d->done) { int ret=GetMessageW(&msg,NULL,0,0);if(ret<=0) { quit=ret==0;break; }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE) SendMessageW(hwnd,WM_COMMAND,IDCANCEL,0);
        else if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN && !SendMessageW(GetFocus(),CB_GETDROPPEDSTATE,0,0)) SendMessageW(hwnd,WM_COMMAND,IDOK,0);
        else if(!IsDialogMessageW(hwnd,&msg)) { TranslateMessage(&msg);DispatchMessageW(&msg); }
    }
    EnableWindow(app.main,TRUE);DestroyWindow(hwnd);SetActiveWindow(app.main);SetFocus(app.canvas);if(quit) PostQuitMessage(0);return d->accepted;
}
static int check_save(void) {
    int answer;if(!app.dirty) return 1;if(app.test_mode) return 0;
    answer=MessageBoxW(app.main,L"Save the annotated drawing before closing?",L"lengto",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(answer==IDNO) return 1;if(answer!=IDYES || !save_output(0)) return 0;
    if(app.dirty) { message(L"Only the current page was saved. Save all pages as TIFF or PDF to keep the remaining annotations.");return 0; }return 1;
}
static void accept_document(Document *doc) {
    Unit unit=app.project->display_unit;int precision=app.project->precision_exp;
    cancel_action();document_close(&app.document);app.document=*doc;memset(doc,0,sizeof(*doc));project_init(app.project,app.document.pages);app.project->display_unit=unit;app.project->precision_exp=precision;
    app.output_path[0]=0;app.dirty=app.document.clipboard;app.active=0;app.selected=-1;recorder_open(&app.recorder,&app.document);recording_check();fit_image();refresh();
}
static int open_path(const wchar_t *input) {
    wchar_t full[P2L_PATH],error[512];DWORD n;Document *doc;
    n=GetFullPathNameW(input,P2L_PATH,full,NULL);if(!n || n>P2L_PATH-48) return 0;
    if(!_wcsicmp(extension(full),L".lengto") || !_wcsicmp(extension(full),L".p2l")) { message(L"Open a TIFF, PNG, JPEG image or a PDF.");return 0; }
    if(!check_save()) return 0;doc=calloc(1,sizeof(*doc));if(!doc) return 0;
    SetCursor(LoadCursorW(NULL,IDC_WAIT));if(!document_open(doc,full,error,512)) { document_close(doc);free(doc);SetCursor(LoadCursorW(NULL,IDC_ARROW));message(error);return 0; }
    accept_document(doc);free(doc);SetCursor(LoadCursorW(NULL,IDC_ARROW));return 1;
}
static void paste_image(void) {
    Document *doc;wchar_t error[512];int ok;if(app.runner || !check_save()) return;
    doc=calloc(1,sizeof(*doc));if(!doc) return;SetCursor(LoadCursorW(NULL,IDC_WAIT));
    ok=document_paste(doc,app.main,error,512);if(ok) accept_document(doc);else document_close(doc);
    free(doc);SetCursor(LoadCursorW(NULL,IDC_ARROW));if(!ok) message(error);
}
static void close_document(void) {
    Unit unit=app.project->display_unit;int precision=app.project->precision_exp;if(!check_save()) return;
    cancel_action();recorder_close(&app.recorder);recording_check();document_close(&app.document);project_init(app.project,1);app.project->display_unit=unit;app.project->precision_exp=precision;app.output_path[0]=0;app.active=app.dirty=0;app.selected=-1;refresh();
}
static void change_page(int page) {
    wchar_t error[512];int measuring=app.mode==MODE_MEASURE;if(!app.document.bitmap || page<0 || page>=app.document.pages || page==app.document.page) return;
    cancel_action();SetCursor(LoadCursorW(NULL,IDC_WAIT));if(document_page(&app.document,page,error,512)) { recorder_page(&app.recorder,page);recording_check();app.selected=-1;app.active=0;fit_image();refresh(); }else message(error);SetCursor(LoadCursorW(NULL,IDC_ARROW));
    if(measuring) set_mode(MODE_MEASURE);
}
static void calibration_command(void) {
    Dialog d={0};d.kind=D_CALIBRATE;d.cal_id=app.active;if(!app.document.bitmap || !show_dialog(&d)) return;app.active=d.cal_id;
    if(d.redraw) { set_mode(MODE_CALIBRATE);app.pending_cal=d.cal;app.replace_cal=d.cal_id; }
    else { Calibration *c=find_cal(app.project,d.cal_id);if(c) { *c=d.cal;recorder_calibration(&app.recorder,&app.document,c);changed(); } }refresh();
}
static void measure_command(void) {
    if(!app.document.bitmap) return;
    if(!app.project->nc) { calibration_command();return; }set_mode(MODE_MEASURE);
}
static void edit_measurement(void) {
    Dialog d={0};Measurement *m;if(app.selected<0) return;m=&app.project->meas[app.selected];d.kind=D_EDIT_MEASURE;wcscpy(d.name,m->name);
    if(!show_dialog(&d)) return;wcscpy(m->name,d.name);recorder_measurement(&app.recorder,&app.document,m,1);changed();
    if(d.redraw) { int index=app.selected;set_mode(MODE_REPLACE_MEASURE);app.replace_measure=index; }
}
static int export_document(const wchar_t *path,ExportFormat format,int all_pages) {
    wchar_t error[1024];int ok;SetCursor(LoadCursorW(NULL,IDC_WAIT));ok=drawing_export(&app.document,app.project,path,format,all_pages,error,1024);
    SetCursor(LoadCursorW(NULL,IDC_ARROW));refresh();if(!ok) message(error);return ok;
}
static ExportFormat format_from_path(const wchar_t *path) {
    const wchar_t *ext=extension(path);
    if(!_wcsicmp(ext,L".tif") || !_wcsicmp(ext,L".tiff")) return EXPORT_TIFF;
    if(!_wcsicmp(ext,L".jpg") || !_wcsicmp(ext,L".jpeg")) return EXPORT_JPEG;
    if(!_wcsicmp(ext,L".pdf")) return EXPORT_PDF;return EXPORT_PNG;
}
static const wchar_t *format_extension(ExportFormat format) {
    return format==EXPORT_TIFF ? L"tif" : format==EXPORT_JPEG ? L"jpg" : format==EXPORT_PNG ? L"png" : L"pdf";
}
static void record_output(const wchar_t *path,ExportFormat format,int all_pages) {
    int i;wcscpy(app.output_path,path);app.output_format=format;app.output_all=all_pages;app.dirty=0;
    recorder_export(&app.recorder,path,all_pages);recording_check();
    if(!all_pages) {
        for(i=0;i<app.project->nm;++i) if(app.project->meas[i].page!=app.document.page) app.dirty=1;
        if(app.project->nc && app.project->cal[0].page!=app.document.page) app.dirty=1;
    }refresh();
}
static int save_output(int save_as) {
    wchar_t path[P2L_PATH];ExportFormat format;int all_pages;
    if(!app.document.bitmap) return 0;
    if(save_as || !*app.output_path) {
        OPENFILENAMEW f={0};const wchar_t *ext;
        format=*app.output_path ? app.output_format : format_from_path(app.document.path);ext=format_extension(format);
        if(*app.output_path) wcscpy(path,app.output_path);else if(app.document.clipboard) swprintf(path,P2L_PATH,L"clipboard.%ls",ext);else swprintf(path,P2L_PATH,L"%ls.measured.%ls",app.document.path,ext);
        f.lStructSize=sizeof(f);f.hwndOwner=app.main;f.lpstrFile=path;f.nMaxFile=P2L_PATH;
        f.lpstrFilter=L"TIFF\0*.tif;*.tiff\0JPEG\0*.jpg;*.jpeg\0PNG\0*.png\0PDF\0*.pdf\0\0";f.nFilterIndex=format+1;f.lpstrDefExt=ext;f.lpstrTitle=L"Save annotated drawing";
        f.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;if(!GetSaveFileNameW(&f)) return 0;
        ext=extension(path);if(_wcsicmp(ext,L".png") && _wcsicmp(ext,L".jpg") && _wcsicmp(ext,L".jpeg") && _wcsicmp(ext,L".tif") && _wcsicmp(ext,L".tiff") && _wcsicmp(ext,L".pdf")) { message(L"Choose a TIFF, JPEG, PNG or PDF extension.");return 0; }
        format=(ExportFormat)(f.nFilterIndex-1);
        if(format_from_path(path)!=format) {
            wchar_t *dot=wcsrchr(path,L'.');size_t base=(size_t)(dot-path);const wchar_t *chosen=format_extension(format);
            if(base+wcslen(chosen)+2>=P2L_PATH) return 0;swprintf(dot,P2L_PATH-base,L".%ls",chosen);
            if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES && MessageBoxW(app.main,L"A file with the selected extension already exists. Replace it?",L"lengto",MB_YESNO|MB_ICONQUESTION)!=IDYES) return 0;
        }
        all_pages=format==EXPORT_TIFF || format==EXPORT_PDF;
    } else { wcscpy(path,app.output_path);format=app.output_format;all_pages=app.output_all; }
    if(!export_document(path,format,all_pages)) return 0;record_output(path,format,all_pages);return 1;
}
static void export_command(void) {
    Dialog d={0};wchar_t path[P2L_PATH],filter[120];const wchar_t *ext;size_t len;
    if(!app.document.bitmap) return;d.kind=D_EXPORT;d.format=EXPORT_PNG;if(!show_dialog(&d)) return;
    ext=format_extension(d.format);if(app.document.clipboard) swprintf(path,P2L_PATH,L"clipboard.%ls",ext);else swprintf(path,P2L_PATH,L"%ls.measured.%ls",app.document.path,ext);
    len=(size_t)swprintf(filter,120,L"Export %ls",ext);swprintf(filter+len+1,120-len-1,L"*.%ls",ext);filter[len+1+wcslen(filter+len+1)+1]=0;
    if(file_dialog(1,path,filter,ext,L"Export annotated drawing")) {
        wchar_t suffix[16];swprintf(suffix,16,L".%ls",ext);
        if(_wcsicmp(extension(path),suffix) && !(d.format==EXPORT_TIFF && !_wcsicmp(extension(path),L".tiff")) && !(d.format==EXPORT_JPEG && !_wcsicmp(extension(path),L".jpeg"))) { message(L"The extension must match the selected format.");return; }
        if(export_document(path,d.format,d.scope!=0)) record_output(path,d.format,d.scope!=0);
    }
}
static void recording_check(void) {
    if(app.recorder.active && (app.recorder.body.failed || *app.recorder.error)) {
        wchar_t error[512];swprintf(error,512,L"%ls",*app.recorder.error ? app.recorder.error : L"Not enough memory to continue recording.");recorder_reset(&app.recorder);message(error);
    }
}
static void start_recording(void) {
    Dialog d={0};if(app.recorder.active || app.runner) return;d.kind=D_RECORD;if(!show_dialog(&d)) return;
    recorder_start(&app.recorder,&app.document,app.project,d.scope);recording_check();refresh();
}
static void stop_recording(void) {
    wchar_t path[P2L_PATH]=L"macro.py";if(!app.recorder.active) return;
    if(!file_dialog(1,path,L"Python script\0*.py\0\0",L"py",L"Save Python recording")) return;
    if(_wcsicmp(extension(path),L".py")) { message(L"Save the macro with a .py extension.");return; }
    if(!recorder_save(&app.recorder,path)) message(L"Cannot save the macro. The recording is still available: choose another destination.");refresh();
}
static int choose_python(void) {
    DWORD n;if(*app.python) return 1;
    n=GetEnvironmentVariableW(L"LENGTO_PYTHON",app.python,P2L_PATH);if(n && n<P2L_PATH && GetFileAttributesW(app.python)!=INVALID_FILE_ATTRIBUTES) return 1;app.python[0]=0;
    if(SearchPathW(NULL,L"py.exe",NULL,P2L_PATH,app.python,NULL)) return 1;
    if(SearchPathW(NULL,L"python.exe",NULL,P2L_PATH,app.python,NULL) && !wcsstr(app.python,L"WindowsApps")) return 1;
    app.python[0]=0;message(L"Select Python 3.8 or later (python.exe or py.exe) to run scripts. A portable interpreter also works.");
    return file_dialog(0,app.python,L"Python interpreter\0python.exe;py.exe\0\0",NULL,L"Select the Python interpreter");
}
static int start_script(const wchar_t *path) {
    wchar_t error[512]=L"Cannot start the script.";if(app.runner || app.recorder.active || !check_save() || !choose_python()) return 0;
    cancel_action();app.runner=runner_start(app.main,app.python,path,error,512);if(!app.runner) { app.python[0]=0;message(error);return 0; }
    SetTimer(app.main,1,50,NULL);refresh();return 1;
}
static void run_script(void) {
    wchar_t path[P2L_PATH]=L"";if(app.runner || app.recorder.active) return;
    if(file_dialog(0,path,L"Python script\0*.py\0\0",L"py",L"Run Python script")) start_script(path);
}
static char *script_request(const char *request) {
    Automation a={0};char *reply;if(!app.runner || runner_cancelled(app.runner)) return _strdup("{\"ok\":false,\"error\":\"Execution interrupted\"}");
    a.document=&app.document;a.project=app.project;a.dirty=app.dirty;reply=automation_execute(&a,request);app.dirty=a.dirty;
    if(a.changes&AUTO_OPEN) app.output_path[0]=0;
    if(a.changes&AUTO_EXPORT) { wcscpy(app.output_path,a.output);app.output_format=a.format;app.output_all=a.all_pages; }
    if(a.changes) { app.selected=-1;if(a.changes&AUTO_VIEW) fit_image();refresh(); }return reply;
}
static void script_tick(void) {
    DWORD exit_code;wchar_t *log;int cancelled;if(!app.runner || !runner_done(app.runner,&exit_code)) return;
    app.script_exit=exit_code;cancelled=runner_cancelled(app.runner);log=runner_log(app.runner);runner_free(app.runner);app.runner=NULL;KillTimer(app.main,1);refresh();
    if(!cancelled && exit_code) { message(log && *log ? log : L"The script ended with an error. Check that Python is available and that the script uses the lengto API.");app.python[0]=0; }
    else if(!cancelled && log && *log) message(log);free(log);
    if(app.closing) { app.closing=0;PostMessageW(app.main,WM_CLOSE,0,0); }
}
static void command(int id) {
    if(app.runner && id!=CMD_HELP && id!=CMD_ABOUT && id!=CMD_FIT && id!=CMD_GITHUB) return;
    Dialog d={0};switch(id) {
    case CMD_OPEN:{ wchar_t path[P2L_PATH]=L"";if(file_dialog(0,path,drawings_filter,NULL,L"Open image or PDF")) open_path(path); }break;
    case CMD_PASTE:paste_image();break;
    case CMD_SAVE:save_output(0);break;case CMD_SAVE_AS:save_output(1);break;case CMD_EXPORT:export_command();break;case CMD_CLOSE:close_document();break;
    case CMD_CALIBRATE:cancel_action();calibration_command();break;case CMD_MEASURE:measure_command();break;
    case CMD_SETTINGS:d.kind=D_SETTINGS;d.unit=app.project->display_unit;d.precision=app.project->precision_exp;
        if(show_dialog(&d)) { app.project->display_unit=d.unit;app.project->precision_exp=d.precision;recorder_settings(&app.recorder,app.project);recording_check();if(app.document.bitmap && app.project->nm) changed();else refresh(); }break;
    case CMD_RECORD:start_recording();break;case CMD_STOP_RECORD:stop_recording();break;case CMD_RUN_SCRIPT:run_script();break;
    case CMD_FIT:fit_image();refresh();break;
    case CMD_HELP:d.kind=D_GUIDE;show_dialog(&d);break;
    case CMD_GITHUB:open_github();break;
    case CMD_ABOUT:d.kind=D_ABOUT;show_dialog(&d);break;
    }
}
static void finish_segment(Point end) {
    if(point_distance(app.first,end)*app.view.sx<3) { app.drawing=0;refresh();return; }
    if(app.mode==MODE_CALIBRATE) {
        if(app.replace_cal) { Calibration *c=find_cal(app.project,app.replace_cal);if(c) { *c=app.pending_cal;c->a=app.first;c->b=end;c->page=app.document.page;app.active=c->id; } }
        else { int id=add_cal(app.project,app.document.page,app.pending_cal.name,app.first,end,app.pending_cal.known,app.pending_cal.unit);if(!id) { message(L"Only one calibration is allowed per document.");cancel_action();return; }app.active=id; }
        recorder_calibration(&app.recorder,&app.document,get_cal(app.project,app.active));
    } else if(app.mode==MODE_MEASURE) {
        wchar_t name[P2L_NAME];swprintf(name,P2L_NAME,L"Measurement %d",app.project->next_meas);
        if(!add_meas(app.project,app.document.page,app.active,name,app.first,end)) { message(L"Cannot add the measurement (maximum 4096 per document).");cancel_action();return; }app.selected=app.project->nm-1;recorder_measurement(&app.recorder,&app.document,&app.project->meas[app.selected],0);
    } else if(app.mode==MODE_REPLACE_MEASURE && app.replace_measure>=0) { Measurement *m=&app.project->meas[app.replace_measure];m->a=app.first;m->b=end;recorder_measurement(&app.recorder,&app.document,m,1); }
    if(app.mode==MODE_MEASURE) { app.drawing=0;app.selected=-1; }else cancel_action();changed();
}
static void click_at(int x,int y) {
    Point p;if(!app.document.bitmap) return;
    if(app.mode==MODE_IDLE) {
        int i;app.selected=hit_measurement(app.project,app.document.page,&app.view,x,y);
        if(app.selected>=0) app.active=app.project->meas[app.selected].calibration;
        else for(i=app.project->nc-1;i>=0;--i) { Calibration *c=&app.project->cal[i];POINT a,b;double dx,dy,t,len;if(c->page!=app.document.page) continue;
            a=view_point(&app.view,c->a);b=view_point(&app.view,c->b);dx=b.x-a.x;dy=b.y-a.y;len=dx*dx+dy*dy;t=len ? fmax(0,fmin(1,((x-a.x)*dx+(y-a.y)*dy)/len)) : 0;
            if(hypot(x-a.x-t*dx,y-a.y-t*dy)<8) { app.active=c->id;break; }
        }refresh();return;
    }
    p=image_point(x,y,0);if(p.x<0 || p.y<0 || p.x>app.document.logical_width || p.y>app.document.logical_height) return;p=constrain(snapped_point(x,y));
    if(!app.drawing) { app.first=app.hover=p;app.drawing=1;refresh(); }else finish_segment(p);
}
static void draw_canvas(HDC dc,RECT r) {
    HBRUSH brush=CreateSolidBrush(RGB(235,236,237));FillRect(dc,&r,brush);DeleteObject(brush);
    if(!app.document.bitmap) {
        RECT text={20,0,r.right-20,r.bottom};SetBkMode(dc,TRANSPARENT);SelectObject(dc,app.font);SetTextColor(dc,RGB(125,128,132));DrawTextW(dc,L"open, drop or paste",-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return;
    }
    render_drawing(dc,&app.document,app.project,&app.view);
    if(app.drawing) {
        wchar_t text[180];const Calibration *c=get_cal(app.project,app.active);
        if(app.mode!=MODE_CALIBRATE && c) format_length(point_distance(app.first,app.hover)*c->known*unit_metres(c->unit)/point_distance(c->a,c->b),app.project->display_unit,app.project->precision_exp,text,180);
        else swprintf(text,180,L"%.2f px",point_distance(app.first,app.hover));render_segment(dc,&app.view,app.first,app.hover,RGB(52,60,65),1,1,text);
    }
}
static LRESULT CALLBACK canvas_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:app.view.width=LOWORD(lp);app.view.height=HIWORD(lp);return 0;
    case WM_PRINTCLIENT:{ RECT r;GetClientRect(hwnd,&r);draw_canvas((HDC)wp,r);return 0; }
    case WM_PAINT:{ PAINTSTRUCT ps;RECT r;HDC dc=BeginPaint(hwnd,&ps),buffer=CreateCompatibleDC(dc);HBITMAP bitmap;HGDIOBJ old;
        GetClientRect(hwnd,&r);bitmap=CreateCompatibleBitmap(dc,max(1,r.right),max(1,r.bottom));old=SelectObject(buffer,bitmap);draw_canvas(buffer,r);BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);EndPaint(hwnd,&ps);return 0; }
    case WM_SETCURSOR:SetCursor(LoadCursorW(NULL,app.panning ? IDC_SIZEALL : app.mode==MODE_IDLE ? IDC_ARROW : IDC_CROSS));return TRUE;
    case WM_LBUTTONDOWN:if(app.runner) return 0;SetFocus(hwnd);app.press=1;app.panning=0;app.down.x=x;app.down.y=y;app.down_ox=app.view.ox;app.down_oy=app.view.oy;SetCapture(hwnd);return 0;
    case WM_MOUSEMOVE:
        if(app.press && app.document.bitmap) {
            if(abs(x-app.down.x)>=max(4,GetSystemMetrics(SM_CXDRAG)) || abs(y-app.down.y)>=max(4,GetSystemMetrics(SM_CYDRAG))) app.panning=1;
            if(app.panning) { app.view.ox=app.down_ox+x-app.down.x;app.view.oy=app.down_oy+y-app.down.y;SetCursor(LoadCursorW(NULL,IDC_SIZEALL));InvalidateRect(hwnd,NULL,FALSE); }
        }
        if(app.drawing) { app.hover=constrain(snapped_point(x,y));InvalidateRect(hwnd,NULL,FALSE); }return 0;
    case WM_LBUTTONUP:{ int dragged=app.panning,pressed=app.press;app.press=app.panning=0;if(GetCapture()==hwnd) ReleaseCapture();if(pressed && !dragged) click_at(x,y);return 0; }
    case WM_CAPTURECHANGED:app.press=app.panning=0;return 0;
    case WM_RBUTTONUP:if(!app.runner && app.mode==MODE_IDLE) { click_at(x,y);if(app.selected>=0) edit_measurement(); }return 0;
    case WM_MOUSEWHEEL:
        if(app.document.bitmap) { POINT pt={x,y};Point p;double zoom;ScreenToClient(hwnd,&pt);p=image_point(pt.x,pt.y,0);
            zoom=fmax(0.001,fmin(fmax(app.view.width,app.view.height),app.view.sx*pow(1.2,GET_WHEEL_DELTA_WPARAM(wp)/120.0)));app.view.sx=app.view.sy=zoom;app.view.ox=pt.x-p.x*zoom;app.view.oy=pt.y-p.y*zoom;
            if(app.press) { app.down=pt;app.down_ox=app.view.ox;app.down_oy=app.view.oy; }if(app.drawing) app.hover=constrain(snapped_point(pt.x,pt.y));refresh();
        }return 0;
    case WM_KEYDOWN:SendMessageW(app.main,msg,wp,lp);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
typedef struct { RECT previous,number,total,next; } PageRects;
static PageRects page_rects(void) {
    RECT r;PageRects p;GetClientRect(app.main,&r);
    p.previous=(RECT){r.right-174,r.bottom-27,r.right-144,r.bottom};
    p.number=(RECT){r.right-141,r.bottom-25,r.right-86,r.bottom-2};
    p.total=(RECT){r.right-84,r.bottom-27,r.right-42,r.bottom};
    p.next=(RECT){r.right-38,r.bottom-27,r.right-8,r.bottom};return p;
}
static void end_page_entry(int apply) {
    wchar_t text[16],*end;unsigned long page=0;if(!app.editing_page) return;app.editing_page=0;
    GetWindowTextW(app.page_edit,text,16);ShowWindow(app.page_edit,SW_HIDE);
    if(apply && *text) { page=wcstoul(text,&end,10);if(!*end) change_page((int)max(1,min((unsigned long)app.document.pages,page))-1); }
    InvalidateRect(app.main,NULL,FALSE);
}
static LRESULT CALLBACK page_entry_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id;(void)data;
    if(msg==WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if(msg==WM_KEYDOWN && (wp==VK_RETURN || wp==VK_ESCAPE)) { end_page_entry(wp==VK_RETURN);SetFocus(app.canvas);return 0; }
    if(msg==WM_KILLFOCUS) end_page_entry(1);
    return DefSubclassProc(hwnd,msg,wp,lp);
}
static void begin_page_entry(void) {
    PageRects p=page_rects();wchar_t text[16];if(app.runner || app.document.pages<2) return;
    swprintf(text,16,L"%d",app.document.page+1);SetWindowTextW(app.page_edit,text);app.editing_page=1;
    MoveWindow(app.page_edit,p.number.left,p.number.top,p.number.right-p.number.left,p.number.bottom-p.number.top,TRUE);
    ShowWindow(app.page_edit,SW_SHOW);SetFocus(app.page_edit);SendMessageW(app.page_edit,EM_SETSEL,0,-1);InvalidateRect(app.main,NULL,FALSE);
}
static void draw_page_arrow(HDC dc,RECT r,int forward,int enabled) {
    int x=(r.left+r.right)/2,y=(r.top+r.bottom)/2,dir=forward ? 1 : -1;HPEN pen=CreatePen(PS_SOLID,2,enabled ? RGB(65,72,76) : RGB(190,194,196));HGDIOBJ old=SelectObject(dc,pen);
    MoveToEx(dc,x-dir*3,y-5,NULL);LineTo(dc,x+dir*3,y);LineTo(dc,x-dir*3,y+5);SelectObject(dc,old);DeleteObject(pen);
}
static void draw_status(HWND hwnd,HDC dc) {
    RECT r,text;wchar_t status[512],tail[32];const Calibration *c=get_cal(app.project,app.active);GetClientRect(hwnd,&r);r.top=max(0,r.bottom-27);
    FillRect(dc,&r,app.background);SetBkMode(dc,TRANSPARENT);SelectObject(dc,app.font_small);SetTextColor(dc,RGB(95,99,104));
    if(app.runner) wcscpy(status,runner_cancelled(app.runner) ? L"Stopping script…" : L"Script running · Esc to stop");
    else if(app.mode!=MODE_IDLE) swprintf(status,512,L"%ls · %ls · pixel snap · Esc to exit",app.mode==MODE_CALIBRATE ? L"Calibrate" : L"Measure",app.drawing ? L"second point" : L"first point");
    else if(app.document.bitmap) swprintf(status,512,L"%ls%ls%ls · %ls · step 10^%d m · %.0f%%%ls",app.selected>=0 ? app.project->meas[app.selected].name : L"",app.selected>=0 ? L" · " : L"",c ? c->name : L"Not calibrated",unit_name(app.project->display_unit),app.project->precision_exp,app.view.sx*100,app.document.width+1<app.document.logical_width ? L" · reduced preview" : L"");
    else status[0]=0;
    if(app.recorder.active) { size_t n=wcslen(status);if(n+6<512) { memmove(status+6,status,(n+1)*sizeof(wchar_t));wmemcpy(status,L"REC · ",6); }SetTextColor(dc,RGB(174,48,42)); }
    text=r;text.left=12;text.right-=app.document.pages>1 ? 184 : 12;DrawTextW(dc,status,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    if(app.document.pages>1) {
        PageRects p=page_rects();draw_page_arrow(dc,p.previous,0,!app.runner && app.document.page>0);draw_page_arrow(dc,p.next,1,!app.runner && app.document.page+1<app.document.pages);
        SetTextColor(dc,app.runner ? RGB(160,164,167) : RGB(65,72,76));
        if(!app.editing_page) { swprintf(tail,32,L"%d",app.document.page+1);DrawTextW(dc,tail,-1,&p.number,DT_RIGHT|DT_SINGLELINE|DT_VCENTER); }
        swprintf(tail,32,L"/ %d",app.document.pages);DrawTextW(dc,tail,-1,&p.total,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
    }
}
static int brand_height(void) { return max(16,GetSystemMetrics(SM_CYMENU)); }
static void draw_brand(HDC dc,RECT r) {
    int height=min(brand_height(),r.bottom-r.top),width=height*2;
    DrawIconEx(dc,r.left+(r.right-r.left-width)/2,r.top+(r.bottom-r.top-height)/2,app.menu_icon,width,height,0,NULL,DI_NORMAL);
}
static HMENU create_menu(void) {
    HMENU bar=CreateMenu(),file=CreatePopupMenu(),measure=CreatePopupMenu(),help=CreatePopupMenu(),script=CreatePopupMenu();
    AppendMenuW(file,MF_STRING,CMD_OPEN,L"&Open…\tCtrl+O");AppendMenuW(file,MF_STRING,CMD_PASTE,L"&Paste\tCtrl+V");AppendMenuW(file,MF_STRING,CMD_SAVE,L"&Save\tCtrl+S");AppendMenuW(file,MF_STRING,CMD_SAVE_AS,L"Save &as…\tCtrl+Shift+S");
    AppendMenuW(file,MF_STRING,CMD_EXPORT,L"&Export…\tCtrl+E");AppendMenuW(file,MF_STRING,CMD_RUN_SCRIPT,L"Run s&cript…");AppendMenuW(file,MF_SEPARATOR,0,NULL);AppendMenuW(file,MF_STRING,CMD_CLOSE,L"&Close\tCtrl+W");
    AppendMenuW(measure,MF_STRING,CMD_CALIBRATE,L"&Calibrate…\tC");AppendMenuW(measure,MF_STRING,CMD_MEASURE,L"&Measure\tM");AppendMenuW(measure,MF_SEPARATOR,0,NULL);
    AppendMenuW(measure,MF_STRING,CMD_SETTINGS,L"&Settings…");
    AppendMenuW(help,MF_STRING,CMD_HELP,L"&Guide\tF1");AppendMenuW(help,MF_STRING,CMD_GITHUB,L"Git&Hub");AppendMenuW(help,MF_STRING,CMD_ABOUT,L"&About");
    AppendMenuW(script,MF_STRING,CMD_RECORD,L"&Start recording…");AppendMenuW(script,MF_STRING,CMD_STOP_RECORD,L"&Stop recording…");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)file,L"&File");AppendMenuW(bar,MF_POPUP,(UINT_PTR)measure,L"&Measurements");AppendMenuW(bar,MF_POPUP,(UINT_PTR)script,L"&Script");AppendMenuW(bar,MF_STRING,CMD_FIT,L"&Fit");AppendMenuW(bar,MF_POPUP,(UINT_PTR)help,L"?");
    /* A disabled, right-aligned emblem leaves menu commands and shortcuts intact. */
    { MENUITEMINFOW item={0};item.cbSize=sizeof(item);item.fMask=MIIM_FTYPE|MIIM_STATE|MIIM_ID|MIIM_STRING;item.fType=MFT_OWNERDRAW|MFT_RIGHTJUSTIFY;item.fState=MFS_DISABLED;item.wID=CMD_BRAND;item.dwTypeData=L"lengto";InsertMenuItemW(bar,GetMenuItemCount(bar),TRUE,&item); }
    return bar;
}
static LRESULT CALLBACK main_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_MEASUREITEM:{ MEASUREITEMSTRUCT *item=(MEASUREITEMSTRUCT *)lp;
        if(item->CtlType==ODT_MENU && item->itemID==CMD_BRAND) { item->itemWidth=brand_height()*2+12;item->itemHeight=brand_height();return TRUE; }break; }
    case WM_DRAWITEM:{ DRAWITEMSTRUCT *item=(DRAWITEMSTRUCT *)lp;
        if(item->CtlType==ODT_MENU && item->itemID==CMD_BRAND) { int saved=SaveDC(item->hDC);FillRect(item->hDC,&item->rcItem,GetSysColorBrush(COLOR_MENUBAR));draw_brand(item->hDC,item->rcItem);RestoreDC(item->hDC,saved);return TRUE; }break; }
    case WM_CREATE:
        app.main=hwnd;app.canvas=CreateWindowExW(0,L"lengto.canvas",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,hwnd,NULL,app.instance,NULL);
        app.page_edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_BORDER|ES_NUMBER|ES_RIGHT|WS_TABSTOP,0,0,0,0,hwnd,(HMENU)301,app.instance,NULL);
        SendMessageW(app.page_edit,WM_SETFONT,(WPARAM)app.font_small,TRUE);SendMessageW(app.page_edit,EM_SETLIMITTEXT,10,0);SetWindowSubclass(app.page_edit,page_entry_proc,1,0);DragAcceptFiles(hwnd,TRUE);return 0;
    case WM_SIZE:
        MoveWindow(app.canvas,0,0,LOWORD(lp),max(0,HIWORD(lp)-27),TRUE);
        if(app.editing_page) { PageRects p=page_rects();MoveWindow(app.page_edit,p.number.left,p.number.top,p.number.right-p.number.left,p.number.bottom-p.number.top,TRUE); }
        InvalidateRect(hwnd,NULL,FALSE);return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO *)lp)->ptMinTrackSize.x=600;((MINMAXINFO *)lp)->ptMinTrackSize.y=420;return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PRINTCLIENT:draw_status(hwnd,(HDC)wp);return 0;
    case WM_PAINT:{ PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);draw_status(hwnd,dc);EndPaint(hwnd,&ps);return 0; }
    case WM_COMMAND:if((HWND)lp==app.page_edit) return 0;command(LOWORD(wp));SetFocus(app.canvas);return 0;
    case WM_SCRIPT_REQUEST:return (LRESULT)script_request((const char *)lp);
    case WM_TIMER:if(wp==1) script_tick();return 0;
    case WM_LBUTTONUP:
        if(!app.runner && app.document.pages>1) {
            PageRects p=page_rects();POINT pt={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(PtInRect(&p.previous,pt)) { end_page_entry(0);change_page(app.document.page-1); }
            else if(PtInRect(&p.next,pt)) { end_page_entry(0);change_page(app.document.page+1); }
            else if(PtInRect(&p.number,pt) || PtInRect(&p.total,pt)) begin_page_entry();
        }return 0;
    case WM_DROPFILES:{ wchar_t path[P2L_PATH];HDROP drop=(HDROP)wp;if(!app.runner && DragQueryFileW(drop,0,path,P2L_PATH)) open_path(path);DragFinish(drop);return 0; }
    case WM_KEYDOWN:
        if(app.runner) { if(wp==VK_ESCAPE) { runner_cancel(app.runner);refresh(); }return 0; }
        if(wp==VK_ESCAPE) { cancel_action();return 0; }if(wp==VK_DELETE) { if(app.selected>=0) { cancel_action();recorder_delete(&app.recorder,app.project->meas[app.selected].id);remove_meas(app.project,app.selected);app.selected=-1;changed(); }return 0; }
        if(wp=='C') { cancel_action();command(CMD_CALIBRATE); }else if(wp=='M') { cancel_action();command(CMD_MEASURE); }else if(wp=='F') fit_image();else if(wp==VK_PRIOR) change_page(app.document.page-1);else if(wp==VK_NEXT) change_page(app.document.page+1);return 0;
    case WM_CLOSE:
        if(app.runner) { app.closing=1;runner_cancel(app.runner);refresh();return 0; }
        if(app.recorder.active) { stop_recording();if(app.recorder.active) return 0; }
        if(check_save()) DestroyWindow(hwnd);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static int smoke_preview(const wchar_t *source,const wchar_t *dest);
static int smoke_script(const wchar_t *source,const wchar_t *dest,const wchar_t *python);
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show) {
    WNDCLASSEXW wc={0};INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_STANDARD_CLASSES};MSG msg;HWND hwnd;LPWSTR *argv;int argc,exit_code=0;
    ACCEL shortcuts[]={{FVIRTKEY|FCONTROL,'O',CMD_OPEN},{FVIRTKEY|FCONTROL,'V',CMD_PASTE},{FVIRTKEY|FCONTROL|FSHIFT,'S',CMD_SAVE_AS},{FVIRTKEY|FCONTROL,'S',CMD_SAVE},{FVIRTKEY|FCONTROL,'E',CMD_EXPORT},{FVIRTKEY|FCONTROL,'W',CMD_CLOSE},{FVIRTKEY,VK_F1,CMD_HELP}};HACCEL accel;
    (void)previous;(void)args;app.instance=instance;app.selected=-1;app.replace_measure=-1;app.view.sx=app.view.sy=1;app.project=calloc(1,sizeof(Project));if(!app.project) return 1;project_init(app.project,1);
    SetThreadUILanguage(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US));SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME,L"en-US\0en\0",NULL);
    argv=CommandLineToArgvW(GetCommandLineW(),&argc);app.test_mode=argc>1 && (!wcscmp(argv[1],L"--preview") || !wcscmp(argv[1],L"--script-test"));
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);InitCommonControlsEx(&ic);if(!imaging_init()) { message(L"Cannot initialize the image readers.");return 1; }
    if(argc==2 && !wcscmp(argv[1],L"--automation")) { exit_code=automation_stdio();imaging_shutdown();CoUninitialize();LocalFree(argv);free(app.project);return exit_code; }
    app.font=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");app.font_small=CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    app.view.font=app.font;app.background=CreateSolidBrush(RGB(249,249,249));
    app.menu_icon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_SQUADRETTA),IMAGE_ICON,brand_height()*2,brand_height(),LR_SHARED);
    wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);
    wc.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_LENGTO),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    wc.hIconSm=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_LENGTO),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    wc.lpfnWndProc=main_proc;wc.lpszClassName=L"lengto";RegisterClassExW(&wc);
    wc.hIcon=wc.hIconSm=NULL;wc.lpfnWndProc=canvas_proc;wc.lpszClassName=L"lengto.canvas";RegisterClassExW(&wc);wc.lpfnWndProc=dialog_proc;wc.lpszClassName=L"lengto.dialog";wc.hbrBackground=app.background;RegisterClassExW(&wc);app.menu=create_menu();
    hwnd=CreateWindowExW(WS_EX_ACCEPTFILES,L"lengto",L"lengto",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1160,820,NULL,app.menu,instance,NULL);if(!hwnd) return 1;
    { RECT r;GetClientRect(hwnd,&r);MoveWindow(app.canvas,0,0,r.right,max(0,r.bottom-27),TRUE); }refresh();accel=CreateAcceleratorTableW(shortcuts,sizeof(shortcuts)/sizeof(shortcuts[0]));
    if(app.test_mode) { exit_code=(!wcscmp(argv[1],L"--preview") ? argc==4 && smoke_preview(argv[2],argv[3]) : argc==5 && smoke_script(argv[2],argv[3],argv[4])) ? 0 : 1;app.dirty=0;DestroyWindow(hwnd); }
    else { ShowWindow(hwnd,show);UpdateWindow(hwnd);if(argc>1) open_path(argv[1]);SetFocus(app.canvas);while(GetMessageW(&msg,NULL,0,0)>0) if(msg.hwnd==app.page_edit || !TranslateAcceleratorW(hwnd,accel,&msg)) { TranslateMessage(&msg);DispatchMessageW(&msg); } }
    recorder_reset(&app.recorder);DestroyAcceleratorTable(accel);LocalFree(argv);document_close(&app.document);imaging_shutdown();CoUninitialize();DeleteObject(app.font);DeleteObject(app.font_small);DeleteObject(app.background);free(app.project);return exit_code;
}
static void test_click(Point p) { POINT s=view_point(&app.view,p);SendMessageW(app.canvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(s.x,s.y));SendMessageW(app.canvas,WM_LBUTTONUP,0,MAKELPARAM(s.x,s.y)); }
static int preview_window(const wchar_t *path) {
    RECT r;HDC dc,buffer;HBITMAP bitmap;HGDIOBJ old;int ok;GetClientRect(app.main,&r);dc=GetDC(app.main);buffer=CreateCompatibleDC(dc);bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom+28);old=SelectObject(buffer,bitmap);
    { RECT top={0,0,r.right,28};FillRect(buffer,&top,app.background);SelectObject(buffer,app.font);SetBkMode(buffer,TRANSPARENT);SetTextColor(buffer,RGB(30,32,35));TextOutW(buffer,12,5,L"File",4);TextOutW(buffer,64,5,L"Measurements",12);TextOutW(buffer,174,5,L"Script",6);TextOutW(buffer,244,5,L"Fit",3);TextOutW(buffer,320,5,L"?",1); }
    { RECT brand={r.right-brand_height()*2-12,0,r.right,28};draw_brand(buffer,brand); }
    SetViewportOrgEx(buffer,0,28,NULL);draw_status(app.main,buffer);GetClientRect(app.canvas,&r);draw_canvas(buffer,r);
    if(app.editing_page) { PageRects p=page_rects();SetViewportOrgEx(buffer,p.number.left,p.number.top+28,NULL);SendMessageW(app.page_edit,WM_PRINT,(WPARAM)buffer,PRF_CLIENT|PRF_NONCLIENT); }
    ok=bitmap_save_png(bitmap,path);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);ReleaseDC(app.main,dc);return ok;
}
static int preview_dialog(Dialog *dialog,const wchar_t *path) {
    HWND hwnd,child;RECT r;HDC dc,buffer;HBITMAP bitmap;HGDIOBJ old;int ok;
    hwnd=CreateWindowExW(WS_EX_DLGMODALFRAME,L"lengto.dialog",L"",WS_POPUP|WS_CAPTION|WS_SYSMENU,0,0,476,dialog_height(dialog->kind),app.main,NULL,app.instance,dialog);if(!hwnd) return 0;
    GetClientRect(hwnd,&r);dc=GetDC(hwnd);buffer=CreateCompatibleDC(dc);bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);old=SelectObject(buffer,bitmap);FillRect(buffer,&r,app.background);
    for(child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        RECT cr;POINT origin;int saved;if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)) continue;GetWindowRect(child,&cr);origin.x=cr.left;origin.y=cr.top;ScreenToClient(hwnd,&origin);
        saved=SaveDC(buffer);SetViewportOrgEx(buffer,origin.x,origin.y,NULL);IntersectClipRect(buffer,0,0,cr.right-cr.left,cr.bottom-cr.top);
        SendMessageW(child,WM_PRINT,(WPARAM)buffer,PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);RestoreDC(buffer,saved);
    }
    ok=bitmap_save_png(bitmap,path);SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);ReleaseDC(hwnd,dc);
    /* Keep it available briefly for the caller to exercise its controls. */
    return ok;
}
#define UI_CHECK(x) do { if(!(x)) { fwprintf(stderr,L"UI check failed at line %d\n",__LINE__);return 0; } } while(0)
static int on_pixel(Point p) { return fabs(p.x-floor(p.x)-0.5)<1e-9 && fabs(p.y-floor(p.y)-0.5)<1e-9; }
static int smoke_preview(const wchar_t *source,const wchar_t *dest) {
    RECT r;HDC dc,buffer;HBITMAP bitmap;HGDIOBJ old;int c,n,ok;double ox,oy;wchar_t path[P2L_PATH];
    UI_CHECK(wcslen(dest)<P2L_PATH-32);swprintf(path,P2L_PATH,L"%ls.empty.png",dest);UI_CHECK(preview_window(path));
    UI_CHECK(open_path(source));app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;
    set_mode(MODE_CALIBRATE);memset(&app.pending_cal,0,sizeof(app.pending_cal));wcscpy(app.pending_cal.name,L"Calibration");app.pending_cal.known=5;app.pending_cal.unit=UNIT_M;
    test_click((Point){100.1,130.1});test_click((Point){600.1,130.1});
    UI_CHECK(app.project->nc==1 && app.mode==MODE_IDLE && on_pixel(app.project->cal[0].a) && on_pixel(app.project->cal[0].b));c=app.active;
    UI_CHECK(!add_cal(app.project,0,L"Second",(Point){0,0},(Point){100,0},1,UNIT_M));
    measure_command();test_click((Point){100.1,420.1});test_click((Point){600.1,420.1});
    UI_CHECK(app.mode==MODE_MEASURE && !app.drawing && app.project->nm==1 && fabs(measured_metres(app.project,&app.project->meas[0])-5)<1e-9);
    test_click((Point){100.1,180.1});test_click((Point){100.1,420.1});UI_CHECK(app.project->nm==2 && app.mode==MODE_MEASURE);
    test_click((Point){300.1,300.1});SendMessageW(app.canvas,WM_KEYDOWN,VK_ESCAPE,0);UI_CHECK(app.project->nm==2 && app.mode==MODE_IDLE && !app.drawing);
    n=app.project->nm;measure_command();test_click((Point){200.2,220.2});
    ox=app.view.ox;oy=app.view.oy;SendMessageW(app.canvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(300,300));SendMessageW(app.canvas,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(345,328));SendMessageW(app.canvas,WM_LBUTTONUP,0,MAKELPARAM(345,328));
    UI_CHECK(fabs(app.view.ox-ox-45)<1e-9 && fabs(app.view.oy-oy-28)<1e-9 && app.project->nm==n && app.drawing);
    test_click((Point){400.2,220.2});UI_CHECK(app.project->nm==n+1 && app.mode==MODE_MEASURE && on_pixel(app.project->meas[n].a) && on_pixel(app.project->meas[n].b));
    SendMessageW(app.canvas,WM_KEYDOWN,VK_ESCAPE,0);test_click((Point){300.5,220.5});UI_CHECK(app.selected==n);SendMessageW(app.canvas,WM_KEYDOWN,VK_DELETE,0);UI_CHECK(app.project->nm==n);
    app.view.sx=app.view.sy=8;app.view.ox=app.view.oy=0;measure_command();test_click((Point){10.1,10.1});test_click((Point){10.8,10.8});UI_CHECK(app.project->nm==n && app.mode==MODE_MEASURE && !app.drawing);
    test_click((Point){10.2,10.2});test_click((Point){12.8,11.7});UI_CHECK(app.project->nm==n+1 && app.project->meas[n].a.x==10.5 && app.project->meas[n].b.x==12.5 && app.project->meas[n].b.y==11.5);
    remove_meas(app.project,n);SendMessageW(app.canvas,WM_KEYDOWN,VK_ESCAPE,0);app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;
    { Dialog d={0};Calibration original=*find_cal(app.project,c);double first=measured_metres(app.project,&app.project->meas[0]);
      d.kind=D_CALIBRATE;d.cal_id=c;swprintf(path,P2L_PATH,L"%ls.calibrate.png",dest);UI_CHECK(preview_dialog(&d,path));
      UI_CHECK(!d.select && !GetDlgItem(d.window,IDCANCEL) && read_unit(d.unit_combo)==UNIT_M);
      SetWindowTextW(d.value,L"7,5");select_unit(d.unit_combo,UNIT_CM);SendMessageW(d.window,WM_COMMAND,F_REDRAW,0);DestroyWindow(d.window);
      UI_CHECK(d.accepted && d.redraw && d.cal.known==7.5 && d.cal.unit==UNIT_CM);
      set_mode(MODE_CALIBRATE);app.pending_cal=d.cal;app.replace_cal=c;test_click((Point){100.1,140.1});cancel_action();UI_CHECK(measured_metres(app.project,&app.project->meas[0])==first);
      set_mode(MODE_CALIBRATE);app.pending_cal=d.cal;app.replace_cal=c;test_click((Point){100.1,140.1});test_click((Point){350.1,140.1});
      UI_CHECK(app.project->nc==1 && fabs(measured_metres(app.project,&app.project->meas[0])-0.15)<1e-9 && fabs(measured_metres(app.project,&app.project->meas[1])-0.072)<1e-9);
      *find_cal(app.project,c)=original;
      memset(&d,0,sizeof(d));d.kind=D_SETTINGS;d.precision=-3;d.unit=UNIT_M;swprintf(path,P2L_PATH,L"%ls.settings.png",dest);UI_CHECK(preview_dialog(&d,path));
      UI_CHECK(!GetDlgItem(d.window,IDCANCEL) && SendMessageW(d.select,CB_GETCOUNT,0,0)==27 && SendMessageW(d.unit_combo,CB_GETCOUNT,0,0)==UNIT_COUNT);
      select_unit(d.unit_combo,UNIT_LY);SendMessageW(d.select,CB_SETCURSEL,0,0);SendMessageW(d.window,WM_COMMAND,IDOK,0);DestroyWindow(d.window);UI_CHECK(d.accepted && d.unit==UNIT_LY && d.precision==-10);
      memset(&d,0,sizeof(d));d.kind=D_EXPORT;d.format=EXPORT_PDF;swprintf(path,P2L_PATH,L"%ls.export.png",dest);UI_CHECK(preview_dialog(&d,path));UI_CHECK(!GetDlgItem(d.window,IDCANCEL));
      if(app.document.pages>1) { UI_CHECK(IsWindowEnabled(d.scope_combo));SendMessageW(d.scope_combo,CB_SETCURSEL,1,0); }
      SendMessageW(d.select,CB_SETCURSEL,EXPORT_PNG,0);SendMessageW(d.window,WM_COMMAND,MAKEWPARAM(F_SELECT,CBN_SELCHANGE),0);
      UI_CHECK(!IsWindowEnabled(d.scope_combo) && SendMessageW(d.scope_combo,CB_GETCURSEL,0,0)==0);SendMessageW(d.window,WM_CLOSE,0,0);DestroyWindow(d.window);UI_CHECK(!d.accepted && d.done);
      memset(&d,0,sizeof(d));d.kind=D_ABOUT;swprintf(path,P2L_PATH,L"%ls.about.png",dest);UI_CHECK(preview_dialog(&d,path));
      UI_CHECK(GetWindowTextLengthW(d.name_edit)>100000);SendMessageW(d.window,WM_COMMAND,IDOK,0);DestroyWindow(d.window);UI_CHECK(d.accepted);
      memset(&d,0,sizeof(d));d.kind=D_GUIDE;swprintf(path,P2L_PATH,L"%ls.guide.png",dest);UI_CHECK(preview_dialog(&d,path));
      UI_CHECK(GetDlgItem(d.window,CMD_GITHUB)!=NULL && GetWindowTextLengthW(d.name_edit)>10000);
      SendMessageW(d.window,WM_COMMAND,CMD_GITHUB,0);UI_CHECK(!d.done);SendMessageW(d.window,WM_COMMAND,IDOK,0);DestroyWindow(d.window);
    }
    if(app.document.pages>1) {
        PageRects p=page_rects();int y=(p.next.top+p.next.bottom)/2;
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM((p.next.left+p.next.right)/2,y));UI_CHECK(app.document.page==1 && !app.editing_page);
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM((p.next.left+p.next.right)/2,y));UI_CHECK(app.document.page==1);
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM((p.previous.left+p.previous.right)/2,y));UI_CHECK(app.document.page==0 && !app.editing_page);
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM((p.previous.left+p.previous.right)/2,y));UI_CHECK(app.document.page==0);
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM(p.next.left+5,p.next.top-5));UI_CHECK(app.document.page==0 && !app.editing_page);
        SendMessageW(app.main,WM_LBUTTONUP,0,MAKELPARAM(p.number.left+5,y));UI_CHECK(app.editing_page);
        swprintf(path,P2L_PATH,L"%ls.page-entry.png",dest);UI_CHECK(preview_window(path));
        SetWindowTextW(app.page_edit,L"2");SendMessageW(app.page_edit,WM_KEYDOWN,VK_RETURN,0);UI_CHECK(app.document.page==1 && !app.editing_page);
        begin_page_entry();SetWindowTextW(app.page_edit,L"1");SendMessageW(app.page_edit,WM_KEYDOWN,VK_ESCAPE,0);UI_CHECK(app.document.page==1 && !app.editing_page);
        begin_page_entry();SetWindowTextW(app.page_edit,L"999");SendMessageW(app.page_edit,WM_KEYDOWN,VK_RETURN,0);UI_CHECK(app.document.page==app.document.pages-1);
        begin_page_entry();SetWindowTextW(app.page_edit,L"1");SendMessageW(app.page_edit,WM_KILLFOCUS,(WPARAM)app.canvas,0);UI_CHECK(app.document.page==0 && !app.editing_page);
        measure_command();change_page(1);UI_CHECK(app.mode==MODE_MEASURE && app.active==c && app.project->nc==1);
        app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;test_click((Point){10.1,10.1});test_click((Point){110.1,10.1});UI_CHECK(app.project->nm==n+1 && fabs(measured_metres(app.project,&app.project->meas[n])-1)<1e-9);
        remove_meas(app.project,n);change_page(0);UI_CHECK(app.mode==MODE_MEASURE);SendMessageW(app.canvas,WM_KEYDOWN,VK_ESCAPE,0);
    }
    { POINT cursor={400,330},screen=cursor;Point before,after;fit_image();before=image_point(cursor.x,cursor.y,0);ClientToScreen(app.canvas,&screen);
      SendMessageW(app.canvas,WM_MOUSEWHEEL,MAKEWPARAM(0,12000),MAKELPARAM(screen.x,screen.y));after=image_point(cursor.x,cursor.y,0);
      UI_CHECK(point_distance(before,after)<1e-9 && app.view.sx>=max(app.view.width,app.view.height));
      /* Keep a known white source pixel in view and check the actual canvas. */
      app.view.ox=app.view.width/2.0-700.5*app.view.sx;app.view.oy=app.view.height/2.0-500.5*app.view.sy;
      GetClientRect(app.canvas,&r);dc=GetDC(app.canvas);buffer=CreateCompatibleDC(dc);bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);old=SelectObject(buffer,bitmap);draw_canvas(buffer,r);
      UI_CHECK(GetPixel(buffer,app.view.width/2,app.view.height/2)==RGB(255,255,255));swprintf(path,P2L_PATH,L"%ls.zoom.png",dest);UI_CHECK(bitmap_save_png(bitmap,path));
      SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);ReleaseDC(app.canvas,dc);fit_image();refresh();
    }
    UI_CHECK(GetMenuItemCount(app.menu)==6 && GetMenuItemCount(GetSubMenu(app.menu,1))==4);
    swprintf(path,P2L_PATH,L"%ls.export.pdf",dest);wcscpy(app.output_path,path);app.output_format=EXPORT_PDF;app.output_all=1;app.dirty=1;
    measure_command();UI_CHECK(save_output(0) && !app.dirty && app.mode==MODE_MEASURE && app.document.page==0 && app.project->nm==n);
    app.dirty=1;UI_CHECK(save_output(0) && !app.dirty);SendMessageW(app.canvas,WM_KEYDOWN,VK_ESCAPE,0);
    ok=preview_window(dest);
    { int pages=app.document.pages;close_document();UI_CHECK(open_path(path) && app.document.pages==pages && app.project->nc==0 && app.project->nm==0); }
    return ok;
}

static int wait_script_test(void) {
    ULONGLONG start=GetTickCount64();MSG msg;int timeout=0;
    while(app.runner) {
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg);DispatchMessageW(&msg); }
        script_tick();if(!app.runner) break;
        if(GetTickCount64()-start>30000 && !timeout) { timeout=1;runner_cancel(app.runner); }
        MsgWaitForMultipleObjects(0,NULL,FALSE,10,QS_ALLINPUT);
    }
    return !timeout && !app.script_exit;
}
static int smoke_script(const wchar_t *source,const wchar_t *dest,const wchar_t *python) {
    wchar_t path[P2L_PATH];Dialog d={0};FILE *f;
    UI_CHECK(wcslen(dest)<P2L_PATH-48 && wcslen(python)<P2L_PATH);wcscpy(app.python,python);
    UI_CHECK(!app.document.bitmap && !(GetMenuState(app.menu,CMD_RECORD,MF_BYCOMMAND)&MF_GRAYED));
    d.kind=D_RECORD;swprintf(path,P2L_PATH,L"%ls.dialog.png",dest);UI_CHECK(preview_dialog(&d,path));
    UI_CHECK(SendMessageW(d.select,CB_GETCOUNT,0,0)==2 && !GetDlgItem(d.window,IDCANCEL));
    SendMessageW(d.window,WM_COMMAND,IDOK,0);DestroyWindow(d.window);UI_CHECK(d.accepted && d.scope);
    UI_CHECK(recorder_start(&app.recorder,&app.document,app.project,1));refresh();
    swprintf(path,P2L_PATH,L"%ls.missing-folder\\macro.py",dest);UI_CHECK(!recorder_save(&app.recorder,path) && app.recorder.active && app.recorder.body.len>0);
    UI_CHECK((GetMenuState(app.menu,CMD_RECORD,MF_BYCOMMAND)&MF_GRAYED) && !(GetMenuState(app.menu,CMD_STOP_RECORD,MF_BYCOMMAND)&MF_GRAYED));
    UI_CHECK(open_path(source));app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;
    set_mode(MODE_CALIBRATE);memset(&app.pending_cal,0,sizeof(app.pending_cal));wcscpy(app.pending_cal.name,L"Calibration");app.pending_cal.known=5;app.pending_cal.unit=UNIT_M;
    test_click((Point){100,130});test_click((Point){600,130});UI_CHECK(app.project->nc==1);
    measure_command();test_click((Point){100,420});test_click((Point){600,420});test_click((Point){100,180});test_click((Point){100,420});
    UI_CHECK(app.project->nm==2);cancel_action();app.selected=0;SendMessageW(app.main,WM_KEYDOWN,VK_DELETE,0);UI_CHECK(app.project->nm==1);
    wcscpy(app.project->meas[0].name,L"Dimension é Ω");recorder_measurement(&app.recorder,&app.document,&app.project->meas[0],1);
    UI_CHECK(app.document.pages==2);change_page(1);app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;
    measure_command();test_click((Point){10,10});test_click((Point){110,10});cancel_action();change_page(0);
    UI_CHECK(app.project->nm==2 && !app.recorder.body.failed && !*app.recorder.error);
    swprintf(path,P2L_PATH,L"%ls.pdf",dest);wcscpy(app.output_path,path);app.output_format=EXPORT_PDF;app.output_all=1;UI_CHECK(save_output(0));
    UI_CHECK(recorder_save(&app.recorder,dest) && !app.recorder.active);close_document();
    UI_CHECK(start_script(dest));UI_CHECK(wait_script_test());UI_CHECK(app.project->nc==1 && app.project->nm==2 && app.document.page==0 && !app.dirty);
    UI_CHECK(fabs(measured_metres(app.project,&app.project->meas[0])-2.4)<1e-9 && !wcscmp(app.project->meas[0].name,L"Dimension é Ω"));
    UI_CHECK(fabs(measured_metres(app.project,&app.project->meas[1])-1)<1e-9);
    change_page(1);UI_CHECK(recorder_start(&app.recorder,&app.document,app.project,0));
    swprintf(path,P2L_PATH,L"%ls.snapshot.py",dest);UI_CHECK(recorder_save(&app.recorder,path) && app.document.page==1);
    app.dirty=0;close_document();UI_CHECK(start_script(path));UI_CHECK(wait_script_test());
    UI_CHECK(app.document.page==1 && app.project->nm==2 && app.project->meas[0].page==0 && app.project->meas[1].page==1 && app.project->cal[0].page==0);
    app.dirty=0;close_document();UI_CHECK(recorder_start(&app.recorder,&app.document,app.project,1));swprintf(path,P2L_PATH,L"%ls.empty.py",dest);UI_CHECK(recorder_save(&app.recorder,path));
    UI_CHECK(start_script(path));UI_CHECK(wait_script_test());UI_CHECK(!app.document.bitmap);
    /* Exercise paste adoption and recording without changing the user's clipboard. */
    { Document *image=calloc(1,sizeof(*image));wchar_t error[512];UI_CHECK(image!=NULL);
      UI_CHECK(recorder_start(&app.recorder,&app.document,app.project,1));UI_CHECK(document_open_image(image,source,error,512));accept_document(image);free(image);
      UI_CHECK(app.document.clipboard && app.dirty && !*app.output_path && app.document.pages==1);
      app.view.sx=app.view.sy=1;app.view.ox=app.view.oy=0;set_mode(MODE_CALIBRATE);memset(&app.pending_cal,0,sizeof(app.pending_cal));wcscpy(app.pending_cal.name,L"Calibration");app.pending_cal.known=5;app.pending_cal.unit=UNIT_M;
      test_click((Point){100,130});test_click((Point){600,130});measure_command();test_click((Point){100,180});test_click((Point){100,420});cancel_action();
      swprintf(path,P2L_PATH,L"%ls.clipboard.pdf",dest);wcscpy(app.output_path,path);app.output_format=EXPORT_PDF;app.output_all=0;UI_CHECK(save_output(0));
      swprintf(path,P2L_PATH,L"%ls.clipboard.py",dest);UI_CHECK(recorder_save(&app.recorder,path));close_document();UI_CHECK(start_script(path));UI_CHECK(wait_script_test());
      UI_CHECK(app.document.clipboard && app.project->nm==1 && fabs(measured_metres(app.project,&app.project->meas[0])-2.4)<1e-9);
      UI_CHECK(recorder_start(&app.recorder,&app.document,app.project,1));swprintf(path,P2L_PATH,L"%ls.clipboard-snapshot.py",dest);UI_CHECK(recorder_save(&app.recorder,path));app.dirty=0;close_document();UI_CHECK(start_script(path));UI_CHECK(wait_script_test());UI_CHECK(app.document.clipboard && app.project->nm==1);app.dirty=0;close_document();
    }
    swprintf(path,P2L_PATH,L"%ls.cancel.py",dest);f=_wfopen(path,L"wb");UI_CHECK(f!=NULL);fputs("import time\ntime.sleep(60)\n",f);fclose(f);
    UI_CHECK(start_script(path));SendMessageW(app.main,WM_KEYDOWN,VK_ESCAPE,0);wait_script_test();UI_CHECK(!app.runner && app.script_exit!=0);
    swprintf(path,P2L_PATH,L"%ls.error.py",dest);f=_wfopen(path,L"wb");UI_CHECK(f!=NULL);fputs("import sys\nsys.stderr.write('test log\\n' * 5000)\nraise RuntimeError('intentional test error')\n",f);fclose(f);
    UI_CHECK(start_script(path));UI_CHECK(!wait_script_test() && !app.runner && app.script_exit!=0);
    return 1;
}
