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
#define LVH_PROCESSNAME     2
#define LVH_HANDLEVALUE     3
#define LVH_FILEPATH        4

enum
{
    CMD_REFRESH = 101,
    CMD_CLOSE,
    CMD_FILTER,
};

#define WM_REVIEW           (WM_USER + 112)
#define WM_REFRESH_COMPLETE (WM_USER + 113)

#define DLGWND_CAPTION      TEXT("文件锁定情况")

static int CALLBACK ListCtrlCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    ListCtrlSortStruct *pLcss = (ListCtrlSortStruct *)lParamSort;
    TCHAR szText1[1000] = TEXT("");
    TCHAR szText2[1000] = TEXT("");

    ListView_GetItemText(pLcss->hListCtrl, lParam1, pLcss->nSubItem, szText1, sizeof(szText1) / sizeof(TCHAR));
    ListView_GetItemText(pLcss->hListCtrl, lParam2, pLcss->nSubItem, szText2, sizeof(szText2) / sizeof(TCHAR));

    if(lstrcmp(szText1, szText2) == 0)
    {
        return 0;
    }

    if(pLcss->nSubItem == LVH_INDEX || pLcss->nSubItem == LVH_PROCESSID)
    {
        int n1 = StrToInt(szText1);
        int n2 = StrToInt(szText2);

        if(n1 > n2)
        {
            return pLcss->nRet;
        }
        else
        {
            return -pLcss->nRet;
        }
    }
    else if(pLcss->nSubItem == LVH_HANDLEVALUE)
    {
        int n1, n2;

        StrToIntEx(szText1, STIF_SUPPORT_HEX, &n1);
        StrToIntEx(szText2, STIF_SUPPORT_HEX, &n2);

        if(n1 > n2)
        {
            return pLcss->nRet;
        }
        else
        {
            return -pLcss->nRet;
        }
    }
    else if(pLcss->nSubItem == LVH_PROCESSNAME || pLcss->nSubItem == LVH_FILEPATH)
    {
        return lstrcmpi(szText1, szText2) * pLcss->nRet;
    }
    else
    {
        return pLcss->nRet;
    }
}

static DWORD __stdcall RefreshThread(LPVOID lpParam)
{
    HWND hwndDlg = (HWND)lpParam;

    DWORD dwCount = 0;
    FileHandleInfo *pHandles = GetFileHandleInfos(&dwCount);

    if (IsWindow(hwndDlg))
    {
        PostMessage(hwndDlg, WM_REFRESH_COMPLETE, dwCount, (LPARAM)pHandles);
    }

    return 0;
}

