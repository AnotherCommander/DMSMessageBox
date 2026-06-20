/*

DMSMessageBox.c


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

*/

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

#include "DMSMessageBox.h"

#define BTN_OK     IDOK
#define BTN_YES    IDYES
#define BTN_NO     IDNO
#define BTN_CANCEL IDCANCEL

#define DLG_MARGIN_X     48
#define DLG_MARGIN_Y     36
#define ICON_SIZE        32
#define ICON_PADDING     12
#define BUTTON_W         100
#define BUTTON_H         28
#define BUTTON_GAP       10
#define BUTTON_MARGIN_X  32
#define BUTTON_MARGIN_Y  48
#define MIN_DLG_WIDTH    360
#define MAX_TEXT_WIDTH   440
#define CMD_AREA_HEIGHT  40
#define MIN_DLG_HEIGHT   180
#define TEXT_START_Y_POS 28
#define ICON_X_POS       20
#define DEFAULT_SYS_DPI  96

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static COLORREF gClrBg;
static COLORREF gClrCmdBg;
static COLORREF gClrText;
static COLORREF gClrBtn;
static COLORREF gClrBtnHot;
static COLORREF gClrBtnHi;

typedef struct {
    LPCWSTR text;
    LPCWSTR title;
    UINT    flags;
    int     result;
    HICON   icon;
    int     buttonY;
    int     textBottom;
    int     defaultBtn;
    int     focusedBtn;
    BOOL    goodToClose;
} DMSMBCTX;


static HFONT  gFont;
static HBRUSH gBgBrush;
static HBRUSH gCmdBrush;
static HWND gHotButton = NULL;
static UINT gDpi = DEFAULT_SYS_DPI;


static wchar_t* Utf8ToWide(const char *str)
{
    if (!str)  return NULL;

    // get the required out buffer size in characters
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

    // the caller is responsible for freeing this
    wchar_t *out = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!out)  return NULL;

    MultiByteToWideChar(CP_UTF8, 0, str, -1, out, len);
    return out;
}


static BOOL IsActive(HWND hwnd)
{
    HWND fg = GetForegroundWindow();
    return (fg && GetAncestor(fg, GA_ROOT) == hwnd);
}


static int DPI(int x)
{
    return MulDiv(x, gDpi, DEFAULT_SYS_DPI);
}


static int GetMaxDialogHeight(HWND hwnd)
{
    MONITORINFO mInfo = { sizeof(mInfo) };
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hMon, &mInfo);

    return mInfo.rcWork.bottom - mInfo.rcWork.top;
}


static int MeasureTextHeight(HWND hwnd, HFONT hFont, LPCWSTR text, int maxWidth)
{
    HDC hdc = GetDC(hwnd);
    HGDIOBJ old = SelectObject(hdc, hFont);

    RECT rc = { 0, 0, maxWidth, 0 };
    DrawTextW(hdc, text, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_EXPANDTABS);

    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return rc.bottom;
}


static HFONT CreateFittedFont(HWND hwnd, DMSMBCTX *ctx)
{
    NONCLIENTMETRICS ncm = { sizeof(ncm) };
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    int maxHeight = GetMaxDialogHeight(hwnd) - DPI(CMD_AREA_HEIGHT) - DPI(DLG_MARGIN_Y) * 2;

    // start from nominal size
    int basePt = 9;
    int minPt  = 7;

    // iterate through font sizes until we find one that fits our maxHeight 
    for (int pt = basePt; pt >= minPt; --pt)
    {
        if (gFont) DeleteObject(gFont);

        ncm.lfMessageFont.lfHeight = -MulDiv(pt, gDpi, 72);
        // reduce font size slightly to match Windows MessageBox if needed
		if(gDpi > DEFAULT_SYS_DPI)  ncm.lfMessageFont.lfHeight += 1; // yes, + not -
        gFont = CreateFontIndirect(&ncm.lfMessageFont);

        int textH = MeasureTextHeight(hwnd, gFont, ctx->text, DPI(MAX_TEXT_WIDTH));

        if (textH <= maxHeight)
        {
            return gFont;
        }
    }

    return gFont; // smallest possible
}


static BOOL CALLBACK SetChildFontProc(HWND hwnd, LPARAM lParam)
{
    HFONT hFont = (HFONT)lParam;
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    return TRUE;
}


