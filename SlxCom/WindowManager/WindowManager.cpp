#include "WindowManager.h"
#include "../SlxComTools.h"
#include <WindowsX.h>
#include <Shlwapi.h>
#include "NotifyClass.h"
#include "WindowAppID.h"

using namespace std;

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

#define NOTIFYWNDCLASS  L"__slx_WindowManager_20140629"

enum
{
    TIMERID_CHECK = 12,
};

enum
{
    HK_HIDEWINDOW = 12,
    HK_HIDEWINDOW2,
    HK_PINWINDOW,
    HK_PINWINDOW2,
    HK_ADDWINDOW,
    HK_ADDWINDOW2,
    HK_UNGROUPWINDOW,
    HK_UNGROUPWINDOW2,
};

class CWindowManager
{
public:
    CWindowManager(HINSTANCE hInstance)
    {
        m_hInstance = hInstance;
        RegisterWindowClass(m_hInstance);
        m_hWindow = CreateWindowExW(
            0,
            NOTIFYWNDCLASS,
            NOTIFYWNDCLASS,
            WS_OVERLAPPEDWINDOW,
            0, 0, 377, 55,
            NULL,
            NULL,
            m_hInstance,
            this
            );

        if (m_hWindow)
        {
            map<HWND, wstring> mapLastNotifys = CNotifyClass::GetAndClearLastNotifys();

            map<HWND, wstring>::iterator it = mapLastNotifys.begin();
            for (; it != mapLastNotifys.end(); ++it)
            {
                if (IsWindow(it->first))
                {
                    AddWindow(it->first, it->second.c_str(), FALSE);
                }
            }

//             ShowWindow(m_hWindow, SW_SHOW);
            UpdateWindow(m_hWindow);

            RegisterHotKey(m_hWindow, HK_HIDEWINDOW, MOD_ALT | MOD_CONTROL, L'H');
            RegisterHotKey(m_hWindow, HK_HIDEWINDOW2, MOD_ALT | MOD_SHIFT, L'H');
            RegisterHotKey(m_hWindow, HK_PINWINDOW, MOD_ALT | MOD_CONTROL, L'T');
            RegisterHotKey(m_hWindow, HK_PINWINDOW2, MOD_ALT | MOD_SHIFT, L'T');
            RegisterHotKey(m_hWindow, HK_ADDWINDOW, MOD_ALT | MOD_CONTROL, L'C');
            RegisterHotKey(m_hWindow, HK_ADDWINDOW2, MOD_ALT | MOD_SHIFT, L'C');
            RegisterHotKey(m_hWindow, HK_UNGROUPWINDOW, MOD_ALT | MOD_CONTROL, L'G');
            RegisterHotKey(m_hWindow, HK_UNGROUPWINDOW2, MOD_ALT | MOD_SHIFT, L'G');
        }
    }

    ~CWindowManager()
    {
        if (IsWindow(m_hWindow))
        {
            UnregisterHotKey(m_hWindow, HK_HIDEWINDOW);
            UnregisterHotKey(m_hWindow, HK_HIDEWINDOW2);
            UnregisterHotKey(m_hWindow, HK_PINWINDOW);
            UnregisterHotKey(m_hWindow, HK_PINWINDOW2);
            UnregisterHotKey(m_hWindow, HK_ADDWINDOW);
            UnregisterHotKey(m_hWindow, HK_ADDWINDOW2);
            UnregisterHotKey(m_hWindow, HK_UNGROUPWINDOW);
            UnregisterHotKey(m_hWindow, HK_UNGROUPWINDOW2);

            DestroyWindow(m_hWindow);
        }

        map<UINT, CNotifyClass *>::iterator it = m_mapNotifyClassesById.begin();
        for (; it != m_mapNotifyClassesById.end(); ++it)
        {
            delete it->second;
        }
    }

