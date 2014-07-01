#include "WindowManager.h"
#include "SlxComTools.h"
#include "SlxComAbout.h"
#include <WindowsX.h>
#include <Shlwapi.h>
#include <map>
#include <iomanip>
#include "lib/charconv.h"

using namespace std;

#define NOTIFYWNDCLASS  TEXT("__slx_WindowManager_20140629")
#define BASE_REG_PATH   TEXT("Software\\Shilyx Studio\\SlxCom\\WindowManager")
#define RECORD_REG_PATH TEXT("Software\\Shilyx Studio\\SlxCom\\WindowManager\\Records")

static enum
{
    WM_CALLBACK = WM_USER + 12,     // 托盘图标的回调消息
    WM_REMOVE_NOTIFY,               // 通知主窗口销毁某托盘，wparam为id，lparam为hwnd
};

static enum
{
    ID_CHECK = 12,
};

static enum
{
    HK_HIDEWINDOW = 12,
    HK_HIDEWINDOW2,
    HK_PINWINDOW,
    HK_PINWINDOW2,
    HK_ADDWINDOW,
    HK_ADDWINDOW2,
};

static enum
{
    CMD_ABOUT = 1,
    CMD_DESTROYONSHOW,
    CMD_SHOWBALLOON,
    CMD_PINWINDOW,
    CMD_SWITCHVISIABLE,
    CMD_DETAIL,
    CMD_DESTROYICON,
};

class CNotifyClass
{
public:
    CNotifyClass(HWND hManagerWindow, HWND hTargetWindow, UINT uId, LPCTSTR lpCreateTime)
    {
        if (lpCreateTime == NULL)
        {
            m_strCreateTime = GetCurrentTimeString();
        }
        else
        {
            m_strCreateTime = lpCreateTime;
        }

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

        if (ms_bShowBalloon)
        {
            TCHAR szWindowText[32] = TEXT("");

            GetWindowText(hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));

            lstrcpyn(m_nid.szInfoTitle, TEXT("SlxCom WindowManager"), RTL_NUMBER_OF(m_nid.szInfoTitle));
            wnsprintf(
                m_nid.szInfo,
                RTL_NUMBER_OF(m_nid.szInfo),
                TEXT("窗口“%s”（句柄：%#x）已被收纳。"),
                szWindowText,
                hTargetWindow
                );

            m_nid.uFlags |= NIF_INFO;
        }

        Shell_NotifyIcon(NIM_ADD, &m_nid);

        if (ms_bShowBalloon)
        {
            m_nid.uFlags &= ~NIF_INFO;
        }

        RecordToRegistry();
    }

    ~CNotifyClass()
    {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);

        if (IsWindow(m_hTargetWindow) && !IsWindowVisible(m_hTargetWindow))
        {
            ShowWindow(m_hTargetWindow, SW_SHOW);
        }

        RemoveFromRegistry();
    }

