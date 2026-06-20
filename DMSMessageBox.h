/*

DMSMessageBox.h


A Dark Mode - Sensitive implementation of MessageBox.
Supports MB_OK, MB_YESNO and MB_OKCANCEL message boxes
with either no or with inormation, warning, error
and questions icons. DMSMessageBox honors the currently
selected mode (light or dark) and can be updated in
real time when such mode changes. It is also DPI-aware.

The API contains both Unicode and ANSI variants of the
implementation which can be called either selectively
with the W or A suffix respectively or automatically by
simply calling DMSMessageBox with the typical parameters
used for the normal MessageBox Win32 API function.

*/

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <commctrl.h>

#ifdef UNICODE
#define DMSMessageBox DMSMessageBoxW
#else
#define DMSMessageBox DMSMessageBoxA
#endif

int DMSMessageBoxW(HWND parent, LPCWSTR text, LPCWSTR title, UINT flags);
int DMSMessageBoxA(HWND parent, const char* text, const char* title, UINT flags);