    void Run()
    {
        // 启动钩子进程
        WCHAR szCmd[MAX_PATH * 2 + 1024] = L"";
        WCHAR szDllPath[MAX_PATH] = L"";

        GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
        wnsprintfW(szCmd, RTL_NUMBER_OF(szCmd), L"rundll32.exe \"%s\" SlxWindowManagerProcess %lu %lu", szDllPath, GetCurrentProcessId(), m_hWindow);

        RunCommand(szCmd, NULL);

        // 消息循环
        MSG msg;

        while (TRUE)
        {
            int nRet = GetMessageW(&msg, NULL, 0, 0);

            if (nRet <= 0)
            {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

private:
    UINT GetNewId() const
    {
        if (m_mapNotifyClassesById.empty())
        {
            return 1;
        }
        else
        {
            return m_mapNotifyClassesById.rbegin()->first + 1;
        }
    }

    BOOL AddWindow(HWND hWindow, LPCWSTR lpCreateTime = NULL, BOOL bShowBalloonThisTime = TRUE)
    {
        UINT uId = GetNewId();
        CNotifyClass *pNotifyClass = new CNotifyClass(m_hWindow, hWindow, uId, lpCreateTime, bShowBalloonThisTime);

        m_mapNotifyClassesById[uId] = pNotifyClass;
        m_mapNotifyClassesByWindow[hWindow] = pNotifyClass;

        return TRUE;
    }

    void DoHide()
    {
        HWND hForegroundWindow = CNotifyClass::GetCaptureableWindow();

        if (IsWindow(hForegroundWindow))
        {
            if (IsWindowVisible(hForegroundWindow))
            {
                ShowWindow(hForegroundWindow, SW_HIDE);
            }

            if (m_mapNotifyClassesByWindow.find(hForegroundWindow) == m_mapNotifyClassesByWindow.end())
            {
                AddWindow(hForegroundWindow);
            }
        }
    }

    void DoAdd()
    {
        HWND hForegroundWindow = CNotifyClass::GetCaptureableWindow();

        if (IsWindow(hForegroundWindow))
        {
            if (m_mapNotifyClassesByWindow.find(hForegroundWindow) == m_mapNotifyClassesByWindow.end())
            {
                AddWindow(hForegroundWindow);
            }
        }
    }

    void DoPin()
    {
        CNotifyClass::DoPin(NULL);
    }

    void DoUngroup()
    {
        HWND hForegroundWindow = CNotifyClass::GetCaptureableWindow();
        ToggleWinAppID(hForegroundWindow);
    }

private:
    static void RegisterWindowClass(HINSTANCE hInstance)
    {
        WNDCLASSEXW wcex = {sizeof(wcex)};
        wcex.lpfnWndProc = WindowManagerWindowProc;
        wcex.lpszClassName = NOTIFYWNDCLASS;
        wcex.hInstance = hInstance;
        wcex.style = CS_HREDRAW | CS_VREDRAW;

        RegisterClassExW(&wcex);
    }

    static CWindowManager *GetManagerClass(HWND hWnd)
    {
        return (CWindowManager *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    static LRESULT WINAPI WindowManagerWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_CREATE:
            SetTimer(hWnd, TIMERID_CHECK, 1888, NULL);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
            return 0;

        case WM_SCREEN_CONTEXT_MENU:
            if (wParam != 0)
            {
                HWND hTargetWindow = (HWND)wParam;
                CWindowManager *pWindowManager = GetManagerClass(hWnd);
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);

                if (pWindowManager->m_mapNotifyClassesByWindow.count(hTargetWindow) > 0)
                {
                    pWindowManager->m_mapNotifyClassesByWindow[hTargetWindow]->DoContextMenu(x, y);
                }
                else
                {
                    CNotifyClass(pWindowManager->m_hWindow, hTargetWindow, 0, NULL, FALSE).DoContextMenu(x, y);
                }
            }
            break;

        case WM_TIMER:
            if (wParam == TIMERID_CHECK)
            {
                CWindowManager *pManager = GetManagerClass(hWnd);
                map<UINT, CNotifyClass *> m_mapNotifyClassesById;
                map<HWND, CNotifyClass *>::iterator it = pManager->m_mapNotifyClassesByWindow.begin();

                while (it != pManager->m_mapNotifyClassesByWindow.end())
                {
                    HWND hTargetWindow = it->first;

                    if (IsWindow(hTargetWindow))
                    {
                        if (CNotifyClass::IsOptionHideOnMinimaze() && IsIconic(hTargetWindow))
                        {
                            ShowWindow(hTargetWindow, SW_HIDE);
                        }

                        it->second->RefreshInfo();
                        ++it;
                    }
                    else
                    {
                        pManager->m_mapNotifyClassesById.erase(it->second->GetId());
                        delete it->second;
                        it = pManager->m_mapNotifyClassesByWindow.erase(it);
                    }
                }
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            KillTimer(hWnd, TIMERID_CHECK);
            PostQuitMessage(0);
            break;

        case WM_REMOVE_NOTIFY:
            if (lParam != NULL)
            {
                CWindowManager *pManager = GetManagerClass(hWnd);
                HWND hTargetWindow = (HWND)lParam;
                map<HWND, CNotifyClass *>::iterator it = pManager->m_mapNotifyClassesByWindow.find(hTargetWindow);

                if (it != pManager->m_mapNotifyClassesByWindow.end())
                {
                    delete it->second;
                    pManager->m_mapNotifyClassesById.erase((UINT)wParam);
                    pManager->m_mapNotifyClassesByWindow.erase(it);
                }
            }
            break;

        case WM_HOTKEY:
            switch (wParam)
            {
            case HK_HIDEWINDOW:
            case HK_HIDEWINDOW2:
                GetManagerClass(hWnd)->DoHide();
                break;

            case HK_PINWINDOW:
            case HK_PINWINDOW2:
                GetManagerClass(hWnd)->DoPin();
                break;

            case HK_ADDWINDOW:
            case HK_ADDWINDOW2:
                GetManagerClass(hWnd)->DoAdd();
                break;

            case HK_UNGROUPWINDOW:
            case HK_UNGROUPWINDOW2:
                GetManagerClass(hWnd)->DoUngroup();
                break;

            default:
                break;
            }
            break;

        case WM_CALLBACK:
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_RBUTTONUP || lParam == WM_RBUTTONUP)
            {
                UINT uId = (UINT)wParam;

                map<UINT, CNotifyClass *>::iterator it = GetManagerClass(hWnd)->m_mapNotifyClassesById.find(uId);

                if (it != GetManagerClass(hWnd)->m_mapNotifyClassesById.end())
                {
                    if (lParam == WM_LBUTTONDBLCLK)
                    {
                        it->second->SwitchVisiable();
                    }
                    else
                    {
                        POINT pt;

                        GetCursorPos(&pt);
                        it->second->DoContextMenu(pt.x, pt.y);
                    }
                }
            }
            else
            {

            }
            break;

        default:
            break;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

private:
    HWND m_hWindow;
    HINSTANCE m_hInstance;
    map<UINT, CNotifyClass *> m_mapNotifyClassesById;
    map<HWND, CNotifyClass *> m_mapNotifyClassesByWindow;
};

static DWORD CALLBACK WindowManagerProc(LPVOID lpParam)
{
    HINSTANCE hInstance = (HINSTANCE)lpParam;
    WCHAR szImagePath[MAX_PATH] = L"";

    GetModuleFileNameW(GetModuleHandleW(NULL), szImagePath, RTL_NUMBER_OF(szImagePath));

    if (lstrcmpiW(L"rundll32.exe", PathFindFileNameW(szImagePath)) != 0)
    {
        DWORD dwSleepTime = 1;
        while (!IsWindow(GetTrayNotifyWndInProcess()))
        {
            Sleep((dwSleepTime++) * 1000);
        }
    }

    CWindowManager(hInstance).Run();
    return 0;
}

void StartWindowManager(HINSTANCE hinstDll)
{
    if (!IsExplorer())
    {
        return;
    }

    if (IsWow64ProcessHelper(GetCurrentProcess()))
    {
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, WindowManagerProc, (LPVOID)hinstDll, 0, NULL);
    CloseHandle(hThread);
}

void __stdcall T3(HWND hwndStub, HINSTANCE hAppInstance, LPWSTR lpszCmdLine, int nCmdShow)
{
    extern HINSTANCE g_hinstDll; //SlxCom.cpp
    WindowManagerProc((LPVOID)g_hinstDll);
}