static void SetDefaultButton(HWND hwnd, DMSMBCTX *ctx, int newDefault)
{
    if (ctx->defaultBtn == newDefault)  return;

    int oldDefault = ctx->defaultBtn;
    ctx->defaultBtn = newDefault;

    // force repaint of all buttons
    if (oldDefault)  InvalidateRect(GetDlgItem(hwnd, oldDefault), NULL, TRUE);
    InvalidateRect(GetDlgItem(hwnd, newDefault), NULL, TRUE);
}


static void NavigateButtons(HWND hwndDlg, DMSMBCTX *ctx, int direction)
{
    int buttons[2];
    int count = 0;

    if (ctx->flags & MB_YESNO)
    {
        buttons[0] = BTN_YES;
        buttons[1] = BTN_NO;
        count = 2;
    }
    else if (ctx->flags & MB_OKCANCEL)
    {
        buttons[0] = BTN_OK;
        buttons[1] = BTN_CANCEL;
        count = 2;
    }
    else  return;

    HWND hFocus = GetFocus();
    int idx = 0;

    if (hFocus == GetDlgItem(hwndDlg, buttons[1]))
        idx = 1;

    idx = (idx + direction + count) % count;

    HWND hNew = GetDlgItem(hwndDlg, buttons[idx]);
    SetDefaultButton(hNew, ctx, buttons[idx]);
    SetFocus(hNew);
}


#if 1 // oolite implementation
static BOOL IsDarkModeEnabled(void)
{
    char buffer[4];
    DWORD bufferSize = sizeof(buffer);
    
    // reading a REG_DWORD value from the Registry
    HRESULT resultRegGetValue = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
    								L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, buffer, &bufferSize);
    if (resultRegGetValue != ERROR_SUCCESS)
    {
        return FALSE;
    }
    
    // get our 4 obtained bytes into integer little endian format
    int i = (int)(buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0]);
    
    // dark mode is 0, light mode is 1
    return i == 0;
}
#else // alternative implementation, slightly easier to read but more API calls
static BOOL IsDarkModeEnabled(void)
{
    BOOL dark = FALSE;
    HKEY hKey;

    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_READ,
            &hKey) == ERROR_SUCCESS)
    {
        DWORD value = 1;
        DWORD size = sizeof(value);

        RegQueryValueExW(
            hKey,
            L"AppsUseLightTheme",
            NULL,
            NULL,
            (LPBYTE)&value,
            &size
        );

        dark = (value == 0);
        RegCloseKey(hKey);
    }

    return dark;
}
#endif


static void EnableDarkOrLightTitleBar(HWND hwnd)
{
    BOOL dark = IsDarkModeEnabled();
    const DWORD DWMWA_DARK_20H1 = 20;
    const DWORD DWMWA_DARK_OLD  = 19;

    // apply dark title bar in pre- and post- 20H1 versions of Windows
    // we call with both the 19 and 20 parameters, the one our OS uses
    // will work, the other will not affect anything
    DwmSetWindowAttribute(hwnd, DWMWA_DARK_20H1, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_DARK_OLD,  &dark, sizeof(dark));
}


static void ApplyTheme(HWND hwnd)
{
    if (IsDarkModeEnabled())
    {
        gClrBg     = RGB(40, 40, 40);
        gClrCmdBg  = RGB(48, 48, 48);
        gClrText   = RGB(235, 235, 235);
        gClrBtn    = RGB(60, 60, 60);
        gClrBtnHot = RGB(80, 80, 80);
        gClrBtnHi  = RGB(70, 70, 70);
    }
    else
    {
        gClrBg     = RGB(250, 250, 250);
        gClrCmdBg  = RGB(240, 240, 240);
        gClrText   = RGB(0, 0, 0);
        gClrBtn    = RGB(230, 230, 230);
        gClrBtnHot = RGB(210, 210, 210);
        gClrBtnHi  = RGB(200, 200, 200);
    }

    if (gBgBrush)  DeleteObject(gBgBrush);
    if (gCmdBrush) DeleteObject(gCmdBrush);

    gBgBrush  = CreateSolidBrush(gClrBg);
    gCmdBrush = CreateSolidBrush(gClrCmdBg);

    EnableDarkOrLightTitleBar(hwnd);

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}


