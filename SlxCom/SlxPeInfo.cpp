#include <Windows.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include "resource.h"
#include "SlxComTools.h"
#pragma warning(disable: 4786)
#include <vector>
#include "SlxString.h"

using namespace std;

extern HINSTANCE g_hinstDll;

#define LVH_INDEX           0
#define LVH_FILEPATH        1
#define LVH_BINARYTYPE      2
#define LVH_LINKVERSION     3
#define LVH_COMPLIETIME     4
#define LVH_PDBPATH         5

static INT_PTR __stdcall PeInfoDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;

    if (uMsg == WM_INITDIALOG)
    {
        //设定对话框图标
        if(hIcon == NULL)
        {
            hIcon = LoadIcon(g_hinstDll, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        //
        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        ListView_SetExtendedListViewStyleEx(hHandleList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        //
        LVCOLUMN col = {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT};

        col.fmt = LVCFMT_LEFT;

        col.cx = 45;
        col.pszText = TEXT("#");
        ListView_InsertColumn(hHandleList, LVH_INDEX, &col);

        col.cx = 75;
        col.pszText = TEXT("文件路径");
        ListView_InsertColumn(hHandleList, LVH_PROCESSID, &col);

        col.cx = 80;
        col.pszText = TEXT("进程映像");
        ListView_InsertColumn(hHandleList, LVH_PROCESSNAME, &col);

        col.cx = 75;
        col.pszText = TEXT("句柄值");
        ListView_InsertColumn(hHandleList, LVH_HANDLEVALUE, &col);

        col.cx = 220;
        col.pszText = TEXT("文件路径");
        ListView_InsertColumn(hHandleList, LVH_FILEPATH, &col);

        //
        DragAcceptFiles(hwndDlg, TRUE);

        //
        int nArgc = 0;
        LPWSTR *lpArgv = CommandLineToArgvW((LPCWSTR)lParam, &nArgc);

        if (lpArgv != NULL)
        {
            if (nArgc > 0)
            {
                TCHAR szFilePath[MAX_PATH];

                for (int nIndex = 0; nIndex < nArgc; nIndex += 1)
                {
                    wnsprintf(szFilePath, _countof(szFilePath), TEXT("%ls"), lpArgv[nIndex]);

                }
            }

            LocalFree(lpArgv);
        }

        return TRUE;
    }
    else if (uMsg == WM_SYSCOMMAND)
    {
        if (wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
    }

    return FALSE;
}

VOID SlxPeInfoW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpCmdLine, int nShow)
{
    DisableWow64FsRedirection();
    DialogBoxParam(g_hinstDll, MAKEINTRESOURCE(IDD_PEINFO), hwndStub, PeInfoDlgProc, (LPARAM)lpCmdLine);
}