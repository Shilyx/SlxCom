#include "WindowManager.h"
#include "SlxComTools.h"
#include <WindowsX.h>
#include <Shlwapi.h>
#include <map>
#include <iomanip>
#include "lib/charconv.h"

using namespace std;

#define NOTIFYWNDCLASS TEXT("__slx_WindowManager_20140629")

static enum
{
    WM_CALLBACK = WM_USER + 12,
};

static enum
{
    HK_HIDEWINDOW = 12,
    HK_HIDEWINDOW2,
    HK_PINWINDOW,
    HK_PINWINDOW2,
};

class CNotifyClass
{
public:
    CNotifyClass(HWND hManagerWindow, HWND hTargetWindow, UINT uId)
    {
        m_hManagerWindow = hManagerWindow;
        m_hTargetWindow = hTargetWindow;
        ZeroMemory(&m_nid, sizeof(m_nid));
        m_nid.cbSize = sizeof(m_nid);
        m_nid.hWnd = hManagerWindow;
        m_nid.uID = uId;
        m_nid.uCallbackMessage = WM_CALLBACK;

        GetStableInfo();
        RefreshInfo();

        m_hIcon = GetWindowIcon(m_hTargetWindow);
        m_hIconSm = GetWindowIconSmall(m_hTargetWindow);

        if (m_hIcon == NULL)
        {
            m_hIcon = LoadIcon(NULL, IDI_INFORMATION);
        }

        if (m_hIconSm == NULL)
        {
            m_hIconSm = m_hIcon;
        }

        m_nid.hIcon = m_hIconSm;
        m_nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;

        Shell_NotifyIcon(NIM_ADD, &m_nid);
    }

    ~CNotifyClass()
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
    }

public:
    void SwitchVisiable()
    {
        if (IsWindow(m_hTargetWindow))
        {
            if (IsWindowVisible(m_hTargetWindow))
            {
                ShowWindow(m_hTargetWindow, SW_HIDE);
            }
            else
            {
                ShowWindow(m_hTargetWindow, SW_SHOW);
            }
        }
    }

    void GetStableInfo()
    {
        LPCTSTR lpCrLf = TEXT("\r\n");
        tstringstream ss;
        DWORD dwProcessId = 0;
        DWORD dwThreadId = 0;
        TCHAR szClassName[1024] = TEXT("");

        dwThreadId = GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);
        GetClassName(m_hTargetWindow, szClassName, RTL_NUMBER_OF(szClassName));

        ss<<TEXT("窗口句柄：0x")<<hex<<m_hTargetWindow<<lpCrLf;
        ss<<TEXT("窗口类：")<<szClassName<<lpCrLf;
        ss<<TEXT("线程id：0x")<<hex<<dwThreadId<<TEXT("")<<dec<<dwThreadId<<TEXT(")")<<lpCrLf;
        ss<<TEXT("进程id：0x")<<hex<<dwProcessId<<TEXT("")<<dec<<dwProcessId<<TEXT(")")<<lpCrLf;

        m_strBaseInfo = ss.str();
    }

    void RefreshInfo()
    {
        TCHAR szWindowStatus[32] = TEXT("");
        TCHAR szWindowText[32] = TEXT("");
        BOOL bIsWindow = IsWindow(m_hTargetWindow);
        BOOL bIsWindowVisiable = FALSE;
        LPCTSTR lpCrLf = TEXT("\r\n");

        if (bIsWindow)
        {
            bIsWindowVisiable = IsWindowVisible(m_hTargetWindow);
            GetWindowText(m_hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));

            if (bIsWindowVisiable)
            {
                lstrcpyn(szWindowStatus, TEXT("可见"), RTL_NUMBER_OF(szWindowStatus));
            }
            else
            {
                lstrcpyn(szWindowStatus, TEXT("隐藏"), RTL_NUMBER_OF(szWindowStatus));
            }
        }
        else
        {
            lstrcpyn(szWindowStatus, TEXT("无效"), RTL_NUMBER_OF(szWindowStatus));
        }

        // 
        wnsprintf(
            m_nid.szTip,
            RTL_NUMBER_OF(m_nid.szTip),
            TEXT("SlxCom WindowManager\r\n")
            TEXT("窗口句柄：%#x\r\n")
            TEXT("窗口状态：%s\r\n")
            TEXT("窗口标题：%s"),
            m_hTargetWindow,
            szWindowStatus,
            szWindowText
            );

        // 
    }

private:
    HWND m_hManagerWindow;
    HWND m_hTargetWindow;
    NOTIFYICONDATA m_nid;
    tstring m_strBaseInfo;
    HICON m_hIcon;
    HICON m_hIconSm;
};

