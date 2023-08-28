#include <Windows.h>
#include <Shlwapi.h>
#include "resource.h"
#include "SlxComTools.h"

extern HINSTANCE g_hinstDll; // SlxCom.cpp

static INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static BOOL s_bInLinkArea = FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        WCHAR szBaseInfo[1000];
        WCHAR szVersion[100] = L"";

        GetVersionString(g_hinstDll, szVersion, RTL_NUMBER_OF(szVersion));
        wnsprintfW(
            szBaseInfo,
            RTL_NUMBER_OF(szBaseInfo),
#ifdef _WIN64
            L"SlxCom (x64) Version: %s",
#else
            L"SlxCom (x86) Version: %s",
#endif
            szVersion
            );

        SetDlgItemTextW(hwndDlg, IDC_BASEINFO, szBaseInfo);
        SetDlgItemTextW(hwndDlg, IDC_COPYRIGHT,
            L"Copyright Shilyx Studio"
#if _MSC_VER > 1200
            L" \251"
#else
            L" (C)"
#endif
            L" 2013");
        SetDlgItemTextW(hwndDlg, IDC_LINK, L"查看功能列表和版本更新日志...");
        return FALSE;
    }

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_LBUTTONDOWN:
        if (s_bInLinkArea)
        {
            WCHAR szHtmFile[MAX_PATH];

            GetTempPathW(RTL_NUMBER_OF(szHtmFile), szHtmFile);
            PathAppendW(szHtmFile, L"\\AboutSlxCom.htm");

            if (SaveResourceToFile(L"RT_FILE", MAKEINTRESOURCEW(IDR_ABOUT), szHtmFile))
            {
                ShellExecuteW(hwndDlg, L"open", szHtmFile, NULL, NULL, SW_SHOW);
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC hDc = (HDC)wParam;
        HWND hControl = (HWND)lParam;

        if (GetDlgCtrlID(hControl) == IDC_LINK)
        {
            if (s_bInLinkArea)
            {
                SetTextColor(hDc, RGB(245, 0, 0));
            }
            else
            {
                SetTextColor(hDc, RGB(0, 0, 245));
            }

            SetBkMode(hDc, TRANSPARENT);
            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }

        break;
    }

    case WM_MOUSEMOVE:
    {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        RECT rect;

        ClientToScreen(hwndDlg, &pt);
        GetWindowRect(GetDlgItem(hwndDlg, IDC_LINK), &rect);

        BOOL bInLinkArea = PtInRect(&rect, pt);

        if (!!(!!bInLinkArea ^ !!s_bInLinkArea))
        {
            s_bInLinkArea = bInLinkArea;
            InvalidateRect(GetDlgItem(hwndDlg, IDC_LINK), NULL, TRUE);
        }

        if (s_bInLinkArea)
        {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
        }

        break;
    }

    default:
        break;
    }

    return FALSE;
}

void SlxComAbout(HWND hParentWindow)
{
    DialogBoxW(g_hinstDll, MAKEINTRESOURCEW(IDD_ABOUT), hParentWindow, DlgProc);
}