#include "SlxWork.h"
#include <CommCtrl.h>
#include <shlwapi.h>

#pragma comment(lib, "ComCtl32.lib")

LPCTSTR szClassName = TEXT("_______Slx___RnMgr___65");
WNDPROC lpOldProc = NULL;

LRESULT WINAPI NewEditWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == LVM_GETEDITCONTROL)
    {
        HWND hEdit = (HWND)CallWindowProc(lpOldProc, hwnd, uMsg, wParam, lParam);

        int nLength = SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0);

        if(nLength > 0 && nLength < MAX_PATH)
        {
            TCHAR szText[MAX_PATH];

            if(GetWindowText(hEdit, szText, MAX_PATH) == nLength)
            {
                int nIndex = nLength;

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

    return CallWindowProc(lpOldProc, hwnd, uMsg, wParam, lParam);
}

BOOL LvmInit(HINSTANCE hInstance)
{
    if(lpOldProc == NULL)
    {
        WNDCLASSEX wcex = {sizeof(wcex)};

        TCHAR szText[300] = TEXT("Slx::RnMgr::LvmInit Called By ");

        GetModuleFileName(GetModuleHandle(NULL), szText + lstrlen(szText), MAX_PATH);
        lstrcat(szText, TEXT("\r\n"));

        OutputDebugString(szText);

        InitCommonControls();

//         wcex.hInstance = hInstance;
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
                lpOldProc = (WNDPROC)GetClassLongPtr(hList, GCLP_WNDPROC);

               SetClassLongPtr(hList, GCLP_WNDPROC, (LONG_PTR)NewEditWindowProc);

//                 TCHAR szText[1000];
//                 wsprintf(szText, TEXT("ÀÏ£º%x ÐÂ£º%x"), lpOldProc, GetClassLongPtr(hList, GCLP_WNDPROC));
//                 OutputDebugString(szText);
            }

            DestroyWindow(hWindow);
        }
    }

    return FALSE;
}

BOOL StartHookShowDesktop()
{
    TCHAR szExePath[MAX_PATH];
    TCHAR szExplorerPath[MAX_PATH];

    GetModuleFileName(GetModuleHandle(NULL), szExePath, MAX_PATH);

    GetSystemDirectory(szExplorerPath, MAX_PATH);
    PathAppend(szExplorerPath, TEXT("..\\Explorer.exe"));

    if(lstrcmpi(szExplorerPath, szExePath) == 0)
    {

    }

    return TRUE;
}

BOOL SlxWork(HINSTANCE hinstDll)
{
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    if(osi.dwMajorVersion <= 5)
    {
        StartHookShowDesktop();
        LvmInit(hinstDll);
    }

    return TRUE;
}