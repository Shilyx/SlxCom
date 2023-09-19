#include "NotifyClass.h"
#include "../SlxComTools.h"
#include "../SlxComAbout.h"
#include "WindowAppID.h"
#include <Shlwapi.h>
#include <set>

using namespace std;

#define BASE_REG_PATH   L"Software\\Shilyx Studio\\SlxCom\\WindowManager"
#define RECORD_REG_PATH L"Software\\Shilyx Studio\\SlxCom\\WindowManager\\Records"

extern BOOL g_bVistaLater; // SlxCom.cpp
BOOL CNotifyClass::ms_bDestroyOnShow = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"DestroyOnShow", TRUE);
BOOL CNotifyClass::ms_bShowBalloon = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"ShowBalloon", TRUE);
BOOL CNotifyClass::ms_bHideOnMinimaze = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"HideOnMinimaze", FALSE);

CNotifyClass::CNotifyClass(HWND hManagerWindow, HWND hTargetWindow, UINT uId, LPCWSTR lpCreateTime, BOOL bShowBalloonThisTime) {
    m_bUseNotifyIcon = uId > 0;

    if (lpCreateTime == NULL) {
        m_strCreateTime = GetCurrentTimeString();
    } else {
        m_strCreateTime = lpCreateTime;
    }

    m_hManagerWindow = hManagerWindow;
    m_hTargetWindow = hTargetWindow;

    if (m_bUseNotifyIcon) {
        ZeroMemory(&m_nid, sizeof(m_nid));
        m_nid.cbSize = sizeof(m_nid);
        m_nid.hWnd = hManagerWindow;
        m_nid.uID = uId;
        m_nid.uCallbackMessage = WM_CALLBACK;

        lstrcpynW(m_nid.szTip, GetTargetWindowBaseInfo().c_str(), RTL_NUMBER_OF(m_nid.szTip));

        m_hIcon = GetWindowIcon(m_hTargetWindow);
        m_hIconSm = GetWindowIconSmall(m_hTargetWindow);

        if (m_hIcon == NULL) {
            m_hIcon = LoadIconW(NULL, IDI_INFORMATION);
        }

        if (m_hIconSm == NULL) {
            m_hIconSm = m_hIcon;
        }

        m_nid.hIcon = m_hIconSm;
        m_nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;

        if (ms_bShowBalloon && bShowBalloonThisTime) {
            lstrcpynW(m_nid.szInfoTitle, L"SlxCom WindowManager", RTL_NUMBER_OF(m_nid.szInfoTitle));
            wnsprintfW(
                m_nid.szInfo,
                RTL_NUMBER_OF(m_nid.szInfo),
                L"窗口“%s”（句柄：%#x）已被收纳。",
                GetTargetWindowCaption().c_str(),
                hTargetWindow
                );

            m_nid.uFlags |= NIF_INFO;
        }

        Shell_NotifyIconW(NIM_ADD, &m_nid);
        m_nid.uFlags &= ~NIF_INFO;

        RecordToRegistry();
    }
}

CNotifyClass::~CNotifyClass() {
    if (m_bUseNotifyIcon) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        ReleaseTargetWindow();
        RemoveFromRegistry();
    }
}

UINT CNotifyClass::GetId() const {
    return m_nid.uID;
}

HWND CNotifyClass::GetTargetWindow() const {
    return m_hTargetWindow;
}

void CNotifyClass::SwitchVisiable() {
    if (IsWindow(m_hTargetWindow)) {
        if (IsWindowVisible(m_hTargetWindow)) {
            ShowWindow(m_hTargetWindow, SW_HIDE);
        } else {
            ReleaseTargetWindow();

            if (ms_bDestroyOnShow) {
                PostMessageW(m_hManagerWindow, WM_REMOVE_NOTIFY, m_nid.uID, (LPARAM)m_hTargetWindow);
            }
        }
    }
}

