#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "resource.h"
#include <Shlwapi.h>
#include "SlxTimePlate.h"
#pragma warning(disable: 4786)
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "lib/charconv.h"
#include "SlxComAbout.h"
#include "SlxComConfigLight.h"
#include "SlxComDisableCtrlCopyInSameDir.h"
#include "lib/StringMatch.h"

using namespace std;
using namespace tr1;

#define NOTIFYWNDCLASS      L"__slxcom_work_ni_notify_wnd"
#define ICON_COUNT          10
#define TIMER_ICON          1
#define TIMER_MENU          2
#define TIMER_DEBUGBREAK    3
#define REG_QUESTION_TITLE  L"处理注册表路径不存在的问题"
#define COMPACT_WIDTH       400

enum {
    WM_CALLBACK = WM_USER + 112,
    WM_HIDEICON,
    WM_SET_DEBUGBREAK_TASK,
};

extern HBITMAP g_hKillExplorerBmp;  // SlxCom.cpp
extern BOOL g_bDebugMode;           // SlxCom.cpp
extern BOOL g_bVistaLater;          // SlxCom.cpp

static HHOOK g_hMsgHook = NULL;
static BOOL g_bChangeButtonText = FALSE;
static WCHAR g_szModulePath[MAX_PATH] = L"";
static struct {
    LPCWSTR lpName;
    LPCWSTR lpPath;
} g_szBuildinRegPaths[] = {
    { L"系统Run",          L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"},
#ifdef _WIN64
    { L"系统Run(32)",      L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run"},
#endif
    { L"用户Run",          L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"},
#ifdef _WIN64
    { L"用户Run(32)",      L"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run"},
#endif
    { L"系统WinLogon",     L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"},
#ifdef _WIN64
    { L"系统WinLogon(32)", L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"},
#endif
    { L"用户WinLogon",     L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"},
#ifdef _WIN64
    { L"用户WinLogon(32)", L"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"},
#endif
    { L"系统Windows",      L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"},
#ifdef _WIN64
    { L"系统Windows(32)",  L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows"},
#endif
    { L"用户Windows",      L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"},
#ifdef _WIN64
    { L"用户Windows(32)",  L"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows"},
#endif
    { L"系统Explorer",     L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"},
#ifdef _WIN64
    { L"系统Explorer(32)", L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"},
#endif
    { L"用户Explorer",     L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"},
#ifdef _WIN64
    { L"用户Explorer(32)", L"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"},
#endif
    { L"系统IE",           L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Internet Explorer"},
#ifdef _WIN64
    { L"系统IE(32)",       L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Internet Explorer"},
#endif
    { L"用户IE",           L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Internet Explorer"},
#ifdef _WIN64
    { L"用户IE(32)",       L"HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Internet Explorer"},
#endif
    { L"Session Manager",  L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager"},
    { L"映像劫持",          L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"},
    { L"已知dll",          L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\KnownDLLs"},
    { L"User Shell Folders", L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders"},
    { L"ShellIconOverlayIdentifiers", L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers"},
};

enum {
    SYS_QUIT = 1,
    SYS_HIDEICON,
    SYS_ABOUT,
    SYS_RESETEXPLORER,
    SYS_DISABLECTRLCOPYINSAMEDIR,
    SYS_DEBUGBREAK,
    SYS_WINDOWMANAGER,
    SYS_SHOWTIMEPALTE_DISABLE,
    SYS_SHOWTIMEPALTE_PER60M,
    SYS_SHOWTIMEPALTE_PER30M,
    SYS_SHOWTIMEPALTE_PER15M,
    SYS_SHOWTIMEPALTE_PER10M,
    SYS_SHOWTIMEPALTE_PER1M,
    SYS_UPDATEMENU,
    SYS_PAINTVIEW,
    SYS_MAXVALUE,
};

enum {
    HK_PAINTVIEW = 112,
};

class MenuItem {
public:
    MenuItem() : m_nCmdId(0) { }
    virtual ~MenuItem() { }
    virtual void DoJob(HWND hWindow, HINSTANCE hInstance) const = 0;
    virtual bool operator<(const MenuItem &other) const { return m_nCmdId < other.m_nCmdId;}
    virtual wstring GetMenuItemText() const { return L"菜单项"; }

protected:
    int m_nCmdId;
    wstring m_strText;
};

// 转义字符串
// 定位注册表
// 定位文件、目录
// 访问网页
// ping地址
enum CBType {
    CT_NONE = 0x0,
    CT_ESCAPE_STRING = 0x1,
    CT_LOCATE_REGPATH = 0x2,
    CT_LOCATE_FILEPATH = 0x4,
    CT_BROWSE_WEB_ADDRESS = 0x8,
    CT_PING_IP_ADDRESS = 0x10,
};

class MenuItemClipboardW : public MenuItem {
public:
    MenuItemClipboardW(int nCmdId, CBType cbType, LPCWSTR lpText)
        : m_cbType(cbType) {
        AssignString(m_strText, lpText);
    }

    virtual void DoJob(HWND hWindow, HINSTANCE hInstance) const {
        switch (m_cbType) {
        case CT_ESCAPE_STRING: {
                wstring strText = m_strText;
                Replace<wchar_t>(strText, L"\\", L"\\\\");
                Replace<wchar_t>(strText, L"\"", L"\\\"");
                Replace<wchar_t>(strText, L"\'", L"\\\'");
                Replace<wchar_t>(strText, L"\r", L"\\r");
                Replace<wchar_t>(strText, L"\n", L"\\n");
                Replace<wchar_t>(strText, L"\t", L"\\t");
                Replace<wchar_t>(strText, L"\b", L"\\b");
                SetClipboardText(strText.c_str());
            }
            break;

        case CT_LOCATE_REGPATH:
            BrowseForRegPath(m_strText.c_str());
            break;

        case CT_LOCATE_FILEPATH:
            BrowseForFile(m_strText.c_str());
            break;

        case CT_BROWSE_WEB_ADDRESS:
            ShellExecuteW(hWindow, L"open", m_strText.c_str(), NULL, NULL, SW_SHOW);
            break;

        case CT_PING_IP_ADDRESS:
            ShellExecuteW(hWindow, L"open", L"cmd", (L"/c ping " + m_strText + L" -t").c_str(), NULL, SW_SHOW);
            break;

        default:
            break;
        }
    }

    virtual wstring GetMenuItemText() const {
        wstring strMenuItemText = L"任务项";
        wstring strSubText;

        if (m_strText.length() > 65) {
            strSubText = m_strText.substr(0, 100);
            strSubText += L"...";
        } else {
            strSubText = m_strText;
        }

        switch (m_cbType) {
        case CT_ESCAPE_STRING:
            strMenuItemText = L"转义后复制到剪贴板\t";
            strMenuItemText += strSubText;
            break;

        case CT_LOCATE_REGPATH:
            strMenuItemText = L"跳转到注册表路径\t";
            strMenuItemText += strSubText;
            break;

        case CT_LOCATE_FILEPATH:
            strMenuItemText = L"定位文件\t";
            strMenuItemText += strSubText;
            break;

        case CT_BROWSE_WEB_ADDRESS:
            strMenuItemText = L"访问网页\t";
            strMenuItemText += strSubText;
            break;

        case CT_PING_IP_ADDRESS:
            strMenuItemText = L"ping此地址\t";
            strMenuItemText += strSubText;
            break;

        default:
            break;
        }

        return strMenuItemText;
    }

    static bool GetClipboardStringAndType(wstring &strText, CBType &cbType) {
        bool bSucceed = false;

        if (OpenClipboard(NULL)) {
            if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
                HGLOBAL hData = (HGLOBAL)GetClipboardData(CF_UNICODETEXT);

                if (hData) {
                    LPCWSTR lpText = (LPCWSTR)GlobalLock((HANDLE)hData);

                    if (lpText) {
                        AssignString(strText, lpText);
                        GlobalUnlock(hData);

                        bSucceed = !strText.empty();
                    }
                }
            }

            CloseClipboard();
        }

        if (bSucceed) {
            cbType = CT_NONE;

            if (strText.find(L'\\') != wstring::npos ||
                strText.find(L'\"') != wstring::npos ||
                strText.find(L'\'') != wstring::npos) {
                (unsigned &)cbType |= CT_ESCAPE_STRING;
            }

            if (strText.substr(0, 5) == L"HKEY_") {
                (unsigned &)cbType |= CT_LOCATE_REGPATH;
            }

            if (PathFileExistsW(strText.c_str())) {
                (unsigned &)cbType |= CT_LOCATE_FILEPATH;
            }

            // (25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)){3}|[\w\-]+(\.[\w\-]+)*?(\.[\w\-]{1,5})
            if (SmMatchExact(strText.c_str(), L"(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)){3}|[\\w\\-]+(\\.[\\w\\-]+)*?(\\.[\\w\\-]{1,5})", true)) {
                (unsigned &)cbType |= CT_PING_IP_ADDRESS;
                (unsigned &)cbType |= CT_BROWSE_WEB_ADDRESS;
            }
            // ((https?|ftp)://)?[\w\-]+(\.[\w\-]+)*?(\.[\w\-]{1,5})(:\d{1,5})?(/[^/]+)*/?
            else if (SmMatchExact(strText.c_str(), L"((https?|ftp)://)?[\\w\\-]+(\\.[\\w\\-]+)*?(\\.[\\w\\-]{1,5})(:\\d{1,5})?(/[^/]+)*/?", true)) {
                (unsigned &)cbType |= CT_BROWSE_WEB_ADDRESS;
            }
        }

        return bSucceed;
    }

private:
    CBType m_cbType;
    wstring m_strText;
};

class MenuItemRegistry : public MenuItem {
public:
    MenuItemRegistry(int nCmdId, LPCWSTR lpText, LPCWSTR lpRegPath) {
        m_nCmdId = nCmdId;
        AssignString(m_strText, lpText);
        AssignString(m_strRegPath, lpRegPath);
    }

    virtual void DoJob(HWND hWindow, HINSTANCE hInstance) const {
        LPCWSTR lpRegPath = NULL;
        HKEY hRootKey = ParseRegPath(m_strRegPath.c_str(), &lpRegPath);

        if (hRootKey == NULL) {
            MessageBoxFormat(hWindow, NULL, MB_ICONERROR, L"%s不是合法的注册表路径，无法自动导航。", m_strRegPath.c_str());
        } else {
            if (IsRegPathExists(hRootKey, lpRegPath)) {
                BrowseForRegPath(m_strRegPath.c_str());
            } else {
                g_bChangeButtonText = TRUE;

                int nId = MessageBoxFormat(
                    hWindow,
                    REG_QUESTION_TITLE,
                    MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3,
                    L"%s路径不存在，您可以跳转到最接近的位置或创建这个路径。\r\n\r\n"
                    L"选择“就近”跳转到最接近的位置\r\n"
                    L"选择“创建”自动创建此路径并跳转\r\n",
                    m_strRegPath.c_str()
                    );

                g_bChangeButtonText = FALSE;

                if (nId == IDYES) {       //就近
                    int len = (lstrlenW(lpRegPath) + 1);
                    WCHAR *szPath = (WCHAR *)malloc(len * sizeof(WCHAR));

                    if (szPath != NULL) {
                        lstrcpynW(szPath, lpRegPath, len);
                        PathRemoveBackslashW(szPath);

                        while (!IsRegPathExists(hRootKey, szPath)) {
                            PathRemoveFileSpecW(szPath);
                        }

                        wstring::size_type pos = m_strRegPath.find(L"\\");

                        if (pos != m_strRegPath.npos) {
                            BrowseForRegPath((m_strRegPath.substr(0, pos + 1) + szPath).c_str());
                        }

                        free((void *)szPath);
                    }
                } else if (nId == IDNO) {   //创建
                    if (TouchRegPath(hRootKey, lpRegPath)) {
                        BrowseForRegPath(m_strRegPath.c_str());
                    } else {
                        MessageBoxFormat(hWindow, NULL, MB_ICONERROR, L"创建%s失败。", m_strRegPath.c_str());
                    }
                }
            }
        }
    }

protected:
    wstring m_strRegPath;
};

class MenuMgr {
public:
    MenuMgr(HWND hWindow, HINSTANCE hInstance) : m_hWindow(hWindow), m_hInstance(hInstance), m_nTimePlageIntervalMinutes(30) {
        m_hMenuFont = GetMenuDefaultFont();
        m_hMenuDc = CreateCompatibleDC(NULL);
        m_hOldFont = SelectObject(m_hMenuDc, (HGDIOBJ)m_hMenuFont);
        m_hMenu = NULL;

        // 在正常范围外取值，将导致未定义后果
        // 菜单项会选中“禁用”，但实际并不禁用
        DWORD dwInterval = cfglGetLong(L"time_plate.interval.every", 30);

        if (dwInterval == -1 || dwInterval == 0) {
            EnableTimePlate(FALSE);
            m_nTimePlageIntervalMinutes = 0;
        } else {
            EnableTimePlate(TRUE);
            m_nTimePlageIntervalMinutes = dwInterval;
        }

        SetTimePlateOption(m_nTimePlageIntervalMinutes);
        UpdateMenu();
    }

    ~MenuMgr() {
        SelectObject(m_hMenuDc, m_hOldFont);
        DeleteDC(m_hMenuDc);

        if (m_hMenuFont) {
            DeleteObject(m_hMenuFont);
        }
    }

    void TrackAndDealPopupMenu(int x, int y) {
        if (g_bDebugMode) {
            DeleteMenu(m_hMenu, SYS_DEBUGBREAK, MF_BYCOMMAND);

            if (IsDebuggerPresent()) {
                AppendMenuW(m_hMenu, MF_STRING, SYS_DEBUGBREAK, L"5秒后DebugBreak");
            }
        }

        int nCmd = TrackPopupMenu(
            m_hMenu,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
            x,
            y,
            NULL,
            m_hWindow,
            NULL
            );

        if (nCmd == 0) {
            return;
        }

        switch (nCmd) {
        case SYS_QUIT:
            PostMessageW(m_hWindow, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;

        case SYS_HIDEICON:
            PostMessageW(m_hWindow, WM_HIDEICON, 0, 0);
            break;

        case SYS_ABOUT:
            SlxComAbout(m_hWindow);
            break;

        case SYS_RESETEXPLORER:
            if (IDYES == MessageBoxW(
                m_hWindow,
                L"要重启Explorer（操作系统外壳程序）吗？",
                L"请确认",
                MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_SYSTEMMODAL
                )) {
                ResetExplorer();
            }
            break;

        case SYS_DISABLECTRLCOPYINSAMEDIR:
            if (DisableCtrlCopyInSameDir_IsRunning()) {
                DisableCtrlCopyInSameDir_Op(DCCO_OFF);
                cfglSetBool(L"DisableCtrlCopyInSameDir", FALSE);
            } else {
                DisableCtrlCopyInSameDir_Op(DCCO_ON);
                cfglSetBool(L"DisableCtrlCopyInSameDir", TRUE);
            }
            UpdateMenu();
            break;

        case SYS_DEBUGBREAK:
            PostMessageW(m_hWindow, WM_SET_DEBUGBREAK_TASK, 0, 0);
            break;

        case SYS_WINDOWMANAGER:
            MessageBoxW(
                m_hWindow,
                L"窗口管理器可以快捷的管理各个顶层窗口（非子窗口），可以右击顶层窗口获得快捷菜单。\r\n"
                L"\r\n"
                L"窗口管理器可以将前端窗口最小化到系统托盘，快捷键为：\r\n"
                L"隐藏前端窗口到托盘，Alt+Ctrl(Shift)+H\r\n"
                L"置顶前端窗口，Alt+Ctrl(Shift)+T\r\n"
                L"在托盘创建前端窗口的图标，Alt+Ctrl(Shift)+C\r\n",
                L"SlxCom WindowManager",
                MB_ICONINFORMATION | MB_TOPMOST
                );
            break;

        case SYS_SHOWTIMEPALTE_DISABLE:
            m_nTimePlageIntervalMinutes = 0;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_DISABLE, MF_BYCOMMAND);
            EnableTimePlate(FALSE);
            cfglSetLong(L"time_plate.interval.every", -1);
            break;

        case SYS_SHOWTIMEPALTE_PER60M:
            m_nTimePlageIntervalMinutes = 60;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER60M, MF_BYCOMMAND);
            EnableTimePlate(TRUE);
            SetTimePlateOption(60);
            cfglSetLong(L"time_plate.interval.every", 60);
            break;

        case SYS_SHOWTIMEPALTE_PER30M:
            m_nTimePlageIntervalMinutes = 30;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER30M, MF_BYCOMMAND);
            EnableTimePlate(TRUE);
            SetTimePlateOption(30);
            cfglSetLong(L"time_plate.interval.every", 30);
            break;

        case SYS_SHOWTIMEPALTE_PER15M:
            m_nTimePlageIntervalMinutes = 15;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER15M, MF_BYCOMMAND);
            EnableTimePlate(TRUE);
            SetTimePlateOption(15);
            cfglSetLong(L"time_plate.interval.every", 15);
            break;

        case SYS_SHOWTIMEPALTE_PER10M:
            m_nTimePlageIntervalMinutes = 10;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER10M, MF_BYCOMMAND);
            EnableTimePlate(TRUE);
            SetTimePlateOption(10);
            cfglSetLong(L"time_plate.interval.every", 10);
            break;

        case SYS_SHOWTIMEPALTE_PER1M:
            m_nTimePlageIntervalMinutes = 1;
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER1M, MF_BYCOMMAND);
            EnableTimePlate(TRUE);
            SetTimePlateOption(1);
            cfglSetLong(L"time_plate.interval.every", 1);
            break;

        case SYS_UPDATEMENU:
            UpdateMenu();
            break;

        case SYS_PAINTVIEW:
            PostMessageW(m_hWindow, WM_HOTKEY, HK_PAINTVIEW, 0);
            break;

        default:
            if (m_mapMenuItems.find(nCmd) != m_mapMenuItems.end()) {
                m_mapMenuItems[nCmd]->DoJob(m_hWindow, m_hInstance);
            }
            break;
        }
    }

    void UpdateMenu() {
        m_setMenuItemsRegistry.clear();
        m_mapMenuItems.clear();

        if (m_hMenu != NULL) {
            DestroyMenu(m_hMenu);
        }

        HMENU hTimePlateMenu = CreatePopupMenu();

        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_PER60M, L"每小时(&A)");
        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_PER30M, L"每半小时(&H)");
        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_PER15M, L"每一刻钟(&Q)");
        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_PER10M, L"每十分钟(&T)");
        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_PER1M, L"每分钟(&M)");
        AppendMenuW(hTimePlateMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hTimePlateMenu, MF_STRING, SYS_SHOWTIMEPALTE_DISABLE, L"关闭(&D)");

        UINT nMenuId = SYS_MAXVALUE;
        m_hMenu = CreatePopupMenu();

        //主菜单
        AppendMenuW(m_hMenu, MF_STRING, SYS_ABOUT, L"关于SlxCom(&A)...");;
//         AppendMenuW(m_hMenu, MF_STRING, SYS_PAINTVIEW, L"桌面画板(&P)...");
//         SetMenuDefaultItem(m_hMenu, SYS_PAINTVIEW, MF_BYCOMMAND);
        AppendMenuW(m_hMenu, MF_STRING, SYS_WINDOWMANAGER, L"窗口管理器(&W)...");
        AppendMenuW(m_hMenu, MF_POPUP, (UINT)(UINT_PTR)hTimePlateMenu, L"整点报时(&T)");
        AppendMenuW(m_hMenu, MF_POPUP, (UINT)(UINT_PTR)InitRegPathSubMenu(&nMenuId), L"注册表快捷通道(&R)");
        AppendMenuW(m_hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m_hMenu, MF_STRING, SYS_RESETEXPLORER, L"重新启动Explorer(&E)");
        if (g_bVistaLater) {
            AppendMenuW(m_hMenu, MF_STRING, SYS_DISABLECTRLCOPYINSAMEDIR, L"禁用同目录内按住Ctrl键制作副本(&C)");
            CheckMenuItemHelper(m_hMenu, SYS_DISABLECTRLCOPYINSAMEDIR, 0, DisableCtrlCopyInSameDir_IsRunning());
        }
        AppendMenuW(m_hMenu, MF_SEPARATOR, 0, NULL);
        SetMenuItemBitmaps(m_hMenu, SYS_RESETEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
        AppendMenuW(m_hMenu, MF_STRING, SYS_UPDATEMENU, L"刷新菜单内容(&U)");
        AppendMenuW(m_hMenu, MF_STRING, SYS_HIDEICON, L"不显示托盘图标(&Q)");

        switch (m_nTimePlageIntervalMinutes) {
        default:
        case 0:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_DISABLE, MF_BYCOMMAND);
            break;

        case 60:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER60M, MF_BYCOMMAND);
            break;

        case 30:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER30M, MF_BYCOMMAND);
            break;

        case 15:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER15M, MF_BYCOMMAND);
            break;

        case 10:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER10M, MF_BYCOMMAND);
            break;

        case 1:
            CheckMenuRadioItem(m_hMenu, SYS_SHOWTIMEPALTE_DISABLE, SYS_SHOWTIMEPALTE_PER1M, SYS_SHOWTIMEPALTE_PER1M, MF_BYCOMMAND);
            break;
        }
    }

private:
    //获取指定注册表路径下的所有子项列表，子项列表中不含路径部分
    static set<wstring> GetRegAllSubPath(HKEY hRootKey, LPCWSTR lpRegPath) {
        set<wstring> result;
        HKEY hKey = NULL;

        if (ERROR_SUCCESS == RegOpenKeyExW(hRootKey, lpRegPath, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey)) {
            DWORD dwSubKeyCount = 0;
            DWORD dwMaxSubKeyLen = 0;

            if (ERROR_SUCCESS == RegQueryInfoKey(
                hKey,
                NULL,
                NULL,
                NULL,
                &dwSubKeyCount,
                &dwMaxSubKeyLen,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
                )) {
                dwMaxSubKeyLen += 1;
                dwMaxSubKeyLen *= 2;

                WCHAR *szSubKey = (WCHAR *)malloc(dwMaxSubKeyLen);

                if (szSubKey != NULL) {
                    for (DWORD dwIndex = 0; dwIndex < dwSubKeyCount; dwIndex += 1) {
                        DWORD dwSubKeySize = dwMaxSubKeyLen;

                        if (ERROR_SUCCESS == RegEnumKeyExW(hKey, dwIndex, szSubKey, &dwSubKeySize, NULL, NULL, NULL, NULL)) {
                            result.insert(szSubKey);
                        }
                    }
                }

                if (szSubKey != NULL) {
                    free((void *)szSubKey);
                }
            }

            RegCloseKey(hKey);
        }

        return result;
    }

    //获取指定注册表路径下的所有字符串键值对应表，只关注REG_SZ类型
    static map<wstring, wstring> GetRegAllSzValuePairs(HKEY hRootKey, LPCWSTR lpRegPath) {
        map<wstring, wstring> result;
        HKEY hKey = NULL;

        if (ERROR_SUCCESS == RegOpenKeyExW(hRootKey, lpRegPath, 0, KEY_QUERY_VALUE, &hKey)) {
            DWORD dwValueCount = 0;
            DWORD dwMaxValueNameLen = 0;
            DWORD dwMaxDataLen = 0;

            if (ERROR_SUCCESS == RegQueryInfoKey(
                hKey,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                &dwValueCount,
                &dwMaxValueNameLen,
                &dwMaxDataLen,
                NULL,
                NULL
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
                                result[szValueName] = (LPCWSTR)szData;
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

        return result;
    }

    //将指定注册表路径映射为菜单，菜单项ID使用pMenuId指向的值递增使用
    //bEraseHead如被指定，则删除值中的最前的一个路径段
    //bHaveChildPath如被指定，则处理子项
    HMENU RegistryPathToMenu(UINT *pMenuId, HKEY hRootKey, LPCWSTR lpRegPath, BOOL bEraseHead, BOOL bHaveChildPath) {
        HMENU hPopupMenu = CreatePopupMenu();

        //处理子项
        if (bHaveChildPath) {
            set<wstring> setSubPaths = GetRegAllSubPath(hRootKey, lpRegPath);
            set<wstring>::iterator itSubPath = setSubPaths.begin();
            for (; itSubPath != setSubPaths.end(); ++itSubPath) {
                wstring strSubPath = lpRegPath;

                strSubPath += L"\\";
                strSubPath += *itSubPath;

                AppendMenuW(
                    hPopupMenu,
                    MF_POPUP,
                    (UINT)(UINT_PTR)RegistryPathToMenu(pMenuId, hRootKey, strSubPath.c_str(), FALSE, TRUE),
                    itSubPath->c_str()
                    );
            }
        }

        //处理当前项下的键
        map<wstring, wstring>::const_iterator itValue;
        map<wstring, wstring> mapMenu = GetRegAllSzValuePairs(hRootKey, lpRegPath);

        for (itValue = mapMenu.begin(); itValue != mapMenu.end(); ++itValue) {
            wstring strRegPath = itValue->second;

            if (bEraseHead) {
                wstring::size_type pos = strRegPath.find(L'\\');
                if (pos != strRegPath.npos) {
                    strRegPath = strRegPath.substr(pos + 1);
                }
            }

            if (!itValue->first.empty()) {        //不处理注册表默认值
                WCHAR szShortRegPath[1000];

                lstrcpynW(szShortRegPath, itValue->second.c_str(), RTL_NUMBER_OF(szShortRegPath));
                PathCompactPathHelper(m_hMenuDc, szShortRegPath, COMPACT_WIDTH);

                AppendMenuW(hPopupMenu, MF_STRING, *pMenuId, (itValue->first + L"\t" + szShortRegPath).c_str());
                m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, itValue->first.c_str(), strRegPath.c_str())).first;
                *pMenuId += 1;
            }
        }

        return hPopupMenu;
    }

    HMENU InitRegPathSubMenu(UINT *pMenuId) {
        LPCWSTR lpSysRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites";
        LPCWSTR lpSlxRegPath = L"Software\\Shilyx Studio\\SlxCom\\RegPath";
        LPCWSTR lpSlxRegPath2 = L"HKEY_CURRENT_USER\\Software\\Shilyx Studio\\SlxCom\\RegPath";
        HMENU hRetMenu = CreatePopupMenu();

        HMENU hBuildinMenu = CreatePopupMenu();
        for (int index = 0; index < RTL_NUMBER_OF(g_szBuildinRegPaths); index += 1) {
            WCHAR szShortRegPath[1000];

            lstrcpynW(szShortRegPath, g_szBuildinRegPaths[index].lpPath, RTL_NUMBER_OF(szShortRegPath));
            PathCompactPathHelper(m_hMenuDc, szShortRegPath, COMPACT_WIDTH);

            AppendMenuW(hBuildinMenu, MF_STRING, *pMenuId, (wstring(g_szBuildinRegPaths[index].lpName) + L"\t" + szShortRegPath).c_str());
            m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, g_szBuildinRegPaths[index].lpName, g_szBuildinRegPaths[index].lpPath)).first;
            *pMenuId += 1;
        }

        if (GetMenuItemCount(hBuildinMenu) > 0) {
            AppendMenuW(hRetMenu, MF_POPUP, (UINT)(UINT_PTR)hBuildinMenu, L"注册表常用位置(&B)");
        }

        HMENU hSysMenu = RegistryPathToMenu(pMenuId, HKEY_CURRENT_USER, lpSysRegPath, TRUE, FALSE);
        AppendMenuW(hRetMenu, MF_POPUP, (UINT)(UINT_PTR)hSysMenu, L"注册表编辑器搜藏位置(&F)");

        HMENU hSlxMenu = RegistryPathToMenu(pMenuId, HKEY_CURRENT_USER, lpSlxRegPath, FALSE, TRUE);
        InsertMenuW(hSlxMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenuW(hSlxMenu, 0, MF_BYPOSITION, *pMenuId, L"配置SlxCom搜藏(&C)...");
        m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, NULL, lpSlxRegPath2)).first;
        AppendMenuW(hRetMenu, MF_POPUP, (UINT)(UINT_PTR)hSlxMenu, L"SlxCom搜藏位置(&S)");
        *pMenuId += 1;

        return hRetMenu;
    }

private:
    HWND m_hWindow;
    HMENU m_hMenu;
    HINSTANCE m_hInstance;
    map<int, const MenuItem *> m_mapMenuItems;
    set<MenuItemRegistry> m_setMenuItemsRegistry;

    int m_nTimePlageIntervalMinutes;

private:
    HFONT m_hMenuFont;
    HGDIOBJ m_hOldFont;
    HDC m_hMenuDc;
};

static DWORD CALLBACK DebugBreakProc(LPVOID) {
    if (IsDebuggerPresent()) {
        DebugBreak();
    }

    return 0;
}

static LRESULT __stdcall NotifyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static NOTIFYICONDATAW nid = {sizeof(nid)};
    static int nIconShowIndex = INT_MAX;
    static HICON arrIcons[ICON_COUNT] = {NULL};
    static MenuMgr *pMenuMgr = NULL;
    static int nDebugBreakIndex = -1;

    if (uMsg == WM_CREATE) {
        LPCREATESTRUCT lpCs = (LPCREATESTRUCT)lParam;

        //load icons
        int nFirstIconId = IDI_BIRD_0;

        for (int i = 0; i < ICON_COUNT; i += 1) {
            arrIcons[i] = LoadIconW(lpCs->hInstance, MAKEINTRESOURCEW(nFirstIconId + i));
        }

        //
        GetModuleFileNameW(lpCs->hInstance, g_szModulePath, RTL_NUMBER_OF(g_szModulePath));

        //add icon to tray
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uCallbackMessage = WM_CALLBACK;
        nid.hIcon = arrIcons[0];
        nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_ICON;
        lstrcpynW(nid.szTip, L"SlxCom", sizeof(nid.szTip));

        Shell_NotifyIconW(NIM_ADD, &nid);

        //
        if (cfglGetBool(L"DisableCtrlCopyInSameDir")) {
            DisableCtrlCopyInSameDir_Op(DCCO_ON);
        }

        //
        SetTimer(hWnd, TIMER_ICON, 55, NULL);
        SetTimer(hWnd, TIMER_MENU, 60 * 1000 + 52, NULL);
        SetTimer(hWnd, TIMER_DEBUGBREAK, 1000, NULL);

        PostMessageW(hWnd, WM_TIMER, TIMER_MENU, 0);

        //
        pMenuMgr = new MenuMgr(hWnd, lpCs->hInstance);

        //
        if (!RegisterHotKey(hWnd, HK_PAINTVIEW, MOD_ALT | MOD_SHIFT, L'P')) {
            SafeDebugMessage(L"SlxCom 桌面画板快捷键注册失败%lu。\r\n", GetLastError());
        }

        return 0;
    } else if (uMsg == WM_TIMER) {
        if (TIMER_ICON == wParam) {
            if (nIconShowIndex < RTL_NUMBER_OF(arrIcons)) {
                nid.hIcon = arrIcons[nIconShowIndex];
                nid.uFlags = NIF_ICON;

                Shell_NotifyIconW(NIM_MODIFY, &nid);
                nIconShowIndex += 1;
            }
        } else if (TIMER_MENU == wParam) {
            pMenuMgr->UpdateMenu();
        } else if (TIMER_DEBUGBREAK == wParam) {
            if (nDebugBreakIndex > 0) {
                --nDebugBreakIndex;

                if (nDebugBreakIndex == 0) {
                    CloseHandle(CreateThread(NULL, 0, DebugBreakProc, NULL, 0, NULL));
                }
            }
        }
    } else if (uMsg == WM_HOTKEY) {
        if (wParam == HK_PAINTVIEW) {
            WCHAR szCommand[MAX_PATH * 2];

            wnsprintfW(
                szCommand,
                RTL_NUMBER_OF(szCommand),
                L"rundll32.exe \"%s\" SlxPaintView",
                g_szModulePath
                );

            RunCommand(szCommand);

            return 0;
        }
    } else if (uMsg == WM_CLOSE) {
        UnregisterHotKey(hWnd, HK_PAINTVIEW);
        Shell_NotifyIconW(NIM_DELETE, &nid);
        DestroyWindow(hWnd);
    } else if (uMsg == WM_DESTROY) {
        if (pMenuMgr != NULL) {
            delete pMenuMgr;
            pMenuMgr = NULL;
        }

        PostQuitMessage(0);
    } else if (uMsg == WM_LBUTTONDOWN) {
        nIconShowIndex = 0;
    } else if (uMsg == WM_CALLBACK) {
        if (lParam == WM_RBUTTONUP) {
            POINT pt;

            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            pMenuMgr->TrackAndDealPopupMenu(pt.x, pt.y);
        } else if (lParam == WM_LBUTTONUP) {
            map<int, shared_ptr<MenuItem>> mapItems;
            int nMenuId = 100;
            wstring strCbText;
            CBType cbType = CT_NONE;

            if (MenuItemClipboardW::GetClipboardStringAndType(strCbText, cbType)) {
                unsigned nType = (unsigned)cbType;
                unsigned nCurrentType = 1;

                for (int i = 0; i < 32; ++i) {
                    if (nCurrentType & nType) {
                        mapItems[nMenuId] = shared_ptr<MenuItem>(new MenuItemClipboardW(nMenuId, (CBType)nCurrentType, strCbText.c_str()));
                        ++nMenuId;
                    }

                    nCurrentType <<= 1;
                }
            }

            if (!mapItems.empty()) {
                POINT pt;
                HMENU hMenu = CreatePopupMenu();

                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);

                for (map<int, shared_ptr<MenuItem>>::const_iterator it = mapItems.begin(); it != mapItems.end(); ++it) {
                    AppendMenuW(hMenu, MF_STRING, it->first, it->second->GetMenuItemText().c_str());
                }

                int nCmd = TrackPopupMenu(
                    hMenu,
                    TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                    pt.x,
                    pt.y,
                    NULL,
                    hWnd,
                    NULL
                    );

                if (mapItems.count(nCmd)) {
                    mapItems[nCmd]->DoJob(hWnd, NULL);
                }

                DestroyMenu(hMenu);
            }
        }

        if (nIconShowIndex >= RTL_NUMBER_OF(arrIcons)) {
            nIconShowIndex = 0;
        }
    } else if (uMsg == WM_HIDEICON) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    } else if (uMsg == WM_SET_DEBUGBREAK_TASK) {
        nDebugBreakIndex = 5;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_bChangeButtonText) {
        LPCWPRETSTRUCT lpCrs = (LPCWPRETSTRUCT)lParam;

        if (lpCrs != NULL && lpCrs->message == WM_INITDIALOG) {
            WCHAR szWindowText[100] = L"";
            HWND hYes = GetDlgItem(lpCrs->hwnd, IDYES);
            HWND hNo = GetDlgItem(lpCrs->hwnd, IDNO);
            HWND hCancel = GetDlgItem(lpCrs->hwnd, IDCANCEL);

            GetWindowTextW(lpCrs->hwnd, szWindowText, RTL_NUMBER_OF(szWindowText));

            if (IsWindow(hYes) && IsWindow(hNo) && IsWindow(hCancel) && lstrcmpiW(szWindowText, REG_QUESTION_TITLE) == 0) {
                SetDlgItemTextW(lpCrs->hwnd, IDYES, L"就近(&N)");
                SetDlgItemTextW(lpCrs->hwnd, IDNO, L"创建(&C)");
                SetDlgItemTextW(lpCrs->hwnd, IDCANCEL, L"取消(&Q)");
            }
        }
    }

    return CallNextHookEx(g_hMsgHook, nCode, wParam, lParam);
}

