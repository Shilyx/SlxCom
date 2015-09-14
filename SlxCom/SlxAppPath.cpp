#include "SlxAppPath.h"
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "resource.h"

#define APPPATH_PATH TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD (0x1600 + 0x000C)
#endif

using namespace std;

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

// BOOL ModifyAppPath_GetCommandFilePath(LPCTSTR lpCommand, TCHAR szFilePath[], DWORD dwSize)
// {
//     DWORD dwRegType = 0;
// 
//     dwSize *= sizeof(TCHAR);
// 
//     SHGetValue(
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

BOOL ModifyAppPath_GetFileCommand(LPCTSTR lpFilePath, TCHAR szCommand[], DWORD dwSize)
{
    DWORD dwRegType = 0;

    dwSize *= sizeof(TCHAR);

    SHGetValue(
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
    TCHAR szCommandInReg[MAX_PATH] = TEXT("");

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(hIcon == NULL)
        {
            hIcon = LoadIcon(g_hinstDll, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        SendDlgItemMessage(hwndDlg, IDOK, BCM_SETSHIELD, 0, TRUE);
        SendDlgItemMessage(hwndDlg, IDCANCEL, BCM_SETSHIELD, 0, TRUE);

        ModifyAppPath_GetFileCommand((LPCTSTR)lParam, szCommandInReg, sizeof(szCommandInReg) / sizeof(TCHAR));

        SetDlgItemText(hwndDlg, IDC_FILE, (LPCTSTR)lParam);
        SetDlgItemText(hwndDlg, IDC_KEY, szCommandInReg);

        if (lstrlen(szCommandInReg) == 0)
        {
            SetDlgItemText(hwndDlg, IDC_KEY, PathFindFileName((LPCTSTR)lParam));
        }

        SendDlgItemMessage(hwndDlg, IDC_KEY, EM_SETLIMITTEXT, sizeof(szCommandInReg) / sizeof(TCHAR), 0);

        return FALSE;

    case WM_CLOSE:
        EndDialog(hwndDlg, 0);
        break;

    case WM_COMMAND:
        if(LOWORD(wParam) == IDOK)
        {
            TCHAR szRegPath[1000];
            TCHAR szFilePath[MAX_PATH] = TEXT("");
            TCHAR szCommand[MAX_PATH] = TEXT("");

            GetDlgItemText(hwndDlg, IDC_FILE, szFilePath, sizeof(szFilePath) / sizeof(TCHAR));
            GetDlgItemText(hwndDlg, IDC_KEY, szCommand, sizeof(szCommand) / sizeof(TCHAR));
            ModifyAppPath_GetFileCommand(szFilePath, szCommandInReg, sizeof(szCommandInReg) / sizeof(TCHAR));

            if (lstrlen(szCommandInReg) > 0)
            {
                wnsprintf(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(TCHAR),
                    TEXT("%s\\%s"),
                    APPPATH_PATH,
                    szCommandInReg
                    );

                SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);
            }

            if (lstrlen(szCommand) <= 0)
            {
                SHDeleteValue(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath);
            }
            else
            {
                if (lstrcmpi(szCommand + lstrlen(szCommand) - 4, TEXT(".exe")) != 0)
                {
                    if (IDYES == MessageBox(
                        hwndDlg,
                        TEXT("您提供的快捷短语未以“.exe”结尾，是否自动添加？"),
                        TEXT("请确认"),
                        MB_ICONQUESTION | MB_YESNO
                        ))
                    {
                        StrCatBuff(szCommand, TEXT(".exe"), sizeof(szCommand) / sizeof(TCHAR));
                        SetDlgItemText(hwndDlg, IDC_KEY, szCommand);
                    }
                }

                SHSetValue(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath, REG_SZ, szCommand, (lstrlen(szCommand) + 1) * sizeof(TCHAR));

                wnsprintf(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(TCHAR),
                    TEXT("%s\\%s"),
                    APPPATH_PATH,
                    szCommand
                    );

                SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, szFilePath, (lstrlen(szFilePath) + 1) * sizeof(TCHAR));

                PathRemoveFileSpec(szFilePath);
                SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, TEXT("Path"), REG_SZ, szFilePath, (lstrlen(szFilePath) + 1) * sizeof(TCHAR));
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
        return (BOOL)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL ModifyAppPath(LPCTSTR lpFilePath)
{
    return 0 != DialogBoxParam(g_hinstDll, MAKEINTRESOURCE(IDD_APPPATH), NULL, ModifyAppPathProc, (LPARAM)lpFilePath);
}

std::tstring GetExistingAppPathCommand(LPCTSTR lpFilePath)
{
    tstring strCommand = RegGetString(HKEY_LOCAL_MACHINE, APPPATH_PATH, lpFilePath, TEXT(""));

    if (strCommand.empty())
    {
        return TEXT("");
    }

    tstring strFilePath = RegGetString(HKEY_LOCAL_MACHINE, (APPPATH_PATH + strCommand).c_str(), NULL, TEXT(""));

    if (lstrcmpi(lpFilePath, strFilePath.c_str()) == 0)
    {
        return strCommand;
    }
    
    return TEXT("");
}