static void AutoSizeDialog(HWND hwnd, DMSMBCTX *ctx)
{
    HDC hdc = GetDC(hwnd);
    SelectObject(hdc, gFont);

    RECT textRc = { 0, 0, DPI(MAX_TEXT_WIDTH), 0 };
    DrawTextW(hdc, ctx->text, -1, &textRc, DT_CALCRECT | DT_WORDBREAK | DT_EXPANDTABS);

    ReleaseDC(hwnd, hdc);

    int textWidth  = textRc.right;
    int textHeight = textRc.bottom + DPI(16);

    // calculate window dimensions
    int contentWidth = textWidth + DPI(DLG_MARGIN_X) * 2;
    if (ctx->icon)
    {
        contentWidth += DPI(ICON_SIZE) + DPI(ICON_PADDING);
    }

    int buttonCount = (ctx->flags & (MB_YESNO | MB_OKCANCEL)) ? 2 : 1;
    int buttonsWidth = (buttonCount * DPI(BUTTON_W)) + ((buttonCount - 1) * DPI(BUTTON_GAP));

    if (contentWidth < buttonsWidth + DPI(DLG_MARGIN_X) * 2)  contentWidth = buttonsWidth + DPI(DLG_MARGIN_X) * 2;

    if (contentWidth < DPI(MIN_DLG_WIDTH))  contentWidth = DPI(MIN_DLG_WIDTH);

    int contentHeight = DPI(DLG_MARGIN_Y) * 2 + max(textHeight, ctx->icon ? DPI(ICON_SIZE) : 0);
    int totalHeight = contentHeight + DPI(CMD_AREA_HEIGHT);
    if (totalHeight < DPI(MIN_DLG_HEIGHT))  totalHeight = DPI(MIN_DLG_HEIGHT);
	
    // make sure we fit inside the screen height
    int maxScreenY = min(GetSystemMetrics(SM_CYSCREEN), GetMaxDialogHeight(hwnd));
    if (totalHeight > maxScreenY)  totalHeight = maxScreenY;

    SetWindowPos(
        hwnd,
        NULL,
        0, 0,
        contentWidth,
		totalHeight,
        SWP_NOMOVE | SWP_NOZORDER
    );
	
    // reposition buttons 
    int cmdTop = totalHeight - DPI(CMD_AREA_HEIGHT);
    ctx->buttonY = cmdTop + (DPI(CMD_AREA_HEIGHT) - DPI(BUTTON_H)) / 2;
    ctx->textBottom = cmdTop - DPI(BUTTON_MARGIN_Y);
	
    int rightEdge = contentWidth - DPI(BUTTON_MARGIN_X);

    if (ctx->flags & (MB_YESNO | MB_OKCANCEL))
    {
        MoveWindow(
            GetDlgItem(hwnd, (ctx->flags & MB_YESNO) ? BTN_NO : BTN_CANCEL),
            rightEdge - DPI(BUTTON_W),
            ctx->buttonY - DPI(BUTTON_H) - DPI(15),
            DPI(BUTTON_W),
            DPI(BUTTON_H),
            TRUE);

        MoveWindow(
            GetDlgItem(hwnd, (ctx->flags & MB_YESNO) ? BTN_YES : BTN_OK),
            rightEdge - DPI(BUTTON_W) * 2 - DPI(BUTTON_GAP),
            ctx->buttonY - DPI(BUTTON_H) - DPI(15),
            DPI(BUTTON_W),
            DPI(BUTTON_H),
            TRUE);
    }
    else
    {
        MoveWindow(
            GetDlgItem(hwnd, BTN_OK),
            rightEdge - DPI(BUTTON_W),
            ctx->buttonY - DPI(BUTTON_H) - DPI(15),
            DPI(BUTTON_W),
            DPI(BUTTON_H),
            TRUE);
    }
}


static void ApplyDpiScaledMetrics(HWND hwnd, DMSMBCTX *ctx, UINT dpi)
{
    // store new DPI
    gDpi = dpi;

    // recreate font for the new DPI
    if (gFont)
    {
        DeleteObject(gFont);
        gFont = NULL;
    }
    gFont = CreateFittedFont(hwnd, ctx);

    // apply new font to all child controls
    EnumChildWindows(hwnd, SetChildFontProc, (LPARAM)gFont);

    AutoSizeDialog(hwnd, ctx);

    // force full repaint
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}


static void EnableRoundedCorners(HWND hwnd)
{
    DWORD pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}


