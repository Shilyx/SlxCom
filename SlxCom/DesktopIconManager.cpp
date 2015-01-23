#include "DesktopIconManager.h"
#include <CommCtrl.h>
#include <WindowsX.h>
#include "SlxComTools.h"

class CMButtonUpListener
{
public:
    CMButtonUpListener(HWND hListCtrl)
        : m_hListCtrl(hListCtrl)
        , m_hShellDll(NULL)
        , m_oldWndProc_ListCtrl(NULL)
        , m_oldWndProc_ShellDll(NULL)
    {
        if (IsWindow(m_hListCtrl))
        {
            m_hShellDll = GetParent(m_hListCtrl);

            m_oldWndProc_ListCtrl = (WNDPROC)SetWindowLongPtr(m_hListCtrl, GWLP_WNDPROC, (LONG_PTR)newWindowProc);
            SetProp(m_hListCtrl, TEXT("CDbClickListener"), (HANDLE)this);

            if (IsWindow(m_hShellDll))
            {
                m_oldWndProc_ShellDll = (WNDPROC)SetWindowLongPtr(m_hShellDll, GWLP_WNDPROC, (LONG_PTR)newWindowProc);
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
        }

        if (IsWindow(m_hShellDll))
        {
            SetWindowLongPtr(m_hShellDll, GWLP_WNDPROC, (LONG_PTR)m_oldWndProc_ShellDll);
            RemoveProp(m_hShellDll, TEXT("CDbClickListener"));
        }
    }

private:
    static INT_PTR GetHitTestPostion(HWND hwnd, POINT pt)
    {
        LVHITTESTINFO info = {pt};

        if (IsWindow(hwnd))
        {
            return SendMessage(hwnd, LVM_HITTEST, 0, (LPARAM)&info);
        }

        return -1;
    }

    static LRESULT CALLBACK newWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CMButtonUpListener *pDbClickListener = (CMButtonUpListener *)GetProp(hwnd, TEXT("CDbClickListener"));

        if (pDbClickListener != NULL)
        {
            if (pDbClickListener->m_hListCtrl == hwnd)
            {
                if (uMsg == WM_MBUTTONUP && IsWindowVisible(pDbClickListener->m_hListCtrl))
                {
                    ShowWindow(pDbClickListener->m_hListCtrl, SW_HIDE);
                }

                return CallWindowProc(pDbClickListener->m_oldWndProc_ListCtrl, pDbClickListener->m_hListCtrl, uMsg, wParam, lParam);
            }
            else if (pDbClickListener->m_hShellDll == hwnd)
            {
                if (uMsg == WM_MBUTTONUP && !IsWindowVisible(pDbClickListener->m_hListCtrl))
                {
                    ShowWindow(pDbClickListener->m_hListCtrl, SW_SHOW);
                }

                return CallWindowProc(pDbClickListener->m_oldWndProc_ShellDll, pDbClickListener->m_hShellDll, uMsg, wParam, lParam);
            }
        }

        return 0;
    }

private:
    HWND m_hListCtrl;
    HWND m_hShellDll;
    WNDPROC m_oldWndProc_ListCtrl;
    WNDPROC m_oldWndProc_ShellDll;
};

static HWND GetDesktopListView()
{
    HWND hProgman = FindWindowEx(NULL, NULL, TEXT("Progman"), TEXT("Program Manager"));

    while (IsWindow(hProgman))
    {
        DWORD dwProcessId = 0;

        GetWindowThreadProcessId(hProgman, &dwProcessId);

        if (dwProcessId == GetCurrentProcessId())
        {
            HWND hSHELLDLL_DefView = FindWindowEx(hProgman, NULL, TEXT("SHELLDLL_DefView"), TEXT(""));

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

        hProgman = FindWindowEx(NULL, hProgman, TEXT("Progman"), TEXT("Program Manager"));
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