void CNotifyClass::DoContextMenu(int x, int y) {
    WCHAR szSimpleText[8192] = L"";
    HMENU hMenu = CreatePopupMenu();
    HMENU hPopupMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, CMD_ABOUT, L"关于SlxCom(&A)...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, CMD_DETAIL, L"显示窗口信息(&D)");

    if (g_bVistaLater) {
        AppendMenuW(hMenu, MF_STRING, CMD_UNGROUPING_WINDOW, L"在任务栏不分组此窗口(&G)\tAlt+Ctrl(Shift)+G");

        if (HasWinAppID(m_hTargetWindow)) {
            CheckMenuItemHelper(hPopupMenu, CMD_UNGROUPING_WINDOW, 0, TRUE);
        }
    }

    AppendMenuW(hMenu, MF_STRING, CMD_PINWINDOW, L"置顶窗口(&T)\tAlt+Ctrl(Shift)+T");

    if (m_bUseNotifyIcon) {
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, CMD_SWITCHVISIABLE, L"显示窗口(&S)\tAlt+Ctrl(Shift)+H");
        AppendMenuW(hMenu, MF_STRING, CMD_DESTROYICON, L"移除托盘图标(&Y)");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_DESTROYONSHOW, L"被控窗口可见后销毁托盘图标(&A)");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_SHOWBALLOON, L"窗口隐藏时显示气泡通知(&B)");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_HIDEONMINIMAZE, L"窗口最小化时候自动进入托盘(&M)");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, L"SlxCom托盘图标管理器全局配置(&G)");

        CheckMenuItemHelper(hPopupMenu, CMD_DESTROYONSHOW, MF_BYCOMMAND, ms_bDestroyOnShow);
        CheckMenuItemHelper(hPopupMenu, CMD_SHOWBALLOON, MF_BYCOMMAND, ms_bShowBalloon);
        CheckMenuItemHelper(hPopupMenu, CMD_HIDEONMINIMAZE, MF_BYCOMMAND, ms_bHideOnMinimaze);
        SetMenuDefaultItem(hMenu, CMD_SWITCHVISIABLE, MF_BYCOMMAND);
    } else {
        AppendMenuW(hMenu, MF_STRING, CMD_ADD_WINDOW, L"收纳窗口(&C)\tAlt+Ctrl(Shift)+C");
        AppendMenuW(hMenu, MF_STRING, CMD_HIDE_WINDOW, L"收纳并隐藏窗口(&S)\tAlt+Ctrl(Shift)+H");
    }

    AppendMenuW(hMenu, MF_STRING, CMD_OPEN_IMAGE_PATH, L"打开窗口进程所在的目录");
    AppendMenuW(hMenu, MF_STRING, CMD_KILL_WINDOW_PROCESS, L"结束窗口所属的进程");
 {
        HMENU hPopupMenu = CreatePopupMenu();

        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_HWNDVALUE, L"窗口句柄值");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_CLASSNAME, L"窗口类名");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_WINDOWTEXT, L"窗口标题");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_CHILDTREE, L"整个窗口树");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_PROCESSID, L"进程id");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_IMAGEPATH, L"进程路径");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_IMAGENAME, L"进程名");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_THREADID, L"线程id");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, L"复制到剪贴板(&C)");
    }
 {
        HMENU hPopupMenu = CreatePopupMenu();

        AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_100, L"100%");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_80, L"80%");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_60, L"60%");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_40, L"40%");
        AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_20, L"20%");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, L"不透明度(&A)");
    }

    CheckMenuItemHelper(hMenu, CMD_PINWINDOW, MF_BYCOMMAND, IsWindowTopMost(m_hTargetWindow));

    if (IsWindow(m_hTargetWindow)) {
        if (IsWindowVisible(m_hTargetWindow)) {
            ModifyMenuW(hMenu, CMD_SWITCHVISIABLE, MF_BYCOMMAND | MF_STRING, CMD_SWITCHVISIABLE, L"隐藏窗口(&H)\tAlt+Ctrl(Shift)+H");
        }
    } else {
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

    switch (nCmd) {
    case CMD_ABOUT:
        SlxComAbout(m_hManagerWindow);
        break;

    case CMD_DESTROYONSHOW:
        ms_bDestroyOnShow = !ms_bDestroyOnShow;
        SHSetValueW(HKEY_CURRENT_USER, BASE_REG_PATH, L"DestroyOnShow", REG_DWORD, &ms_bDestroyOnShow, sizeof(ms_bDestroyOnShow));
        break;

    case CMD_SHOWBALLOON:
        ms_bShowBalloon = !ms_bShowBalloon;
        SHSetValueW(HKEY_CURRENT_USER, BASE_REG_PATH, L"ShowBalloon", REG_DWORD, &ms_bShowBalloon, sizeof(ms_bShowBalloon));
        break;

    case CMD_HIDEONMINIMAZE:
        ms_bHideOnMinimaze = !ms_bHideOnMinimaze;
        SHSetValueW(HKEY_CURRENT_USER, BASE_REG_PATH, L"HideOnMinimaze", REG_DWORD, &ms_bHideOnMinimaze, sizeof(ms_bHideOnMinimaze));
        break;

    case CMD_PINWINDOW:
        DoPin(m_hTargetWindow);
        break;

    case CMD_SWITCHVISIABLE:
        SwitchVisiable();
        break;

    case CMD_DETAIL:
        MessageBoxW(NULL, GetTargetWindowFullInfo().c_str(), L"info", MB_ICONINFORMATION | MB_TOPMOST);
        break;

    case CMD_DESTROYICON:
        PostMessageW(m_hManagerWindow, WM_REMOVE_NOTIFY, m_nid.uID, (LPARAM)m_hTargetWindow);
        break;

    case CMD_ADD_WINDOW:
        break;

    case CMD_HIDE_WINDOW:
        break;

    case CMD_COPY_HWNDVALUE:
        SetClipboardText(AnyTypeToString((const void *)m_hTargetWindow).c_str());
        break;

    case CMD_COPY_CLASSNAME:
        GetClassNameW(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText));
        SetClipboardText(szSimpleText);
        break;

    case CMD_COPY_WINDOWTEXT:
        GetWindowTextW(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText));
        SetClipboardText(szSimpleText);
        break;

    case CMD_COPY_CHILDTREE:
        break;

    case CMD_COPY_PROCESSID: {
            DWORD dwProcessId = 0;

            GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);
            SetClipboardText(AnyTypeToString(dwProcessId).c_str());
        }
        break;

    case CMD_COPY_IMAGEPATH:
        GetWindowImageFileName(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText));
        SetClipboardText(szSimpleText);
        break;

    case CMD_COPY_IMAGENAME:
        GetWindowImageFileName(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText));
        SetClipboardText(PathFindFileNameW(szSimpleText));
        break;

    case CMD_COPY_THREADID:
        SetClipboardText(AnyTypeToString(GetWindowThreadProcessId(m_hTargetWindow, NULL)).c_str());
        break;

    case CMD_ALPHA_100:
        SetWindowUnalphaValue(m_hTargetWindow, WUV100);
        break;

    case CMD_ALPHA_80:
        SetWindowUnalphaValue(m_hTargetWindow, WUV80);
        break;

    case CMD_ALPHA_60:
        SetWindowUnalphaValue(m_hTargetWindow, WUV60);
        break;

    case CMD_ALPHA_40:
        SetWindowUnalphaValue(m_hTargetWindow, WUV40);
        break;

    case CMD_ALPHA_20:
        SetWindowUnalphaValue(m_hTargetWindow, WUV20);
        break;

    case CMD_OPEN_IMAGE_PATH:
        if (GetWindowImageFileName(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText)) > 0) {
            if (PathFileExistsW(szSimpleText)) {
                BrowseForFile(szSimpleText);
            }
        }
        break;

    case CMD_KILL_WINDOW_PROCESS: {
            DWORD dwProcessId = 0;

            GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);

            if (dwProcessId != 0) {
                WCHAR szProcessName[MAX_PATH] = L"null";

                GetProcessNameById(dwProcessId, szProcessName, RTL_NUMBER_OF(szProcessName));

                if (IDYES == MessageBoxFormat(
                    m_hManagerWindow,
                    L"请确认",
                    MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3,
                    L"要结束进程（%s，%lu）吗？",
                    szProcessName,
                    dwProcessId)) {
                    KillProcess(dwProcessId);
                }
            }
        }
        break;

    case CMD_UNGROUPING_WINDOW:
        ToggleWinAppID(m_hTargetWindow);
        break;

    default:
        break;
    }

    if (!IsWindow(m_hTargetWindow)) {
        DestroyMenu(hPopupMenu);
    }

    DestroyMenu(hMenu);
}

