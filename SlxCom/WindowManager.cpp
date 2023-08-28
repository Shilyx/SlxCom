#include "WindowManager.h"
#include "SlxComTools.h"
#include "SlxComAbout.h"
#include <WindowsX.h>
#include <Shlwapi.h>
#include <map>
#include <set>
#include <iomanip>
#include "lib/charconv.h"

using namespace std;

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

#define NOTIFYWNDCLASS  L"__slx_WindowManager_20140629"
#define BASE_REG_PATH   L"Software\\Shilyx Studio\\SlxCom\\WindowManager"
#define RECORD_REG_PATH L"Software\\Shilyx Studio\\SlxCom\\WindowManager\\Records"

enum
{
    WM_CALLBACK = WM_USER + 12,     // 托盘图标的回调消息
    WM_REMOVE_NOTIFY,               // 通知主窗口销毁某托盘，wparam为id，lparam为hwnd
};

enum
{
    ID_CHECK = 12,
};

enum
{
    HK_HIDEWINDOW = 12,
    HK_HIDEWINDOW2,
    HK_PINWINDOW,
    HK_PINWINDOW2,
    HK_ADDWINDOW,
    HK_ADDWINDOW2,
};

enum
{
    CMD_ABOUT = 1,          // 关于
    CMD_DESTROYONSHOW,      // 控制是否在窗口显示时销毁托盘图标
    CMD_SHOWBALLOON,        // 控制是否显示气球
    CMD_HIDEONMINIMAZE,     // 控制是否在最小化时自动隐藏
    CMD_PINWINDOW,          // 指定窗口
    CMD_SWITCHVISIABLE,     // 显示或隐藏窗口
    CMD_DETAIL,             // 查看窗口详细信息
    CMD_DESTROYICON,        // 销毁托盘图标

    CMD_ADD_WINDOW,         // 收纳窗口
    CMD_HIDE_WINDOW,        // 收纳并隐藏窗口
    CMD_COPY_HWNDVALUE,     // 复制到剪贴板，句柄值
    CMD_COPY_CLASSNAME,     // 复制到剪贴板，窗口类名
    CMD_COPY_WINDOWTEXT,    // 复制到剪贴板，窗口标题
    CMD_COPY_CHILDTREE,     // 复制到剪贴板，子窗口树
    CMD_COPY_PROCESSID,     // 复制到剪贴板，进程id
    CMD_COPY_IMAGEPATH,     // 复制到剪贴板，进程路径
    CMD_COPY_IMAGENAME,     // 复制到剪贴板，进程名
    CMD_COPY_THREADID,      // 复制到剪贴板，线程id
    CMD_ALPHA_100,          // 不透明度，100%
    CMD_ALPHA_80,           // 不透明度，80%
    CMD_ALPHA_60,           // 不透明度，60%
    CMD_ALPHA_40,           // 不透明度，40%
    CMD_ALPHA_20,           // 不透明度，20%
    CMD_OPEN_IMAGE_PATH,    // 打开进程所在目录
    CMD_KILL_WINDOW_PROCESS,// 杀死窗口进程
};

