#include <Windows.h>
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "resource.h"
#pragma warning(disable: 4786)
#include <map>
#include "lib/charconv.h"
#include "SlxComOverlay.h"
#include "SlxQuickPopupMenu.h"

using namespace std;

#define WM_SIGNRESULT (WM_USER + 101)

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

DWORD CALLBACK ManualCheckSignatureThreadProc(LPVOID lpParam)
{
    while (lpParam == 0)
    {
        SleepEx(INFINITE, TRUE);
    }

    map<wstring, HWND> *pMapPaths = (map<wstring, HWND> *)lpParam;

    if (pMapPaths != NULL && !pMapPaths->empty())
    {
        map<wstring, HWND>::iterator it = pMapPaths->begin();
        HWND hTargetWindow = it->second;
        DWORD dwFileIndex = 0;

        if (IsWindow(hTargetWindow))             //通过排队，窗口可能已经被取消
        {
            for (; it != pMapPaths->end(); it++, dwFileIndex++)
            {
                BOOL bSigned = FALSE;

                if (PathFileExistsW(it->first.c_str()))
                {
                    WCHAR szString[MAX_PATH + 100];

                    if (CSlxComOverlay::BuildFileMarkString(
                        it->first.c_str(),
                        szString,
                        sizeof(szString) / sizeof(WCHAR)
                        ))
                    {
                        if (IsFileSigned(it->first.c_str()))
                        {
                            CSlxComOverlay::m_cache.AddCache(szString, SS_1);
                            bSigned = TRUE;
                        }
                        else
                        {
                            CSlxComOverlay::m_cache.AddCache(szString, SS_2);
                        }
                    }
                }

                if (IsWindow(hTargetWindow))
                {
                    SendMessageW(hTargetWindow, WM_SIGNRESULT, (WPARAM)it->first.c_str(), bSigned);
                }
                else
                {
                    break;
                }
            }
        }
    }

    delete pMapPaths;
    return 0;
}

#define LVH_INDEX       0
#define LVH_PATH        1
#define LVH_RESULT      2

static int CALLBACK ListCtrlCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    ListCtrlSortStruct *pLcss = (ListCtrlSortStruct *)lParamSort;
    WCHAR szText1[1000] = L"";
    WCHAR szText2[1000] = L"";

    ListView_GetItemText(pLcss->hListCtrl, lParam1, pLcss->nSubItem, szText1, sizeof(szText1) / sizeof(WCHAR));
    ListView_GetItemText(pLcss->hListCtrl, lParam2, pLcss->nSubItem, szText2, sizeof(szText2) / sizeof(WCHAR));

    if (lstrcmpW(szText1, szText2) == 0)
    {
        return 0;
    }

    if (pLcss->nSubItem == LVH_INDEX)
    {
        int n1 = StrToIntW(szText1);
        int n2 = StrToIntW(szText2);

        if (n1 > n2)
        {
            return pLcss->nRet;
        }
        else
        {
            return -pLcss->nRet;
        }
    }
    else if (pLcss->nSubItem == LVH_PATH)
    {
        return lstrcmpW(szText1, szText2) * pLcss->nRet;
    }
    else if (pLcss->nSubItem == LVH_RESULT)
    {
        return lstrcmpW(szText1, szText2) * pLcss->nRet;
    }
    else
    {
        return pLcss->nRet;
    }
}

