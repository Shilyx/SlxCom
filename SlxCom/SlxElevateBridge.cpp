/*
 *  @file  : SlxElevateBridge.cpp
 *  @author: luteng
 *  @date  : 2015-08-07 17:41:24.764
 *  @note  : Generated by SlxTemplates
 */

#include "SlxElevateBridge.h"
#include <Shlwapi.h>

#define BridgeVersion L"1.0@20150807"
#define CLASS_NAME L"__SlxElevateBridgeWindow"

extern HINSTANCE g_hinstDll;    // SlxCom.cpp
extern OSVERSIONINFO g_osi; // SlxCom.cpp

enum {
    WM_TO_BRIDGE_INFO = WM_USER + 65,
};

struct TaskDiscription {
    WCHAR szCommand[MAX_PATH];
    WCHAR szArguments[4096];
    WCHAR szCurrentDirectory[MAX_PATH];
    int nShowCmd;
};

static BOOL NeedElevated() {
    if (g_osi.dwMajorVersion < 6) {
        return FALSE;
    }

    BOOL bIsElevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD dwReturnLength = 0;
        struct {
            DWORD TokenIsElevated;
        } te;

        if (GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)20, &te, sizeof(te), &dwReturnLength)) {
            if (dwReturnLength == sizeof(te)) {
                bIsElevated = !!te.TokenIsElevated;
            }
        }

        CloseHandle(hToken);
    }

    return !bIsElevated;
}

static HWND GetBridgeHandle() {
    return FindWindowW(CLASS_NAME, BridgeVersion);
}

static void GetTaskStorePath(const ULARGE_INTEGER &uliTaskId, WCHAR szPath[], int nBufferSize) {
    wnsprintfW(szPath, nBufferSize, L"Software\\slx_tmp\\tasks\\%s\\%08x_%08x", BridgeVersion, uliTaskId.HighPart, uliTaskId.LowPart);
}

static ULARGE_INTEGER MakeTask(LPCWSTR lpCommand, LPCWSTR lpArguments, LPCWSTR lpDirectory, int nShowCmd) {
    ULARGE_INTEGER uliTaskId = { 0 };
    GUID guid = { 0 };
    WCHAR szRegPath[1024];

    if (lpCommand == NULL) {
        lpCommand = L"";
    }

    if (lpArguments == NULL) {
        lpArguments = L"";
    }

    if (lpDirectory == NULL) {
        lpDirectory = L"";
    }

    UuidCreate(&guid);
    uliTaskId.HighPart = GetTickCount();
    uliTaskId.LowPart = guid.Data1;

    GetTaskStorePath(uliTaskId, szRegPath, RTL_NUMBER_OF(szRegPath));

    HKEY hKey = NULL;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, szRegPath, 0, NULL, REG_OPTION_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"cmd", 0, REG_SZ, (const BYTE *)lpCommand, (lstrlenW(lpCommand) + 1) * sizeof(WCHAR));
        RegSetValueExW(hKey, L"arg", 0, REG_SZ, (const BYTE *)lpArguments, (lstrlenW(lpArguments) + 1) * sizeof(WCHAR));
        RegSetValueExW(hKey, L"dir", 0, REG_SZ, (const BYTE *)lpDirectory, (lstrlenW(lpDirectory) + 1) * sizeof(WCHAR));
        RegSetValueExW(hKey, L"sw", 0, REG_DWORD, (const BYTE *)&nShowCmd, sizeof(nShowCmd));

        RegCloseKey(hKey);

        return uliTaskId;
    }

    return ULARGE_INTEGER();
}

static void DeleteTask(const ULARGE_INTEGER &uliTaskId) {
    WCHAR szRegPath[1024];

    GetTaskStorePath(uliTaskId, szRegPath, RTL_NUMBER_OF(szRegPath));
    RegDeleteKeyW(HKEY_CURRENT_USER, szRegPath);
}

