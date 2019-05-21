#define INITGUID
#include <Guiddef.h>
#include <Windows.h>
#include <shlwapi.h>
#include "SlxComWork.h"
#include "SlxComFactory.h"
#include "resource.h"
#include "SlxComTools.h"
#include <set>

#ifndef LVS_EX_SNAPTOGRID
#define LVS_EX_SNAPTOGRID       0x00080000  // Icons automatically snap to grid.
#endif

#pragma comment(lib, "RpcRt4.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Version.lib")

using namespace std;

#if !defined(UNICODE) || !defined(_UNICODE)
#error Need UNICODE & _UNICODE pre-defined
#endif

#define APPNAME TEXT("SlxAddin")

// {191B1456-2DC2-41a7-A3FA-AC4D017889DA}
DEFINE_GUID(GUID_SLXCOM, 0x191b1456, 0x2dc2, 0x41a7, 0xa3, 0xfa, 0xac, 0x4d, 0x1, 0x78, 0x89, 0xda);
static LPCTSTR g_lpGuidSlxCom = TEXT("{191B1456-2DC2-41a7-A3FA-AC4D017889DA}");

HINSTANCE g_hinstDll = NULL;
WCHAR g_szSlxComDllFullPath[MAX_PATH] = L"";              // slxcom.dll完整路径
WCHAR g_szSlxComDllDirectory[MAX_PATH] = L"";             // slxcom.dll所在的目录
HBITMAP g_hInstallBmp = NULL;
HBITMAP g_hUninstallBmp = NULL;
HBITMAP g_hCombineBmp = NULL;
HBITMAP g_hCopyFullPathBmp = NULL;
HBITMAP g_hAddToCopyBmp = NULL;
HBITMAP g_hAddToCutBmp = NULL;
HBITMAP g_hTryRunBmp = NULL;
HBITMAP g_hTryRunWithArgumentsBmp = NULL;
HBITMAP g_hRunCmdHereBmp = NULL;
HBITMAP g_hOpenWithNotepadBmp = NULL;
HBITMAP g_hKillExplorerBmp = NULL;
HBITMAP g_hManualCheckSignatureBmp = NULL;
HBITMAP g_hUnescapeBmp = NULL;
HBITMAP g_hAppPathBmp = NULL;
HBITMAP g_hDriverBmp = NULL;
HBITMAP g_hUnlockFileBmp = NULL;
HBITMAP g_hCopyPictureHtmlBmp = NULL;
HBITMAP g_hCreateLinkBmp = NULL;
HBITMAP g_hEdfBmp = NULL;
HBITMAP g_hLocateBmp = NULL;
OSVERSIONINFO g_osi = {sizeof(g_osi)};
BOOL g_bVistaLater = FALSE;
BOOL g_bXPLater = FALSE;
BOOL g_bElevated = FALSE;
BOOL g_bHasWin10Bash = FALSE;
BOOL g_bDebugMode = FALSE;                                  // 调试模式，dll文件名中含有DEBUG字样后启用

DWORD __stdcall OpenLastPathProc(LPVOID lpParam)
{
    TCHAR szLastPath[MAX_PATH];
    DWORD dwType = REG_NONE;
    DWORD dwSize = sizeof(szLastPath);

    SHGetValue(HKEY_CURRENT_USER,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), TEXT("SlxLastPath"),
        &dwType, szLastPath, &dwSize);

    if(dwType == REG_SZ)
    {
        WaitForInputIdle(GetCurrentProcess(), INFINITE);

        if(PathFileExists(szLastPath))
        {
            BrowseForFile(szLastPath);
        }
    }

    return 0;
}

std::set<HWND> GetAllWnds()
{
    set<HWND> setWnds;
    HWND hWindow = FindWindowExW(NULL, NULL, NULL, NULL);

    while (IsWindow(hWindow))
    {
        if (IsWindowVisible(hWindow))
        {
            setWnds.insert(hWindow);
        }

        hWindow = FindWindowExW(NULL, hWindow, NULL, NULL);
    }

    return setWnds;
}