class CNotifyClass
{
public:
    CNotifyClass(HWND hManagerWindow, HWND hTargetWindow, UINT uId, LPCWSTR lpCreateTime, BOOL bShowBalloonThisTime)
    {
        m_bUseNotifyIcon = uId > 0;

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

        if (m_bUseNotifyIcon)
        {
            ZeroMemory(&m_nid, sizeof(m_nid));
            m_nid.cbSize = sizeof(m_nid);
            m_nid.hWnd = hManagerWindow;
            m_nid.uID = uId;
            m_nid.uCallbackMessage = WM_CALLBACK;

            lstrcpynW(m_nid.szTip, GetTargetWindowBaseInfo().c_str(), RTL_NUMBER_OF(m_nid.szTip));

            m_hIcon = GetWindowIcon(m_hTargetWindow);
            m_hIconSm = GetWindowIconSmall(m_hTargetWindow);

            if (m_hIcon == NULL)
            {
                m_hIcon = LoadIconW(NULL, IDI_INFORMATION);
            }

            if (m_hIconSm == NULL)
            {
                m_hIconSm = m_hIcon;
            }

            m_nid.hIcon = m_hIconSm;
            m_nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;

            if (ms_bShowBalloon && bShowBalloonThisTime)
            {
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

    ~CNotifyClass()
    {
        if (m_bUseNotifyIcon)
        {
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            ReleaseTargetWindow();
            RemoveFromRegistry();
        }
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
                ReleaseTargetWindow();

                if (ms_bDestroyOnShow)
                {
                    PostMessageW(m_hManagerWindow, WM_REMOVE_NOTIFY, m_nid.uID, (LPARAM)m_hTargetWindow);
                }
            }
        }
    }

    void DoContextMenu(int x, int y)
    {
        WCHAR szSimpleText[8192] = L"";
        HMENU hMenu = CreatePopupMenu();
        HMENU hPopupMenu = CreatePopupMenu();

        AppendMenuW(hMenu, MF_STRING, CMD_ABOUT, L"关于SlxCom(&A)...");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, CMD_DETAIL, L"显示窗口信息(&D)");
        AppendMenuW(hMenu, MF_STRING, CMD_PINWINDOW, L"置顶窗口(&T)\tAlt+Ctrl(Shift)+T");

        if (m_bUseNotifyIcon)
        {
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
        }
        else
        {
            AppendMenuW(hMenu, MF_STRING, CMD_ADD_WINDOW, L"收纳窗口(&C)\tAlt+Ctrl(Shift)+C");
            AppendMenuW(hMenu, MF_STRING, CMD_HIDE_WINDOW, L"收纳并隐藏窗口(&S)\tAlt+Ctrl(Shift)+H");
        }

        AppendMenuW(hMenu, MF_STRING, CMD_OPEN_IMAGE_PATH, L"打开窗口进程所在的目录");
        AppendMenuW(hMenu, MF_STRING, CMD_KILL_WINDOW_PROCESS, L"结束窗口所属的进程");

        {
            HMENU hPopupMenu = CreatePopupMenu();

            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_HWNDVALUE,  L"窗口句柄值");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_CLASSNAME,  L"窗口类名");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_WINDOWTEXT, L"窗口标题");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_CHILDTREE,  L"整个窗口树");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_PROCESSID,  L"进程id");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_IMAGEPATH,  L"进程路径");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_IMAGENAME,  L"进程名");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_COPY_THREADID,   L"线程id");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, L"复制到剪贴板(&C)");
        }

        {
            HMENU hPopupMenu = CreatePopupMenu();

            AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_100, L"100%");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_80,  L"80%");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_60,  L"60%");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_40,  L"40%");
            AppendMenuW(hPopupMenu, MF_STRING, CMD_ALPHA_20,  L"20%");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopupMenu, L"不透明度(&A)");
        }

        CheckMenuItemHelper(hMenu, CMD_PINWINDOW, MF_BYCOMMAND, IsWindowTopMost(m_hTargetWindow));

        if (IsWindow(m_hTargetWindow))
        {
            if (IsWindowVisible(m_hTargetWindow))
            {
                ModifyMenuW(hMenu, CMD_SWITCHVISIABLE, MF_BYCOMMAND | MF_STRING, CMD_SWITCHVISIABLE, L"隐藏窗口(&H)\tAlt+Ctrl(Shift)+H");
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

        case CMD_COPY_PROCESSID:
            {
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
            if (GetWindowImageFileName(m_hTargetWindow, szSimpleText, RTL_NUMBER_OF(szSimpleText)) > 0)
            {
                if (PathFileExistsW(szSimpleText))
                {
                    BrowseForFile(szSimpleText);
                }
            }
            break;

        case CMD_KILL_WINDOW_PROCESS:
            {
                DWORD dwProcessId = 0;

                GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);

                if (dwProcessId != 0)
                {
                    WCHAR szProcessName[MAX_PATH] = L"null";

                    GetProcessNameById(dwProcessId, szProcessName, RTL_NUMBER_OF(szProcessName));

                    if (IDYES == MessageBoxFormat(
                        m_hManagerWindow,
                        L"请确认",
                        MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3,
                        L"要结束进程（%s，%lu）吗？",
                        szProcessName,
                        dwProcessId))
                    {
                        KillProcess(dwProcessId);
                    }
                }
            }
            break;

        default:
            break;
        }

        if (!IsWindow(m_hTargetWindow))
        {
            DestroyMenu(hPopupMenu);
        }

        DestroyMenu(hMenu);
    }

    wstring GetTargetWindowFullInfo()
    {
        LPCWSTR lpCrLf = L"\r\n";
        wstringstream ss;
        DWORD dwProcessId = 0;
        DWORD dwThreadId = 0;
        WCHAR szClassName[1024] = L"";
        WCHAR szWindowText[1024] = L"";
        RECT rect = {0};
        DWORD dwStyle = 0;
        DWORD dwExStyle = 0;

        if (IsWindow(m_hTargetWindow))
        {
            dwThreadId = GetWindowThreadProcessId(m_hTargetWindow, &dwProcessId);
            GetClassNameW(m_hTargetWindow, szClassName, RTL_NUMBER_OF(szClassName));
            GetWindowTextW(m_hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));
            GetWindowRect(m_hTargetWindow, &rect);
            dwStyle = (DWORD)GetWindowLongPtr(m_hTargetWindow, GWL_STYLE);
            dwExStyle = (DWORD)GetWindowLongPtr(m_hTargetWindow, GWL_EXSTYLE);
        }

        ss<<L"窗口句柄：0x"<<hex<<m_hTargetWindow;
        if (!IsWindow(m_hTargetWindow))
        {
            ss<<L"，无效";
        }
        else
        {
            if (IsWindowTopMost(m_hTargetWindow))
            {
                ss<<L"，置顶";
            }

            if (IsWindowVisible(m_hTargetWindow))
            {
                ss<<L"，可见";
            }
            else
            {
                ss<<L"，隐藏";
            }

            if (IsZoomed(m_hTargetWindow))
            {
                ss<<L"，最大化";
            }

            if (IsIconic(m_hTargetWindow))
            {
                ss<<L"，最小化";
            }
        }
        ss<<lpCrLf;

        ss<<L"窗口类："<<szClassName<<lpCrLf;
        ss<<L"窗口标题："<<szWindowText<<lpCrLf;
        ss<<L"线程id：0x"<<hex<<dwThreadId<<L"("<<dec<<dwThreadId<<L")"<<lpCrLf;
        ss<<L"进程id：0x"<<hex<<dwProcessId<<L"("<<dec<<dwProcessId<<L")"<<lpCrLf;
        ss<<L"风格：0x"<<hex<<dwStyle<<lpCrLf;
        ss<<L"扩展风格：0x"<<hex<<dwExStyle<<lpCrLf;
        ss<<L"当前位置："<<dec<<rect.left<<L"，"<<rect.top<<L"，"<<rect.right<<L"，"<<rect.bottom<<lpCrLf;
        ss<<L"收纳于："<<m_strCreateTime<<lpCrLf;

        return ss.str();
    }

    void RefreshInfo()
    {
        wstring strNewTip = GetTargetWindowBaseInfo();

        // 
        if (StrCmpNIW(strNewTip.c_str(), m_nid.szTip, RTL_NUMBER_OF(m_nid.szTip)) != 0)
        {
            lstrcpynW(m_nid.szTip, strNewTip.c_str(), RTL_NUMBER_OF(m_nid.szTip));
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
        }
    }

    wstring GetTargetWindowCaption()
    {
        wstring result;
        WCHAR szWindowText[32] = L"";

        GetWindowTextW(m_hTargetWindow, szWindowText, RTL_NUMBER_OF(szWindowText));
        result = szWindowText;

        if (GetWindowTextLength(m_hTargetWindow) >= RTL_NUMBER_OF(szWindowText))
        {
            result += L"...";
        }

        return result;
    }

    wstring GetTargetWindowBaseInfo()
    {
        WCHAR szTip[128];
        WCHAR szWindowStatus[32] = L"";
        BOOL bIsWindow = IsWindow(m_hTargetWindow);
        BOOL bIsWindowVisiable = FALSE;
        LPCWSTR lpCrLf = L"\r\n";

        if (bIsWindow)
        {
            bIsWindowVisiable = IsWindowVisible(m_hTargetWindow);

            if (bIsWindowVisiable)
            {
                lstrcpynW(szWindowStatus, L"可见", RTL_NUMBER_OF(szWindowStatus));
            }
            else
            {
                lstrcpynW(szWindowStatus, L"隐藏", RTL_NUMBER_OF(szWindowStatus));
            }
        }
        else
        {
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

    static void DoPin(HWND hTargetWindow)
    {
        CloseHandle(CreateThread(NULL, 0, PinWindowProc, (LPVOID)hTargetWindow, 0, NULL));
    }

    // 得到一个当前可捕获的窗口
    // 满足条件：非桌面，是最前的，不在同一进程内，或在同一进程内但可最小化的
    static HWND GetCaptureableWindow()
    {
        HWND hTargetWindow = GetForegroundWindow();

        if (IsWindow(hTargetWindow) && IsWindowVisible(hTargetWindow))
        {
            DWORD dwTargetProcessId = GetCurrentProcessId();
            HWND hDesktopWindow = GetDesktopWindow();

            GetWindowThreadProcessId(hTargetWindow, &dwTargetProcessId);

            if (dwTargetProcessId != GetCurrentProcessId())
            {
                return hTargetWindow;
            }
            else
            {
                LONG_PTR dwStype = GetWindowLongPtr(hTargetWindow, GWL_STYLE);

                if (dwStype != 0 && (dwStype & WS_MINIMIZEBOX) != 0)
                {
                    return hTargetWindow;
                }
                else
                {
                    return NULL;
                }
            }
        }
        else
        {
            return NULL;
        }
    }

    static map<HWND, wstring> GetAndClearLastNotifys()
    {
        map<HWND, wstring> result;
        set<wstring> setToDelete;
        wstring strRegPath = RECORD_REG_PATH;
        WCHAR szHwnd[32] = L"";
        HKEY hKey = NULL;

        strRegPath += L"\\";
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));

        if (ERROR_SUCCESS == RegOpenKeyExW(HKEY_CURRENT_USER, strRegPath.c_str(), 0, KEY_QUERY_VALUE, &hKey))
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

                WCHAR *szValueName = (WCHAR *)malloc(dwMaxValueNameLen);
                unsigned char *szData = (unsigned char *)malloc(dwMaxDataLen);

                if (szValueName != NULL && szData != NULL)
                {
                    for (DWORD dwIndex = 0; dwIndex < dwValueCount; dwIndex += 1)
                    {
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
                            ))
                        {
                            if (dwRegType == REG_SZ)
                            {
                                HWND hWnd = (HWND)(INT_PTR)StrToInt64Def(szValueName, 0);

                                if (!IsWindow(hWnd))
                                {
                                    setToDelete.insert(szValueName);
                                }
                                else
                                {
                                    result.insert(make_pair(hWnd, (WCHAR *)szData));
                                }
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

        set<wstring>::iterator it = setToDelete.begin();
        for (; it != setToDelete.end(); ++it)
        {
            SHDeleteValueW(HKEY_CURRENT_USER, strRegPath.c_str(), it->c_str());
        }

        return result;
    }

    static BOOL IsOptionHideOnMinimaze()
    {
        return ms_bHideOnMinimaze;
    }

private:
    void RecordToRegistry()
    {
        WCHAR szNull[] = L"";
        wstring strRegPath = RECORD_REG_PATH;
        WCHAR szHwnd[32] = L"";

        SHSetValueW(HKEY_CURRENT_USER, RECORD_REG_PATH, NULL, REG_SZ, szNull, (lstrlenW(szNull) + 1) * sizeof(WCHAR));

        strRegPath += L"\\";
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
        wnsprintfW(szHwnd, RTL_NUMBER_OF(szHwnd), L"%u", m_hTargetWindow);

        SHSetTempValue(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd, REG_SZ, m_strCreateTime.c_str(), (DWORD)(m_strCreateTime.length() + 1) * sizeof(WCHAR));
    }

    void RemoveFromRegistry()
    {
        wstring strRegPath = RECORD_REG_PATH;
        WCHAR szHwnd[32] = L"";

        strRegPath += L"\\";
        strRegPath += GetDesktopName(GetThreadDesktop(GetCurrentThreadId()));
        wnsprintfW(szHwnd, RTL_NUMBER_OF(szHwnd), L"%u", m_hTargetWindow);

        SHDeleteValueW(HKEY_CURRENT_USER, strRegPath.c_str(), szHwnd);
    }

    void ReleaseTargetWindow()
    {
        if (IsWindow(m_hTargetWindow))
        {
            if (!IsWindowVisible(m_hTargetWindow))
            {
                ShowWindow(m_hTargetWindow, SW_SHOW);
            }

            if (IsIconic(m_hTargetWindow))
            {
                ShowWindow(m_hTargetWindow, SW_RESTORE);
            }

            SetForegroundWindow(m_hTargetWindow);
        }
    }

    static DWORD CALLBACK PinWindowProc(LPVOID lpParam)
    {
        HWND hTargetWindow = (HWND)lpParam;

        if (hTargetWindow == NULL)
        {
            hTargetWindow = GetCaptureableWindow();
        }

        if (IsWindow(hTargetWindow))
        {
            LONG_PTR nExStyle = GetWindowLongPtr(hTargetWindow, GWL_EXSTYLE);
            WCHAR szNewWindowText[4096];
            LPWSTR lpWindowText = szNewWindowText;

            if (nExStyle & WS_EX_TOPMOST)
            {
                lpWindowText += wnsprintfW(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), L"已取消置顶<<<<");
                SetWindowPos(hTargetWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            else
            {
                lpWindowText += wnsprintfW(szNewWindowText, RTL_NUMBER_OF(szNewWindowText), L"已置顶>>>>");
                SetWindowPos(hTargetWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }

            if (GetWindowTextLength(hTargetWindow) < RTL_NUMBER_OF(szNewWindowText) - 100)
            {
                GetWindowTextW(hTargetWindow, lpWindowText, RTL_NUMBER_OF(szNewWindowText) - 100);
                SetWindowTextW(hTargetWindow, szNewWindowText);

                for (int i = 0; i < 3; i += 1)
                {
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

private:
    BOOL m_bUseNotifyIcon;
    HWND m_hManagerWindow;
    HWND m_hTargetWindow;
    NOTIFYICONDATAW m_nid;
    HICON m_hIcon;
    HICON m_hIconSm;
    wstring m_strCreateTime;

private:
    static BOOL ms_bDestroyOnShow;
    static BOOL ms_bShowBalloon;
    static BOOL ms_bHideOnMinimaze;
};

BOOL CNotifyClass::ms_bDestroyOnShow = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"DestroyOnShow", TRUE);
BOOL CNotifyClass::ms_bShowBalloon = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"ShowBalloon", TRUE);
BOOL CNotifyClass::ms_bHideOnMinimaze = RegGetDWORD(HKEY_CURRENT_USER, BASE_REG_PATH, L"HideOnMinimaze", FALSE);

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
            SetTimer(hWnd, ID_CHECK, 1888, NULL);
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
            if (wParam == ID_CHECK)
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