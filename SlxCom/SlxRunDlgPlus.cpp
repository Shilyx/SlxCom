#include "SlxRunDlgPlus.h"
#include <Windows.h>
#include "SlxComTools.h"
#include "SlxElevateBridge.h"
#include <map>
#include <memory>
#include <vector>

using namespace std;
using namespace tr1;

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

class CRunDlgPlus {
    enum {
        ID_ADMIN = 0xf3,
        ID_ADMIN_BRIDGE,
    };
public:
    CRunDlgPlus(HWND hDlg)
        : m_hDlg(hDlg)
        , m_procOldDlgProc(NULL)
        , m_hRunAdminMode(NULL)
        , m_hRunAdminModeByBridge(NULL) {
        if (IsWindow(m_hDlg)) {
            SetWindowLongPtr(m_hDlg, GWLP_USERDATA, (LONG_PTR)this);
            m_procOldDlgProc = (WNDPROC)SetWindowLongPtr(m_hDlg, GWLP_WNDPROC, (LONG_PTR)newRunDlgProcPublic);
            PostMessageW(m_hDlg, WM_USER, WM_USER + 1, WM_USER + 2);
        }
    }

    ~CRunDlgPlus() {
        if (IsWindow(m_hDlg)) {
            SetWindowLongPtr(m_hDlg, GWLP_WNDPROC, (LONG_PTR)m_procOldDlgProc);
            SetWindowLongPtr(m_hDlg, GWLP_USERDATA, 0);
            m_procOldDlgProc = NULL;
        }
    }

public:
    static BOOL IsRunDlgWindow(HWND hWindow) {
        if (!IsWindow(hWindow)) {
            return FALSE;
        }

        DWORD dwWindowProcessId = 0;

        GetWindowThreadProcessId(hWindow, &dwWindowProcessId);

        if (dwWindowProcessId != GetCurrentProcessId()) {
            return FALSE;
        }

        WCHAR szClassName[1024] = L"";

        GetClassNameW(hWindow, szClassName, RTL_NUMBER_OF(szClassName));

        if (lstrcmpW(szClassName, L"#32770") != 0) {
            return FALSE;
        }

        vector<HWND> vectorButtons = GetButtonsInWindow(hWindow);

        if (vectorButtons.size() < 3) {
            return FALSE;
        }

        RECT rectButtons[3];
        vector<HWND>::const_reverse_iterator it = vectorButtons.rbegin();
        for (int i = 0; i < 3; ++i, ++it) {
            GetWindowRect(*it, rectButtons + i);
        }

        if (rectButtons[0].top != rectButtons[1].top ||
            rectButtons[0].top != rectButtons[2].top ||
            rectButtons[0].bottom != rectButtons[1].bottom ||
            rectButtons[0].bottom != rectButtons[2].bottom ||
            abs(rectButtons[0].left + rectButtons[2].left - rectButtons[1].left * 2) > 4) {
            return FALSE;
        }

        HWND hComboBox = FindWindowExW(hWindow, NULL, L"ComboBox", NULL);

        if (!IsWindow(hComboBox) || !IsWindow(FindWindowExW(hComboBox, NULL, NULL, NULL))) {
            return FALSE;
        }

        HWND hFirstChild = FindWindowExW(hWindow, NULL, NULL, NULL);
        HWND hFirstStatic = FindWindowExW(hWindow, NULL, L"Static", NULL);
        HWND hSecondStatic = FindWindowExW(hWindow, hFirstStatic, L"Static", NULL);

        if (hFirstStatic != hFirstStatic || !IsWindow(hSecondStatic)) {
            return FALSE;
        }

        return TRUE;
    }

private:
    LRESULT newRunDlgProcPrivate(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (hDlg == m_hDlg) {
            switch (uMsg) {
            case WM_USER:
                if (wParam == WM_USER + 1 && lParam == WM_USER + 2) {
                    vector<HWND> vectorButtons = GetButtonsInWindow(m_hDlg);

                    if (vectorButtons.size() >= 3) {
                        RECT rectButtons[3];
                        RECT rectNewButton;
                        vector<HWND>::const_reverse_iterator it = vectorButtons.rbegin();
                        HWND hCommit = NULL;
                        m_hComboBox = FindWindowExW(m_hDlg, NULL, L"ComboBox", NULL);

                        for (int i = 0; i < 3; ++i, ++it) {
                            GetWindowRect(*it, rectButtons + i);
                            hCommit = *it;
                        }

                        rectNewButton.top = rectButtons[2].top;
                        rectNewButton.bottom = rectButtons[2].bottom;
                        rectNewButton.left = rectButtons[2].left * 2 - rectButtons[1].left;
                        rectNewButton.right = rectButtons[2].right * 2 - rectButtons[1].right;

                        ScreenToClient(m_hDlg, (LPPOINT)&rectNewButton);
                        ScreenToClient(m_hDlg, (LPPOINT)&rectNewButton + 1);

                        m_hRunAdminMode = CreateWindowExW(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            L"BUTTON",
                            L"��Ȩ(&R)",
                            WS_CHILDWINDOW | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT,
                            rectNewButton.left,
                            rectNewButton.top,
                            rectNewButton.right - rectNewButton.left,
                            rectNewButton.bottom - rectNewButton.top,
                            m_hDlg,
                            (HMENU)ID_ADMIN,
                            g_hinstDll,
                            NULL);
                        m_hRunAdminModeByBridge = CreateWindowExW(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            L"BUTTON",
                            L"��ʽ��Ȩ(&E)",
                            WS_CHILDWINDOW | BS_PUSHBUTTON | BS_TEXT | SW_SHOW,
                            -110,
                            -110,
                            11,
                            11,
                            m_hDlg,
                            (HMENU)ID_ADMIN_BRIDGE,
                            g_hinstDll,
                            NULL);

                        if (IsWindow(m_hRunAdminMode)) {
                            SendMessageW(m_hRunAdminMode, BCM_SETSHIELD, 0, TRUE);
                            SetWindowPos(m_hRunAdminMode, GetWindow(hCommit, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                            SendMessageW(m_hRunAdminMode, WM_SETFONT, SendMessageW(*vectorButtons.rbegin(), WM_GETFONT, 0, 0), TRUE);
                            ShowWindow(m_hRunAdminMode, SW_SHOW);
                        }

                        PostMessageW(m_hDlg, WM_COMMAND, MAKELONG(0, CBN_EDITCHANGE), (LPARAM)m_hComboBox);
                    }
                }
                return 0;

            case WM_COMMAND:
                if ((LOWORD(wParam) == ID_ADMIN || LOWORD(wParam) == ID_ADMIN_BRIDGE) &&
                    HIWORD(wParam) == BN_CLICKED &&
                    IsWindow(m_hComboBox)) {
                    WCHAR szText[8192] = L"/c start ";
                    int nLength = lstrlenW(szText);
                    SHELLEXECUTEINFOW si = {sizeof(si)};
                    BOOL bSucceed = FALSE;

                    GetWindowTextW(m_hComboBox, szText + nLength, RTL_NUMBER_OF(szText) - nLength);

                    if (LOWORD(wParam) == ID_ADMIN) {
                        si.hwnd = m_hDlg;
                        si.lpVerb = L"runas";
                        si.lpFile = L"cmd.exe";
                        si.lpParameters = szText;
                        si.nShow = SW_HIDE;

                        bSucceed = ShellExecuteExW(&si);
                    } else if (LOWORD(wParam) == ID_ADMIN_BRIDGE) {
                        bSucceed = ElevateAndRunW(L"cmd.exe", szText, NULL, SW_HIDE) > 32;
                    }

                    if (bSucceed) {
                        PostMessageW(m_hDlg, WM_SYSCOMMAND, SC_CLOSE, 0);

                        // д��ע���HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU
                    } else {
                        SetFocus(m_hComboBox);
                    }
                } else if (HIWORD(wParam) == CBN_EDITCHANGE && m_hComboBox == (HWND)lParam) {
                    BOOL bShouldEnabled = GetWindowTextLength(m_hComboBox) > 0;

                    EnableWindow(m_hRunAdminMode, bShouldEnabled);
                    EnableWindow(m_hRunAdminModeByBridge, bShouldEnabled);
                }
                break;

            default:
                break;
            }
        }

        return CallWindowProc(m_procOldDlgProc, hDlg, uMsg, wParam, lParam);
    }

    static LRESULT CALLBACK newRunDlgProcPublic(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        CRunDlgPlus *pRunDlgPlusObject = GetRunDlgPlusObject(hDlg);

        if (pRunDlgPlusObject != NULL) {
            return pRunDlgPlusObject->newRunDlgProcPrivate(hDlg, uMsg, wParam, lParam);
        } else {
            return DefWindowProc(hDlg, uMsg, wParam, lParam);
        }
    }

    static CRunDlgPlus *GetRunDlgPlusObject(HWND hDlg) {
        return (CRunDlgPlus *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }

    static vector<HWND> GetButtonsInWindow(HWND hWindow) {
        vector<HWND> vectorButtons;
        HWND hButton = FindWindowExW(hWindow, NULL, L"BUTTON", NULL);

        while (IsWindow(hButton)) {
            vectorButtons.push_back(hButton);
            hButton = FindWindowExW(hWindow, hButton, L"BUTTON", NULL);
        }

        return vectorButtons;
    }

private:
    HWND m_hDlg;
    WNDPROC m_procOldDlgProc;
    HWND m_hRunAdminMode;
    HWND m_hRunAdminModeByBridge;
    HWND m_hComboBox;
};

static DWORD CALLBACK RunDlgPlusMonitorProc(LPVOID lpParam) {
    HWND hLastWindow = NULL;
    DWORD dwTimerIndex = 0;
    map<HWND, shared_ptr<CRunDlgPlus> > mapManagedDlgs;

    while (true) {
        HWND hWindow = GetForegroundWindow();

        if (hWindow != hLastWindow) {
            hLastWindow = hWindow;

            if (mapManagedDlgs.count(hWindow) == 0 && CRunDlgPlus::IsRunDlgWindow(hWindow)) {
                mapManagedDlgs.insert(make_pair(hWindow, new CRunDlgPlus(hWindow)));
            }
        }

        // ����ɴ���
        if (dwTimerIndex % (10 * 30) == 0) {
            map<HWND, shared_ptr<CRunDlgPlus> >::iterator it = mapManagedDlgs.begin();

            while (it != mapManagedDlgs.end()) {
                if (!IsWindow(it->first)) {
                    it = mapManagedDlgs.erase(it);
                } else {
                    ++it;
                }
            }
        }

        ++dwTimerIndex;
        Sleep(100);
    }

    return 0;
}

void StartRunDlgPlusMonitor() {
    CloseHandle(CreateThread(NULL, 0, RunDlgPlusMonitorProc, NULL, 0, NULL));
}
