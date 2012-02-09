#include "SlxComWork_lvm.h"
#include <CommCtrl.h>

#pragma comment(lib, "ComCtl32.lib")

LPCTSTR szClassName = TEXT("_______Slx___RnMgr___65_1");
static WNDPROC lpOldEditProc = NULL;

LRESULT WINAPI NewEditWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == LVM_GETEDITCONTROL)
    {
        HWND hEdit = (HWND)CallWindowProc(lpOldEditProc, hwnd, uMsg, wParam, lParam);
        LONG_PTR nLength = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);

        if(nLength > 0 && nLength < MAX_PATH)
        {
            TCHAR szText[MAX_PATH];

            if(GetWindowText(hEdit, szText, MAX_PATH) == nLength)
            {
                LONG_PTR nIndex = nLength;

                while(nIndex > 0)
                {
                    if(szText[nIndex] == TEXT('.'))
                    {
                        PostMessage(hEdit, EM_SETSEL, 0, nIndex);

                        break;
                    }

                    nIndex -= 1;
                }
            }
        }

        return (LRESULT)hEdit;
    }

    return CallWindowProc(lpOldEditProc, hwnd, uMsg, wParam, lParam);
}

BOOL LvmInit(HINSTANCE hInstance)
{
    if(lpOldEditProc == NULL)
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
            HWND hList = CreateWindowEx(0, TEXT("SysListView32"), NULL, WS_CHILD, 0, 0, 10, 10,
                hWindow, NULL, NULL, NULL);

            if(IsWindow(hList))
            {
                lpOldEditProc = (WNDPROC)GetClassLongPtr(hList, GCLP_WNDPROC);

                SetClassLongPtr(hList, GCLP_WNDPROC, (LONG_PTR)NewEditWindowProc);

                TCHAR szText[1000];
                wsprintf(szText, TEXT("SlxCom::ÀÏ£º%x ÐÂ£º%x"), lpOldEditProc, GetClassLongPtr(hList, GCLP_WNDPROC));
                OutputDebugString(szText);
            }

            DestroyWindow(hWindow);
        }
    }

    return FALSE;
}