std::wstring CNotifyClass::GetTargetWindowFullInfo() {
    LPCWSTR lpCrLf = L"\r\n";
    wstringstream ss;
    DWORD dwProcessId = 0;
    DWORD dwThreadId = 0;
    WCHAR szClassName[1024] = L"";
    WCHAR szWindowText[1024] = L"";
    RECT rect = { 0 };
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;

    if (IsWindow(m_hTargetWindow)) {
        dwThreadId = GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);
        GetClassNameW(m_hTargetWindow, szClassName, RTL_NUMBER_OF(szClassName));
        GetWindowTextW(m_hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));
        GetWindowRect(m_hTargetWindow, &rect);
        dwStyle = (DWORD)GetWindowLongPtr(m_hTargetWindow, GWL_STYLE);
        dwExStyle = (DWORD)GetWindowLongPtr(m_hTargetWindow, GWL_EXSTYLE);
    }

    ss << L"窗口句柄：0x" << hex << m_hTargetWindow;
    if (!IsWindow(m_hTargetWindow)) {
        ss << L"，无效";
    } else {
        if (IsWindowTopMost(m_hTargetWindow)) {
            ss << L"，置顶";
        }

        if (IsWindowVisible(m_hTargetWindow)) {
            ss << L"，可见";
        } else {
            ss << L"，隐藏";
        }

        if (IsZoomed(m_hTargetWindow)) {
            ss << L"，最大化";
        }

        if (IsIconic(m_hTargetWindow)) {
            ss << L"，最小化";
        }
    }
    ss << lpCrLf;

    ss << L"窗口类：" << szClassName << lpCrLf;
    ss << L"窗口标题：" << szWindowText << lpCrLf;
    ss << L"线程id：0x" << hex << dwThreadId << L"(" << dec << dwThreadId << L")" << lpCrLf;
    ss << L"进程id：0x" << hex << dwProcessId << L"(" << dec << dwProcessId << L")" << lpCrLf;
    ss << L"风格：0x" << hex << dwStyle << lpCrLf;
    ss << L"扩展风格：0x" << hex << dwExStyle << lpCrLf;
    ss << L"当前位置：" << dec << rect.left << L"，" << rect.top << L"，" << rect.right << L"，" << rect.bottom << lpCrLf;
    ss << L"收纳于：" << m_strCreateTime << lpCrLf;

    return ss.str();
}

