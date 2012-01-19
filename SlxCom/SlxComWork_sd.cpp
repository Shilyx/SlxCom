#include "SlxComWork_sd.h"
#include <Shlwapi.h>
#include <shldisp.h>

static WNDPROC lpOldWndProc = NULL;

LRESULT WINAPI NewWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_PAINT)
    {
        LRESULT lResult = CallWindowProc(lpOldWndProc, hwnd, uMsg, wParam, lParam);

        HDC hDc = GetDC(hwnd);

        if(hDc != NULL)
        {
            RECT rect;
            HPEN hPen = CreatePen(PS_SOLID, 20, RGB(36, 93, 219));
            HPEN hTempPen = (HPEN)SelectObject(hDc, hPen);

            GetClientRect(hwnd, &rect);

            MoveToEx(hDc, rect.right - 10, 0, NULL);
            LineTo(hDc, rect.right - 10, rect.bottom);

            SelectObject(hDc, hTempPen);
            DeleteObject(hPen);

            ReleaseDC(hwnd, hDc);
        }

        return lResult;
    }
    else if(uMsg == WM_LBUTTONDOWN)
    {
        RECT rect;
        int x = LOWORD(lParam);

        GetClientRect(hwnd, &rect);

        if(x < rect.right - rect.left && x > rect.right - rect.left - 10)
        {
            IShellDispatch4 *pDisp = NULL;

            if(SUCCEEDED(CoCreateInstance(CLSID_Shell, NULL, CLSCTX_ALL, IID_IShellDispatch4, (void **)&pDisp)))
            {
                pDisp->ToggleDesktop();

                pDisp->Release();
            }
        }
    }

    return CallWindowProc(lpOldWndProc, hwnd, uMsg, wParam, lParam);
}

DWORD __stdcall WaitAndHookShowDesktop(LPVOID lpParam)
{
    HWND hTrayNotifyWnd = NULL;
    MSG msg;

    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    int nTimerId = SetTimer(NULL, 0, 700, NULL);

    while(TRUE)
    {
        int nRet = GetMessage(&msg, NULL, 0, 0);

        if(nRet <= 0)
        {
            break;
        }

        if(msg.message == WM_TIMER)
        {
            if(!IsWindow(hTrayNotifyWnd))
            {
                HWND hParentWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);
                HWND hChildWindow = NULL;

                while(IsWindow(hParentWnd))
                {
                    DWORD dwProcessId = 0;

                    GetWindowThreadProcessId(hParentWnd, &dwProcessId);

                    if(dwProcessId == GetCurrentProcessId())
                    {
                        hChildWindow = FindWindowEx(hParentWnd, NULL, TEXT("TrayNotifyWnd"), NULL);

                        if(IsWindow(hChildWindow))
                        {
                            break;
                        }
                    }

                    hParentWnd = FindWindowEx(NULL, hParentWnd, TEXT("Shell_TrayWnd"), NULL);
                }

                if(IsWindow(hParentWnd))
                {
                    hTrayNotifyWnd = hChildWindow;

                    lpOldWndProc = (WNDPROC)SetWindowLongPtr(hTrayNotifyWnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
                }
            }
        }
    }

    KillTimer(NULL, nTimerId);

    return 0;
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
        HANDLE hThread = CreateThread(NULL, 0, WaitAndHookShowDesktop, NULL, 0, NULL);
        CloseHandle(hThread);
    }

    return TRUE;
}
