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

#define APPNAME L"SlxAddin"

// {191B1456-2DC2-41a7-A3FA-AC4D017889DA}
DEFINE_GUID(GUID_SLXCOM, 0x191b1456, 0x2dc2, 0x41a7, 0xa3, 0xfa, 0xac, 0x4d, 0x1, 0x78, 0x89, 0xda);
static LPCWSTR g_lpGuidSlxCom = L"{191B1456-2DC2-41a7-A3FA-AC4D017889DA}";

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

DWORD CALLBACK OpenLastPathProc(LPVOID lpParam)
{
    WCHAR szLastPath[MAX_PATH];
    DWORD dwType = REG_NONE;
    DWORD dwSize = sizeof(szLastPath);

    SHGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"SlxLastPath",
        &dwType, szLastPath, &dwSize);

    if(dwType == REG_SZ)
    {
        WaitForInputIdle(GetCurrentProcess(), INFINITE);

        if(PathFileExistsW(szLastPath))
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
    DWORD dwStyle = GetWindowLong(hWindow, GWL_STYLE);

    dwStyle |= dwAdd;
    dwStyle &= ~dwRemove;

    SetWindowLong(hWindow, GWL_STYLE, dwStyle);
}

void ModifyExStyle(HWND hWindow, DWORD dwAdd, DWORD dwRemove)
{
    DWORD dwStyle = GetWindowLong(hWindow, GWL_EXSTYLE);

    dwStyle |= dwAdd;
    dwStyle &= ~dwRemove;

    SetWindowLong(hWindow, GWL_EXSTYLE, dwStyle);
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
//     MessageBoxW(NULL, NULL, NULL, MB_ICONERROR | MB_TOPMOST);
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

        SafeDebugMessage(L"找到了ListView %p", hListView);

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

        SafeDebugMessage(L"CellSize：%d\n", nCellSize);

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

            if (IsDebuggerPresent() && MessageBoxW(NULL, NULL, NULL, MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
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

                SHGetValueW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"SlxLastPathTime",
                    &dwType, &dwLastPathTime, &dwSize);

                if(dwType == REG_DWORD && dwTickCount > dwLastPathTime && dwTickCount - dwLastPathTime < 10000)
                {
                    SHDeleteValueW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                        L"SlxLastPathTime");

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

        g_hInstallBmp               = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_INSTALL));
        g_hUninstallBmp             = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_UNINSTALL));
        g_hCombineBmp               = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_COMBINE));
        g_hCopyFullPathBmp          = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_COPYFULLPATH));
        g_hAddToCopyBmp             = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_ADDTOCOPY));
        g_hAddToCutBmp              = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_ADDTOCUT));
        g_hTryRunBmp                = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_TRYRUN));
        g_hTryRunWithArgumentsBmp   = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_TRYRUNWITHARGUMENTS));
        g_hRunCmdHereBmp            = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_RUNCMDHERE));
        g_hOpenWithNotepadBmp       = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_OPENWITHNOTEPAD));
        g_hKillExplorerBmp          = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_KILLEXPLORER));
        g_hManualCheckSignatureBmp  = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_CHECKSIGNATURE));
        g_hUnescapeBmp              = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_UNESCAPE));
        g_hAppPathBmp               = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_APPPATH));
        g_hDriverBmp                = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_DRIVER));
        g_hUnlockFileBmp            = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_UNLOCKFILE));
        g_hCopyPictureHtmlBmp       = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_COPY_PICTURE_HTML));
        g_hCreateLinkBmp            = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_CREATELINK));
        g_hEdfBmp                   = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_EDF));
        g_hLocateBmp                = LoadBitmapW(hInstance, MAKEINTRESOURCEW(IDB_LOCATE));

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
            WCHAR szCommand[MAX_PATH + 100];
            WCHAR szDllPath[MAX_PATH];

            GetModuleFileNameW(g_hinstDll, szDllPath, MAX_PATH);

            wnsprintfW(
                szCommand,
                sizeof(szCommand) / sizeof(WCHAR),
                L"rundll32 \"%s\" BrowserLinkFilePosition \"%%1\"",
                szDllPath
                );

            return SHSetValueW(
                HKEY_CLASSES_ROOT,
                L"lnkfile\\shell\\打开文件位置(&B)\\command",
                NULL,
                REG_SZ,
                szCommand,
                (lstrlenW(szCommand) + 1) * sizeof(WCHAR)
                );
        }
        else
        {
            return SHDeleteKeyW(HKEY_CLASSES_ROOT, L"lnkfile\\shell\\打开文件位置(&B)");
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
    WCHAR szRegPath[1024];
    int nSumValue = 0;

    wnsprintfW(szRegPath, RTL_NUMBER_OF(szRegPath),
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s",
        APPNAME);

    SHDeleteKeyW(HKEY_LOCAL_MACHINE, szRegPath);
}