void CNotifyClass::RefreshInfo() {
    wstring strNewTip = GetTargetWindowBaseInfo();

    //
    if (StrCmpNIW(strNewTip.c_str(), m_nid.szTip, RTL_NUMBER_OF(m_nid.szTip)) != 0) {
        lstrcpynW(m_nid.szTip, strNewTip.c_str(), RTL_NUMBER_OF(m_nid.szTip));
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

std::wstring CNotifyClass::GetTargetWindowCaption() {
    wstring result;
    WCHAR szWindowText[32] = L"";

    GetWindowTextW(m_hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));
    result = szWindowText;

    if (GetWindowTextLength(m_hTargetWindow) >= RTL_NUMBER_OF(szWindowText)) {
        result += L"...";
    }

    return result;
}

std::wstring CNotifyClass::GetTargetWindowBaseInfo() {
    WCHAR szTip[128];
    WCHAR szWindowStatus[32] = L"";
    BOOL bIsWindow = IsWindow(m_hTargetWindow);
    BOOL bIsWindowVisiable = FALSE;
    LPCWSTR lpCrLf = L"\r\n";

    if (bIsWindow) {
        bIsWindowVisiable = IsWindowVisible(m_hTargetWindow);

        if (bIsWindowVisiable) {
            lstrcpynW(szWindowStatus, L"可见", RTL_NUMBER_OF(szWindowStatus));
        } else {
            lstrcpynW(szWindowStatus, L"隐藏", RTL_NUMBER_OF(szWindowStatus));
        }
    } else {
        lstrcpynW(szWindowStatus, L"无效", RTL_NUMBER_OF(szWindowStatus));
    }

    //
    wnsprintfW(
        szTip,
        RTL_NUMBER_OF(szTip),
        L"SlxCom WindowManager\r\n"
        L"窗口句柄：%#x\r\n"
        L"窗口状态：%s\r\n"
        L"窗口标题：%s",
        m_hTargetWindow,
        szWindowStatus,
        GetTargetWindowCaption().c_str()
        );

    return szTip;
}

void CNotifyClass::DoPin(HWND hTargetWindow) {
    CloseHandle(CreateThread(NULL, 0, PinWindowProc, (LPVOID)hTargetWindow, 0, NULL));
}

HWND CNotifyClass::GetCaptureableWindow() {
    HWND hTargetWindow = GetForegroundWindow();

    if (IsWindow(hTargetWindow) && IsWindowVisible(hTargetWindow)) {
        DWORD dwTargetProcessId = GetCurrentProcessId();
        HWND hDesktopWindow = GetDesktopWindow();

        GetWindowThreadProcessId(hTargetWindow, &dwTargetProcessId);

        if (dwTargetProcessId != GetCurrentProcessId()) {
            return hTargetWindow;
        } else {
            LONG_PTR dwStype = GetWindowLongPtr(hTargetWindow, GWL_STYLE);

            if (dwStype != 0 && (dwStype & WS_MINIMIZEBOX) != 0) {
                return hTargetWindow;
            } else {
                return NULL;
            }
        }
    } else {
        return NULL;
    }
}

map<HWND, wstring> CNotifyClass::GetAndClearLastNotifys() {
    map<HWND, wstring> result;
    set<wstring> setToDelete;
    wstring strRegPath = RECORD_REG_PATH;
    WCHAR szHwnd[32] = L"";
    HKEY hKey = NULL;

    strRegPath += L"\\";
    strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));

    if (ERROR_SUCCESS == RegOpenKeyExW(HKEY_CURRENT_USER, strRegPath.c_str(), 0, KEY_QUERY_VALUE, &hKey)) {
        DWORD dwValueCount = 0;
        DWORD dwMaxValueNameLen = 0;
        DWORD dwMaxDataLen = 0;

        if (ERROR_SUCCESS == RegQueryInfoKey(
            hKey, NULL, NULL, NULL, NULL, NULL, NULL,
            &dwValueCount,
            &dwMaxValueNameLen,
            &dwMaxDataLen,
            NULL, NULL
            )) {
            dwMaxValueNameLen += 1;
            dwMaxValueNameLen *= 2;
            dwMaxDataLen += 1;

            WCHAR *szValueName = (WCHAR *)malloc(dwMaxValueNameLen);
            unsigned char *szData = (unsigned char *)malloc(dwMaxDataLen);

            if (szValueName != NULL && szData != NULL) {
                for (DWORD dwIndex = 0; dwIndex < dwValueCount; dwIndex += 1) {
                    DWORD dwRegType = REG_NONE;
                    DWORD dwValueSize = dwMaxValueNameLen;
                    DWORD dwDataSize = dwMaxDataLen;

                    ZeroMemory(szValueName, dwMaxValueNameLen);
                    ZeroMemory(szData, dwMaxDataLen);

                    if (ERROR_SUCCESS == RegEnumValueW(
                        hKey,
                        dwIndex,
                        szValueName,
                        &dwValueSize,
                        NULL,
                        &dwRegType,
                        szData,
                        &dwDataSize
                        )) {
                        if (dwRegType == REG_SZ) {
                            HWND hWnd = (HWND)(INT_PTR)StrToInt64Def(szValueName, 0);

                            if (!IsWindow(hWnd)) {
                                setToDelete.insert(szValueName);
                            } else {
                                result.insert(make_pair(hWnd, (WCHAR *)szData));
                            }
                        }
                    }
                }
            }

            if (szValueName != NULL) {
                free((void *)szValueName);
            }

            if (szData != NULL) {
                free((void *)szData);
            }
        }

        RegCloseKey(hKey);
    }

    set<wstring>::iterator it = setToDelete.begin();
    for (; it != setToDelete.end(); ++it) {
        SHDeleteValueW(HKEY_CURRENT_USER, strRegPath.c_str(), it->c_str());
    }

    return result;
}

