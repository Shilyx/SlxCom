#include "SlxRunDlgPlus.h"
#include <Windows.h>
#include "SlxComTools.h"
#include <map>
#include <memory>
#include <vector>

using namespace std;
using namespace tr1;

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

class CRunDlgPlus
{
public:
    CRunDlgPlus(HWND hDlg)
        : m_hDlg(hDlg)
        , m_procOldDlgProc(NULL)
        , m_hRunAdminMode(NULL)
    {
        if (IsWindow(m_hDlg))
        {
            SetWindowLongPtr(m_hDlg, GWLP_USERDATA, (LONG_PTR)this);
            m_procOldDlgProc = (WNDPROC)SetWindowLongPtr(m_hDlg, GWLP_WNDPROC, (LONG_PTR)newRunDlgProcPublic);
            PostMessage(m_hDlg, WM_USER, WM_USER + 1, WM_USER + 2);
        }
    }

    ~CRunDlgPlus()
    {
        if (IsWindow(m_hDlg))
        {
            SetWindowLongPtr(m_hDlg, GWLP_WNDPROC, (LONG_PTR)m_procOldDlgProc);
            SetWindowLongPtr(m_hDlg, GWLP_USERDATA, 0);
            m_procOldDlgProc = NULL;
        }
    }

public:
    static BOOL IsRunDlgWindow(HWND hWindow)
    {
        if (!IsWindow(hWindow))
        {
            return FALSE;
        }

        DWORD dwWindowProcessId = 0;

        GetWindowThreadProcessId(hWindow, &dwWindowProcessId);

        if (dwWindowProcessId != GetCurrentProcessId())
        {
            return FALSE;
        }

        TCHAR szClassName[1024] = TEXT("");

        GetClassName(hWindow, szClassName, RTL_NUMBER_OF(szClassName));

        if (lstrcmp(szClassName, TEXT("#32770")) != 0)
        {
            return FALSE;
        }

        vector<HWND> vectorButtons = GetButtonsInWindow(hWindow);

        if (vectorButtons.size() < 3)
        {
            return FALSE;
        }

        RECT rectButtons[3];
        vector<HWND>::const_reverse_iterator it = vectorButtons.rbegin();
        for (int i = 0; i < 3; ++i, ++it)
        {
            GetWindowRect(*it, rectButtons + i);
        }

        if (rectButtons[0].top != rectButtons[1].top ||
            rectButtons[0].top != rectButtons[2].top ||
            rectButtons[0].bottom != rectButtons[1].bottom ||
            rectButtons[0].bottom != rectButtons[2].bottom ||
            abs(rectButtons[0].left + rectButtons[2].left - rectButtons[1].left * 2) > 4)
        {
            return FALSE;
        }

        HWND hComboBox = FindWindowEx(hWindow, NULL, TEXT("ComboBox"), NULL);

        if (!IsWindow(hComboBox) || !IsWindow(FindWindowEx(hComboBox, NULL, NULL, NULL)))
        {
            return FALSE;
        }

        HWND hFirstChild = FindWindowEx(hWindow, NULL, NULL, NULL);
        HWND hFirstStatic = FindWindowEx(hWindow, NULL, TEXT("Static"), NULL);
        HWND hSecondStatic = FindWindowEx(hWindow, hFirstStatic, TEXT("Static"), NULL);

        if (hFirstStatic != hFirstStatic || !IsWindow(hSecondStatic))
        {
            return FALSE;
        }

        return TRUE;
    }

private:
    LRESULT newRunDlgProcPrivate(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (hDlg == m_hDlg)
        {
            switch (uMsg)
            {
            case WM_USER:
                if (wParam == WM_USER + 1 && lParam == WM_USER + 2)
                {
                    vector<HWND> vectorButtons = GetButtonsInWindow(m_hDlg);

                    if (vectorButtons.size() >= 3)
                    {
                        RECT rectButtons[3];
                        RECT rectNewButton;
                        vector<HWND>::const_reverse_iterator it = vectorButtons.rbegin();
                        HWND hCommit = NULL;
                        m_hComboBox = FindWindowEx(m_hDlg, NULL, TEXT("ComboBox"), NULL);

                        for (int i = 0; i < 3; ++i, ++it)
                        {
                            GetWindowRect(*it, rectButtons + i);
                            hCommit = *it;
                        }

                        rectNewButton.top = rectButtons[2].top;
                        rectNewButton.bottom = rectButtons[2].bottom;
                        rectNewButton.left = rectButtons[2].left * 2 - rectButtons[1].left;
                        rectNewButton.right = rectButtons[2].right * 2 - rectButtons[1].right;

                        ScreenToClient(m_hDlg, (LPPOINT)&rectNewButton);
                        ScreenToClient(m_hDlg, (LPPOINT)&rectNewButton + 1);

                        m_hRunAdminMode = CreateWindowEx(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            TEXT("BUTTON"),
                            TEXT("提权(&R)"),
                            WS_CHILDWINDOW | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT,
                            rectNewButton.left,
                            rectNewButton.top,
                            rectNewButton.right - rectNewButton.left,
                            rectNewButton.bottom - rectNewButton.top,
                            m_hDlg,
                            (HMENU)0x112,
                            g_hinstDll,
                            NULL);

                        if (IsWindow(m_hRunAdminMode))
                        {
                            SendMessage(m_hRunAdminMode, BCM_SETSHIELD, 0, TRUE);
                            SetWindowPos(m_hRunAdminMode, GetWindow(hCommit, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                            SendMessage(m_hRunAdminMode, WM_SETFONT, SendMessage(*vectorButtons.rbegin(), WM_GETFONT, 0, 0), TRUE);
                            ShowWindow(m_hRunAdminMode, SW_SHOW);
                            PostMessage(m_hDlg, WM_COMMAND, MAKELONG(0, CBN_EDITCHANGE), (LPARAM)m_hComboBox);
                        }
                    }
                }
                return 0;

            case WM_COMMAND:
                if (LOWORD(wParam) == 0x112 && HIWORD(wParam) == BN_CLICKED && (HWND)lParam == m_hRunAdminMode && IsWindow(m_hComboBox))
                {
                    WCHAR szText[8192] = TEXT("/c start ");
                    int nLength = lstrlenW(szText);
                    SHELLEXECUTEINFOW si = {sizeof(si)};

                    GetWindowTextW(m_hComboBox, szText + nLength, RTL_NUMBER_OF(szText) - nLength);

                    si.hwnd = m_hDlg;
                    si.lpVerb = L"runas";
                    si.lpFile = L"cmd.exe";
                    si.lpParameters = szText;
                    si.nShow = SW_HIDE;

                    if (ShellExecuteExW(&si))
                    {
                        PostMessage(m_hDlg, WM_SYSCOMMAND, SC_CLOSE, 0);

                        // 写入注册表HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU
                    }
                    else
                    {
                        SetFocus(m_hComboBox);
                    }
                }
                else if (HIWORD(wParam) == CBN_EDITCHANGE && m_hComboBox == (HWND)lParam)
                {
                    EnableWindow(m_hRunAdminMode, GetWindowTextLength(m_hComboBox) > 0);
                }
                break;

            default:
                break;
            }
        }

        return CallWindowProc(m_procOldDlgProc, hDlg, uMsg, wParam, lParam);
    }

    static LRESULT CALLBACK newRunDlgProcPublic(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CRunDlgPlus *pRunDlgPlusObject = GetRunDlgPlusObject(hDlg);

        if (pRunDlgPlusObject != NULL)
        {
            return pRunDlgPlusObject->newRunDlgProcPrivate(hDlg, uMsg, wParam, lParam);
        }
        else
        {
            return DefWindowProc(hDlg, uMsg, wParam, lParam);
        }
    }

    static CRunDlgPlus *GetRunDlgPlusObject(HWND hDlg)
    {
        return (CRunDlgPlus *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }

    static vector<HWND> GetButtonsInWindow(HWND hWindow)
    {
        vector<HWND> vectorButtons;
        HWND hButton = FindWindowEx(hWindow, NULL, TEXT("BUTTON"), NULL);

        while (IsWindow(hButton))
        {
            vectorButtons.push_back(hButton);
            hButton = FindWindowEx(hWindow, hButton, TEXT("BUTTON"), NULL);
        }

        return vectorButtons;
    }

private:
    HWND m_hDlg;
    WNDPROC m_procOldDlgProc;
    HWND m_hRunAdminMode;
    HWND m_hComboBox;
};

static DWORD CALLBACK RunDlgPlusMonitorProc(LPVOID lpParam)
{
    HWND hLastWindow = NULL;
    DWORD dwTimerIndex = 0;
    map<HWND, shared_ptr<CRunDlgPlus> > mapManagedDlgs;

    while (true)
    {
        HWND hWindow = GetForegroundWindow();

        if (hWindow != hLastWindow)
        {
            hLastWindow = hWindow;

            if (mapManagedDlgs.count(hWindow) == 0 && CRunDlgPlus::IsRunDlgWindow(hWindow))
            {
                mapManagedDlgs.insert(make_pair(hWindow, new CRunDlgPlus(hWindow)));
            }
        }

        // 清理旧窗口
        if (dwTimerIndex % (10 * 30) == 0)
        {
            map<HWND, shared_ptr<CRunDlgPlus> >::iterator it = mapManagedDlgs.begin();

            while (it != mapManagedDlgs.end())
            {
                if (!IsWindow(it->first))
                {
                    it = mapManagedDlgs.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        ++dwTimerIndex;
        Sleep(100);
    }

    return 0;
}

void StartRunDlgPlusMonitor()
{
    CloseHandle(CreateThread(NULL, 0, RunDlgPlusMonitorProc, NULL, 0, NULL));
}