HWND GetShellDllDefViewWnd()
{
    HWND hShellDllDefView = NULL;
    vector<HWND> vectorDesktopWnds;
    HWND hWindow = FindWindowExW(NULL, NULL, L"Progman", L"Program Manager");

    while (IsWindow(hWindow))
    {
        vectorDesktopWnds.push_back(hWindow);
        hWindow = FindWindowExW(NULL, hWindow, L"Progman", L"Program Manager");
    }

    hWindow = FindWindowExW(NULL, NULL, L"WorkerW", L"");

    while (IsWindow(hWindow))
    {
        vectorDesktopWnds.push_back(hWindow);
        hWindow = FindWindowExW(NULL, hWindow, L"WorkerW", L"");
    }

    for (vector<HWND>::const_iterator it = vectorDesktopWnds.begin(); it != vectorDesktopWnds.end(); ++it)
    {
        HWND hWindow = *it;
        hShellDllDefView = FindWindowExW(hWindow, NULL, L"SHELLDLL_DefView", NULL);

        if (IsWindow(hShellDllDefView))
        {
            break;
        }
    }

    return hShellDllDefView;
}

HWND GetDesktopListViewWnd()
{
    HWND hShellDllDefView = GetShellDllDefViewWnd();
    HWND hSysListView32 = FindWindowExW(hShellDllDefView, NULL, L"SysListView32", L"FolderView");

    if (!IsWindow(hSysListView32))
    {
        hSysListView32 = FindWindowExW(hShellDllDefView, NULL, L"SysListView32", NULL);
    }

    return hSysListView32;
}

void ModifyStyle(HWND hWindow, DWORD dwAdd, DWORD dwRemove)
{
    DWORD_PTR dwStyle = GetWindowLongPtr(hWindow, GWL_STYLE);

    dwStyle |= dwAdd;
    dwStyle &= ~dwRemove;

    SetWindowLongPtr(hWindow, GWL_STYLE, dwStyle);
}

void ModifyExStyle(HWND hWindow, DWORD dwAdd, DWORD dwRemove)
{
    DWORD_PTR dwStyle = GetWindowLongPtr(hWindow, GWL_EXSTYLE);

    dwStyle |= dwAdd;
    dwStyle &= ~dwRemove;

    SetWindowLongPtr(hWindow, GWL_EXSTYLE, dwStyle);
}

vector<POINT> GetAllIconPositions(HWND hListView)
{
    vector<POINT> vectorPositions;
    int nCount = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);

    for (int i = 0; i < nCount; ++i)
    {
        POINT pt = {-1, -1};
        SendMessageW(hListView, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
        vectorPositions.push_back(pt);
    }

    return vectorPositions;
}

vector<POINT> GetAllIconShouldBePositions(HWND hListView, int nCellSize, int nTaskBarSize, int nIconCount)
{
    RECT rect;
    vector<POINT> vectorPositions;

    GetWindowRect(hListView, &rect);

    int nRowCount = (rect.bottom - rect.top - nTaskBarSize) / nCellSize;

    for (int i = 0; i < nIconCount; ++i)
    {
        POINT pt;

        pt.x = (i / nRowCount) * nCellSize;
        pt.y = (i % nRowCount) * nCellSize;
        vectorPositions.push_back(pt);
    }

    return vectorPositions;
}

void SetListViewIconPositions(HWND hListView, const vector<POINT> &vectorPositions)
{
    LVITEMW li = {LVIF_PARAM};
    int nCount = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);

    for (int i = 0; i < nCount; ++i)
    {
        li.iItem = i;
        li.lParam = i;

        SendMessageW(hListView, LVM_SETITEM, 0, (LPARAM)&li);
    }

}

bool operator==(const POINT &pt1, const POINT &pt2)
{
    return pt1.x == pt2.x && pt1.y == pt2.y;
}