BOOL CNotifyClass::IsOptionHideOnMinimaze() {
    return ms_bHideOnMinimaze;
}

void CNotifyClass::RecordToRegistry() {
    WCHAR szNull[] = L"";
    wstring strRegPath = RECORD_REG_PATH;
    WCHAR szHwnd[32] = L"";

    SHSetValueW(HKEY_CURRENT_USER, RECORD_REG_PATH, NULL, REG_SZ, szNull, (lstrlenW(szNull) + 1) * sizeof(WCHAR));

    strRegPath += L"\\";
    strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
    wnsprintfW(szHwnd, RTL_NUMBER_OF(szHwnd), L"%u", m_hTargetWindow);

    SHSetTempValue(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd, REG_SZ, m_strCreateTime.c_str(), (DWORD)(m_strCreateTime.length() + 1) * sizeof(WCHAR));
}

void CNotifyClass::RemoveFromRegistry() {
    wstring strRegPath = RECORD_REG_PATH;
    WCHAR szHwnd[32] = L"";

    strRegPath += L"\\";
    strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
    wnsprintfW(szHwnd, RTL_NUMBER_OF(szHwnd), L"%u", m_hTargetWindow);

    SHDeleteValueW(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd);
}

void CNotifyClass::ReleaseTargetWindow() {
    if (IsWindow(m_hTargetWindow)) {
        if (!IsWindowVisible(m_hTargetWindow)) {
            ShowWindow(m_hTargetWindow, SW_SHOW);
        }

        if (IsIconic(m_hTargetWindow)) {
            ShowWindow(m_hTargetWindow, SW_RESTORE);
        }

        SetForegroundWindow(m_hTargetWindow);
    }
}

