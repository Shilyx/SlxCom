#include <Windows.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include "resource.h"
#include "SlxComTools.h"
#include "SlxFileHandles.h"

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

#define LVH_INDEX           0
#define LVH_PROCESSID       1
#define LVH_PROCESSIMAGE    2
#define LVH_HANDLEVALUE     3

#define CMD_REFRESH         1
#define CMD_CLOSE           2

VOID RefreshHandles(HWND hwndDlg, HWND hHandleList)
{
    ListView_DeleteAllItems(hHandleList);

    DWORD dwCount = 0;
    FileHandleInfo *pHandles = GetFileHandleInfos(&dwCount);

    if (pHandles != NULL)
    {
        DWORD dwHandleIndex = 0;
        TCHAR szTargetFilePath[MAX_PATH] = TEXT("");

        GetDlgItemText(hwndDlg, IDC_FILEPATH, szTargetFilePath, sizeof(szTargetFilePath) / sizeof(TCHAR));

        for (; dwHandleIndex < dwCount; dwHandleIndex += 1)
        {
            if (lstrcmpi(szTargetFilePath, pHandles[dwHandleIndex].szFilePath) == 0)
            {
                DWORD dwItemIndex = ListView_GetItemCount(hHandleList);
                TCHAR szText[1000];
                LV_ITEM item = {LVIF_TEXT};

                item.pszText = szText;

                item.iItem = dwItemIndex;
                wnsprintf(szText, sizeof(szText) / sizeof(TCHAR), TEXT("%lu"), dwItemIndex + 1);
                ListView_InsertItem(hHandleList, &item);

                wnsprintf(szText, sizeof(szText) / sizeof(TCHAR), TEXT("%lu"), pHandles[dwHandleIndex].dwProcessId);
                ListView_SetItemText(hHandleList, dwItemIndex, LVH_PROCESSID, szText);

                ListView_SetItemText(hHandleList, dwItemIndex, LVH_PROCESSIMAGE, pHandles[dwHandleIndex].szFilePath);

                wnsprintf(szText, sizeof(szText) / sizeof(TCHAR), TEXT("%#x"), pHandles[dwHandleIndex].hFile);
                ListView_SetItemText(hHandleList, dwItemIndex, LVH_HANDLEVALUE, szText);
            }
        }

        FreeFileHandleInfos(pHandles);
    }
}

INT_PTR __stdcall UnlockFileFromPathDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG)
    {
        SetDlgItemText(hwndDlg, IDC_FILEPATH, (LPCTSTR)lParam);

        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        ListView_SetExtendedListViewStyleEx(hHandleList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        //填充列表
        LVCOLUMN col = {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT};

        col.fmt = LVCFMT_LEFT;

        col.cx = 45;
        col.pszText = TEXT("#");
        ListView_InsertColumn(hHandleList, LVH_INDEX, &col);

        col.cx = 75;
        col.pszText = TEXT("pid");
        ListView_InsertColumn(hHandleList, LVH_PROCESSID, &col);

        col.cx = 300;
        col.pszText = TEXT("进程映像");
        ListView_InsertColumn(hHandleList, LVH_PROCESSIMAGE, &col);

        col.cx = 75;
        col.pszText = TEXT("句柄值");
        ListView_InsertColumn(hHandleList, LVH_HANDLEVALUE, &col);

        return TRUE;
    }
    else if (uMsg == WM_SYSCOMMAND)
    {
        if (wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
    }
    else if (uMsg == WM_COMMAND)
    {
        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        if (LOWORD(wParam) == CMD_REFRESH)
        {
            RefreshHandles(hwndDlg, hHandleList);
        }
        else if (LOWORD(wParam) == CMD_CLOSE)
        {
            MessageBox(hwndDlg, TEXT("CMD_CLOSE"), NULL, 0);
        }
    }
    else if (uMsg == WM_NOTIFY)
    {
        LPNMHDR lpNmHdr = (LPNMHDR)lParam;

        if (lpNmHdr != NULL)
        {
            if (lpNmHdr->idFrom = IDC_HANDLELIST)
            {
                if (lpNmHdr->code == NM_RCLICK)
                {
                    HMENU hPopMenu = CreatePopupMenu();

                    if (hPopMenu != NULL)
                    {
                        LPNMITEMACTIVATE lpNmItemAct = (LPNMITEMACTIVATE)lParam;
                        POINT pt;

                        GetCursorPos(&pt);

                        if (lpNmItemAct->iItem != -1)
                        {
                            AppendMenu(hPopMenu, MF_STRING, CMD_REFRESH, TEXT("关闭选中的句柄"));
                        }

                        AppendMenu(hPopMenu, MF_STRING, CMD_REFRESH, TEXT("刷新句柄列表"));

                        TrackPopupMenu(hPopMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        DestroyMenu(hPopMenu);
                    }
                }
            }
        }
    }

    return FALSE;
}

void UnlockFileFromPathInternal(HWND hwndStub, LPCTSTR lpFilePath)
{
    DialogBoxParam(
        g_hinstDll,
        MAKEINTRESOURCE(IDD_UNLOCKFILE_DIALOG),
        NULL,
        UnlockFileFromPathDialogProc,
        (LPARAM)lpFilePath
        );
}

void WINAPI UnlockFileFromPathW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    TCHAR szFilePath[MAX_PATH] = TEXT("");

    wnsprintf(szFilePath, sizeof(szFilePath) / sizeof(TCHAR), TEXT("%ls"), lpszCmdLine);

    EnableDebugPrivilege(TRUE);
    UnlockFileFromPathInternal(hwndStub, szFilePath);
}