DWORD CALLBACK ReorderDesktopIcons(LPVOID lpParam)
{
//     MessageBox(NULL, NULL, NULL, MB_ICONERROR | MB_TOPMOST);
    while (true)
    {
        HWND hListView = GetDesktopListViewWnd();
        HWND hSddv = GetParent(hListView);

        if (!IsWindow(hListView) || !IsWindow(hSddv) || !IsWindowVisible(hSddv))
        {
            Sleep(1000);
            continue;
        }

        DWORD dwProcessId = 0;

        GetWindowThreadProcessId(hSddv, &dwProcessId);

        if (dwProcessId != GetCurrentProcessId())
        {
            Sleep(1000);
            continue;
        }

        SafeDebugMessage(TEXT("找到了ListView %p"), hListView);

        ModifyStyle(hListView, LVS_AUTOARRANGE, 0);
        ModifyExStyle(hListView, LVS_EX_SNAPTOGRID, 0);
//         ModifyStyle(hListView, LVS_LIST, LVS_ICON);

        vector<POINT> vectorPositions = GetAllIconPositions(hListView);
        int nCellSize = 100;

        if (vectorPositions.size() > 1)
        {
            nCellSize = vectorPositions.at(1).y - vectorPositions.at(0).y;

            if (nCellSize < 0)
            {
                nCellSize = -nCellSize;
            }
        }

        SafeDebugMessage(TEXT("CellSize：%d\n"), nCellSize);

        ModifyStyle(hListView, 0, LVS_AUTOARRANGE);
        ModifyExStyle(hListView, 0, LVS_EX_SNAPTOGRID);
        ModifyStyle(hListView, LVS_ALIGNTOP, LVS_ALIGNLEFT);

        RECT rect;

        GetWindowRect(hSddv, &rect);
        SetWindowPos(hSddv, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top - 300, SWP_NOZORDER);

        do 
        {
            bool bChanged = false;
            vector<POINT> vectorPositions = GetAllIconPositions(hListView);
            vector<POINT> vectorShouldBePositions = GetAllIconShouldBePositions(hListView, nCellSize, 40, (int)vectorPositions.size());

            if (vectorPositions != vectorShouldBePositions)
            {
                SetListViewIconPositions(hListView, vectorShouldBePositions);
            }

            if (IsDebuggerPresent() && MessageBox(NULL, NULL, NULL, MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
            {
                DebugBreak();
            }
            Sleep(3000);

        } while (IsWindow(hListView));
    }

    return 0;
}

DWORD CALLBACK CheckWin10BashProc(LPVOID)
{
    WCHAR szBashPath[MAX_PATH];

    GetSystemDirectoryW(szBashPath, RTL_NUMBER_OF(szBashPath));
    PathAppendW(szBashPath, L"\\bash.exe");

    if (PathFileExistsW(szBashPath))
    {
        g_bHasWin10Bash = TRUE;

        if (IsExplorer())
        {
            OutputDebugStringW(L"SlxCom检测到系统中安装了ubuntu子系统");
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        GetVersionEx(&g_osi);

        g_bVistaLater = g_osi.dwMajorVersion >= 6;
        g_bXPLater = g_bVistaLater || (g_osi.dwMajorVersion == 5 && g_osi.dwMinorVersion >= 1);
        g_bElevated = IsAdminMode();

        if (g_osi.dwMajorVersion >= 10)
        {
            CloseHandle(CreateThread(NULL, 0, CheckWin10BashProc, NULL, 0, NULL));
        }

        if(IsExplorer())
        {
            //CloseHandle(CreateThread(NULL, 0, ReorderDesktopIcons, NULL, 0, NULL));

            FILETIME ftExit, ftKernel, ftUser;
            FILETIME ftCreation = {0};
            FILETIME ftNow;
            FILETIME ftNowLocal;

            SYSTEMTIME st;

            GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser);

            GetLocalTime(&st);
            SystemTimeToFileTime(&st, &ftNowLocal);
            LocalFileTimeToFileTime(&ftNowLocal, &ftNow);

            if((*(unsigned __int64 *)&ftNow - *(unsigned __int64 *)&ftCreation) / 10 / 1000 / 1000 < 10)    //如果是启动时执行DllMain
            {
                DWORD dwLastPathTime = 0;
                DWORD dwSize = sizeof(dwLastPathTime);
                DWORD dwType = REG_NONE;
                DWORD dwTickCount = GetTickCount();

                SHGetValue(HKEY_CURRENT_USER,
                    TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"), TEXT("SlxLastPathTime"),
                    &dwType, &dwLastPathTime, &dwSize);

                if(dwType == REG_DWORD && dwTickCount > dwLastPathTime && dwTickCount - dwLastPathTime < 10000)
                {
                    SHDeleteValue(HKEY_CURRENT_USER,
                        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"),
                        TEXT("SlxLastPathTime"));

                    HANDLE hOpenLastPathThread = CreateThread(NULL, 0, OpenLastPathProc, NULL, 0, NULL);
                    CloseHandle(hOpenLastPathThread);
                }
            }
        }

        g_hinstDll = hInstance;
        GetModuleFileNameW(g_hinstDll, g_szSlxComDllFullPath, RTL_NUMBER_OF(g_szSlxComDllFullPath));
        lstrcpynW(g_szSlxComDllDirectory, g_szSlxComDllFullPath, RTL_NUMBER_OF(g_szSlxComDllDirectory));
        GetModuleFileNameW(g_hinstDll, g_szSlxComDllDirectory, RTL_NUMBER_OF(g_szSlxComDllDirectory));
        PathRemoveFileSpecW(g_szSlxComDllDirectory);

        WCHAR szDllName[MAX_PATH];
        lstrcpynW(szDllName, PathFindFileNameW(g_szSlxComDllFullPath), RTL_NUMBER_OF(szDllName));
        CharLowerBuffW(szDllName, RTL_NUMBER_OF(szDllName));
        g_bDebugMode = StrStrW(PathFindFileNameW(g_szSlxComDllFullPath), L"debug") != NULL ||
            StrStrW(PathFindFileNameW(g_szSlxComDllFullPath), L"dbg") != NULL ;

        if (g_bDebugMode && IsExplorer())
        {
            SafeDebugMessage(L"SlxCom 启用Debug模式\n");
        }

        g_hInstallBmp               = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_INSTALL));
        g_hUninstallBmp             = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_UNINSTALL));
        g_hCombineBmp               = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_COMBINE));
        g_hCopyFullPathBmp          = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_COPYFULLPATH));
        g_hAddToCopyBmp             = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ADDTOCOPY));
        g_hAddToCutBmp              = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ADDTOCUT));
        g_hTryRunBmp                = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_TRYRUN));
        g_hTryRunWithArgumentsBmp   = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_TRYRUNWITHARGUMENTS));
        g_hRunCmdHereBmp            = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_RUNCMDHERE));
        g_hOpenWithNotepadBmp       = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_OPENWITHNOTEPAD));
        g_hKillExplorerBmp          = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_KILLEXPLORER));
        g_hManualCheckSignatureBmp  = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_CHECKSIGNATURE));
        g_hUnescapeBmp              = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_UNESCAPE));
        g_hAppPathBmp               = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_APPPATH));
        g_hDriverBmp                = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_DRIVER));
        g_hUnlockFileBmp            = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_UNLOCKFILE));
        g_hCopyPictureHtmlBmp       = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_COPY_PICTURE_HTML));
        g_hCreateLinkBmp            = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_CREATELINK));
        g_hEdfBmp                   = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_EDF));
        g_hLocateBmp                = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_LOCATE));

        DisableThreadLibraryCalls(hInstance);
        SlxWork(hInstance);
    }
    else if(dwReason == DLL_PROCESS_DETACH)
    {
        return FALSE;
    }

    return TRUE;
}

STDAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    *ppv = NULL;

    if(rclsid == GUID_SLXCOM)
    {
        CSlxComFactory *pFactory = new CSlxComFactory;

        if(pFactory == NULL)
        {
            return E_OUTOFMEMORY;
        }

        return pFactory->QueryInterface(riid, ppv);
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

LRESULT ConfigBrowserLinkFilePosition(BOOL bInstall)
{
    if (g_osi.dwMajorVersion <= 5)
    {
        if(bInstall)
        {
            TCHAR szCommand[MAX_PATH + 100];
            TCHAR szDllPath[MAX_PATH];

            GetModuleFileName(g_hinstDll, szDllPath, MAX_PATH);

            wnsprintf(
                szCommand,
                sizeof(szCommand) / sizeof(TCHAR),
                TEXT("rundll32 \"%s\" BrowserLinkFilePosition \"%%1\""),
                szDllPath
                );

            return SHSetValue(
                HKEY_CLASSES_ROOT,
                TEXT("lnkfile\\shell\\打开文件位置(&B)\\command"),
                NULL,
                REG_SZ,
                szCommand,
                (lstrlen(szCommand) + 1) * sizeof(TCHAR)
                );
        }
        else
        {
            return SHDeleteKey(HKEY_CLASSES_ROOT, TEXT("lnkfile\\shell\\打开文件位置(&B)"));
        }
    }

    return ERROR_SUCCESS;
}

// 老版本的SlxCom的ShellIconOverlayIdentifiers的标记为99_SlxAddin
// 目的是为了给svn让位，不影响svn的图标覆盖显示，但在装了office2013
// 以后，微软直接建立了三个skyDrive相关的图标覆盖标记，且都是以空格
// 开头，为了争得先机，新的SlxCom也将以空格开头。基于兼容性考虑，会
// 自动删除原有的99_SlxAddin标记。安装和卸载时都自动调用
static void Ensure__99_SlxAddin__NotExists()
{
    TCHAR szRegPath[1024];
    int nSumValue = 0;

    wnsprintf(szRegPath, RTL_NUMBER_OF(szRegPath),
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s"),
        APPNAME);

    SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);
}