DWORD CALLBACK CNotifyClass::PinWindowProc(LPVOID lpParam) {
    HWND hTargetWindow = (HWND)lpParam;

    if (hTargetWindow == NULL) {
        hTargetWindow = GetCaptureableWindow();
    }

    if (IsWindow(hTargetWindow)) {
        LONG_PTR nExStyle = GetWindowLongPtr(hTargetWindow, GWL_EXSTYLE);
        WCHAR szNewWindowText[4096];
        LPWSTR lpWindowText = szNewWindowText;

        if (nExStyle & WS_EX_TOPMOST) {
            lpWindowText += wnsprintfW(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), L"已取消置顶<<<<");
            SetWindowPos(hTargetWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        } else {
            lpWindowText += wnsprintfW(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), L"已置顶>>>>");
            SetWindowPos(hTargetWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        if (GetWindowTextLength(hTargetWindow) < RTL_NUMBER_OF(szNewWindowText) - 100) {
            GetWindowTextW(hTargetWindow, lpWindowText, RTL_NUMBER_OF(szNewWindowText) - 100);
            SetWindowTextW(hTargetWindow, szNewWindowText);

            for (int i = 0; i < 3; i += 1) {
                FlashWindow(hTargetWindow, TRUE);
                Sleep(300);
                FlashWindow(hTargetWindow, TRUE);
            }

            Sleep(500);
            SetWindowTextW(hTargetWindow, lpWindowText);
        }
    }

    return 0;
}
