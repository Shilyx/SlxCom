#include "SlxComWork_ni.h"
#include "SlxComTools.h"
#include "resource.h"
#include <Shlwapi.h>
#pragma warning(disable: 4786)
#include <vector>
#include <map>
#include <set>
#include "lib/charconv.h"
#include "SlxComAbout.h"

using namespace std;

#define NOTIFYWNDCLASS      TEXT("__slxcom_work_ni_notify_wnd")
#define ICON_COUNT          10
#define TIMER_ICON          1
#define TIMER_MENU          2
#define REG_QUESTION_TITLE  TEXT("����ע���·�������ڵ�����")
#define COMPACT_WIDTH       400

static enum
{
    WM_CALLBACK = WM_USER + 112,
    WM_HIDEICON,
};

extern HBITMAP g_hKillExplorerBmp; //SlxCom.cpp

static HHOOK g_hMsgHook = NULL;
static BOOL g_bChangeButtonText = FALSE;
static TCHAR g_szModulePath[MAX_PATH] = TEXT("");
static struct
{
    LPCTSTR lpName;
    LPCTSTR lpPath;
} g_szBuildinRegPaths[] = {
    { TEXT("ϵͳRun"),          TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run")},
#ifdef _WIN64
    { TEXT("ϵͳRun(32)"),      TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run")},
#endif
    { TEXT("�û�Run"),          TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run")},
#ifdef _WIN64
    { TEXT("�û�Run(32)"),      TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run")},
#endif
    { TEXT("ϵͳWinLogon"),     TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon")},
#ifdef _WIN64
    { TEXT("ϵͳWinLogon(32)"), TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon")},
#endif
    { TEXT("�û�WinLogon"),     TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon")},
#ifdef _WIN64
    { TEXT("�û�WinLogon(32)"), TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon")},
#endif
    { TEXT("ϵͳWindows"),      TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows")},
#ifdef _WIN64
    { TEXT("ϵͳWindows(32)"),  TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows")},
#endif
    { TEXT("�û�Windows"),      TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows")},
#ifdef _WIN64
    { TEXT("�û�Windows(32)"),  TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows")},
#endif
    { TEXT("ϵͳExplorer"),     TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer")},
#ifdef _WIN64
    { TEXT("ϵͳExplorer(32)"), TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer")},
#endif
    { TEXT("�û�Explorer"),     TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer")},
#ifdef _WIN64
    { TEXT("�û�Explorer(32)"), TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer")},
#endif
    { TEXT("ϵͳIE"),           TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Internet Explorer")},
#ifdef _WIN64
    { TEXT("ϵͳIE(32)"),       TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Internet Explorer")},
#endif
    { TEXT("�û�IE"),           TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Internet Explorer")},
#ifdef _WIN64
    { TEXT("�û�IE(32)"),       TEXT("HKEY_CURRENT_USER\\SOFTWARE\\Wow6432Node\\Microsoft\\Internet Explorer")},
#endif
    { TEXT("Session Manager"), TEXT("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager")},
    { TEXT("ӳ��ٳ�"),          TEXT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options")},
    { TEXT("��֪dll"),          TEXT("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\KnownDLLs")},
};

static enum
{
    SYS_QUIT = 1,
    SYS_HIDEICON,
    SYS_ABOUT,
    SYS_RESETEXPLORER,
    SYS_WINDOWMANAGER,
    SYS_UPDATEMENU,
    SYS_PAINTVIEW,
    SYS_MAXVALUE,
};

static enum
{
    HK_PAINTVIEW = 112,
};

class MenuItem
{
public:
    MenuItem() : m_nCmdId(0) { }
    virtual ~MenuItem() { }
    virtual void DoJob(HWND hWindow, HINSTANCE hInstance) = 0;
    virtual bool operator<(const MenuItem &other) const { return m_nCmdId < other.m_nCmdId;}

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
        LPCTSTR lpRegPath = NULL;
        HKEY hRootKey = ParseRegPath(m_strRegPath.c_str(), &lpRegPath);

        if (hRootKey == NULL)
        {
            MessageBoxFormat(hWindow, NULL, MB_ICONERROR, TEXT("%s���ǺϷ���ע���·�����޷��Զ�������"), m_strRegPath.c_str());
        }
        else
        {
            if (IsRegPathExists(hRootKey, lpRegPath))
            {
                BrowseForRegPath(m_strRegPath.c_str());
            }
            else
            {
                g_bChangeButtonText = TRUE;

                int nId = MessageBoxFormat(
                    hWindow,
                    REG_QUESTION_TITLE,
                    MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3,
                    TEXT("%s·�������ڣ���������ת����ӽ���λ�û򴴽����·����\r\n\r\n")
                    TEXT("ѡ�񡰾ͽ�����ת����ӽ���λ��\r\n")
                    TEXT("ѡ�񡰴������Զ�������·������ת\r\n"),
                    m_strRegPath.c_str()
                    );

                g_bChangeButtonText = FALSE;

                if (nId == IDYES)       //�ͽ�
                {
                    int len = (lstrlen(lpRegPath) + 1);
                    TCHAR *szPath = (TCHAR *)malloc(len * sizeof(TCHAR));

                    if (szPath != NULL)
                    {
                        lstrcpyn(szPath, lpRegPath, len);
                        PathRemoveBackslash(szPath);

                        while (!IsRegPathExists(hRootKey, szPath))
                        {
                            PathRemoveFileSpec(szPath);
                        }

                        tstring::size_type pos = m_strRegPath.find(TEXT("\\"));

                        if (pos != m_strRegPath.npos)
                        {
                            BrowseForRegPath((m_strRegPath.substr(0, pos + 1) + szPath).c_str());
                        }

                        free((void *)szPath);
                    }
                }
                else if (nId == IDNO)   //����
                {
                    if (TouchRegPath(hRootKey, lpRegPath))
                    {
                        BrowseForRegPath(m_strRegPath.c_str());
                    }
                    else
                    {
                        MessageBoxFormat(hWindow, NULL, MB_ICONERROR, TEXT("����%sʧ�ܡ�"), m_strRegPath.c_str());
                    }
                }
            }
        }
    }

protected:
    tstring m_strRegPath;
};

class MenuMgr
{
public:
    MenuMgr(HWND hWindow, HINSTANCE hInstance)
        : m_hWindow(hWindow), m_hInstance(hInstance)
    {
        m_hMenuFont = GetMenuDefaultFont();
        m_hMenuDc = CreateCompatibleDC(NULL);
        m_hOldFont = SelectObject(m_hMenuDc, (HGDIOBJ)m_hMenuFont);

        m_hMenu = NULL;
        UpdateMenu();
    }

    ~MenuMgr()
    {
        SelectObject(m_hMenuDc, m_hOldFont);
        DeleteDC(m_hMenuDc);

        if (m_hMenuFont)
        {
            DeleteObject(m_hMenuFont);
        }
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

        if (nCmd == 0)
        {
            return;
        }

        switch (nCmd)
        {
        case SYS_QUIT:
            PostMessage(m_hWindow, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;

        case SYS_HIDEICON:
            PostMessage(m_hWindow, WM_HIDEICON, 0, 0);
            break;

        case SYS_ABOUT:
            SlxComAbout(m_hWindow);
            break;

        case SYS_RESETEXPLORER:
            if (IDYES == MessageBox(
                m_hWindow,
                TEXT("Ҫ����Explorer������ϵͳ��ǳ�����"),
                TEXT("��ȷ��"),
                MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_SYSTEMMODAL
                ))
            {
                ResetExplorer();
            }
            break;

        case SYS_WINDOWMANAGER:
            MessageBox(
                m_hWindow,
                TEXT("���ڹ��������Կ�ݵĹ���������㴰�ڣ����Ӵ��ڣ��������һ����㴰�ڻ�ÿ�ݲ˵���\r\n")
                TEXT("\r\n")
                TEXT("���ڹ��������Խ�ǰ�˴�����С����ϵͳ���̣���ݼ�Ϊ��\r\n")
                TEXT("����ǰ�˴��ڵ����̣�Alt+Ctrl(Shift)+H\r\n")
                TEXT("�ö�ǰ�˴��ڣ�Alt+Ctrl(Shift)+T\r\n")
                TEXT("�����̴���ǰ�˴��ڵ�ͼ�꣬Alt+Ctrl(Shift)+C\r\n"),
                TEXT("SlxCom WindowManager"),
                MB_ICONINFORMATION | MB_TOPMOST
                );
            break;

        case SYS_UPDATEMENU:
            UpdateMenu();
            break;

        case SYS_PAINTVIEW:
            PostMessage(m_hWindow, WM_HOTKEY, HK_PAINTVIEW, 0);
            break;

        default:
            if (m_mapMenuItems.find(nCmd) != m_mapMenuItems.end())
            {
                m_mapMenuItems[nCmd]->DoJob(m_hWindow, m_hInstance);
            }
            break;
        }
    }

    void UpdateMenu()
    {
        m_setMenuItemsRegistry.clear();
        m_mapMenuItems.clear();

        if (m_hMenu != NULL)
        {
            DestroyMenu(m_hMenu);
        }

        UINT nMenuId = SYS_MAXVALUE;
        m_hMenu = CreatePopupMenu();

        //���˵�
        AppendMenu(m_hMenu, MF_STRING, SYS_ABOUT, TEXT("����SlxCom(&A)..."));;
//         AppendMenu(m_hMenu, MF_STRING, SYS_PAINTVIEW, TEXT("���滭��(&P)..."));
//         SetMenuDefaultItem(m_hMenu, SYS_PAINTVIEW, MF_BYCOMMAND);
        AppendMenu(m_hMenu, MF_STRING, SYS_WINDOWMANAGER, TEXT("���ڹ�����(&W)..."));
        AppendMenu(m_hMenu, MF_POPUP, (UINT)InitRegPathSubMenu(&nMenuId), TEXT("ע�����ͨ��(&R)"));
        AppendMenu(m_hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(m_hMenu, MF_STRING, SYS_RESETEXPLORER, TEXT("��������Explorer(&E)"));
        AppendMenu(m_hMenu, MF_SEPARATOR, 0, NULL);
        SetMenuItemBitmaps(m_hMenu, SYS_RESETEXPLORER, MF_BYCOMMAND, g_hKillExplorerBmp, g_hKillExplorerBmp);
        AppendMenu(m_hMenu, MF_STRING, SYS_UPDATEMENU, TEXT("ˢ�²˵�����(&U)"));
        AppendMenu(m_hMenu, MF_STRING, SYS_HIDEICON, TEXT("����ʾ����ͼ��(&Q)"));
    }

private:
    //��ȡָ��ע���·���µ����������б������б��в���·������
    static set<tstring> GetRegAllSubPath(HKEY hRootKey, LPCTSTR lpRegPath)
    {
        set<tstring> result;
        HKEY hKey = NULL;

        if (ERROR_SUCCESS == RegOpenKeyEx(hRootKey, lpRegPath, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey))
        {
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
                ))
            {
                dwMaxSubKeyLen += 1;
                dwMaxSubKeyLen *= 2;

                TCHAR *szSubKey = (TCHAR *)malloc(dwMaxSubKeyLen);

                if (szSubKey != NULL)
                {
                    for (DWORD dwIndex = 0; dwIndex < dwSubKeyCount; dwIndex += 1)
                    {
                        DWORD dwSubKeySize = dwMaxSubKeyLen;

                        if (ERROR_SUCCESS == RegEnumKeyEx(hKey, dwIndex, szSubKey, &dwSubKeySize, NULL, NULL, NULL, NULL))
                        {
                            result.insert(szSubKey);
                        }
                    }
                }

                if (szSubKey != NULL)
                {
                    free((void *)szSubKey);
                }
            }

            RegCloseKey(hKey);
        }

        return result;
    }

    //��ȡָ��ע���·���µ������ַ�����ֵ��Ӧ��ֻ��עREG_SZ����
    static map<tstring, tstring> GetRegAllSzValuePairs(HKEY hRootKey, LPCTSTR lpRegPath)
    {
        map<tstring, tstring> result;
        HKEY hKey = NULL;

        if (ERROR_SUCCESS == RegOpenKeyEx(hRootKey, lpRegPath, 0, KEY_QUERY_VALUE, &hKey))
        {
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
                                result[szValueName] = (LPCTSTR)szData;
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

        return result;
    }

    //��ָ��ע���·��ӳ��Ϊ�˵����˵���IDʹ��pMenuIdָ���ֵ����ʹ��
    //bEraseHead�类ָ������ɾ��ֵ�е���ǰ��һ��·����
    //bHaveChildPath�类ָ������������
    HMENU RegistryPathToMenu(UINT *pMenuId, HKEY hRootKey, LPCTSTR lpRegPath, BOOL bEraseHead, BOOL bHaveChildPath)
    {
        HMENU hPopupMenu = CreatePopupMenu();

        //��������
        if (bHaveChildPath)
        {
            set<tstring> setSubPaths = GetRegAllSubPath(hRootKey, lpRegPath);
            set<tstring>::iterator itSubPath = setSubPaths.begin();
            for (; itSubPath != setSubPaths.end(); ++itSubPath)
            {
                tstring strSubPath = lpRegPath;

                strSubPath += TEXT("\\");
                strSubPath += *itSubPath;

                AppendMenu(
                    hPopupMenu,
                    MF_POPUP,
                    (UINT)RegistryPathToMenu(pMenuId, hRootKey, strSubPath.c_str(), FALSE, TRUE),
                    itSubPath->c_str()
                    );
            }
        }

        //����ǰ���µļ�
        map<tstring, tstring>::const_iterator itValue;
        map<tstring, tstring> mapMenu = GetRegAllSzValuePairs(hRootKey, lpRegPath);

        for (itValue = mapMenu.begin(); itValue != mapMenu.end(); ++itValue)
        {
            tstring strRegPath = itValue->second;

            if (bEraseHead)
            {
                tstring::size_type pos = strRegPath.find(TEXT('\\'));
                if (pos != strRegPath.npos)
                {
                    strRegPath = strRegPath.substr(pos + 1);
                }
            }

            if (!itValue->first.empty())        //������ע���Ĭ��ֵ
            {
                TCHAR szShortRegPath[1000];

                lstrcpyn(szShortRegPath, itValue->second.c_str(), RTL_NUMBER_OF(szShortRegPath));
                PathCompactPathHelper(m_hMenuDc, szShortRegPath, COMPACT_WIDTH);

                AppendMenu(hPopupMenu, MF_STRING, *pMenuId, (itValue->first + TEXT("\t") + szShortRegPath).c_str());
                m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, itValue->first.c_str(), strRegPath.c_str())).first;
                *pMenuId += 1;
            }
        }

        return hPopupMenu;
    }

    HMENU InitRegPathSubMenu(UINT *pMenuId)
    {
        LPCTSTR lpSysRegPath = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites");
        LPCTSTR lpSlxRegPath = TEXT("Software\\Shilyx Studio\\SlxCom\\RegPath");
        LPCTSTR lpSlxRegPath2 = TEXT("HKEY_CURRENT_USER\\Software\\Shilyx Studio\\SlxCom\\RegPath");
        HMENU hRetMenu = CreatePopupMenu();

        HMENU hBuildinMenu = CreatePopupMenu();
        for (int index = 0; index < RTL_NUMBER_OF(g_szBuildinRegPaths); index += 1)
        {
            TCHAR szShortRegPath[1000];

            lstrcpyn(szShortRegPath, g_szBuildinRegPaths[index].lpPath, RTL_NUMBER_OF(szShortRegPath));
            PathCompactPathHelper(m_hMenuDc, szShortRegPath, COMPACT_WIDTH);

            AppendMenu(hBuildinMenu, MF_STRING, *pMenuId, (tstring(g_szBuildinRegPaths[index].lpName) + TEXT("\t") + szShortRegPath).c_str());
            m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, g_szBuildinRegPaths[index].lpName, g_szBuildinRegPaths[index].lpPath)).first;
            *pMenuId += 1;
        }

        if (GetMenuItemCount(hBuildinMenu) > 0)
        {
            AppendMenu(hRetMenu, MF_POPUP, (UINT)hBuildinMenu, TEXT("ע�����λ��(&B)"));
        }

        HMENU hSysMenu = RegistryPathToMenu(pMenuId, HKEY_CURRENT_USER, lpSysRegPath, TRUE, FALSE);
        AppendMenu(hRetMenu, MF_POPUP, (UINT)hSysMenu, TEXT("ע���༭���Ѳ�λ��(&F)"));

        HMENU hSlxMenu = RegistryPathToMenu(pMenuId, HKEY_CURRENT_USER, lpSlxRegPath, FALSE, TRUE);
        InsertMenu(hSlxMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hSlxMenu, 0, MF_BYPOSITION, *pMenuId, TEXT("����SlxCom�Ѳ�(&C)..."));
        m_mapMenuItems[*pMenuId] = &*m_setMenuItemsRegistry.insert(MenuItemRegistry(*pMenuId, NULL, lpSlxRegPath2)).first;
        AppendMenu(hRetMenu, MF_POPUP, (UINT)hSlxMenu, TEXT("SlxCom�Ѳ�λ��(&S)"));
        *pMenuId += 1;

        return hRetMenu;
    }

private:
    HWND m_hWindow;
    HMENU m_hMenu;
    HINSTANCE m_hInstance;
    map<int, MenuItem *> m_mapMenuItems;
    set<MenuItemRegistry> m_setMenuItemsRegistry;

private:
    HFONT m_hMenuFont;
    HGDIOBJ m_hOldFont;
    HDC m_hMenuDc;
};

static LRESULT __stdcall NotifyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static NOTIFYICONDATA nid = {sizeof(nid)};
    static int nIconShowIndex = INT_MAX;
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

        //
        GetModuleFileName(lpCs->hInstance, g_szModulePath, RTL_NUMBER_OF(g_szModulePath));

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
        SetTimer(hWnd, TIMER_MENU, 60 * 1000 + 52, NULL);

        PostMessage(hWnd, WM_TIMER, TIMER_MENU, 0);

        //
        pMenuMgr = new MenuMgr(hWnd, lpCs->hInstance);

        //
        if (!RegisterHotKey(hWnd, HK_PAINTVIEW, MOD_ALT | MOD_SHIFT, TEXT('P')))
        {
            SafeDebugMessage(TEXT("SlxCom ���滭���ݼ�ע��ʧ��%lu��\r\n"), GetLastError());
        }

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
    else if (uMsg == WM_HOTKEY)
    {
        if (wParam == HK_PAINTVIEW)
        {
            TCHAR szCommand[MAX_PATH * 2];

            wnsprintf(
                szCommand,
                RTL_NUMBER_OF(szCommand),
                TEXT("rundll32.exe \"%s\" SlxPaintView"),
                g_szModulePath
                );

            RunCommand(szCommand);

            return 0;
        }
    }
    else if (uMsg == WM_CLOSE)
    {
        UnregisterHotKey(hWnd, HK_PAINTVIEW);
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
//         else if (lParam == WM_LBUTTONDBLCLK)
//         {
//             PostMessage(hWnd, WM_HOTKEY, HK_PAINTVIEW, 0);
//         }

        if (nIconShowIndex >= RTL_NUMBER_OF(arrIcons))
        {
            nIconShowIndex = 0;
        }
    }
    else if (uMsg == WM_HIDEICON)
    {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK CallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_bChangeButtonText)
    {
        LPCWPRETSTRUCT lpCrs = (LPCWPRETSTRUCT)lParam;

        if (lpCrs != NULL && lpCrs->message == WM_INITDIALOG)
        {
            TCHAR szWindowText[100] = TEXT("");
            HWND hYes = GetDlgItem(lpCrs->hwnd, IDYES);
            HWND hNo = GetDlgItem(lpCrs->hwnd, IDNO);
            HWND hCancel = GetDlgItem(lpCrs->hwnd, IDCANCEL);

            GetWindowText(lpCrs->hwnd, szWindowText, RTL_NUMBER_OF(szWindowText));

            if (IsWindow(hYes) && IsWindow(hNo) && IsWindow(hCancel) && lstrcmpi(szWindowText, REG_QUESTION_TITLE) == 0)
            {
                SetDlgItemText(lpCrs->hwnd, IDYES, TEXT("�ͽ�(&N)"));
                SetDlgItemText(lpCrs->hwnd, IDNO, TEXT("����(&C)"));
                SetDlgItemText(lpCrs->hwnd, IDCANCEL, TEXT("ȡ��(&Q)"));
            }
        }
    }

    return CallNextHookEx(g_hMsgHook, nCode, wParam, lParam);
}

static DWORD __stdcall NotifyIconManagerProc(LPVOID lpParam)
{
    if (g_hMsgHook != NULL)
    {
        return 0;
    }

    TCHAR szImagePath[MAX_PATH] = TEXT("");

    GetModuleFileName(GetModuleHandle(NULL), szImagePath, RTL_NUMBER_OF(szImagePath));

    if (lstrcmpi(TEXT("rundll32.exe"), PathFindFileName(szImagePath)) != 0)
    {
        DWORD dwSleepTime = 1;
        while (!IsWindow(GetTrayNotifyWndInProcess()))
        {
            Sleep((dwSleepTime++) * 1000);
        }
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
        55,
        NULL,
        NULL,
        (HINSTANCE)lpParam,
        NULL
        );

//     ShowWindow(hMainWnd, SW_SHOW);
//     UpdateWindow(hMainWnd);

    g_hMsgHook = SetWindowsHookEx(WH_CALLWNDPROCRET, CallWndRetProc, (HINSTANCE)lpParam, GetCurrentThreadId());

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

    UnhookWindowsHookEx(g_hMsgHook);
    g_hMsgHook = NULL;

    return 0;
}

void StartNotifyIconManager(HINSTANCE hinstDll)
{
    if (!IsExplorer())
    {
        return;
    }

    if (IsWow64ProcessHelper(GetCurrentProcess()))
    {
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, NotifyIconManagerProc, (LPVOID)hinstDll, 0, NULL);
    CloseHandle(hThread);
}