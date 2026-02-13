#include "SlxRunDlgPlus.h"
#include <Windows.h>
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "SlxElevateBridge.h"
#include <map>
#include <memory>
#include <vector>

using namespace std;

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

class CRunDlgPlus {
    enum {
        ID_ADMIN = 0xf3,
        ID_FORCE_ASINVOKER,
        ID_ADMIN_BRIDGE,
    };
public:
    CRunDlgPlus(HWND hDlg)
        : m_hDlg(hDlg)
        , m_procOldDlgProc(NULL)
        , m_hRunAdminMode(NULL)
        , m_hRunAdminModeByBridge(NULL)
        , m_hAsInvoker(NULL)
    {
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
                    HWND hCommit = GetDlgItem(hDlg, 1);
                    HWND hCancel = GetDlgItem(hDlg, 2);
                    m_hComboBox = FindWindowExW(m_hDlg, NULL, L"ComboBox", NULL);

                    if (IsWindow(hCommit) && IsWindow(hCancel) && IsWindow(m_hComboBox)) {
                        RECT rectCommit;
                        RECT rectCancel;
                        RECT rectRunAdminButton;

                        GetWindowRect(hCommit, &rectCommit);
                        GetWindowRect(hCancel, &rectCancel);

                        ScreenToClient(m_hDlg, (LPPOINT)&rectCommit);
                        ScreenToClient(m_hDlg, (LPPOINT)&rectCommit + 1);
                        ScreenToClient(m_hDlg, (LPPOINT)&rectCancel);
                        ScreenToClient(m_hDlg, (LPPOINT)&rectCancel + 1);

                        rectRunAdminButton.top = rectCommit.top;
                        rectRunAdminButton.bottom = rectCommit.bottom;
                        rectRunAdminButton.left = rectCommit.left * 2 - rectCancel.left;
                        rectRunAdminButton.right = rectCommit.right * 2 - rectCancel.right;

                        m_hRunAdminMode = CreateWindowExW(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            L"BUTTON",
                            L"提权(&R)",
                            WS_CHILDWINDOW | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT,
                            rectRunAdminButton.left,
                            rectRunAdminButton.top,
                            rectRunAdminButton.right - rectRunAdminButton.left,
                            rectRunAdminButton.bottom - rectRunAdminButton.top,
                            m_hDlg,
                            (HMENU)ID_ADMIN,
                            g_hinstDll,
                            NULL);
                        m_hAsInvoker = CreateWindowExW(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            L"BUTTON",
                            L"&AsInvoker",
                            WS_CHILDWINDOW | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT,
                            rectCommit.left,
                            rectCommit.top,
                            rectCommit.right - rectCommit.left,
                            rectCommit.bottom - rectCommit.top,
                            m_hDlg,
                            (HMENU)ID_FORCE_ASINVOKER,
                            g_hinstDll,
                            NULL);
                        m_hRunAdminModeByBridge = CreateWindowExW(
                            WS_EX_LEFT | WS_EX_LTRREADING,
                            L"BUTTON",
                            L"桥式提权(&E)",
                            WS_CHILDWINDOW | BS_PUSHBUTTON | BS_TEXT | SW_SHOW,
                            -110,
                            -110,
                            11,
                            11,
                            m_hDlg,
                            (HMENU)ID_ADMIN_BRIDGE,
                            g_hinstDll,
                            NULL);
                        // 移动ok -> cancel
                        SetWindowPos(hCommit, NULL, rectCancel.left, rectCancel.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                        // 移出cancel
                        SetWindowPos(hCancel, NULL, -220, -220, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

                        if (IsWindow(m_hRunAdminMode)) {
                            SendMessageW(m_hRunAdminMode, BCM_SETSHIELD, 0, TRUE);
                            SendMessageW(m_hRunAdminModeByBridge, BCM_SETSHIELD, 0, TRUE);

                            SetWindowPos(m_hRunAdminMode, GetWindow(hCommit, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                            SetWindowPos(m_hAsInvoker, m_hRunAdminMode, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

                            LRESULT hFont = SendMessageW(hCommit, WM_GETFONT, 0, 0);
                            SendMessageW(m_hRunAdminMode, WM_SETFONT, hFont, TRUE);
                            SendMessageW(m_hAsInvoker, WM_SETFONT, hFont, TRUE);
                            SendMessageW(m_hRunAdminModeByBridge, WM_SETFONT, hFont, TRUE);

                            ShowWindow(m_hRunAdminMode, SW_SHOW);
                            ShowWindow(m_hAsInvoker, SW_SHOW);
                            ShowWindow(m_hRunAdminModeByBridge, SW_SHOW);
                            ShowWindow(hCancel, SW_HIDE);
                        }

                        PostMessageW(m_hDlg, WM_COMMAND, MAKELONG(0, CBN_EDITCHANGE), (LPARAM)m_hComboBox);
                    }
                }
                return 0;

            case WM_COMMAND:
                if ((LOWORD(wParam) == ID_ADMIN || LOWORD(wParam) == ID_ADMIN_BRIDGE) &&
                    HIWORD(wParam) == BN_CLICKED &&
                    IsWindow(m_hComboBox))
                {
                    WCHAR szInput[8192] = L"";
                    WCHAR szArgumets[10240] = L"/c start ";
                    SHELLEXECUTEINFOW si = { sizeof(si) };
                    BOOL bSucceed = FALSE;

                    GetWindowTextW(m_hComboBox, szInput, RTL_NUMBER_OF(szInput));
                    StrCatBuffW(szArgumets, szInput, RTL_NUMBER_OF(szArgumets));

                    if (LOWORD(wParam) == ID_ADMIN) {
                        si.hwnd = m_hDlg;
                        si.lpVerb = L"runas";
                        si.lpFile = L"cmd.exe";
                        si.lpParameters = szArgumets;
                        si.nShow = SW_HIDE;

                        bSucceed = ShellExecuteExW(&si);
                    } else if (LOWORD(wParam) == ID_ADMIN_BRIDGE) {
                        bSucceed = ElevateAndRunW(L"cmd.exe", szArgumets, NULL, SW_HIDE) > 32;
                    }

                    if (bSucceed) {
                        PostMessageW(m_hDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
                        UpdateRunMRU(szInput);
                    } else {
                        SetFocus(m_hComboBox);
                    }
                } else if (LOWORD(wParam) == ID_FORCE_ASINVOKER) {
                    WCHAR szInput[8192] = L"";
                    WCHAR szCmd[10240] = L"";
                    extern WCHAR g_szSlxComDllFullPath[];

                    GetWindowTextW(m_hComboBox, szInput, RTL_NUMBER_OF(szInput));
                    wnsprintfW(szCmd, RTL_NUMBER_OF(szCmd), L"rundll32.exe \"%ls\" StartupForceAsInvoker %ls", g_szSlxComDllFullPath, szInput);

                    if (RunCommand(szCmd)) {
                        PostMessageW(m_hDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
                        UpdateRunMRU(szInput);
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

    // 写入注册表HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU
    static void UpdateRunMRU(LPCWSTR lpInput) {
        if (lpInput == NULL || *lpInput == L'\0') {
            return;
        }

        // RunMRU 存储格式必须以 \1 结尾，否则 Explorer 无法正常解析显示
        std::wstring formattedInput = lpInput;
        formattedInput += L"\\1";

        HKEY hKey = NULL;
        const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            return;
        }

        // 读取当前记录列表
        wchar_t mruList[64] = { 0 };
        DWORD type = 0;
        DWORD size = sizeof(mruList);
        RegQueryValueExW(hKey, L"MRUList", NULL, &type, (LPBYTE)mruList, &size);

        wchar_t targetLetter = 0;
        size_t mruLen = wcslen(mruList);

        // 查找该命令是否已在现有字母中定义
        for (size_t i = 0; i < mruLen; ++i) {
            wchar_t valName[2] = { mruList[i], 0 };
            wchar_t existingData[MAX_PATH] = { 0 };
            DWORD dataSize = sizeof(existingData);
            if (RegQueryValueExW(hKey, valName, NULL, NULL, (LPBYTE)existingData, &dataSize) == ERROR_SUCCESS) {
                if (_wcsicmp(existingData, formattedInput.c_str()) == 0) {
                    targetLetter = mruList[i];
                    break;
                }
            }
        }

        // 如果是新命令，分配未使用的字母；若 26 个字母已满，则重用 MRUList 最后的字母
        if (targetLetter == 0) {
            bool used[26] = { false };
            for (size_t i = 0; i < mruLen; ++i) {
                if (mruList[i] >= L'a' && mruList[i] <= L'z') {
                    used[mruList[i] - L'a'] = true;
                }
            }

            for (int i = 0; i < 26; ++i) {
                if (!used[i]) {
                    targetLetter = L'a' + i;
                    break;
                }
            }

            if (targetLetter == 0) {
                targetLetter = (mruLen > 0) ? mruList[mruLen - 1] : L'a';
            }
        }

        // 写入具体命令内容
        wchar_t valName[2] = { targetLetter, 0 };
        RegSetValueExW(
            hKey,
            valName,
            0,
            REG_SZ,
            (const BYTE*)formattedInput.c_str(),
            (DWORD)((formattedInput.length() + 1) * sizeof(wchar_t)));

        // 更新 MRUList：将当前使用的字母移动到字符串首位（最顶层）
        std::wstring newList;
        newList += targetLetter;
        for (size_t i = 0; i < mruLen; ++i) {
            if (mruList[i] != targetLetter) {
                newList += mruList[i];
            }
        }

        RegSetValueExW(
            hKey,
            L"MRUList",
            0,
            REG_SZ,
            (const BYTE*)newList.c_str(),
            (DWORD)((newList.length() + 1) * sizeof(wchar_t)));

        RegCloseKey(hKey);
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
    HWND m_hAsInvoker;
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

        // 清理旧窗口
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

// 褫夺uac权限的申请，如果程序已经是管理员权限了，那么这个函数不会有任何效果；如果程序是被uac限制了权限的，那么这个函数会让程序以被uac限制的权限运行（也就是褫夺uac权限）
void WINAPI StartupForceAsInvokerW(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow) {
    SHELLEXECUTEINFOW si = { sizeof(si) };
    WCHAR szCmd[10240] = L"/c start ";
    
    SetEnvironmentVariableA("__COMPAT_LAYER", "RUNASINVOKER");
    StrCatBuffW(szCmd, lpszCmdLine, RTL_NUMBER_OF(szCmd));
    si.hwnd = hwndStub;
    si.lpVerb = L"open";
    si.lpFile = L"cmd.exe";
    si.lpParameters = szCmd;
    si.nShow = SW_HIDE;

    ShellExecuteExW(&si);
}