INT_PTR CALLBACK ManualCheckSignatureDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    BOOL bDlgProcResult = FALSE;
    HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);

    if (uMsg == WM_INITDIALOG)
    {
        //设定对话框图标
        if (hIcon == NULL)
        {
            hIcon = LoadIconW(g_hinstDll, MAKEINTRESOURCEW(IDI_CONFIG_ICON));
        }

        if (hIcon != NULL)
        {
            SendMessageW(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        //设置全选风格
        ListView_SetExtendedListViewStyleEx(hFileList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        //填充列表
        LVCOLUMNW col = {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT};

        col.fmt = LVCFMT_LEFT;

        col.cx = 35;
        col.pszText = L"#";
        ListView_InsertColumn(hFileList, LVH_INDEX, &col);

        col.cx = 345;
        col.pszText = L"路径";
        ListView_InsertColumn(hFileList, LVH_PATH, &col);

        col.cx = 50;
        col.pszText = L"结果";
        ListView_InsertColumn(hFileList, LVH_RESULT, &col);

        //开始启动校验过程
        map<wstring, HWND> *pMapPaths = (map<wstring, HWND> *)lParam;

        if (pMapPaths != NULL && !pMapPaths->empty())
        {
            //填充列表
            WCHAR szText[1000];
            DWORD dwIndex = 0;
            LVITEMW item = {LVIF_TEXT};
            map<wstring, HWND>::iterator it = pMapPaths->begin();

            it->second = hwndDlg;

            for (; it != pMapPaths->end(); it++, dwIndex++)
            {
                item.iItem = dwIndex;
                item.pszText = szText;

                wnsprintfW(szText, sizeof(szText) / sizeof(WCHAR), L"%lu", dwIndex + 1);
                ListView_InsertItem(hFileList, &item);

                lstrcpynW(szText, it->first.c_str(), sizeof(szText) / sizeof(WCHAR));
                ListView_SetItemText(hFileList, dwIndex, LVH_PATH, szText);

                lstrcpynW(szText, L"", sizeof(szText) / sizeof(WCHAR));
                ListView_SetItemText(hFileList, dwIndex, LVH_RESULT, szText);
            }

            //记录总文件个数
            WCHAR szWindowText[100];

            wnsprintfW(szWindowText, sizeof(szWindowText) / sizeof(WCHAR), L"校验数字签名 0/%lu(0.00%%)", pMapPaths->size());
            SetWindowTextW(hwndDlg, szWindowText);

            //开始校验
            extern volatile HANDLE m_hManualCheckSignatureThread; //Slx
            QueueUserAPC(
                (PAPCFUNC)ManualCheckSignatureThreadProc,
                m_hManualCheckSignatureThread,
                (ULONG_PTR)pMapPaths
                );
        }

        //开启定时器
        SetTimer(hwndDlg, 1, 40, NULL);

        bDlgProcResult = TRUE;
    }
    else if (uMsg == WM_TIMER)
    {
        if (wParam == 1)
        {
            static DWORD dwTimeStamp = 0;

            WCHAR szText[100];
            WCHAR *szTimeStr[] = {
                L"◤",
                L"◥",
                L"◢",
                L"◣"
            };
            HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);

            for (int nIndex = 0; nIndex < ListView_GetItemCount(GetDlgItem(hwndDlg, IDC_FILELIST)); nIndex += 1)
            {
                ListView_GetItemText(hFileList, nIndex, LVH_RESULT, szText, sizeof(szText) / sizeof(WCHAR));

                if (lstrlenW(szText) <= 1)
                {
                    ListView_SetItemText(
                        hFileList,
                        nIndex,
                        LVH_RESULT,
                        szTimeStr[(dwTimeStamp + nIndex) % (sizeof(szTimeStr) / sizeof(szTimeStr[0]))]
                    );
                }
            }

            dwTimeStamp += 1;
        }
    }
    else if (uMsg == WM_NOTIFY)
    {
        LPNMHDR lpNmHdr = (LPNMHDR)lParam;

        if (lpNmHdr != NULL && lpNmHdr->idFrom == IDC_FILELIST)
        {
            if (lpNmHdr->code == NM_CUSTOMDRAW)
            {
                LPNMLVCUSTOMDRAW lpNMCustomDraw = (LPNMLVCUSTOMDRAW)lParam;

                if (CDDS_PREPAINT == lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                }
                else if (CDDS_ITEMPREPAINT == lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    WCHAR szResult[100] = L"";

                    ListView_GetItemText(
                        hFileList,
                        lpNMCustomDraw->nmcd.dwItemSpec,
                        LVH_RESULT,
                        szResult,
                        sizeof(szResult) / sizeof(WCHAR)
                        );

                    if (lstrcmpiW(szResult, L"已签名") == 0)
                    {
                        lpNMCustomDraw->clrText = RGB(0, 0, 255);
                    }

                    if (lpNMCustomDraw->nmcd.dwItemSpec % 2)
                    {
                        lpNMCustomDraw->clrTextBk = RGB(240, 240, 230);
                    }
                    else
                    {
                        lpNMCustomDraw->clrTextBk = RGB(255, 255, 255);
                    }

                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                }
                else
                {
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                }

                bDlgProcResult = TRUE;
            }
            else if (lpNmHdr->code == LVN_COLUMNCLICK)
            {
                static int nRet = 1;
                LPNMLISTVIEW lpNmListView = (LPNMLISTVIEW)lpNmHdr;

                nRet = -nRet;

                ListCtrlSortStruct lcss;

                lcss.nRet = nRet;
                lcss.hListCtrl = hFileList;
                lcss.nSubItem = lpNmListView->iSubItem;

                LVITEMW item = {LVIF_PARAM};

                for (int nIndex = 0; nIndex < ListView_GetItemCount(hFileList); nIndex += 1)
                {
                    item.iItem = nIndex;
                    item.lParam = nIndex;

                    ListView_SetItem(hFileList, &item);
                }

                ListView_SortItems(hFileList, ListCtrlCompareProc, &lcss);
            }
            else if (lpNmHdr->code == NM_DBLCLK)
            {
                LPNMITEMACTIVATE lpNmItemActivate = (LPNMITEMACTIVATE)lParam;
                WCHAR szFilePath[MAX_PATH] = L"";

                ListView_GetItemText(hFileList, lpNmItemActivate->iItem, LVH_PATH, szFilePath, sizeof(szFilePath) / sizeof(WCHAR));

                if (PathFileExistsW(szFilePath))
                {
                    BrowseForFile(szFilePath);
                }
                else
                {
                    MessageBoxW(hwndDlg, L"文件不存在，无法定位。", NULL, MB_ICONERROR);
                }
            }
            else if (lpNmHdr->code == NM_RCLICK)
            {
                LPNMITEMACTIVATE lpNmItemActivate = (LPNMITEMACTIVATE)lParam;

                if (lpNmItemActivate->iItem >= 0)
                {
                    enum
                    {
                        IDM_BROWSE_FILE = 1000,
                        IDM_REMOVE_SIGNATURE,
                    };
                    POINT pt;
                    int nSelCount = ListView_GetSelectedCount(hFileList);

                    GetCursorPos(&pt);

                    switch (CSlxQuickPopupMenu()
                            .AddItemIf(L"定位到文件(&B)", IDM_BROWSE_FILE, FALSE, FALSE, FALSE, nSelCount == 1)
                            .AddItemIf(L"去除数字签名(&R)", IDM_REMOVE_SIGNATURE, FALSE, FALSE, FALSE, nSelCount > 0)
                            .Popup(hFileList, pt.x, pt.y))
                    {
                    case IDM_BROWSE_FILE:
                        lpNmHdr->code = NM_DBLCLK;
                        SendMessageW(hwndDlg, WM_NOTIFY, wParam, lParam);
                        lpNmHdr->code = NM_RCLICK;
                        break;

                    case IDM_REMOVE_SIGNATURE:
                        if (MessageBoxW(hwndDlg, L"要尝试去除这些文件的数字签名吗？如有需要请自行备份文件。", L"请确认", MB_ICONQUESTION | MB_YESNOCANCEL) == IDYES)
                        {
                            WCHAR szPath[MAX_PATH];
                            vector<wstring> vectorFilesFailed;
                            vector<wstring> vectorFilesSucced;

                            int pos = ListView_GetNextItem(hFileList, -1, LVNI_SELECTED);
                            while (pos >= 0)
                            {
                                ListView_GetItemText(hFileList, pos, LVH_PATH, szPath, RTL_NUMBER_OF(szPath));

                                if (!RemoveFileSignatrueArea(szPath))
                                {
                                    vectorFilesFailed.push_back(szPath);
                                }
                                else
                                {
                                    vectorFilesSucced.push_back(szPath);
                                }

                                pos = ListView_GetNextItem(hFileList, pos, LVNI_SELECTED);
                            }
                            
                            if (!vectorFilesFailed.empty())
                            {
                                wstring strText = L"以下文件操作失败:\r\n\r\n";
                                for (vector<wstring>::const_iterator it = vectorFilesFailed.begin(); it != vectorFilesFailed.end(); ++it)
                                {
                                    strText += *it;
                                    strText += L"\r\n";
                                }
                                MessageBoxW(hwndDlg, strText.c_str(), NULL, MB_ICONERROR);
                            }

                            // 更新界面
                            if (!vectorFilesSucced.empty())
                            {
                                map<wstring, HWND>* pMapPaths = new map<wstring, HWND>;
                                for (vector<wstring>::const_iterator it = vectorFilesSucced.begin(); it != vectorFilesSucced.end(); ++it)
                                {
                                    (*pMapPaths)[*it] = hwndDlg;
                                }

                                extern volatile HANDLE m_hManualCheckSignatureThread; //Slx
                                QueueUserAPC(
                                    (PAPCFUNC)ManualCheckSignatureThreadProc,
                                    m_hManualCheckSignatureThread,
                                    (ULONG_PTR)pMapPaths);
                            }
                        }
                        break;

                    default:
                        break;
                    }
                }
            }
        }
    }
    else if (uMsg == WM_SIZING)
    {
        LPRECT pRect = (LPRECT)lParam;
        const LONG lWidthLimit = 482;
        const LONG lHeightLimit = 306;

        if (pRect->right - pRect->left < lWidthLimit)
        {
            if (wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_LEFT)
            {
                pRect->left = pRect->right - lWidthLimit;
            }
            else
            {
                pRect->right = pRect->left + lWidthLimit;
            }
        }

        if (pRect->bottom - pRect->top < lHeightLimit)
        {
            if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOP)
            {
                pRect->top = pRect->bottom - lHeightLimit;
            }
            else
            {
                pRect->bottom = pRect->top + lHeightLimit;
            }
        }
    }
    else if (uMsg == WM_SIZE)
    {
        RECT rectClient;

        GetClientRect(hwndDlg, &rectClient);
        InflateRect(&rectClient, -9, -9);

        MoveWindow(
            hFileList,
            rectClient.left,
            rectClient.top,
            rectClient.right - rectClient.left,
            rectClient.bottom - rectClient.top,
            TRUE
            );
    }
    else if (uMsg == WM_SYSCOMMAND)
    {
        if (wParam == SC_CLOSE)
        {
            KillTimer(hwndDlg, 1);
            EndDialog(hwndDlg, 0);
        }
    }
    else if (uMsg == WM_SIGNRESULT)
    {
        WCHAR szPath[MAX_PATH];
        WCHAR szResult[100];
        HWND hFileList = GetDlgItem(hwndDlg, IDC_FILELIST);
        int nCount = ListView_GetItemCount(GetDlgItem(hwndDlg, IDC_FILELIST));

        //更新列表状态
        for (int nIndex = 0; nIndex < nCount; nIndex += 1)
        {
            ListView_GetItemText(hFileList, nIndex, LVH_PATH, szPath, RTL_NUMBER_OF(szPath));

            if (lstrcmpiW((LPCWSTR)wParam, szPath) == 0)
            {
                if ((BOOL)lParam)
                {
                    lstrcpynW(szResult, L"已签名", sizeof(szResult) / sizeof(WCHAR));
                }
                else
                {
                    lstrcpynW(szResult, L"未签名", sizeof(szResult) / sizeof(WCHAR));
                }

                ListView_SetItemText(hFileList, nIndex, LVH_RESULT, szResult);
                ListView_Update(hFileList, nIndex);

                break;
            }
        }
    }

    return bDlgProcResult;
}
