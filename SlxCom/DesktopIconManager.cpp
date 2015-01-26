#include "DesktopIconManager.h"
#include <CommCtrl.h>
#include <WindowsX.h>
#include <list>
#include "SlxComTools.h"

using namespace std;

class CMButtonUpListener
{
private:
    enum TIMER_ID
    {
        TI_AUTOHIDE = 0x20,
    };

public:
    CMButtonUpListener(HWND hListCtrl)
        : m_hListCtrl(hListCtrl)
        , m_hShellDll(NULL)
        , m_oldWndProc_ListCtrl(NULL)
        , m_oldWndProc_ShellDll(NULL)
        , m_dwListCtrlAutoHideTickCount(0)
    {
        if (IsWindow(m_hListCtrl))
        {
            m_hShellDll = GetParent(m_hListCtrl);

            m_oldWndProc_ListCtrl = (WNDPROC)SetWindowLongPtr(m_hListCtrl, GWLP_WNDPROC, (LONG_PTR)newWindowProc_ListCtrl);
            SetTimer(m_hListCtrl, TI_AUTOHIDE, 1001, NULL);
            SetProp(m_hListCtrl, TEXT("CDbClickListener"), (HANDLE)this);

            if (IsWindow(m_hShellDll))
            {
                m_oldWndProc_ShellDll = (WNDPROC)SetWindowLongPtr(m_hShellDll, GWLP_WNDPROC, (LONG_PTR)newWindowProc_ShellDll);
                SetProp(m_hShellDll, TEXT("CDbClickListener"), (HANDLE)this);
            }
        }
    }

    ~CMButtonUpListener()
    {
        if (IsWindow(m_hListCtrl))
        {
            SetWindowLongPtr(m_hListCtrl, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc_ListCtrl);
            RemoveProp(m_hListCtrl, TEXT("CDbClickListener"));
            KillTimer(m_hListCtrl, TI_AUTOHIDE);
        }

        if (IsWindow(m_hShellDll))
        {
            SetWindowLongPtr(m_hShellDll, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc_ShellDll);
            RemoveProp(m_hShellDll, TEXT("CDbClickListener"));
        }
    }

private:
    static INT_PTR GetHitTestPostion(HWND hwnd, int x, int y)
    {
        LVHITTESTINFO info = {{x, y}};

        if (IsWindow(hwnd))
        {
            return SendMessage(hwnd, LVM_HITTEST, 0, (LPARAM)&info);
        }

        return -1;
    }

    static LRESULT CALLBACK newWindowProc_ListCtrl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CMButtonUpListener *pDbClickListener = (CMButtonUpListener *)GetProp(hwnd, TEXT("CDbClickListener"));

        if (pDbClickListener != NULL && pDbClickListener->m_hListCtrl == hwnd)
        {
            switch (uMsg)
            {
            case WM_MBUTTONDOWN:
                if (-1 == GetHitTestPostion(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
                {
                    ShowWindow(pDbClickListener->m_hListCtrl, SW_HIDE);
                }
                break;

            case WM_TIMER:
                if (wParam == TI_AUTOHIDE)
                {
                    if (IsWindowVisible(hwnd))
                    {
                        if (++pDbClickListener->m_dwListCtrlAutoHideTickCount > 10)
                        {
                            ShowWindow(pDbClickListener->m_hListCtrl, SW_HIDE);
                        }
                    }
                    else
                    {
                        pDbClickListener->m_dwListCtrlAutoHideTickCount = 0;
                    }
                }
                break;

            default:
                break;
            }

            return CallWindowProc(pDbClickListener->m_oldWndProc_ListCtrl, pDbClickListener->m_hListCtrl, uMsg, wParam, lParam);
        }

        return 0;
    }

    static LRESULT CALLBACK newWindowProc_ShellDll(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CMButtonUpListener *pDbClickListener = (CMButtonUpListener *)GetProp(hwnd, TEXT("CDbClickListener"));

        if (pDbClickListener != NULL && hwnd == pDbClickListener->m_hShellDll)
        {
            switch (uMsg)
            {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_MOUSEHWHEEL:
                if (!IsWindowVisible(pDbClickListener->m_hListCtrl))
                {
                    ShowWindow(pDbClickListener->m_hListCtrl, SW_SHOW);
                }
                break;

            default:
                break;
            }

            return CallWindowProc(pDbClickListener->m_oldWndProc_ShellDll, pDbClickListener->m_hShellDll, uMsg, wParam, lParam);
        }

        return 0;
    }

private:
    DWORD m_dwListCtrlAutoHideTickCount;
    HWND m_hListCtrl;
    HWND m_hShellDll;
    WNDPROC m_oldWndProc_ListCtrl;
    WNDPROC m_oldWndProc_ShellDll;
};

static HWND GetDesktopListView()
{
    list<HWND> listDesktopWindows = GetDoubtfulDesktopWindowsInSelfProcess();

    for (list<HWND>::iterator it = listDesktopWindows.begin(); it != listDesktopWindows.end(); ++it)
    {
        HWND hSHELLDLL_DefView = FindWindowEx(*it, NULL, TEXT("SHELLDLL_DefView"), TEXT(""));

        if (IsWindow(hSHELLDLL_DefView))
        {
            HWND hSysListView32 = FindWindowEx(hSHELLDLL_DefView, NULL, TEXT("SysListView32"), TEXT("FolderView"));

            if (!IsWindow(hSysListView32))
            {
                hSysListView32 = FindWindowEx(hSHELLDLL_DefView, NULL, TEXT("SysListView32"), NULL);
            }

            if (IsWindow(hSysListView32))
            {
                return hSysListView32;
            }
        }
    }

    return NULL;
}

static DWORD CALLBACK StartDesktopIconManager(LPVOID lpParam)
{
    HWND hListView = GetDesktopListView();

    while (!IsWindow(hListView))
    {
        Sleep(1101);
        hListView = GetDesktopListView();
    }

    CMButtonUpListener listener(hListView);

    while (IsWindow(hListView))
    {
        Sleep(101);
    }

    return 0;
}

BOOL StartDesktopIconManager()
{
    HANDLE hThread = CreateThread(NULL, 0, StartDesktopIconManager, NULL, 0, NULL);
    CloseHandle(hThread);

    return hThread != NULL;
}