// center relative to parent or screen
static void CenterWindow(HWND hwnd, HWND parent)
{
    RECT rc, rp = {0};
    GetWindowRect(hwnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (parent)
    {
        GetWindowRect(parent, &rp);
    }
    else
    {
        rp.right  = GetSystemMetrics(SM_CXSCREEN);
        rp.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int x = rp.left + ((rp.right - rp.left) - w) / 2;
    int y = rp.top  + ((rp.bottom - rp.top) - h) / 2;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}


LRESULT CALLBACK ButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subClassId, DWORD_PTR ref)
{
    HWND hwndDlg = GetParent(hwnd);
    DMSMBCTX *ctx = (DMSMBCTX*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

    switch (msg)
    {
        case WM_MOUSEMOVE:
            if (gHotButton != hwnd)
            {
                if (gHotButton)
                {
                    InvalidateRect(gHotButton, NULL, TRUE);
                }

                gHotButton = hwnd;
                InvalidateRect(hwnd, NULL, TRUE);
        
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            break;

        // explicitly state that we want to handle all keyboard input
        // otherwise default Windows dialog mgmt mechanisms may interfere
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;

        case WM_KEYDOWN:
            switch (wParam)
            {
                case VK_TAB:
                    NavigateButtons(hwndDlg, ctx, (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : +1);
                    return 0;

                case VK_LEFT:
                case VK_UP:
                    NavigateButtons(hwndDlg, ctx, -1);
                    return 0;

                case VK_RIGHT:
                case VK_DOWN:
                    NavigateButtons(hwndDlg, ctx, +1);
                    return 0;

                case VK_RETURN:
                    // simulate button clicked event
                    SendMessage(hwnd, BM_CLICK, 0, 0);
                    return 0;
					
				case VK_ESCAPE:
                    if (ctx->flags & MB_YESNO)
                    {
                        ctx->result = IDNO;
                    }
                    else if (ctx->flags & MB_OKCANCEL)
                    {
                        ctx->result = IDCANCEL;
                    }
                    else
                    {
                        ctx->result = IDOK;
                    }
                    ctx->goodToClose = TRUE;
                    DestroyWindow(hwndDlg);
                    return 0;
            }
            break;

        case WM_LBUTTONDOWN:
        {
            int id = GetDlgCtrlID(hwnd);
            // move default button + focus immediately
            SetDefaultButton(hwndDlg, ctx, id);
            SetFocus(hwnd);
        
            break;
        }

        case WM_MOUSELEAVE:
            if (gHotButton == hwnd)
            {
                gHotButton = NULL;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}


LRESULT CALLBACK DMSMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DMSMBCTX *ctx = (DMSMBCTX*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_CREATE:
        { 
            ctx = ((LPCREATESTRUCT)lParam)->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);
        
            NONCLIENTMETRICS ncm = { sizeof(ncm) };
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);			
            // reduce font size slightly if scaling is higher than 100%
            gDpi = GetDpiForSystem();

            gFont = CreateFittedFont(hwnd, ctx);//CreateFontIndirect(&ncm.lfMessageFont);
            gBgBrush = CreateSolidBrush(gClrBg);
            gCmdBrush = CreateSolidBrush(gClrCmdBg);

            if (ctx->flags & MB_YESNO)
            {
                HWND hYes = CreateWindowW(L"BUTTON", L"Yes",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                    90, 90, 80, 28, hwnd, (HMENU)BTN_YES, NULL, NULL);
                SetWindowSubclass(hYes, ButtonSubclassProc, 0, 0);
            
                HWND hNo = CreateWindowW(L"BUTTON", L"No",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    190, 90, 80, 28, hwnd, (HMENU)BTN_NO, NULL, NULL);
                SetWindowSubclass(hNo, ButtonSubclassProc, 0, 0);

                ctx->defaultBtn = BTN_YES;
                SetFocus(GetDlgItem(hwnd, BTN_YES));
            }
            else if (ctx->flags & MB_OKCANCEL)
            {
                HWND hOK = CreateWindowW(L"BUTTON", L"OK",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                    90, 90, 80, 28, hwnd, (HMENU)BTN_OK, NULL, NULL);
                SetWindowSubclass(hOK, ButtonSubclassProc, 0, 0);
            
                HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    190, 90, 80, 28, hwnd, (HMENU)BTN_CANCEL, NULL, NULL);
                SetWindowSubclass(hCancel, ButtonSubclassProc, 0, 0);

                ctx->defaultBtn = BTN_OK;
                SetFocus(GetDlgItem(hwnd, BTN_OK));
            }
            else
            {
                HWND hOK = CreateWindowW(L"BUTTON", L"OK",
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                    140, 90, 80, 28, hwnd, (HMENU)BTN_OK, NULL, NULL);
                SetWindowSubclass(hOK, ButtonSubclassProc, 0, 0);

                ctx->defaultBtn = BTN_OK;
                SetFocus(GetDlgItem(hwnd, BTN_OK));
            }
        
            UINT icon = ctx->flags & MB_ICONMASK;
            switch (icon)
            {
                case MB_ICONERROR:        // MB_ICONHAND
                    ctx->icon = LoadIcon(NULL, IDI_ERROR);
                    break;
            
                case MB_ICONWARNING:      // MB_ICONEXCLAMATION
                    ctx->icon = LoadIcon(NULL, IDI_WARNING);
                    break;
            
                case MB_ICONINFORMATION:  // MB_ICONASTERISK
                    ctx->icon = LoadIcon(NULL, IDI_INFORMATION);
                    break;
            
                case MB_ICONQUESTION:
                    ctx->icon = LoadIcon(NULL, IDI_QUESTION);
                    break;
            
                default:
                    ctx->icon = NULL;
                    break;
            }

            gDpi = GetDpiForWindow(hwnd);

            SetWindowTextW(hwnd, ctx->title);
            ApplyTheme(hwnd);
            EnableRoundedCorners(hwnd);
            AutoSizeDialog(hwnd, ctx);
            CenterWindow(hwnd, GetParent(hwnd));
            return 0;
        }

        case WM_ACTIVATE:
        case WM_NCACTIVATE:
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
            return msg == WM_NCACTIVATE;

        case WM_DRAWITEM:
        {
            BOOL dark = IsDarkModeEnabled();

            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
            // if DMSMessageBox hasn't got the focus, we do not designate a
            // default button (matches Windows MessageBox behaviour)
            BOOL isDefault = (dis->CtlID == ctx->defaultBtn && IsActive(hwnd));

            if (dis->CtlType == ODT_BUTTON)
            {
				LPCWSTR text = NULL;
                switch (dis->CtlID)
                {
                    case BTN_OK:  text = L"OK";  break;
                    case BTN_YES: text = L"Yes"; break;
                    case BTN_NO:  text = L"No";  break;
					case BTN_CANCEL: text = L"Cancel"; break;
                    default:
                        return FALSE;
                }
				
                BOOL pressed = (dis->itemState & ODS_SELECTED) != 0;
                BOOL hot = (dis->hwndItem == gHotButton);
                
                COLORREF color =
                    pressed ? gClrBtnHi :
                    hot     ? gClrBtnHot :
                              gClrBtn;
                
                HBRUSH bg = CreateSolidBrush(color);
                FillRect(dis->hDC, &dis->rcItem, bg);

                // outer default-button border (like Windows MessageBox)
                if (isDefault)
                {
                    RECT r = dis->rcItem;
                    InflateRect(&r, -1, -1);
                
                    HPEN pen = CreatePen(
                        PS_SOLID,
                        DPI(1),
                        dark ? RGB(25, 85, 140) : RGB(90, 90, 90)
                    );

                    HGDIOBJ oldPen   = SelectObject(dis->hDC, pen);
                    HGDIOBJ oldBrush = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));

                    RoundRect(
                        dis->hDC,
                        r.left,
                        r.top,
                        r.right,
                        r.bottom,
                        DPI(4),   // corner ellipse width
                        DPI(4)    // corner ellipse height
                    );

                    SelectObject(dis->hDC, oldBrush);
                    SelectObject(dis->hDC, oldPen);
                    DeleteObject(pen);
                }

                DeleteObject(bg);
    
                SelectObject(dis->hDC, gFont);
                SetTextColor(dis->hDC, gClrText);
                SetBkMode(dis->hDC, TRANSPARENT);
    
                DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_EXPANDTABS);
    
                //if ((dis->itemState & ODS_FOCUS) && !(dis->itemState & ODS_SELECTED) && !isDefault)
                //{
                //    DrawFocusRect(dis->hDC, &dis->rcItem);
                //}

                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
        
            switch (id)
            {
                case BTN_OK:
                case BTN_YES:
                case BTN_NO:
                case BTN_CANCEL:
                    SetDefaultButton(hwnd, ctx, id);
                    ctx->result = id;
                    ctx->goodToClose = TRUE;
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, gClrText);
            SetBkColor(hdc, gClrBg);
            return (LRESULT)gBgBrush;
        }
    
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
        
            RECT rc;
            GetClientRect(hwnd, &rc);

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            // command area (bottom strip)
            RECT rcCmd = rcClient;
            FillRect(hdc, &rcCmd, gCmdBrush);
            
            // content area (everything above)
            RECT rcContent = rcClient;
            rcContent.bottom = ctx->textBottom;
            FillRect(hdc, &rcContent, gBgBrush);
        
            int textLeft = DPI(ICON_X_POS);
        
            if (ctx->icon)
            {
                DrawIconEx(
                    hdc,
                    DPI(ICON_X_POS), DPI(TEXT_START_Y_POS),
                    ctx->icon,
                    DPI(ICON_SIZE), DPI(ICON_SIZE),
                    0,
                    NULL,
                    DI_NORMAL
                );
                textLeft += DPI(40);
            }
        
            SelectObject(hdc, gFont);
            SetTextColor(hdc, gClrText);
            SetBkMode(hdc, TRANSPARENT);
            
            RECT textRc = {
                textLeft,
                DPI(TEXT_START_Y_POS),
                rc.right - DPI(24),
                ctx->textBottom
            };

            DrawTextW(hdc, ctx->text, -1, &textRc, DT_WORDBREAK | DT_EXPANDTABS | (DT_TABSTOP | 0x1F00));
        
            EndPaint(hwnd, &ps);
            return 0;
        }
		
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        {
            ApplyTheme(hwnd);
            return 0;
        }

        case WM_DPICHANGED:
        {
            // save focused button if any
            HWND hFocus = GetFocus();
            if (hFocus)
            {
                ctx->focusedBtn = GetDlgCtrlID(hFocus);
            }
            else
            {
                ctx->focusedBtn = 0;
            }
			
            UINT dpi = HIWORD(wParam);
            RECT* rc = (RECT*)lParam;
        
            SetWindowPos(
                hwnd,
                NULL,
                rc->left,
                rc->top,
                rc->right  - rc->left,
                rc->bottom - rc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        
            ApplyDpiScaledMetrics(hwnd, ctx, dpi);

            // rstore focus
            int btn = ctx->focusedBtn ? ctx->focusedBtn : ctx->defaultBtn;
            HWND hBtn = GetDlgItem(hwnd, btn);
            if (hBtn)  SetFocus(hBtn);

            return 0;
        }

        case WM_SETFOCUS:
        {
            HWND hBtn = GetDlgItem(hwnd, ctx->defaultBtn);
            if (hBtn)  SetFocus(hBtn);
            return 0;
        }

        case WM_CLOSE:
        {
            ctx->result = IDCANCEL;
            ctx->goodToClose = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY:
        {
            if(gFont)  {DeleteObject(gFont); gFont = NULL;}
            if(gBgBrush)  {DeleteObject(gBgBrush); gBgBrush = NULL;}
            if (gCmdBrush)  {DeleteObject(gCmdBrush); gCmdBrush = NULL;}
            gHotButton = NULL;
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}


int DMSMessageBoxW(HWND parent, LPCWSTR text, LPCWSTR title, UINT flags)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DMSMsgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"DMSMessageBox";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    DMSMBCTX ctx = { text, title, flags, 0, NULL };
    ctx.goodToClose = FALSE;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"DMSMessageBox",
        title,
        WS_CAPTION | WS_POPUP | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        360, 160,
        parent, NULL,
        GetModuleHandle(NULL),
        &ctx);

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!ctx.goodToClose && GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessage(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    return ctx.result;
}


// ANSI version of the dark mode sensitive messagebox API
int DMSMessageBoxA(HWND parent, const char *text, const char *title, UINT flags)
{
    wchar_t *wText  = Utf8ToWide(text);
    wchar_t *wTitle = Utf8ToWide(title);

    int result = DMSMessageBoxW(parent, wText, wTitle, flags);

    // Utf8ToWide allocates wide char strings, so free them now
    free(wText);
    free(wTitle);

    return result;
}