static DWORD CALLBACK NotifyIconManagerProc(LPVOID lpParam) {
    if (g_hMsgHook != NULL) {
        return 0;
    }

    WCHAR szImagePath[MAX_PATH] = L"";

    GetModuleFileNameW(GetModuleHandleW(NULL), szImagePath, RTL_NUMBER_OF(szImagePath));

    if (lstrcmpiW(L"rundll32.exe", PathFindFileNameW(szImagePath)) != 0) {
        DWORD dwSleepTime = 1;
        while (!IsWindow(GetTrayNotifyWndInProcess())) {
            Sleep((dwSleepTime++) * 1000);
        }
    }

    WNDCLASSEXW wcex = {sizeof(wcex)};
    wcex.lpfnWndProc = NotifyWndProc;
    wcex.lpszClassName = NOTIFYWNDCLASS;
    wcex.hInstance = (HINSTANCE)lpParam;
    wcex.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wcex);

    HWND hMainWnd = CreateWindowExW(
        0,
        NOTIFYWNDCLASS,
        NOTIFYWNDCLASS,
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        333,
        55,
        NULL,
        NULL,
        (HINSTANCE)lpParam,
        NULL
        );

//     ShowWindow(hMainWnd, SW_SHOW);
//     UpdateWindow(hMainWnd);

    g_hMsgHook = SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProc, (HINSTANCE)lpParam, GetCurrentThreadId());

    MSG msg;

    while (TRUE) {
        int nRet = GetMessageW(&msg, NULL, 0, 0);

        if (nRet <= 0) {
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hMsgHook);
    g_hMsgHook = NULL;

    return 0;
}

void StartNotifyIconManager(HINSTANCE hinstDll) {
    if (!IsExplorer()) {
        return;
    }

    if (IsWow64ProcessHelper(GetCurrentProcess())) {
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, NotifyIconManagerProc, (LPVOID)hinstDll, 0, NULL);
    CloseHandle(hThread);
}