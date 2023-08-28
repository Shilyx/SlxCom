#include "SlxComWork_sd.h"
#include <Shlwapi.h>
#include <shldisp.h>

static WNDPROC lpOldStartButtonProc = NULL;

static LRESULT WINAPI NewStartButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_PAINT)
    {
        LRESULT lResult = CallWindowProc(lpOldStartButtonProc, hwnd, uMsg, wParam, lParam);

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

    return CallWindowProc(lpOldStartButtonProc, hwnd, uMsg, wParam, lParam);
}

DWORD CALLBACK WaitAndHookShowDesktop(LPVOID lpParam)
{
    HWND hTrayNotifyWnd = NULL;
    MSG msg;

    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    INT_PTR nTimerId = SetTimer(NULL, 0, 700, NULL);

    while(TRUE)
    {
        int nRet = GetMessageW(&msg, NULL, 0, 0);

        if(nRet <= 0)
        {
            break;
        }

        if(msg.message == WM_TIMER)
        {
            if(!IsWindow(hTrayNotifyWnd))
            {
                //设定托盘承载窗口的新窗口过程
                HWND hParentWnd = FindWindowExW(NULL, NULL, L"Shell_TrayWnd", NULL);
                HWND hChildWindow = NULL;

                while(IsWindow(hParentWnd))
                {
                    DWORD dwProcessId = 0;

                    GetWindowThreadProcessId(hParentWnd, &dwProcessId);

                    if(dwProcessId == GetCurrentProcessId())
                    {
                        hChildWindow = FindWindowExW(hParentWnd, NULL, L"TrayNotifyWnd", NULL);

                        if(IsWindow(hChildWindow))
                        {
                            break;
                        }
                    }

                    hParentWnd = FindWindowExW(NULL, hParentWnd, L"Shell_TrayWnd", NULL);
                }

                if(IsWindow(hParentWnd))
                {
                    hTrayNotifyWnd = hChildWindow;

                    lpOldStartButtonProc = (WNDPROC)SetWindowLongPtr(hTrayNotifyWnd, GWLP_WNDPROC, (LONG_PTR)NewStartButtonProc);
                }
            }
            else
            {
                //调整“开始”按钮的大小
                HWND hParent = GetParent(hTrayNotifyWnd);

                if(IsWindow(hParent))
                {
                    HWND hStart = FindWindowExW(hParent, NULL, L"Button", NULL);

                    if(IsWindow(hStart))
                    {
                        RECT rectParent, rectStart;

                        GetWindowRect(hParent, &rectParent);
                        GetWindowRect(hStart, &rectStart);

                        ScreenToClient(hParent, (LPPOINT)&rectStart + 0);
                        ScreenToClient(hParent, (LPPOINT)&rectStart + 1);

                        if(rectStart.bottom - rectStart.top != rectParent.bottom - rectParent.top)
                        {
                            MoveWindow(
                                hStart,
                                rectStart.left,
                                rectStart.top,
                                rectStart.right - rectStart.left,
                                rectParent.bottom - rectParent.top,
                                TRUE
                                );
                        }
                    }
                }
            }
        }
    }

    KillTimer(NULL, nTimerId);

    return 0;
}

BOOL StartHookShowDesktop()
{
    WCHAR szExePath[MAX_PATH];
    WCHAR szExplorerPath[MAX_PATH];

    GetModuleFileNameW(GetModuleHandleW(NULL), szExePath, MAX_PATH);

    GetSystemDirectoryW(szExplorerPath, MAX_PATH);
    PathAppendW(szExplorerPath, L"..\\Explorer.exe");

    if(lstrcmpiW(szExplorerPath, szExePath) == 0)
    {
        HANDLE hThread = CreateThread(NULL, 0, WaitAndHookShowDesktop, NULL, 0, NULL);
        CloseHandle(hThread);
    }

    return TRUE;
}