public:
    UINT GetId() const
    {
        return m_nid.uID;
    }

    HWND GetTargetWindow() const
    {
        return m_hTargetWindow;
    }

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

                if (ms_bDestroyOnShow)
                {
                    PostMessage(m_hManagerWindow, WM_REMOVE_NOTIFY, m_nid.uID, (LPARAM)m_hTargetWindow);
                }
            }
        }
    }

    void DoContextMenu(int x, int y)
    {
        HMENU hMenu = CreatePopupMenu();
        HMENU hPopupMenu = CreatePopupMenu();

        AppendMenu(hMenu, MF_STRING, CMD_ABOUT, TEXT("关于SlxCom(&A)..."));
        AppendMenu(hPopupMenu, MF_STRING, CMD_DESTROYONSHOW, TEXT("被控窗口可见后销毁托盘图标(&A)"));
        AppendMenu(hPopupMenu, MF_STRING, CMD_SHOWBALLOON, TEXT("窗口隐藏时显示气泡通知(&B)"));
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, TEXT("全局配置(&G)"));
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, CMD_PINWINDOW, TEXT("置顶窗口(&T)\tAlt+Ctrl(Shift)+T"));
        AppendMenu(hMenu, MF_STRING, CMD_SWITCHVISIABLE, TEXT("显示窗口(&S)\tAlt+Ctrl(Shift)+H"));
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, CMD_DETAIL, TEXT("显示窗口信息(&D)"));
        AppendMenu(hMenu, MF_STRING, CMD_DESTROYICON, TEXT("销毁此图标(&Y)"));

        CheckMenuItemHelper(hPopupMenu, CMD_DESTROYONSHOW, MF_BYCOMMAND, ms_bDestroyOnShow);
        CheckMenuItemHelper(hPopupMenu, CMD_SHOWBALLOON, MF_BYCOMMAND, ms_bShowBalloon);
        CheckMenuItemHelper(hMenu, CMD_PINWINDOW, MF_BYCOMMAND, IsWindowTopMost(m_hTargetWindow));
        SetMenuDefaultItem(hMenu, CMD_SWITCHVISIABLE, MF_BYCOMMAND);

        if (IsWindow(m_hTargetWindow))
        {
            if (IsWindowVisible(m_hTargetWindow))
            {
                ModifyMenu(hMenu, CMD_SWITCHVISIABLE, MF_STRING, 0, TEXT("隐藏窗口(&H)"));
            }
        }
        else
        {
            EnableMenuItemHelper(hMenu, CMD_SWITCHVISIABLE, MF_BYCOMMAND, FALSE);
            EnableMenuItemHelper(hMenu, CMD_PINWINDOW, MF_BYCOMMAND, FALSE);
        }

        SetForegroundWindow(m_hManagerWindow);

        int nCmd = TrackPopupMenu(
            hMenu,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
            x,
            y,
            NULL,
            m_hManagerWindow,
            NULL
            );

        switch (nCmd)
        {
        case CMD_ABOUT:
            SlxComAbout(m_hManagerWindow);
            break;

        case CMD_DESTROYONSHOW:
            ms_bDestroyOnShow = !ms_bDestroyOnShow;
            SHSetValue(HKEY_CURRENT_USER, BASE_REG_PATH, TEXT("DestroyOnShow"), REG_DWORD, &ms_bDestroyOnShow, sizeof(ms_bDestroyOnShow));
            break;

        case CMD_SHOWBALLOON:
            ms_bShowBalloon = !ms_bShowBalloon;
            SHSetValue(HKEY_CURRENT_USER, BASE_REG_PATH, TEXT("ShowBalloon"), REG_DWORD, &ms_bShowBalloon, sizeof(ms_bShowBalloon));
            break;

        case CMD_PINWINDOW:
            DoPin(m_hTargetWindow);
            break;

        case CMD_SWITCHVISIABLE:
            SwitchVisiable();
            break;

        case CMD_DETAIL:
            break;

        case CMD_DESTROYICON:
//             if (IsWindow(m_hTargetWindow) &&
//                 !IsWindowVisible(m_hTargetWindow) &&
//                 IDYES == MessageBox(
//                     m_hTargetWindow,
//                     TEXT("被管理的窗口当前不可见，是否要先将其显示出来？"),
//                     TEXT("请确认"),
//                     MB_ICONQUESTION | MB_YESNO | MB_TOPMOST
//                 ))
//             {
//                 ShowWindow(m_hTargetWindow, SW_SHOW);
//             }
            PostMessage(m_hManagerWindow, WM_REMOVE_NOTIFY, m_nid.uID, (LPARAM)m_hTargetWindow);
            break;

        default:
            break;
        }

        DestroyMenu(hMenu);
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

    static void DoPin(HWND hTargetWindow)
    {
        CloseHandle(CreateThread(NULL, 0, PinWindowProc, (LPVOID)hTargetWindow, 0, NULL));
    }

    static map<HWND, tstring> GetAndClearLastNotifys()
    {
        map<HWND, tstring> result;
        tstring strRegPath = RECORD_REG_PATH;
        TCHAR szHwnd[32] = TEXT("");
        HKEY hKey = NULL;

        strRegPath += TEXT("\\");
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));

        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, strRegPath.c_str(), 0, KEY_QUERY_VALUE, &hKey))
        {
            DWORD dwValueCount = 0;
            DWORD dwMaxValueNameLen = 0;
            DWORD dwMaxDataLen = 0;

            if (ERROR_SUCCESS == RegQueryInfoKey(
                hKey, NULL, NULL, NULL, NULL, NULL, NULL,
                &dwValueCount,
                &dwMaxValueNameLen,
                &dwMaxDataLen,
                NULL, NULL
                ))
            {
                dwMaxValueNameLen += 1;
                dwMaxValueNameLen *= 2;
                dwMaxDataLen += 1;

                TCHAR *szValueName = (TCHAR *)malloc(dwMaxValueNameLen);
                unsigned char *szData = (unsigned char *)malloc(dwMaxDataLen);

                if (szValueName != NULL && szData != NULL)
                {
                    for (DWORD dwIndex = 0; dwIndex < dwValueCount; dwIndex += 1)
                    {
                        DWORD dwRegType = REG_NONE;
                        DWORD dwValueSize = dwMaxValueNameLen;
                        DWORD dwDataSize = dwMaxDataLen;

                        if (ERROR_SUCCESS == RegEnumValue(
                            hKey,
                            dwIndex,
                            szValueName,
                            &dwValueSize,
                            NULL,
                            &dwRegType,
                            szData,
                            &dwDataSize
                            ))
                        {
                            if (dwRegType == REG_SZ)
                            {
                                result[(HWND)StrToInt(szValueName)] = (TCHAR *)szData;
                            }
                        }
                    }
                }

                if (szValueName != NULL)
                {
                    free((void *)szValueName);
                }

                if (szData != NULL)
                {
                    free((void *)szData);
                }
            }

            RegCloseKey(hKey);
        }

        SHDeleteKey(HKEY_CURRENT_USER, strRegPath.c_str());

        return result;
    }

