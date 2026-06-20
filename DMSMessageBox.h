/*

DMSMessageBox.h

Oolite
Copyright (C) 2004-2026 Giles C Williams and contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.



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

