#include "SlxComWork_lvm.h"
#include <CommCtrl.h>
#include <Shlwapi.h>

#pragma comment(lib, "ComCtl32.lib")

LPCWSTR szClassName = L"_______Slx___RnMgr___65_1";
static WNDPROC lpOldEditProc = NULL;
static WNDPROC lpOldListViewProc = NULL;

INT_PTR GetDotIndex(HWND hEdit)
{
    if (IsWindow(hEdit))
    {
        WCHAR szWindowText[1000] = L"";
        int nTextLength = GetWindowTextW(hEdit, szWindowText, RTL_NUMBER_OF(szWindowText));

        INT_PTR nTar = (INT_PTR)StrRStrIW(szWindowText, NULL, L".tar.");
        INT_PTR nDot = (INT_PTR)StrRChrW(szWindowText, NULL, L'.');

        if (nTar != NULL && nDot - nTar == 4 * sizeof(WCHAR))
        {
            return (nTar - (INT_PTR)szWindowText) / sizeof(WCHAR);
        }

        if (nDot != NULL)
        {
            return (nDot - (INT_PTR)szWindowText) / sizeof(WCHAR);
        }
    }

    return -1;
}

LRESULT WINAPI NewEditWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_F2)
        {
            HWND hParent = GetParent(hwnd);
            WCHAR szParentClassName[100] = L"";

            if (IsWindow(hParent))
            {
                GetClassNameW(hParent, szParentClassName, RTL_NUMBER_OF(szParentClassName));

                if (lstrcmpiW(szParentClassName, L"SysListView32") == 0 ||
                    lstrcmpiW(szParentClassName, L"CtrlNotifySink") == 0
                    )
                {
                    INT_PTR nDotIndex = GetDotIndex(hwnd);

                    if (nDotIndex != -1)
                    {
                        DWORD_PTR dwUserData = GetWindowLongPtr(hwnd, GWLP_USERDATA) % 4;

                        switch (dwUserData)
                        {
                        case 0:
                            SendMessageW(hwnd, EM_SETSEL, 0, -1);
                            break;
                        case 1:
                            SendMessageW(hwnd, EM_SETSEL, nDotIndex, -1);
                            break;
                        case 2:
                            SendMessageW(hwnd, EM_SETSEL, nDotIndex + 1, -1);
                            break;
                        case 3:
                            SendMessageW(hwnd, EM_SETSEL, 0, nDotIndex);
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
        INT_PTR nDotIndex = GetDotIndex(hEdit);

        if (nDotIndex != -1)
        {
            PostMessageW(hEdit, EM_SETSEL, 0, nDotIndex);
        }

        return (LRESULT)hEdit;
    }

    return CallWindowProc(lpOldListViewProc, hwnd, uMsg, wParam, lParam);
}

BOOL LvmInit(HINSTANCE hInstance, BOOL bIsXpOrEarlier)
{
    if(lpOldListViewProc == NULL)
    {
        WNDCLASSEXW wcex = {sizeof(wcex)};

        WCHAR szText[300] = L"SlxCom::LvmInit Called By ";

        GetModuleFileNameW(GetModuleHandleW(NULL), szText + lstrlenW(szText), MAX_PATH);
        lstrcatW(szText, L"\r\n");

        OutputDebugStringW(szText);

        InitCommonControls();

        wcex.hInstance = hInstance;
        wcex.lpszClassName = szClassName;
        wcex.lpfnWndProc = DefWindowProc;

        RegisterClassExW(&wcex);

        HWND hWindow = CreateWindowExW(0, szClassName, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
            NULL, NULL, NULL, NULL);

        if(IsWindow(hWindow))
        {
            HWND hEdit = CreateWindowExW(0, L"Edit", NULL, WS_CHILD, 10, 10, 10, 10, hWindow, NULL, NULL, NULL);

            if (IsWindow(hEdit))
            {
                lpOldEditProc = (WNDPROC)GetClassLongPtr(hEdit, GCLP_WNDPROC);

                SetClassLongPtr(hEdit, GCLP_WNDPROC, (LONG_PTR)NewEditWindowProc);

//                 WCHAR szText[1000];
//                 wsprintf(szText, L"SlxCom::Edit:老：%x 新：%x", lpOldEditProc, GetClassLongPtr(hEdit, GCLP_WNDPROC));
//                 OutputDebugStringW(szText);
            }

            if (bIsXpOrEarlier)
            {
                HWND hList = CreateWindowExW(0, L"SysListView32", NULL, WS_CHILD, 0, 0, 10, 10, hWindow, NULL, NULL, NULL);

                if(IsWindow(hList))
                {
                    lpOldListViewProc = (WNDPROC)GetClassLongPtr(hList, GCLP_WNDPROC);

                    SetClassLongPtr(hList, GCLP_WNDPROC, (LONG_PTR)NewListViewWindowProc);

//                     WCHAR szText[1000];
//                     wsprintf(szText, L"SlxCom::ListView:老：%x 新：%x", lpOldListViewProc, GetClassLongPtr(hList, GCLP_WNDPROC));
//                     OutputDebugStringW(szText);
                }
            }

            DestroyWindow(hWindow);
        }
    }

    return FALSE;
}
