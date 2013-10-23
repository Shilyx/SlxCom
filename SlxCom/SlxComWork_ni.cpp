#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "resource.h"
#pragma warning(disable: 4786)
#include <vector>
#include <map>
#include "lib/charconv.h"

using namespace std;

#define NOTIFYWNDCLASS  TEXT("__slxcom_notify_wnd")
#define WM_CALLBACK     (WM_USER + 112)
#define ICON_COUNT      10
#define TIMER_ICON      1
#define TIMER_MENU      2

extern HBITMAP g_hKillExplorerBmp; //SlxCom.cpp

enum
{
    SYS_QUIT        = 1,
    SYS_ABOUT,
    SYS_RESETEXPLORER,
    SYS_MAXVALUE,
};

template <typename T>
basic_string<T> &AssignString(basic_string<T> &str, const T *p)
{
    if (p != NULL)
    {
        str = p;
    }
    else
    {
        str.clear();
    }

    return str;
}

class MenuItem
{
public:
    MenuItem() : m_nCmdId(0) { }
    virtual ~MenuItem() { }
    virtual void DoJob(HWND hWindow, HINSTANCE hInstance) = 0;

protected:
    int m_nCmdId;
    tstring m_strText;
};

class MenuItemRegistry : public MenuItem
{
public:
    MenuItemRegistry(int nCmdId, LPCTSTR lpText, LPCTSTR lpRegPath)
    {
        m_nCmdId = nCmdId;
        AssignString(m_strText, lpText);
        AssignString(m_strRegPath, lpRegPath);
    }

    virtual void DoJob(HWND hWindow, HINSTANCE hInstance)
    {
        MessageBox(hWindow, m_strRegPath.c_str(), m_strText.c_str(), 0);
    }

protected:
    tstring m_strRegPath;
};

// class MenuItemSystem : public MenuItem
// {
// public:
//     MenuItemSystem()
// };

class MenuMgr
{
public:
    MenuMgr(HWND hWindow, HINSTANCE hInstance)
        : m_hWindow(hWindow), m_hInstance(hInstance)
    {
        m_hMenu = NULL;
        UpdateMenu();
    }

    void TrackAndDealPopupMenu(int x, int y)
    {
        int nCmd = TrackPopupMenu(
            m_hMenu,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
            x,
            y,
            NULL,
            m_hWindow,
            NULL
            );

        switch (nCmd)
        {
        case SYS_QUIT:
            PostMessage(m_hWindow, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;

        case SYS_ABOUT:
            MessageBox(m_hWindow, TEXT("About"), NULL, 0);
            break;

        case SYS_RESETEXPLORER:
            if (IDYES == MessageBox(
                m_hWindow,
                TEXT("要重启Explorer（操作系统外壳程序）吗？"),
                TEXT("请确认"),
                MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_SYSTEMMODAL
                ))
            {
                KillAllExplorers();
            }
            break;

        default:
            if (m_mapMenuItems.count(nCmd) >= 0)
            {
                m_mapMenuItems[nCmd]->DoJob(m_hWindow, m_hInstance);
            }
            break;
        }
    }

    void UpdateMenu()
    {
        BOOL bShiftDown = GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0;

        if (m_hMenu != NULL)
        {
            DestroyMenu(m_hMenu);
            m_hMenu = NULL;
        }

        m_hMenu = CreatePopupMenu();

        AppendMenu(m_hMenu, MF_STRING, SYS_ABOUT, TEXT("关于(&A)"));
        AppendMenu(m_hMenu, MF_STRING, SYS_QUIT, TEXT("不显示托盘图标(&Q)"));

        if (bShiftDown)
        {
            AppendMenu(m_hMenu, MF_STRING, SYS_RESETEXPLORER, TEXT("重新启动Explorer(&R)"));
            SetMenuItemBitmaps(m_hMenu, SYS_RESETEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
        }

        AppendMenu(m_hMenu, MF_SEPARATOR, 0, NULL);

    }

private:
    HWND m_hWindow;
    HMENU m_hMenu;
    HINSTANCE m_hInstance;
    map<int, MenuItem *> m_mapMenuItems;
};

static LRESULT __stdcall NotifyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static NOTIFYICONDATA nid = {sizeof(nid)};
    static int nIconShowIndex = MAXINT;
    static HICON arrIcons[ICON_COUNT] = {NULL};
    static MenuMgr *pMenuMgr = NULL;

    if (uMsg == WM_CREATE)
    {
        LPCREATESTRUCT lpCs = (LPCREATESTRUCT)lParam;

        //load icons
        int nFirstIconId = IDI_BIRD_0;

        for (int i = 0; i < ICON_COUNT; i += 1)
        {
            arrIcons[i] = LoadIcon(lpCs->hInstance, MAKEINTRESOURCE(nFirstIconId + i));
        }

        //add icon to tray
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uCallbackMessage = WM_CALLBACK;
        nid.hIcon = arrIcons[0];
        nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;
        lstrcpyn(nid.szTip, TEXT("SlxCom"), sizeof(nid.szTip));

        Shell_NotifyIcon(NIM_ADD, &nid);

        //
        SetTimer(hWnd, TIMER_ICON, 55, NULL);
        SetTimer(hWnd, TIMER_MENU, 5003, NULL);

        PostMessage(hWnd, WM_TIMER, TIMER_MENU, 0);

        //
        pMenuMgr = new MenuMgr(hWnd, lpCs->hInstance);

        return 0;
    }
    else if (uMsg == WM_TIMER)
    {
        if (TIMER_ICON == wParam)
        {
            if (nIconShowIndex < RTL_NUMBER_OF(arrIcons))
            {
                nid.hIcon = arrIcons[nIconShowIndex];
                nid.uFlags = NIF_ICON;

                Shell_NotifyIcon(NIM_MODIFY, &nid);
                nIconShowIndex += 1;
            }
        }
        else if (TIMER_MENU == wParam)
        {
            pMenuMgr->UpdateMenu();
        }
    }
    else if (uMsg == WM_CLOSE)
    {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        DestroyWindow(hWnd);
    }
    else if (uMsg == WM_DESTROY)
    {
        if (pMenuMgr != NULL)
        {
            delete pMenuMgr;
            pMenuMgr = NULL;
        }

        PostQuitMessage(0);
    }
    else if (uMsg == WM_LBUTTONDOWN)
    {
        nIconShowIndex = 0;
    }
    else if (uMsg == WM_CALLBACK)
    {
        if (lParam == WM_RBUTTONUP)
        {
            POINT pt;

            GetCursorPos(&pt);

            SetForegroundWindow(hWnd);
            pMenuMgr->TrackAndDealPopupMenu(pt.x, pt.y);
        }

        if (nIconShowIndex >= RTL_NUMBER_OF(arrIcons))
        {
            nIconShowIndex = 0;
        }
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
 //   while (!IsWindow(GetTrayNotifyWndInProcess()))
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