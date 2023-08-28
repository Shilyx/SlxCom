#include "SlxAppPath.h"
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "resource.h"

#define APPPATH_PATH L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD (0x1600 + 0x000C)
#endif

using namespace std;

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

// BOOL ModifyAppPath_GetCommandFilePath(LPCWSTR lpCommand, WCHAR szFilePath[], DWORD dwSize)
// {
//     DWORD dwRegType = 0;
// 
//     dwSize *= sizeof(WCHAR);
// 
//     SHGetValueW(
//         HKEY_LOCAL_MACHINE,
//         APPPATH_PATH,
//         lpFilePath,
//         &dwRegType,
//         szCommand,
//         &dwSize
//         );
// 
//     return dwRegType == REG_SZ;
// }

BOOL ModifyAppPath_GetFileCommand(LPCWSTR lpFilePath, WCHAR szCommand[], DWORD dwSize)
{
    DWORD dwRegType = 0;

    dwSize *= sizeof(WCHAR);

    SHGetValueW(
        HKEY_LOCAL_MACHINE,
        APPPATH_PATH,
        lpFilePath,
        &dwRegType,
        szCommand,
        &dwSize
        );

    return dwRegType == REG_SZ;
}

INT_PTR CALLBACK ModifyAppPathProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    WCHAR szCommandInReg[MAX_PATH] = L"";

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(hIcon == NULL)
        {
            hIcon = LoadIconW(g_hinstDll, MAKEINTRESOURCEW(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessageW(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        SendDlgItemMessageW(hwndDlg, IDOK, BCM_SETSHIELD, 0, TRUE);
        SendDlgItemMessageW(hwndDlg, IDCANCEL, BCM_SETSHIELD, 0, TRUE);

        ModifyAppPath_GetFileCommand((LPCWSTR)lParam, szCommandInReg, sizeof(szCommandInReg) / sizeof(WCHAR));

        SetDlgItemTextW(hwndDlg, IDC_FILE, (LPCWSTR)lParam);
        SetDlgItemTextW(hwndDlg, IDC_KEY, szCommandInReg);

        if (lstrlenW(szCommandInReg) == 0)
        {
            SetDlgItemTextW(hwndDlg, IDC_KEY, PathFindFileNameW((LPCWSTR)lParam));
        }

        SendDlgItemMessageW(hwndDlg, IDC_KEY, EM_SETLIMITTEXT, sizeof(szCommandInReg) / sizeof(WCHAR), 0);

        return FALSE;

    case WM_CLOSE:
        EndDialog(hwndDlg, 0);
        break;

    case WM_COMMAND:
        if(LOWORD(wParam) == IDOK)
        {
            WCHAR szRegPath[1000];
            WCHAR szFilePath[MAX_PATH] = L"";
            WCHAR szCommand[MAX_PATH] = L"";

            GetDlgItemTextW(hwndDlg, IDC_FILE, szFilePath, sizeof(szFilePath) / sizeof(WCHAR));
            GetDlgItemTextW(hwndDlg, IDC_KEY, szCommand, sizeof(szCommand) / sizeof(WCHAR));
            ModifyAppPath_GetFileCommand(szFilePath, szCommandInReg, sizeof(szCommandInReg) / sizeof(WCHAR));

            if (lstrlenW(szCommandInReg) > 0)
            {
                wnsprintfW(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(WCHAR),
                    L"%s\\%s",
                    APPPATH_PATH,
                    szCommandInReg
                    );

                SHDeleteKeyW(HKEY_LOCAL_MACHINE, szRegPath);
            }

            if (lstrlenW(szCommand) <= 0)
            {
                SHDeleteValueW(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath);
            }
            else
            {
                if (lstrcmpiW(szCommand + lstrlenW(szCommand) - 4, L".exe") != 0)
                {
                    if (IDYES == MessageBoxW(
                        hwndDlg,
                        L"您提供的快捷短语未以“.exe”结尾，是否自动添加？",
                        L"请确认",
                        MB_ICONQUESTION | MB_YESNO
                        ))
                    {
                        StrCatBuffW(szCommand, L".exe", sizeof(szCommand) / sizeof(WCHAR));
                        SetDlgItemTextW(hwndDlg, IDC_KEY, szCommand);
                    }
                }

                SHSetValueW(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath, REG_SZ, szCommand, (lstrlenW(szCommand) + 1) * sizeof(WCHAR));

                wnsprintfW(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(WCHAR),
                    L"%s\\%s",
                    APPPATH_PATH,
                    szCommand
                    );

                SHSetValueW(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, szFilePath, (lstrlenW(szFilePath) + 1) * sizeof(WCHAR));

                PathRemoveFileSpecW(szFilePath);
                SHSetValueW(HKEY_LOCAL_MACHINE, szRegPath, L"Path", REG_SZ, szFilePath, (lstrlenW(szFilePath) + 1) * sizeof(WCHAR));
            }

            EndDialog(hwndDlg, IDOK);
        }
        else if(LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hwndDlg, IDCANCEL);
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (INT_PTR)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL ModifyAppPath(LPCWSTR lpFilePath)
{
    return 0 != DialogBoxParamW(g_hinstDll, MAKEINTRESOURCEW(IDD_APPPATH), NULL, ModifyAppPathProc, (LPARAM)lpFilePath);
}

std::wstring GetExistingAppPathCommand(LPCWSTR lpFilePath)
{
    wstring strCommand = RegGetString(HKEY_LOCAL_MACHINE, APPPATH_PATH, lpFilePath, L"");

    if (strCommand.empty())
    {
        return L"";
    }

    wstring strFilePath = RegGetString(HKEY_LOCAL_MACHINE, (APPPATH_PATH + strCommand).c_str(), NULL, L"");

    if (lstrcmpiW(lpFilePath, strFilePath.c_str()) == 0)
    {
        return strCommand;
    }
    
    return L"";
}
