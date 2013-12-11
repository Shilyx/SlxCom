#include "SlxComDirShortcut.h"
#include "resource.h"

extern HINSTANCE g_hinstDll; //SlxCom.cpp

static LPCTSTR g_lpMgrWindowText = TEXT("文件夹快捷通道 - SlxCom");
static LONG_PTR g_lUserData = 542823L;

static INT_PTR CALLBACK DirShotcutMgrDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, g_lUserData);
        SetWindowText(hwndDlg, g_lpMgrWindowText);
        return FALSE;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_COPYDATA:
        break;
    }

    return FALSE;
}

static HWND GetExistWindow()
{
    HWND hWnd = FindWindowEx(NULL, NULL, TEXT("#32770"), g_lpMgrWindowText);

    while (IsWindow(hWnd))
    {
        if (GetWindowLongPtr(hWnd, GWLP_USERDATA) == g_lUserData)
        {
            return hWnd;
        }

        hWnd = FindWindowEx(NULL, hWnd, TEXT("#32770"), g_lpMgrWindowText);
    }

    return NULL;
}

int ShowDirShotcutMgr(HWND hwndParent)
{
    HWND hOldWindow = GetExistWindow();

    if (IsWindow(hOldWindow))
    {
        if (IsIconic(hOldWindow))
        {
            ShowWindow(hOldWindow, SW_RESTORE);
        }

        SetForegroundWindow(hOldWindow);
    }
    else
    {
        DialogBox(g_hinstDll, MAKEINTRESOURCE(IDD_DIRSHORTCUTMGR), hwndParent, DirShotcutMgrDlgProc);
    }
    
    return 0;
}

void AskForAddDirShotcut(HWND hwndParent, LPCTSTR lpDirPath, LPCTSTR lpNickName)
{

}

void __stdcall T4(HWND hwndStub, HINSTANCE hAppInstance, LPTSTR lpszCmdLine, int nCmdShow)
{
    ShowDirShotcutMgr(hwndStub);
}