INT_PTR __stdcall UnlockFileFromPathDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    static BOOL bFilter = TRUE;
    static BOOL bRefreshing = FALSE;

    LPCTSTR lpPropHandles = TEXT("Handles");
    LPCTSTR lpPropCount = TEXT("Count");

    if (uMsg == WM_TIMER)
    {
        if (wParam == 1)
        {
            if (bRefreshing)
            {
                static DWORD dwTimerIndex = 0;
                TCHAR szCaption[1000] = DLGWND_CAPTION TEXT(" - 正在刷新....");

                dwTimerIndex += 1;

                szCaption[lstrlen(szCaption) - 3 + dwTimerIndex % 4] = TEXT('\0');
                SetWindowText(hwndDlg, szCaption);
            }
        }
    }
    else if (uMsg == WM_INITDIALOG)
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
        SetWindowText(hwndDlg, DLGWND_CAPTION);
        SetDlgItemText(hwndDlg, IDC_FILEPATH, (LPCTSTR)lParam);

        //
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
        SetTimer(hwndDlg, 1, 400, NULL);

        //
        PostMessage(hwndDlg, WM_COMMAND, MAKELONG(CMD_REFRESH, 0), 0);

        return TRUE;
    }
    else if (uMsg == WM_SYSCOMMAND)
    {
        if (wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
    }
    else if (uMsg == WM_REFRESH_COMPLETE)
    {
        bRefreshing = FALSE;
        SetWindowText(hwndDlg, DLGWND_CAPTION);

        DWORD dwCount = (DWORD)wParam;
        FileHandleInfo *pHandles = (FileHandleInfo *)lParam;
        FileHandleInfo *pLastHandles = (FileHandleInfo *)GetProp(hwndDlg, lpPropHandles);
        DWORD dwLastCount = (DWORD)GetProp(hwndDlg, lpPropCount);

        if (pLastHandles != NULL)
        {
            FreeFileHandleInfos(pLastHandles);
        }

        SetProp(hwndDlg, lpPropHandles, (HANDLE)pHandles);
        SetProp(hwndDlg, lpPropCount, (HANDLE)dwCount);

        PostMessage(hwndDlg, WM_REVIEW, 0, 0);
    }
    else if (uMsg == WM_COMMAND)
    {
        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        if (LOWORD(wParam) == IDCANCEL)
        {
            SendMessage(hwndDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
        }
        else if (LOWORD(wParam) == CMD_REFRESH)
        {
            bRefreshing = TRUE;

            HANDLE hThread = CreateThread(NULL, 0, RefreshThread, (LPVOID)hwndDlg, 0, NULL);
            CloseHandle(hThread);
        }
        else if (LOWORD(wParam) == CMD_FILTER)
        {
            bFilter = !bFilter;
            PostMessage(hwndDlg, WM_REVIEW, 0, 0);
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

                    lResult = ListView_GetNextItem(hHandleList, lResult, LVNI_SELECTED);
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

                    PostMessage(hwndDlg, WM_COMMAND, MAKELONG(CMD_REFRESH, 0), 0);
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
    }
    else if (uMsg == WM_REVIEW)
    {
        HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

        ListView_DeleteAllItems(hHandleList);

        FileHandleInfo *pHandles = (FileHandleInfo *)GetProp(hwndDlg, lpPropHandles);
        DWORD dwCount = (DWORD)GetProp(hwndDlg, lpPropCount);

        if (pHandles != NULL)
        {
            DWORD dwHandleIndex = 0;
            TCHAR szTargetFilePath[MAX_PATH] = TEXT("");

            GetDlgItemText(hwndDlg, IDC_FILEPATH, szTargetFilePath, sizeof(szTargetFilePath) / sizeof(TCHAR));

            for (; dwHandleIndex < dwCount; dwHandleIndex += 1)
            {
                if (!bFilter || bFilter && IsSameFilePath(szTargetFilePath, pHandles[dwHandleIndex].szFilePath))
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

                    ListView_SetItemText(hHandleList, dwItemIndex, LVH_PROCESSNAME, pHandles[dwHandleIndex].szProcessName);

                    wnsprintf(szText, sizeof(szText) / sizeof(TCHAR), TEXT("%#x"), pHandles[dwHandleIndex].hFile);
                    ListView_SetItemText(hHandleList, dwItemIndex, LVH_HANDLEVALUE, szText);

                    ListView_SetItemText(hHandleList, dwItemIndex, LVH_FILEPATH, pHandles[dwHandleIndex].szFilePath);
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
                HWND hHandleList = GetDlgItem(hwndDlg, IDC_HANDLELIST);

                if (lpNmHdr->code == LVN_COLUMNCLICK)
                {
                    static int nRet = 1;
                    LPNMLISTVIEW lpNmListView = (LPNMLISTVIEW)lpNmHdr;

                    nRet = -nRet;

                    ListCtrlSortStruct lcss;

                    lcss.nRet = nRet;
                    lcss.hListCtrl = hHandleList;
                    lcss.nSubItem = lpNmListView->iSubItem;

                    LVITEM item = {LVIF_PARAM};

                    for(int nIndex = 0; nIndex < ListView_GetItemCount(hHandleList); nIndex += 1)
                    {
                        item.iItem = nIndex;
                        item.lParam = nIndex;

                        ListView_SetItem(hHandleList, &item);
                    }

                    ListView_SortItems(hHandleList, ListCtrlCompareProc, &lcss);
                }
                else if (lpNmHdr->code == NM_DBLCLK)
                {
                    LPNMITEMACTIVATE lpNmItemAct = (LPNMITEMACTIVATE)lParam;
                    int nIndex = ListView_GetSelectionMark(hHandleList);

                    if (nIndex != -1)
                    {
                        TCHAR szFilePath[MAX_PATH] = TEXT("");

                        ListView_GetItemText(hHandleList, lpNmItemAct->iItem, LVH_FILEPATH, szFilePath, sizeof(szFilePath) / sizeof(TCHAR));

                        BrowseForFile(szFilePath);
                    }
                    
                }
                else if (lpNmHdr->code == NM_RCLICK)
                {
                    HMENU hPopMenu = CreatePopupMenu();

                    if (hPopMenu != NULL)
                    {
                        LPNMITEMACTIVATE lpNmItemAct = (LPNMITEMACTIVATE)lParam;
                        POINT pt;

                        GetCursorPos(&pt);

                        AppendMenu(hPopMenu, MF_STRING, CMD_FILTER, TEXT("只显示匹配的文件句柄"));

                        if (bFilter)
                        {
                            CheckMenuItem(hPopMenu, CMD_FILTER, MF_CHECKED | MF_BYCOMMAND);
                        }

                        if (lpNmItemAct->iItem != -1)
                        {
                            AppendMenu(hPopMenu, MF_STRING, CMD_CLOSE, TEXT("关闭选中的句柄"));
                        }

                        AppendMenu(hPopMenu, MF_STRING, CMD_REFRESH, TEXT("刷新句柄列表"));

                        if (bRefreshing)
                        {
                            EnableMenuItem(hPopMenu, CMD_REFRESH, MF_GRAYED | MF_BYCOMMAND);
                        }

                        TrackPopupMenu(hPopMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        DestroyMenu(hPopMenu);
                    }
                }
            }
        }
    }
    else if(uMsg == WM_SIZING)
    {
        LPRECT pRect = (LPRECT)lParam;
        const LONG lWidthLimit = 561;
        const LONG lHeightLimit = 311;

        if(pRect->right - pRect->left < lWidthLimit)
        {
            if(wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_LEFT)
            {
                pRect->left = pRect->right - lWidthLimit;
            }
            else
            {
                pRect->right = pRect->left + lWidthLimit;
            }
        }

        if(pRect->bottom - pRect->top < lHeightLimit)
        {
            if(wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOP)
            {
                pRect->top = pRect->bottom - lHeightLimit;
            }
            else
            {
                pRect->bottom = pRect->top + lHeightLimit;
            }
        }
    }
    else if(uMsg == WM_SIZE)
    {
        RECT rectClient;
        RECT rectEdit;

        GetClientRect(hwndDlg, &rectClient);
        InflateRect(&rectClient, -11, -10);

        rectClient.top += 31;
        MoveWindow(
            GetDlgItem(hwndDlg, IDC_HANDLELIST),
            rectClient.left,
            rectClient.top,
            rectClient.right - rectClient.left,
            rectClient.bottom - rectClient.top,
            TRUE
            );

        GetClientRect(hwndDlg, &rectClient);
        GetWindowRect(GetDlgItem(hwndDlg, IDC_FILEPATH), &rectEdit);
        ScreenToClient(hwndDlg, (POINT *)&rectEdit);
        ScreenToClient(hwndDlg, (POINT *)&rectEdit + 1);
        rectEdit.right = rectClient.right - 11;
        MoveWindow(
            GetDlgItem(hwndDlg, IDC_FILEPATH),
            rectEdit.left,
            rectEdit.top,
            rectEdit.right - rectEdit.left,
            rectEdit.bottom - rectEdit.top,
            TRUE
            );
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