class CWindowManager
{
public:
    CWindowManager(HINSTANCE hInstance)
    {
        m_hInstance = hInstance;
        RegisterWindowClass(m_hInstance);
        m_hWindow = CreateWindowEx(
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
            ShowWindow(m_hWindow, SW_SHOW);
            UpdateWindow(m_hWindow);

            RegisterHotKey(m_hWindow, HK_HIDEWINDOW, MOD_ALT | MOD_CONTROL, TEXT('H'));
            RegisterHotKey(m_hWindow, HK_HIDEWINDOW2, MOD_ALT | MOD_SHIFT, TEXT('H'));
            RegisterHotKey(m_hWindow, HK_PINWINDOW, MOD_ALT | MOD_CONTROL, TEXT('T'));
            RegisterHotKey(m_hWindow, HK_PINWINDOW2, MOD_ALT | MOD_SHIFT, TEXT('T'));
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

            DestroyWindow(m_hWindow);
        }
    }

    void Run()
    {
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

    void DoHide()
    {
        HWND hForegroundWindow = GetForegroundWindow();
        HWND hDesktopWindow = GetDesktopWindow();

        if (hForegroundWindow != hDesktopWindow && IsWindow(hForegroundWindow))
        {
            if (IsWindowVisible(hForegroundWindow))
            {
                ShowWindow(hForegroundWindow, SW_HIDE);
            }
            else
            {
                ShowWindow(hForegroundWindow, SW_SHOW);
            }

            if (m_mapNotifyClassesByWindow.find(hForegroundWindow) == m_mapNotifyClassesByWindow.end())
            {
                UINT uId = GetNewId();
                CNotifyClass *pNotifyClass = new CNotifyClass(m_hWindow, hForegroundWindow, uId);

                m_mapNotifyClassesById[uId] = pNotifyClass;
                m_mapNotifyClassesByWindow[hForegroundWindow] = pNotifyClass;
            }
        }
    }

    void DoPin()
    {
        CloseHandle(CreateThread(NULL, 0, PinWindowProc, NULL, 0, NULL));
    }

private:
    static DWORD CALLBACK PinWindowProc(LPVOID lpParam)
    {
        HWND hForegroundWindow = GetForegroundWindow();
        HWND hDesktopWindow = GetDesktopWindow();

        if (hForegroundWindow != hDesktopWindow && IsWindow(hForegroundWindow))
        {
            LONG_PTR nExStyle = GetWindowLongPtr(hForegroundWindow, GWL_EXSTYLE);
            TCHAR szNewWindowText[4096];
            LPTSTR lpWindowText = szNewWindowText;

            if (nExStyle & WS_EX_TOPMOST)
            {
                lpWindowText += wnsprintf(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), TEXT("已取消置顶<<<<"));
                SetWindowPos(hForegroundWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            else
            {
                lpWindowText += wnsprintf(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), TEXT("已置顶>>>>"));
                SetWindowPos(hForegroundWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }

            if (GetWindowTextLength(hForegroundWindow) < RTL_NUMBER_OF(szNewWindowText) - 100)
            {
                GetWindowText(hForegroundWindow, lpWindowText, RTL_NUMBER_OF(szNewWindowText) - 100);
                SetWindowText(hForegroundWindow, szNewWindowText);

                for (int i = 0; i < 3; i += 1)
                {
                    FlashWindow(hForegroundWindow, TRUE);
                    Sleep(300);
                    FlashWindow(hForegroundWindow, TRUE);
                }

                Sleep(500);
                SetWindowText(hForegroundWindow, lpWindowText);
            }
        }

        return 0;
    }

    static void RegisterWindowClass(HINSTANCE hInstance)
    {
        WNDCLASSEX wcex = {sizeof(wcex)};
        wcex.lpfnWndProc = WindowManagerWindowProc;
        wcex.lpszClassName = NOTIFYWNDCLASS;
        wcex.hInstance = hInstance;
        wcex.style = CS_HREDRAW | CS_VREDRAW;

        RegisterClassEx(&wcex);
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
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
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

            default:
                break;
            }
            break;

        case WM_CALLBACK:
            if (lParam == WM_LBUTTONDBLCLK)
            {
                UINT uId = (UINT)wParam;

                map<UINT, CNotifyClass *>::iterator it = GetManagerClass(hWnd)->m_mapNotifyClassesById.find(uId);

                if (it != GetManagerClass(hWnd)->m_mapNotifyClassesById.end())
                {
                    it->second->SwitchVisiable();
                }
            }
            else if (lParam == WM_RBUTTONUP || lParam == WM_RBUTTONUP)
            {
                POINT pt = {GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
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

void __stdcall T3(HWND hwndStub, HINSTANCE hAppInstance, LPTSTR lpszCmdLine, int nCmdShow)
{
    extern HINSTANCE g_hinstDll; //SlxCom.cpp
    WindowManagerProc((LPVOID)g_hinstDll);
}