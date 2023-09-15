#include "WindowManagerProcess.h"
#include "WindowManager.h"
#include "../SlxComTools.h"
#include <Windows.h>
#include <Shlwapi.h>

#define CLASS_NAME L"__slx_WindowManagerProcess_140829"

extern HINSTANCE g_hinstDll; // SlxCom.cpp
static DWORD gs_dwSlxComWindowManagerWindowProcessId = 0;
static HWND gs_hSlxComWindowManagerWindow = NULL;
static HWND gs_hMainWindow = NULL;

enum
{
    WM_TASK = WM_USER + 112,
};

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION &&
        (wParam == WM_RBUTTONUP || wParam == WM_RBUTTONDOWN) &&
        lParam != 0 &&
        GetKeyState(VK_CONTROL) < 0 &&
        IsWindow(gs_hSlxComWindowManagerWindow) &&
        gs_dwSlxComWindowManagerWindowProcessId != 0 &&
        IsWindow(gs_hMainWindow))
    {
        LPMSLLHOOKSTRUCT lpMhs = (LPMSLLHOOKSTRUCT)lParam;
        HWND hTargetWindow = WindowFromPoint(lpMhs->pt);

        if (IsWindow(hTargetWindow) && gs_hMainWindow != hTargetWindow)
        {
            if (wParam == WM_RBUTTONUP)
            {
                PostMessageW(gs_hMainWindow, WM_TASK, (WPARAM)hTargetWindow, MAKELONG(lpMhs->pt.x, lpMhs->pt.y));
            }

            return 1;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void DoTask(HWND hTargetWindow, int x, int y)
{
    WCHAR szClassName[1024] = L"";
    WCHAR szWindowText[1024] = L"";

    GetClassNameW(hTargetWindow, szClassName, RTL_NUMBER_OF(szClassName));
    GetWindowTextW(hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));

    SafeDebugMessage(L"%x[%s][%s], %d,%d\r\n", hTargetWindow, szClassName, szWindowText, x, y);

    if (!IsWindow(FindWindowW(L"#32768", NULL)) &&
        !IsWindowFullScreen(hTargetWindow))
    {
        LONG_PTR dwStyle = GetWindowLongPtr(hTargetWindow, GWL_STYLE);

        if ((dwStyle & WS_CHILDWINDOW) == 0)
        {
            if (AdvancedSetForegroundWindow(gs_hMainWindow))
            {
                if (AllowSetForegroundWindow(gs_dwSlxComWindowManagerWindowProcessId))
                {
                    PostMessageW(gs_hSlxComWindowManagerWindow, WM_SCREEN_CONTEXT_MENU, (WPARAM)hTargetWindow, MAKELONG(x, y));
                }
            }
        }
    }
}

static LRESULT CALLBACK HookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        break;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
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

static HWND CreateOneWindow(LPCWSTR lpWindowText)
{
    HWND hMainWnd = CreateWindowExW(
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
//     ShowWindow(hMainWnd, SW_SHOW);
    UpdateWindow(hMainWnd);
#endif

    return hMainWnd;
}

void WINAPI SlxWindowManagerProcessW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    int nArgCount = 0;
    LPWSTR *lpArgValue = CommandLineToArgvW(lpszCmdLine, &nArgCount);

    if (nArgCount < 2)
    {
        return;
    }

    gs_hSlxComWindowManagerWindow = (HWND)StrToInt64Def(lpArgValue[1], 0);

#ifndef _DEBUG
    if (!IsWindow(gs_hSlxComWindowManagerWindow))
    {
        return;
    }
#endif

    GetWindowThreadProcessId(gs_hSlxComWindowManagerWindow, &gs_dwSlxComWindowManagerWindowProcessId);

    DWORD dwProcessId = StrToIntW(lpArgValue[0]);
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, dwProcessId);

#ifdef _DEBUG
    if (hProcess == NULL)
    {
        hProcess = CreateEventW(NULL, FALSE, FALSE, NULL);
    }
#endif

    if (hProcess == NULL)
    {
        return;
    }

    WNDCLASSEXW wcex = {sizeof(wcex)};
    wcex.lpfnWndProc = HookWndProc;
    wcex.lpszClassName = CLASS_NAME;
    wcex.hInstance = g_hinstDll;
    wcex.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wcex);

    gs_hMainWindow = CreateOneWindow(CLASS_NAME);

    DWORD dwThreadId = 0;
    HHOOK hHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, g_hinstDll, 0);
    UINT_PTR nTimerId = SetTimer(NULL, 0, 345, NULL);
    MSG msg;

    while (TRUE)
    {
        int nRet = GetMessageW(&msg, NULL, 0, 0);

        if (nRet <= 0)
        {
            break;
        }

        if (msg.message == WM_TIMER)
        {
            if (WAIT_OBJECT_0 == WaitForSingleObject(hProcess, 0))
            {
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(NULL, nTimerId);

    UnhookWindowsHookEx(hHook);
    CloseHandle(hProcess);
}