int StartBridgeWithTaskId(const ULARGE_INTEGER &uliTaskId) {
    extern HINSTANCE g_hinstDll;

    WCHAR szRundll32[MAX_PATH] = L"";
    WCHAR szDllPath[MAX_PATH] = L"";
    WCHAR szArguments[MAX_PATH + 20] = L"";

    GetSystemDirectoryW(szRundll32, RTL_NUMBER_OF(szRundll32));
    PathAppendW(szRundll32, L"\\rundll32.exe");

    GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
    PathQuoteSpacesW(szDllPath);

    wnsprintfW(szArguments, RTL_NUMBER_OF(szArguments), L"%s SlxElevateBridge %lu %lu", szDllPath, uliTaskId.HighPart, uliTaskId.LowPart);

    return (int)(INT_PTR)ShellExecuteW(NULL, L"runas", szRundll32, szArguments, NULL, SW_SHOW);
}

int WINAPI ElevateAndRunA(LPCSTR lpCommand, LPCSTR lpArguments, LPCSTR lpDirectory, int nShowCmd) {
    WCHAR szCommand[MAX_PATH];
    WCHAR szArguments[4096];
    WCHAR szDirectory[MAX_PATH];

    wnsprintfW(szCommand, RTL_NUMBER_OF(szCommand), L"%hs", lpCommand);
    wnsprintfW(szArguments, RTL_NUMBER_OF(szArguments), L"%hs", lpArguments);
    wnsprintfW(szDirectory, RTL_NUMBER_OF(szDirectory), L"%hs", lpDirectory);

    return ElevateAndRunW(szCommand, szArguments, szDirectory, nShowCmd);
}

int WINAPI ElevateAndRunW(LPCWSTR lpCommand, LPCWSTR lpArguments, LPCWSTR lpDirectory, int nShowCmd) {
    static BOOL bNeedElevated = NeedElevated();

    if (!bNeedElevated) {
        return (int)(INT_PTR)ShellExecuteW(NULL, L"open", lpCommand, lpArguments, lpDirectory, nShowCmd);
    }

    ULARGE_INTEGER uliTaskId = MakeTask(lpCommand, lpArguments, lpDirectory, nShowCmd);

    if (uliTaskId.QuadPart == 0) {
        return SE_ERR_ACCESSDENIED;
    }

    HWND hBridge = GetBridgeHandle();

    if (IsWindow(hBridge)) {
        DWORD dwProcessId = 0;

        GetWindowThreadProcessId(hBridge, &dwProcessId);
        AllowSetForegroundWindow(dwProcessId);

        DWORD_PTR dwResult = 0;

        if (SendMessageTimeout(hBridge, WM_TO_BRIDGE_INFO, uliTaskId.HighPart, uliTaskId.LowPart, SMTO_ABORTIFHUNG | SMTO_BLOCK, 1234, &dwResult)) {
            return (int)dwResult;
        } else {
            DeleteTask(uliTaskId);
            return SE_ERR_DDETIMEOUT;
        }
    }

    int nResult = StartBridgeWithTaskId(uliTaskId);

    if (nResult <= 32) {
        DeleteTask(uliTaskId);
    }

    return nResult;
}

namespace Bridge {
    PROC GetProcAddressFromDll(LPCWSTR lpDllName, LPCSTR lpFunctionName) {
        HMODULE hModule = GetModuleHandleW(lpDllName);

        if (hModule == NULL) {
            hModule = LoadLibraryW(lpDllName);
        }

        if (hModule == NULL) {
            return NULL;
        }

        return GetProcAddress(hModule, lpFunctionName);
    }

    BOOL AddMessageToWindowMessageFilter(UINT uMsg) {
        static PROC pChangeWindowMessageFilter = GetProcAddressFromDll(L"user32.dll", "ChangeWindowMessageFilter");

        if (pChangeWindowMessageFilter != NULL) {
            return ((BOOL(WINAPI *)(UINT, DWORD))pChangeWindowMessageFilter)(uMsg, 1);
        } else {
            return FALSE;
        }
    }