private:
    void RecordToRegistry()
    {
        TCHAR szNull[] = TEXT("");
        tstring strRegPath = RECORD_REG_PATH;
        TCHAR szHwnd[32] = TEXT("");

        SHSetValue(HKEY_CURRENT_USER, RECORD_REG_PATH, NULL, REG_SZ, szNull, (lstrlen(szNull) + 1) * sizeof(TCHAR));

        strRegPath += TEXT("\\");
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
        wnsprintf(szHwnd, RTL_NUMBER_OF(szHwnd), TEXT("%u"), m_hTargetWindow);

        SHSetTempValue(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd, REG_SZ, m_strCreateTime.c_str(), (DWORD)(m_strCreateTime.length() + 1) * sizeof(TCHAR));
    }

    void RemoveFromRegistry()
    {
        tstring strRegPath = RECORD_REG_PATH;
        TCHAR szHwnd[32] = TEXT("");

        strRegPath += TEXT("\\");
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
        wnsprintf(szHwnd, RTL_NUMBER_OF(szHwnd), TEXT("%u"), m_hTargetWindow);

        SHDeleteValue(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd);
    }

    static DWORD CALLBACK PinWindowProc(LPVOID lpParam)
    {
        HWND hTargetWindow = (HWND)lpParam;
        HWND hDesktopWindow = GetDesktopWindow();

        if (hTargetWindow == NULL)
        {
            hTargetWindow = GetForegroundWindow();

            if (!IsWindowVisible(hTargetWindow))
            {
                hTargetWindow = NULL;
            }
        }

        if (hTargetWindow != hDesktopWindow && IsWindow(hTargetWindow))
        {
            LONG_PTR nExStyle = GetWindowLongPtr(hTargetWindow, GWL_EXSTYLE);
            TCHAR szNewWindowText[4096];
            LPTSTR lpWindowText = szNewWindowText;

            if (nExStyle & WS_EX_TOPMOST)
            {
                lpWindowText += wnsprintf(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), TEXT("已取消置顶<<<<"));
                SetWindowPos(hTargetWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            else
            {
                lpWindowText += wnsprintf(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), TEXT("已置顶>>>>"));
                SetWindowPos(hTargetWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }

            if (GetWindowTextLength(hTargetWindow) < RTL_NUMBER_OF(szNewWindowText) - 100)
            {
                GetWindowText(hTargetWindow, lpWindowText, RTL_NUMBER_OF(szNewWindowText) - 100);
                SetWindowText(hTargetWindow, szNewWindowText);

                for (int i = 0; i < 3; i += 1)
                {
                    FlashWindow(hTargetWindow, TRUE);
                    Sleep(300);
                    FlashWindow(hTargetWindow, TRUE);
                }

                Sleep(500);
                SetWindowText(hTargetWindow, lpWindowText);
            }
        }

        return 0;
    }

private:
    HWND m_hManagerWindow;
    HWND m_hTargetWindow;
    NOTIFYICONDATA m_nid;
    tstring m_strBaseInfo;
    HICON m_hIcon;
    HICON m_hIconSm;
    tstring m_strCreateTime;

private:
    static BOOL ms_bDestroyOnShow;
    static BOOL ms_bShowBalloon;
};

