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
OSVERSIONINFO g_osi = {sizeof(g_osi)};

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

VOID ConfigBrowserLinkFilePosition(BOOL bInstall)
{
    OSVERSIONINFO osi = {sizeof(osi)};

    GetVersionEx(&osi);

    if(osi.dwMajorVersion <= 5)
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

            SHSetValue(
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
            SHDeleteKey(HKEY_CLASSES_ROOT, TEXT("lnkfile\\shell\\打开文件位置(&B)"));
        }
    }
}

STDAPI DllRegisterServer(void)
{
    TCHAR szRegPath[1000];
    TCHAR szDllPath[MAX_PATH];

    GetModuleFileName(g_hinstDll, szDllPath, MAX_PATH);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s"),
        APPNAME);
    SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, g_lpGuidSlxCom, (lstrlen(g_lpGuidSlxCom) + 1) * sizeof(TCHAR));

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("CLSID\\%s\\InprocServer32"),
        g_lpGuidSlxCom);
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szDllPath, (lstrlen(szDllPath) + 1) * sizeof(TCHAR));
    SHSetValue(HKEY_CLASSES_ROOT, szRegPath, TEXT("ThreadingModel"), REG_SZ, TEXT("Apartment"), 20);

    ConfigBrowserLinkFilePosition(TRUE);

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    TCHAR szRegPath[1000];

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s"),
        APPNAME);
    SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
        APPNAME);
    SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
        TEXT("CLSID\\%s"),
        g_lpGuidSlxCom);
    SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

    ConfigBrowserLinkFilePosition(FALSE);

    return S_OK;
}
