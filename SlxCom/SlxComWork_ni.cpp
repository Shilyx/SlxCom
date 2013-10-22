#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "resource.h"

#define NOTIFYWNDCLASS  TEXT("__slxcom_notify_wnd")
#define WM_CALLBACK     (WM_USER + 112)

static LRESULT __stdcall NotifyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static NOTIFYICONDATA nid = {sizeof(nid)};

    if (uMsg == WM_CREATE)
    {
        LPCREATESTRUCT lpCs = (LPCREATESTRUCT)lParam;

        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uCallbackMessage = WM_CALLBACK;
        nid.hIcon = LoadIcon(lpCs->hInstance, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        nid.dwState = NIF_TIP | NIF_MESSAGE | NIF_ICON;
        lstrcpyn(nid.szTip, TEXT("SlxCom"), sizeof(nid.szTip));

        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    else if (uMsg == WM_CLOSE)
    {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        DestroyWindow(hWnd);
    }
    else if (uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static HWND GetTrayNotifyWndInProcess()
{
    DWORD dwPid = GetCurrentProcessId();
    HWND hShell_TrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);

    while (IsWindow(hShell_TrayWnd))
    {
        DWORD dwWindowPid = 0;

        GetWindowThreadProcessId(hShell_TrayWnd, &dwWindowPid);

#ifndef _DEBUG
        if (dwWindowPid == dwPid)
#endif
        {
            HWND hTrayNotifyWnd = FindWindowEx(hShell_TrayWnd, NULL, TEXT("TrayNotifyWnd"), NULL);

            if (IsWindow(hTrayNotifyWnd))
            {
                return hTrayNotifyWnd;
            }
        }

        hShell_TrayWnd = FindWindowEx(NULL, hShell_TrayWnd, TEXT("Shell_TrayWnd"), NULL);
    }

    return NULL;
}

static DWORD __stdcall NotifyIconManagerProc(LPVOID lpParam)
{
    DWORD dwSleepTime = 1;
    while (!IsWindow(GetTrayNotifyWndInProcess()))
    {
        Sleep((dwSleepTime++) * 1000);
    }

    WNDCLASSEX wcex = {sizeof(wcex)};
    wcex.lpfnWndProc = NotifyWndProc;
    wcex.lpszClassName = NOTIFYWNDCLASS;
    wcex.hInstance = (HINSTANCE)lpParam;
    wcex.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&wcex);

    HWND hMainWnd = CreateWindowEx(
        0,
        NOTIFYWNDCLASS,
        NOTIFYWNDCLASS,
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        333,
        333,
        NULL,
        NULL,
        (HINSTANCE)lpParam,
        NULL
        );

    ShowWindow(hMainWnd, SW_SHOW);
    UpdateWindow(hMainWnd);

    MSG msg;

    while (TRUE)
    {
        int nRet = GetMessage(&msg, NULL, 0, 0);

        if (nRet <= 0)
        {
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void StartNotifyIconManager(HINSTANCE hinstDll)
{
    if (!IsExplorer())
    {
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, NotifyIconManagerProc, (LPVOID)hinstDll, 0, NULL);
    CloseHandle(hThread);
}

void __stdcall T3(HWND hwndStub, HINSTANCE hAppInstance, LPTSTR lpszCmdLine, int nCmdShow)
{
    extern HINSTANCE g_hinstDll; //SlxCom.cpp
    NotifyIconManagerProc((LPVOID)g_hinstDll);
}