STDAPI DllRegisterServer(void)
{
    WCHAR szRegPath[1000];
    WCHAR szDllPath[MAX_PATH];
    int nSumValue = 0;

    GetModuleFileNameW(g_hinstDll, szDllPath, MAX_PATH);
    Ensure__99_SlxAddin__NotExists();

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ 00_%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"*\\shellex\\PropertySheetHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\PropertySheetHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"*\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\DragDropHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\Background\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Drive\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, L"dllfile\\ShellEx\\IconHandler", NULL, REG_SZ, g_lpGuidSlxCom, (lstrlenW(g_lpGuidSlxCom) + 1) * sizeof(WCHAR));

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"CLSID\\%s\\InprocServer32",
        g_lpGuidSlxCom);
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szDllPath, (lstrlenW(szDllPath) + 1) * sizeof(WCHAR));
    nSumValue += !!SHSetValueW(HKEY_CLASSES_ROOT, szRegPath, L"ThreadingModel", REG_SZ, L"Apartment", 20);

    nSumValue += !!ConfigBrowserLinkFilePosition(TRUE);

    if (nSumValue > 2)
    {
        if (g_osi.dwMajorVersion >= 6 && !g_bElevated)
        {
            if (IDYES == MessageBoxW(
                NULL,
                L"注册未完全成功，是否尝试以管理员方式注册？\r\n\r\n"
                L"您也可以手动尝试以管理员方式运行regsvr32程序来解决问题。\r\n"
                L"点击“是”尝试以管理员方式运行注册工具。",
#ifdef _WIN64
                L"注册SlxCom 64位",
#else
                L"注册SlxCom 32位",
#endif
                MB_YESNOCANCEL | MB_ICONQUESTION
                ))
            {
                WCHAR szRegSvr32[MAX_PATH];

                GetSystemDirectoryW(szRegSvr32, RTL_NUMBER_OF(szRegSvr32));
                PathAppendW(szRegSvr32, L"\\regsvr32.exe");
                PathQuoteSpacesW(szDllPath);

                ShellExecuteW(NULL, L"runas", szRegSvr32, szDllPath, NULL, SW_SHOW);
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
    WCHAR szRegPath[1000];
    int nSumValue = 0;

    Ensure__99_SlxAddin__NotExists();

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\ 00_%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_LOCAL_MACHINE, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"*\\shellex\\PropertySheetHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\PropertySheetHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\DragDropHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"*\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Directory\\Background\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"Drive\\shellex\\ContextMenuHandlers\\%s",
        APPNAME);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintfW(szRegPath, sizeof(szRegPath) / sizeof(WCHAR),
        L"CLSID\\%s",
        g_lpGuidSlxCom);
    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, szRegPath);

    nSumValue += !!SHDeleteKeyW(HKEY_CLASSES_ROOT, L"dllfile\\ShellEx\\IconHandler");

    nSumValue += !!ConfigBrowserLinkFilePosition(FALSE);

    if (nSumValue != ERROR_SUCCESS)
    {
        if (g_osi.dwMajorVersion >= 6 && !g_bElevated)
        {
            if (IDYES == MessageBoxW(
                NULL,
                L"卸载未完全成功，是否尝试以管理员方式卸载？\r\n\r\n"
                L"您也可以手动尝试以管理员方式运行regsvr32程序来解决问题。\r\n"
                L"点击“是”尝试以管理员方式运行卸载工具。",
#ifdef _WIN64
                L"卸载SlxCom 64位",
#else
                L"卸载SlxCom 32位",
#endif
                MB_YESNOCANCEL | MB_ICONQUESTION
                ))
            {
                WCHAR szRegSvr32[MAX_PATH];
                WCHAR szDllPath[MAX_PATH + 20];

                GetSystemDirectoryW(szRegSvr32, RTL_NUMBER_OF(szRegSvr32));
                PathAppendW(szRegSvr32, L"\\regsvr32.exe");

                GetModuleFileNameW(g_hinstDll, szDllPath, RTL_NUMBER_OF(szDllPath));
                PathQuoteSpacesW(szDllPath);
                StrCatBuffW(szDllPath, L" /u", RTL_NUMBER_OF(szDllPath));

                ShellExecuteW(NULL, L"runas", szRegSvr32, szDllPath, NULL, SW_SHOW);
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
