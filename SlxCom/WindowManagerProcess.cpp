#include "WindowManagerProcess.h"
#include "SlxComTools.h"
#include <Windows.h>
#include <Shlwapi.h>

#define CLASS_NAME TEXT("__slx_WindowManagerProcess_140829")

extern HINSTANCE g_hinstDll; // SlxCom.cpp
static HWND g_hSlxComWindowManagerWindow = NULL;
static HWND gs_hMainWindow = NULL;

static enum
{
    WM_TASK = WM_USER + 112,
};

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_RBUTTONUP && lParam != 0 && GetKeyState(VK_CONTROL) < 0)
    {
        LPMSLLHOOKSTRUCT lpMhs = (LPMSLLHOOKSTRUCT)lParam;
        HWND hTargetWindow = WindowFromPoint(lpMhs->pt);

        if (IsWindow(hTargetWindow) && IsWindow(gs_hMainWindow))
        {
            PostMessage(gs_hMainWindow, WM_TASK, (WPARAM)hTargetWindow, MAKELONG(lpMhs->pt.x, lpMhs->pt.y));
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void DoTask(HWND hTargetWindow, int x, int y)
{
    DWORD_PTR dwResult = 0;

    if (SendMessageTimeout(hTargetWindow, WM_NCHITTEST, 0, MAKELONG(x, y), SMTO_ABORTIFHUNG, 345, &dwResult))
    {
        if (dwResult == HTCAPTION)
        {
            SafeDebugMessage(TEXT("%x, %d,%d\r\n"), hTargetWindow, x, y);
        }
    }
}

static LRESULT CALLBACK HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        SetTimer(hWnd, 1, 345, NULL);
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;

    case WM_TASK:
        DoTask((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
        break;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static HWND CreateOneWindow(LPCTSTR lpWindowText)
{
    HWND hMainWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        lpWindowText,
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        333,
        55,
        NULL,
        NULL,
        (HINSTANCE)g_hinstDll,
        NULL
        );

#ifdef _DEBUG
    ShowWindow(hMainWnd, SW_SHOW);
    UpdateWindow(hMainWnd);
#endif

    return hMainWnd;
}

static DWORD CALLBACK HookThreadProc(LPVOID lpParam)
{
    HWND hMainWnd = (HWND)lpParam;
    HWND hSetHookWindow = CreateOneWindow(NULL);
    HHOOK hHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, g_hinstDll, 0);
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

    UnhookWindowsHookEx(hHook);

    return 0;
}

void WINAPI SlxWindowManagerProcessW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    int nArgCount = 0;
    LPWSTR *lpArgValue = CommandLineToArgvW(lpszCmdLine, &nArgCount);

    if (nArgCount < 2)
    {
        return;
    }

    HWND hNotifyWindow = (HWND)StrToInt64Def(lpArgValue[1], 0);

#ifndef _DEBUG
    if (!IsWindow(hNotifyWindow))
    {
        return;
    }
#endif

    DWORD dwProcessId = StrToInt(lpArgValue[0]);
    HANDLE hProcess = NULL;

#ifdef _DEBUG
    hProcess = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
    hProcess = OpenProcess(SYNCHRONIZE, FALSE, dwProcessId);
#endif

    if (hProcess == NULL)
    {
        return;
    }

    WNDCLASSEX wcex = {sizeof(wcex)};
    wcex.lpfnWndProc = HookWndProc;
    wcex.lpszClassName = CLASS_NAME;
    wcex.hInstance = g_hinstDll;
    wcex.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&wcex);

    gs_hMainWindow = CreateOneWindow(CLASS_NAME);

    DWORD dwThreadId = 0;
    HANDLE hThread = CreateThread(NULL, 0, HookThreadProc, NULL, 0, &dwThreadId);
    MSG msg;

    while (TRUE)
    {
        int nRet = GetMessage(&msg, NULL, 0, 0);

        if (nRet <= 0)
        {
            break;
        }

        if (nRet == WM_TIMER)
        {
            if (WAIT_OBJECT_0 == WaitForSingleObject(hProcess, 0))
            {
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    PostThreadMessage(dwThreadId, WM_QUIT, 0, 0);

    WaitForSingleObject(hThread, 333);
    TerminateThread(hThread, 0);

    CloseHandle(hThread);
    CloseHandle(hProcess);
}