    BOOL RegGetString(HKEY hKey, LPCWSTR lpRegPath, LPCWSTR lpRegValue, WCHAR szBuffer[], DWORD dwSize) {
        SHGetValueW(hKey, lpRegPath, lpRegValue, NULL, szBuffer, &dwSize);

        return dwSize > 0;
    }

    int DoBridgeJob(WPARAM wParam, LPARAM lParam) {
        static BOOL bNeedElevated = NeedElevated();

        if (bNeedElevated) {  // should not happen
            return SE_ERR_ACCESSDENIED;
        }

        int nResult = 0;
        ULARGE_INTEGER uli;
        WCHAR szRegPath[1024];
        TaskDiscription taskDisc;

        ZeroMemory(&taskDisc, sizeof(taskDisc));
        uli.HighPart = (DWORD)wParam;
        uli.LowPart = (DWORD)lParam;

        GetTaskStorePath(uli, szRegPath, RTL_NUMBER_OF(szRegPath));

        if (RegGetString(HKEY_CURRENT_USER, szRegPath, L"cmd", taskDisc.szCommand, sizeof(taskDisc.szCommand)) &&
            RegGetString(HKEY_CURRENT_USER, szRegPath, L"arg", taskDisc.szArguments, sizeof(taskDisc.szArguments)) &&
            RegGetString(HKEY_CURRENT_USER, szRegPath, L"dir", taskDisc.szCurrentDirectory, sizeof(taskDisc.szCurrentDirectory))) {
            DWORD dwSize = sizeof(taskDisc.nShowCmd);

            SHGetValueW(HKEY_CURRENT_USER, szRegPath, L"sw", NULL, &taskDisc.nShowCmd, &dwSize);

            nResult = (int)(INT_PTR)ShellExecuteW(NULL, L"open", taskDisc.szCommand, taskDisc.szArguments, taskDisc.szCurrentDirectory, taskDisc.nShowCmd);
        } else {
            nResult = SE_ERR_PNF;
        }

        SHDeleteKeyW(HKEY_CURRENT_USER, szRegPath);

        return nResult;
    }

    LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE:
            AddMessageToWindowMessageFilter(WM_TO_BRIDGE_INFO);
            return 0;

        case WM_TO_BRIDGE_INFO:
            return DoBridgeJob(wParam, lParam);

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void WINAPI SlxElevateBridgeW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow) {
        if (IsWindow(GetBridgeHandle()) || lpszCmdLine == NULL || *lpszCmdLine == L'\0') {
            return;
        }

        LPCWSTR lpStr2 = StrStrW(lpszCmdLine, L" ");

        if (lpStr2 == NULL) {
            return;
        } else {
            lpStr2 += 1;
        }

        WNDCLASSEXW wcex = { sizeof(wcex) };

        wcex.lpszClassName = CLASS_NAME;
        wcex.hInstance = g_hinstDll;
        wcex.lpfnWndProc = WndProc;
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hIcon = NULL;
        wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

        if (RegisterClassExW(&wcex)) {
            HWND hWindow = CreateWindowExW(
                0,
                CLASS_NAME,
                BridgeVersion,
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                NULL,
                NULL,
                g_hinstDll,
                NULL
                );

            if (IsWindow(hWindow)) {
//                 ShowWindow(hWindow, SW_SHOW);
//                 UpdateWindow(hWindow);

                PostMessageW(hWindow, WM_TO_BRIDGE_INFO, StrToIntW(lpszCmdLine), StrToIntW(lpStr2));

                MSG msg;

                while (TRUE) {
                    int nRet = GetMessageW(&msg, NULL, 0, 0);

                    if (nRet <= 0) {
                        break;
                    }

                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }
    }
}