BOOL CNotifyClass::ms_bDestroyOnShow = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, TEXT("DestroyOnShow"), TRUE);
BOOL CNotifyClass::ms_bShowBalloon = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, TEXT("ShowBalloon"), TRUE);

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
            map<HWND, tstring> mapLastNotifys = CNotifyClass::GetAndClearLastNotifys();

            map<HWND, tstring>::iterator it = mapLastNotifys.begin();
            for (; it != mapLastNotifys.end(); ++it)
            {
                if (IsWindow(it->first))
                {
                    AddWindow(it->first, it->second.c_str());
                }
            }

//             ShowWindow(m_hWindow, SW_SHOW);
            UpdateWindow(m_hWindow);

            RegisterHotKey(m_hWindow, HK_HIDEWINDOW, MOD_ALT | MOD_CONTROL, TEXT('H'));
            RegisterHotKey(m_hWindow, HK_HIDEWINDOW2, MOD_ALT | MOD_SHIFT, TEXT('H'));
            RegisterHotKey(m_hWindow, HK_PINWINDOW, MOD_ALT | MOD_CONTROL, TEXT('T'));
            RegisterHotKey(m_hWindow, HK_PINWINDOW2, MOD_ALT | MOD_SHIFT, TEXT('T'));
            RegisterHotKey(m_hWindow, HK_ADDWINDOW, MOD_ALT | MOD_CONTROL, TEXT('C'));
            RegisterHotKey(m_hWindow, HK_ADDWINDOW2, MOD_ALT | MOD_SHIFT, TEXT('C'));
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

    BOOL AddWindow(HWND hWindow, LPCTSTR lpCreateTime = NULL)
    {
        UINT uId = GetNewId();
        CNotifyClass *pNotifyClass = new CNotifyClass(m_hWindow, hWindow, uId, lpCreateTime);

        m_mapNotifyClassesById[uId] = pNotifyClass;
        m_mapNotifyClassesByWindow[hWindow] = pNotifyClass;

        return TRUE;
    }

    void DoHide()
    {
        HWND hForegroundWindow = GetForegroundWindow();
        HWND hDesktopWindow = GetDesktopWindow();

        if (hForegroundWindow != hDesktopWindow && IsWindow(hForegroundWindow) && IsWindowVisible(hForegroundWindow))
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
                AddWindow(hForegroundWindow);
            }
        }
    }

    void DoAdd()
    {
        HWND hForegroundWindow = GetForegroundWindow();
        HWND hDesktopWindow = GetDesktopWindow();

        if (hForegroundWindow != hDesktopWindow && IsWindow(hForegroundWindow) && IsWindowVisible(hForegroundWindow))
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

private:
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
            SetTimer(hWnd, ID_CHECK, 3333, NULL);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
            return 0;

        case WM_TIMER:
            if (wParam == ID_CHECK)
            {
                CWindowManager *pManager = GetManagerClass(hWnd);
                map<UINT, CNotifyClass *> m_mapNotifyClassesById;
                map<HWND, CNotifyClass *>::iterator it = pManager->m_mapNotifyClassesByWindow.begin();

                while (it != pManager->m_mapNotifyClassesByWindow.end())
                {
                    if (IsWindow(it->first))
                    {
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
            KillTimer(hWnd, ID_CHECK);
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