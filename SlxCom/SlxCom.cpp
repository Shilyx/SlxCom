#define INITGUID
#include <Guiddef.h>
#include <Windows.h>
#include <shlwapi.h>
#include "SlxComWork.h"
#include "SlxComFactory.h"
#include "resource.h"

#pragma comment(lib, "RpcRt4.lib")
#pragma comment(lib, "Shlwapi.lib")

#define APPNAME TEXT("SlxAddin")

// {191B1456-2DC2-41a7-A3FA-AC4D017889DA}
DEFINE_GUID(GUID_SLXCOM, 0x191b1456, 0x2dc2, 0x41a7, 0xa3, 0xfa, 0xac, 0x4d, 0x1, 0x78, 0x89, 0xda);

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
BOOL g_isExplorer = FALSE;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        TCHAR szExePath[MAX_PATH] = TEXT("");

        GetModuleFileName(GetModuleHandle(NULL), szExePath, MAX_PATH);
        g_isExplorer = lstrcmpi(PathFindFileName(szExePath), TEXT("explorer.exe")) == 0;

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

STDAPI DllRegisterServer(void)
{
    TCHAR *szGuid = NULL;

    if(RPC_S_OK == UuidToString((GUID *)&GUID_SLXCOM, &szGuid))
    {
        TCHAR szRegPath[1000];
        TCHAR szGuidWithQuota[100];
        TCHAR szDllPath[MAX_PATH];

        GetModuleFileName(g_hinstDll, szDllPath, MAX_PATH);

        wnsprintf(szGuidWithQuota, sizeof(szGuidWithQuota) / sizeof(TCHAR), TEXT("{%s}"), szGuid);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s"),
            APPNAME);
        SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, szGuidWithQuota, (lstrlen(szGuidWithQuota) + 1) * sizeof(TCHAR));

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szGuidWithQuota, (lstrlen(szGuidWithQuota) + 1) * sizeof(TCHAR));

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szGuidWithQuota, (lstrlen(szGuidWithQuota) + 1) * sizeof(TCHAR));

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szGuidWithQuota, (lstrlen(szGuidWithQuota) + 1) * sizeof(TCHAR));

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szGuidWithQuota, (lstrlen(szGuidWithQuota) + 1) * sizeof(TCHAR));

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("CLSID\\%s\\InprocServer32"),
            szGuidWithQuota);
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, NULL, REG_SZ, szDllPath, (lstrlen(szDllPath) + 1) * sizeof(TCHAR));
        SHSetValue(HKEY_CLASSES_ROOT, szRegPath, TEXT("ThreadingModel"), REG_SZ, TEXT("Apartment"), 20);

        RpcStringFree(&szGuid);
    }

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    TCHAR *szGuid = NULL;

    if(RPC_S_OK == UuidToString((GUID *)&GUID_SLXCOM, &szGuid))
    {
        TCHAR szRegPath[1000];
        TCHAR szGuidWithQuota[100];

        wnsprintf(szGuidWithQuota, sizeof(szGuidWithQuota) / sizeof(TCHAR), TEXT("{%s}"), szGuid);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\99_%s"),
            APPNAME);
        SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("*\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Directory\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Directory\\Background\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("Drive\\shellex\\ContextMenuHandlers\\%s"),
            APPNAME);
        SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);

        wnsprintf(szRegPath, sizeof(szRegPath) / sizeof(TCHAR),
            TEXT("CLSID\\%s"),
            szGuidWithQuota);
        SHDeleteKey(HKEY_CLASSES_ROOT, szRegPath);

        RpcStringFree(&szGuid);
    }

    return S_OK;
}
