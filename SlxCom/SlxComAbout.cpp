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
        TCHAR szBaseInfo[1000];
        TCHAR szVersion[100] = TEXT("");

        GetVersionString(g_hinstDll, szVersion, RTL_NUMBER_OF(szVersion));
        wnsprintf(
            szBaseInfo,
            RTL_NUMBER_OF(szBaseInfo),
#ifdef _WIN64
            TEXT("SlxCom (x64) Version: %s"),
#else
            TEXT("SlxCom (x86) Version: %s"),
#endif
            szVersion
            );

        SetDlgItemText(hwndDlg, IDC_BASEINFO, szBaseInfo);
        SetDlgItemText(hwndDlg, IDC_COPYRIGHT,
            TEXT("Copyright Shilyx Studio")
#if _MSC_VER > 1200
            TEXT(" \251")
#else
            TEXT(" (C)")
#endif
            TEXT(" 2013"));
        SetDlgItemText(hwndDlg, IDC_LINK, TEXT("查看功能列表和版本更新日志..."));
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
            TCHAR szHtmFile[MAX_PATH];

            GetTempPath(RTL_NUMBER_OF(szHtmFile), szHtmFile);
            PathAppend(szHtmFile, TEXT("\\AboutSlxCom.htm"));

            if (SaveResourceToFile(TEXT("RT_FILE"), MAKEINTRESOURCE(IDR_ABOUT), szHtmFile))
            {
                ShellExecute(hwndDlg, TEXT("open"), szHtmFile, NULL, NULL, SW_SHOW);
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
            return (BOOL)GetStockObject(NULL_BRUSH);
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
            SetCursor(LoadCursor(NULL, IDC_HAND));
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
    DialogBox(g_hinstDll, MAKEINTRESOURCE(IDD_ABOUT), hParentWindow, DlgProc);
}