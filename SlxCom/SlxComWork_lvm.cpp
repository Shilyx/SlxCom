#include "SlxComWork_lvm.h"
#include <CommCtrl.h>
#include <Shlwapi.h>

#pragma comment(lib, "ComCtl32.lib")

LPCTSTR szClassName = TEXT("_______Slx___RnMgr___65_1");
static WNDPROC lpOldEditProc = NULL;
static WNDPROC lpOldListViewProc = NULL;

int GetDotIndex(HWND hEdit)
{
    int nRet = -1;

    if (IsWindow(hEdit))
    {
        LPCTSTR lpMark = TEXT(".tar.gz");
        TCHAR szWindowText[1000] = TEXT("");
        int nTextLength = GetWindowText(hEdit, szWindowText, RTL_NUMBER_OF(szWindowText));
        int nMarkLength = lstrlen(lpMark);

        if(nTextLength > nMarkLength)
        {
            if(lstrcmpi(szWindowText + nTextLength - nMarkLength, lpMark) == 0)
            {
                nRet = nTextLength - nMarkLength;
            }
        }

        if(nRet == -1)
        {
            while(nTextLength > 0)
            {
                if(szWindowText[nTextLength] == TEXT('.'))
                {
                    nRet = nTextLength;
                    break;
                }

                nTextLength -= 1;
            }
        }
    }

    return nRet;
}

LRESULT WINAPI NewEditWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_F2)
        {
            HWND hParent = GetParent(hwnd);
            TCHAR szParentClassName[100] = TEXT("");

            if (IsWindow(hParent))
            {
                GetClassName(hParent, szParentClassName, RTL_NUMBER_OF(szParentClassName));

                if (lstrcmpi(szParentClassName, TEXT("SysListView32")) == 0 ||
                    lstrcmpi(szParentClassName, TEXT("CtrlNotifySink")) == 0
                    )
                {
                    int nDotIndex = GetDotIndex(hwnd);

                    if (nDotIndex != -1)
                    {
                        DWORD_PTR dwUserData = GetWindowLongPtr(hwnd, GWLP_USERDATA) % 4;

                        switch (dwUserData)
                        {
                        case 0:
                            SendMessage(hwnd, EM_SETSEL, 0, -1);
                            break;
                        case 1:
                            SendMessage(hwnd, EM_SETSEL, nDotIndex, -1);
                            break;
                        case 2:
                            SendMessage(hwnd, EM_SETSEL, nDotIndex + 1, -1);
                            break;
                        case 3:
                            SendMessage(hwnd, EM_SETSEL, 0, nDotIndex);
                            break;
                        default:
                            break;
                        }

                        dwUserData += 1;
                        SetWindowLongPtr(hwnd, GWLP_USERDATA, dwUserData);
                    }
                }
            }
        }
    }

    return CallWindowProc(lpOldEditProc, hwnd, uMsg, wParam, lParam);
}

LRESULT WINAPI NewListViewWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == LVM_GETEDITCONTROL)
    {
        HWND hEdit = (HWND)CallWindowProc(lpOldListViewProc, hwnd, uMsg, wParam, lParam);
        LONG_PTR nLength = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);

        if(nLength > 0 && nLength < MAX_PATH)
        {
            TCHAR szText[MAX_PATH];

            if(GetWindowText(hEdit, szText, MAX_PATH) == nLength)
            {
                BOOL bToPost = FALSE;
                LPCTSTR lpMark = TEXT(".tar.gz");
                LONG_PTR nIndex = nLength;
                LONG_PTR nMarkLength = lstrlen(lpMark);

                if(nLength > nMarkLength)
                {
                    if(lstrcmpi(szText + nLength - nMarkLength, lpMark) == 0)
                    {
                        nIndex = nLength - nMarkLength;
                        bToPost = TRUE;
                    }
                }

                if(nIndex == nLength)
                {
                    while(nIndex > 0)
                    {
                        if(szText[nIndex] == TEXT('.'))
                        {
                            bToPost = TRUE;
                            break;
                        }

                        nIndex -= 1;
                    }
                }

                if(bToPost)
                {
                    PostMessage(hEdit, EM_SETSEL, 0, nIndex);
                }
            }
        }

        return (LRESULT)hEdit;
    }

    return CallWindowProc(lpOldListViewProc, hwnd, uMsg, wParam, lParam);
}

BOOL LvmInit(HINSTANCE hInstance, BOOL bIsXpOrEarlier)
{
    if(lpOldListViewProc == NULL)
    {
        WNDCLASSEX wcex = {sizeof(wcex)};

        TCHAR szText[300] = TEXT("SlxCom::LvmInit Called By ");

        GetModuleFileName(GetModuleHandle(NULL), szText + lstrlen(szText), MAX_PATH);
        lstrcat(szText, TEXT("\r\n"));

        OutputDebugString(szText);

        InitCommonControls();

        wcex.hInstance = hInstance;
        wcex.lpszClassName = szClassName;
        wcex.lpfnWndProc = DefWindowProc;

        RegisterClassEx(&wcex);

        HWND hWindow = CreateWindowEx(0, szClassName, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
            NULL, NULL, NULL, NULL);

        if(IsWindow(hWindow))
        {
            HWND hEdit = CreateWindowEx(0, TEXT("Edit"), NULL, WS_CHILD, 10, 10, 10, 10, hWindow, NULL, NULL, NULL);

            if (IsWindow(hEdit))
            {
                lpOldEditProc = (WNDPROC)GetClassLongPtr(hEdit, GCLP_WNDPROC);

                SetClassLongPtr(hEdit, GCLP_WNDPROC, (LONG_PTR)NewEditWindowProc);

                TCHAR szText[1000];
                wsprintf(szText, TEXT("SlxCom::Edit:老：%x 新：%x"), lpOldEditProc, GetClassLongPtr(hEdit, GCLP_WNDPROC));
                OutputDebugString(szText);
            }

            if (bIsXpOrEarlier)
            {
                HWND hList = CreateWindowEx(0, TEXT("SysListView32"), NULL, WS_CHILD, 0, 0, 10, 10, hWindow, NULL, NULL, NULL);

                if(IsWindow(hList))
                {
                    lpOldListViewProc = (WNDPROC)GetClassLongPtr(hList, GCLP_WNDPROC);

                    SetClassLongPtr(hList, GCLP_WNDPROC, (LONG_PTR)NewListViewWindowProc);

                    TCHAR szText[1000];
                    wsprintf(szText, TEXT("SlxCom::ListView:老：%x 新：%x"), lpOldListViewProc, GetClassLongPtr(hList, GCLP_WNDPROC));
                    OutputDebugString(szText);
                }
            }

            DestroyWindow(hWindow);
        }
    }

    return FALSE;
}
