#include <Windows.h>
#include <Shlwapi.h>
#include "SlxComKeyMapper.h"

extern HINSTANCE g_hinstDll;
static HHOOK gs_llKeyboardHook = NULL;
static DWORD gs_dwKeyboardTime = 0;

static LRESULT CALLBACK _LLKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        WCHAR lpMessage[128] = { 0 };
        wnsprintfW(lpMessage, RTL_NUMBER_OF(lpMessage), L"key pressed, virtual code: %d", p->vkCode);
        OutputDebugStringW(lpMessage);

        WCHAR lpVal[32] = { 0 };
        wsprintfW(lpVal, L"%d", p->vkCode);
        DWORD dwType;
        DWORD dwData;
        DWORD dwSize = sizeof(dwData);
        if (ERROR_SUCCESS == SHGetValueW(HKEY_CURRENT_USER, L"Software\\Shilyx Studio\\SlxCom\\KeyMapper", lpVal, &dwType, &dwData, &dwSize)) {
            if (dwType == REG_DWORD) {
                wnsprintfW(lpMessage, RTL_NUMBER_OF(lpMessage), L"map key code %d->%d", p->vkCode, dwData);
                OutputDebugStringW(lpMessage);

                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = (WORD)dwData;
                input.ki.dwFlags = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) ? KEYEVENTF_KEYUP : 0;
                SendInput(1, &input, sizeof(INPUT));

                return 1; // 阻止事件继续传递
            }
        }
    }

    return CallNextHookEx(0, nCode, wParam, lParam);
}

static LRESULT CALLBACK _WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (WM_CREATE == uMsg) {
        gs_llKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, _LLKeyboardHook, (HINSTANCE)g_hinstDll, 0);
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static DWORD WINAPI _LLKeyboardHookThread(void* param) {
#define __CLS_NAME                      L"{FA9429D4-E65F-4c5a-8B45-88E0387D4A5A}"
    WNDCLASSEXW wcex = { sizeof(wcex) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = (WNDPROC)_WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = g_hinstDll;
    wcex.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = __CLS_NAME;

    RegisterClassExW(&wcex);
    HWND hWnd = CreateWindowExW(0, __CLS_NAME, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, g_hinstDll, NULL);
    if (!IsWindow(hWnd)) { return 1; }
    UpdateWindow(hWnd);
    
    MSG msg;
    while (GetMessage(&msg, hWnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}

void StartKeyMapper() {
    static BOOL s_bFirst = TRUE;
    if (s_bFirst) {
        s_bFirst = FALSE;
        CloseHandle(CreateThread(NULL, 0, _LLKeyboardHookThread, NULL, 0, NULL));
    }
}