STDAPI DllRegisterServer(void)
{
    TCHAR szRegPath[1000];
    TCHAR szDllPath[MAX_PATH];
    int nSumValue = 0;

    GetModuleFileName(g_hinstDll, szDllPath, MAX_PATH);
    Ensure__99_SlxAddin__NotExists();

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ 00_%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\PropertySheetHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\PropertySheetHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\DragDropHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, TEXT("dllfile\\ShellEx\\IconHandler"), NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("CLSID\\%s\\InprocServer32"),
        g_lpGuidSlxCom);
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szDllPath, (lstrlen(szDllPath) + 1) * sizeof(TCHAR));
    nSumValue += !!SHSetValue(HKEY_CLASSES_ROOT, szRegPath, TEXT("ThreadingModel"), REG_SZ, TEXT("Apartment"), 20);

    nSumValue += !!ConfigBrowserLinkFilePosition(TRUE);

    if (nSumValue > 2)
    {
        if (g_osi.dwMajorVersion >= 6 && !g_bElevated)
        {
            if (IDYES == MessageBox(
                NULL,
                TEXT("注册未完全成功，是否尝试以管理员方式注册？\r\n\r\n")
                TEXT("您也可以手动尝试以管理员方式运行regsvr32程序来解决问题。\r\n")
                TEXT("点击“是”尝试以管理员方式运行注册工具。"),
#ifdef _WIN64
                TEXT("注册SlxCom 64位"),
#else
                TEXT("注册SlxCom 32位"),
#endif
                MB_YESNOCANCEL | MB_ICONQUESTION
                ))
            {
                TCHAR szRegSvr32[MAX_PATH];

                GetSystemDirectory(szRegSvr32, RTL_NUMBER_OF(szRegSvr32));
                PathAppend(szRegSvr32, TEXT("\\regsvr32.exe"));
                PathQuoteSpaces(szDllPath);

                ShellExecute(NULL, TEXT("runas"), szRegSvr32, szDllPath, NULL, SW_SHOW);
            }

            ExitProcess(0);
        }
        else
        {
            return S_FALSE;
        }
    }

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    TCHAR szRegPath[1000];
    int nSumValue = 0;

    Ensure__99_SlxAddin__NotExists();

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ 00_%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\PropertySheetHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\PropertySheetHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\DragDropHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("CLSID\\%s"),
        g_lpGuidSlxCom);
    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    nSumValue += !!SHDeleteKey(HKEY_CLASSES_ROOT, TEXT("dllfile\\ShellEx\\IconHandler"));

    nSumValue += !!ConfigBrowserLinkFilePosition(FALSE);

    if (nSumValue != ERROR_SUCCESS)
    {
        if (g_osi.dwMajorVersion >= 6 && !g_bElevated)
        {
            if (IDYES == MessageBox(
                NULL,
                TEXT("卸载未完全成功，是否尝试以管理员方式卸载？\r\n\r\n")
                TEXT("您也可以手动尝试以管理员方式运行regsvr32程序来解决问题。\r\n")
                TEXT("点击“是”尝试以管理员方式运行卸载工具。"),
#ifdef _WIN64
                TEXT("卸载SlxCom 64位"),
#else
                TEXT("卸载SlxCom 32位"),
#endif
                MB_YESNOCANCEL | MB_ICONQUESTION
                ))
            {
                TCHAR szRegSvr32[MAX_PATH];
                TCHAR szDllPath[MAX_PATH + 20];

                GetSystemDirectory(szRegSvr32, RTL_NUMBER_OF(szRegSvr32));
                PathAppend(szRegSvr32, TEXT("\\regsvr32.exe"));

                GetModuleFileName(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
                PathQuoteSpaces(szDllPath);
                StrCatBuff(szDllPath, TEXT(" /u"), RTL_NUMBER_OF(szDllPath));

                ShellExecute(NULL, TEXT("runas"), szRegSvr32, szDllPath, NULL, SW_SHOW);
            }

            ExitProcess(0);
        }
        else
        {
            return S_FALSE;
        }
    }

    return S_OK;
}
