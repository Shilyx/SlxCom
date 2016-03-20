#define INITGUID
#include <Guiddef.h>
#include <Windows.h>
#include <shlwapi.h>
#include "SlxComWork.h"
#include "SlxComFactory.h"
#include "resource.h"
#include "SlxComTools.h"

#pragma comment(lib, "RpcRt4.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Version.lib")

#if !defined(UNICODE) || !defined(_UNICODE)
#error Need UNICODE & _UNICODE pre-defined
#endif

#define APPNAME TEXT("SlxAddin")

// {191B1456-2DC2-41a7-A3FA-AC4D017889DA}
DEFINE_GUID(GUID_SLXCOM, 0x191b1456, 0x2dc2, 0x41a7, 0xa3, 0xfa, 0xac, 0x4d, 0x1, 0x78, 0x89, 0xda);
static LPCTSTR g_lpGuidSlxCom = TEXT("{191B1456-2DC2-41a7-A3FA-AC4D017889DA}");

HINSTANCE g_hinstDll = NULL;
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
OSVERSIONINFO g_osi = {sizeof(g_osi)};
BOOL g_bVistaLater = FALSE;
BOOL g_bXPLater = FALSE;
BOOL g_bElevated = FALSE;

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

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        GetVersionEx(&g_osi);

        g_bVistaLater = g_osi.dwMajorVersion >= 6;
        g_bXPLater = g_bVistaLater || (g_osi.dwMajorVersion == 5 && g_osi.dwMinorVersion >= 1);
        g_bElevated = IsAdminMode();

        if(IsExplorer())
        {
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

        g_hinstDll                  = hInstance;
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
