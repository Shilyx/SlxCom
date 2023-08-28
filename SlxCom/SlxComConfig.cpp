#include <Windows.h>
#include <shlwapi.h>
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

#define SC_REGPATH  L"Software\\Shilyx Studio\\SlxCom\\Config"

BOOL RefreshControls(HWND hwndDlg)
{
    BOOL bEnableOverlayIcon = SendDlgItemMessageW(hwndDlg, IDC_ENABLE_OVERLAYICON, BM_GETCHECK, 0, 0) == BST_CHECKED;

    EnableWindow(GetDlgItem(hwndDlg, IDC_ASYNC_OVERLAYICON), bEnableOverlayIcon);

    return TRUE;
}

BOOL LoadOption(HWND hwndDlg)
{
    BOOL bEnableOverlayIcon = FALSE;
    BOOL bAsyncOverlayIcon = FALSE;
    DWORD dwSize;

    dwSize = sizeof(bEnableOverlayIcon);
    SHGetValueW(HKEY_CURRENT_USER, SC_REGPATH, L"IDC_ENABLE_OVERLAYICON", NULL, &bEnableOverlayIcon, &dwSize);

    if(bEnableOverlayIcon)
    {
        SendDlgItemMessageW(hwndDlg, IDC_ENABLE_OVERLAYICON, BM_SETCHECK, BST_CHECKED, 0);
    }

    dwSize = sizeof(bAsyncOverlayIcon);
    SHGetValueW(HKEY_CURRENT_USER, SC_REGPATH, L"IDC_ASYNC_OVERLAYICON", NULL, &bAsyncOverlayIcon, &dwSize);

    if(bAsyncOverlayIcon)
    {
        SendDlgItemMessageW(hwndDlg, IDC_ASYNC_OVERLAYICON, BM_SETCHECK, BST_CHECKED, 0);
    }

    return RefreshControls(hwndDlg);
}

BOOL SaveOption(HWND hwndDlg)
{
    BOOL bEnableOverlayIcon = SendDlgItemMessageW(hwndDlg, IDC_ENABLE_OVERLAYICON, BM_GETCHECK, 0, 0) == BST_CHECKED;
    BOOL bAsyncOverlayIcon = SendDlgItemMessageW(hwndDlg, IDC_ASYNC_OVERLAYICON, BM_GETCHECK, 0, 0) == BST_CHECKED;

    SHSetValueW(HKEY_CURRENT_USER, SC_REGPATH, L"IDC_ENABLE_OVERLAYICON", REG_DWORD, &bEnableOverlayIcon, sizeof(bEnableOverlayIcon));
    SHSetValueW(HKEY_CURRENT_USER, SC_REGPATH, L"IDC_ASYNC_OVERLAYICON", REG_DWORD, &bAsyncOverlayIcon, sizeof(bAsyncOverlayIcon));

    return FALSE;
}

BOOL __stdcall SlxComConfigDlgProc(HWND hwndDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    switch(uMessage)
    {
    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_ENABLE_OVERLAYICON:
            RefreshControls(hwndDlg);
            break;

        case IDC_ASYNC_OVERLAYICON:
            break;

        case IDOK:
            SaveOption(hwndDlg);
            EndDialog(hwndDlg, 0);
            break;

        case IDCANCEL:
            EndDialog(hwndDlg, 0);
            break;

        default:
            break;
        }

        break;

    case WM_SYSCOMMAND:
        if(wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }

        break;

    case WM_INITDIALOG:
        SetClassLongPtr(hwndDlg, GCLP_HICON, (LONG_PTR)LoadIconW(g_hinstDll, MAKEINTRESOURCEW(IDI_CONFIG_ICON)));

        LoadOption(hwndDlg);

        break;

    default:
        break;
    }

    return FALSE;
}

void WINAPI SlxComConfig(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    DialogBoxParamW(g_hinstDll, MAKEINTRESOURCEW(IDD_CONFIG_DIALOG), hwndStub, (DLGPROC)SlxComConfigDlgProc, NULL);
}