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

#define WM_REFRESH_VIEW     (WM_USER + 112)

BOOL CloseRemoteHandle(DWORD dwProcessId, HANDLE hRemoteHandle)
{
    BOOL bResult = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);

    if (hProcess != NULL)
    {
        HANDLE hLocalHandle = NULL;

        if (DuplicateHandle(
            hProcess,
            hRemoteHandle,
            GetCurrentProcess(),
            &hLocalHandle,
            0,
            FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE
            ))
        {
            bResult = TRUE;
            CloseHandle(hLocalHandle);
        }

        CloseHandle(hProcess);
    }

    return bResult;
}

INT_PTR __stdcall UnlockFileFromPathDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPCTSTR lpPropHandles = TEXT("Handles");
    LPCTSTR lpPropCount = TEXT("Count");

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
            DWORD dwCount = 0;
            FileHandleInfo *pHandles = GetFileHandleInfos(&dwCount);
            FileHandleInfo *pLastHandles = (FileHandleInfo *)GetProp(hwndDlg, lpPropHandles);
            DWORD dwLastCount = (DWORD)GetProp(hwndDlg, lpPropCount);

            if (pLastHandles != NULL)
            {
                FreeFileHandleInfos(pLastHandles);
            }

            SetProp(hwndDlg, lpPropHandles, (HANDLE)pHandles);
            SetProp(hwndDlg, lpPropCount, (HANDLE)dwCount);

            PostMessage(hwndDlg, WM_REFRESH_VIEW, 0, 0);
        }
        else if (LOWORD(wParam) == CMD_CLOSE)
        {
            DWORD dwSelectCount = ListView_GetSelectedCount(hHandleList);
            DWORD dwSucceedCount = 0;
            TCHAR *szErrorInfo = (TCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSelectCount * 100 * sizeof(TCHAR));

            if (szErrorInfo != NULL)
            {
                LRESULT lResult = ListView_GetNextItem(hHandleList, -1, LVNI_SELECTED);

                while (lResult != -1)
                {
                    TCHAR szText[100] = TEXT("");
                    DWORD dwProcessId = 0;
                    HANDLE hHandle = NULL;

                    ListView_GetItemText(hHandleList, lResult, LVH_PROCESSID, szText, sizeof(szText) / sizeof(TCHAR));
                    dwProcessId = StrToInt(szText);

                    ListView_GetItemText(hHandleList, lResult, LVH_HANDLEVALUE, szText, sizeof(szText) / sizeof(TCHAR));
                    StrToIntEx(szText, STIF_SUPPORT_HEX, (int *)&hHandle);

                    if (CloseRemoteHandle(dwProcessId, hHandle))
                    {
                        dwSucceedCount += 1;
                    }
                    else
                    {
                        wnsprintf(
                            szErrorInfo + lstrlen(szErrorInfo),
                            dwSelectCount * 100 - lstrlen(szErrorInfo),
                            TEXT("进程%lu中的句柄%#x关闭失败\r\n"),
                            dwProcessId,
                            hHandle
                            );
                    }

                    lResult = ListView_GetNextItem(hHandleList, lResult + 1, LVNI_SELECTED);
                }

                if (dwSucceedCount == dwSelectCount)
                {
                    MessageBoxFormat(
                        hwndDlg,
                        TEXT("信息"),
                        MB_ICONINFORMATION,
                        TEXT("成功关闭了%lu个句柄。"),
                        dwSucceedCount
                        );

                    PostMessage(hwndDlg, WM_REFRESH_VIEW, 0, 0);
                }
                else
                {
                    MessageBoxFormat(
                        hwndDlg,
                        TEXT("信息"),
                        MB_ICONINFORMATION,
                        TEXT("成功关闭了%lu个句柄，以下句柄未能成功关闭：\r\n\r\n%s"),
                        dwSucceedCount,
                        szErrorInfo
                        );
                }

                HeapFree(GetProcessHeap(), 0, szErrorInfo);
            }
        }
        else if (LOWORD(wParam) == IDC_FILTER)
        {
            PostMessage(hwndDlg, WM_REFRESH_VIEW, 0, 0);
        }
    }
    else if (uMsg == WM_REFRESH_VIEW)
    {
        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        ListView_DeleteAllItems(hHandleList);

        FileHandleInfo *pHandles = (FileHandleInfo *)GetProp(hwndDlg, lpPropHandles);
        DWORD dwCount = (DWORD)GetProp(hwndDlg, lpPropCount);
        BOOL bFilter = SendDlgItemMessage(hwndDlg, IDC_FILTER, BM_GETCHECK, 0, 0) == BST_CHECKED;

        if (pHandles != NULL)
        {
            DWORD dwHandleIndex = 0;
            TCHAR szTargetFilePath[MAX_PATH] = TEXT("");

            GetDlgItemText(hwndDlg, IDC_FILEPATH, szTargetFilePath, sizeof(szTargetFilePath) / sizeof(TCHAR));

            for (; dwHandleIndex < dwCount; dwHandleIndex += 1)
            {
                if (!bFilter || bFilter && lstrcmpi(szTargetFilePath, pHandles[dwHandleIndex].szFilePath) == 0)
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
                            AppendMenu(hPopMenu, MF_STRING, CMD_CLOSE, TEXT("关闭选中的句柄"));
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
