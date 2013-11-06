#include "SlxComTools.h"
#include <wintrust.h>
#include <Softpub.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <WinTrust.h>
#include "resource.h"
#include "lib/deelx.h"
#include <TlHelp32.h>
#include <shlobj.h>
#pragma warning(disable: 4786)
#include <set>
#include <iostream>
#include <sstream>
#include "lib/charconv.h"

using namespace std;

#pragma comment(lib, "Wintrust.lib")

typedef HANDLE HCATADMIN;
typedef HANDLE HCATINFO;

typedef BOOL (WINAPI* PFN_CryptCATAdminAcquireContext)(
    HCATADMIN* phCatAdmin,
    const GUID* pgSubsystem,
    DWORD dwFlags
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminReleaseContext)(
    HCATADMIN hCatAdmin,
    DWORD dwFlags
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminCalcHashFromFileHandle)(
    HANDLE hFile,
    DWORD* pcbHash,
    BYTE* pbHash,
    DWORD dwFlags
    );

typedef HCATINFO (WINAPI* PFN_CryptCATAdminEnumCatalogFromHash)(
    HCATADMIN hCatAdmin,
    BYTE* pbHash,
    DWORD cbHash,
    DWORD dwFlags,
    HCATINFO* phPrevCatInfo
    );

typedef BOOL (WINAPI* PFN_CryptCATAdminReleaseCatalogContext)(
    HCATADMIN hCatAdmin,
    HCATINFO hCatInfo,
    DWORD dwFlags
    );

typedef LONG (WINAPI* PFN_WinVerifyTrust)(
    HWND hWnd,
    GUID* pgActionID,
    WINTRUST_DATA* pWinTrustData
    );

static PFN_CryptCATAdminAcquireContext          g_pfnCryptCATAdminAcquireContext            = NULL;
static PFN_CryptCATAdminReleaseContext          g_pfnCryptCATAdminReleaseContext            = NULL;
static PFN_CryptCATAdminCalcHashFromFileHandle  g_pfnCryptCATAdminCalcHashFromFileHandle    = NULL;
static PFN_CryptCATAdminEnumCatalogFromHash     g_pfnCryptCATAdminEnumCatalogFromHash       = NULL;
static PFN_CryptCATAdminReleaseCatalogContext   g_pfnCryptCATAdminReleaseCatalogContext     = NULL;
static PFN_WinVerifyTrust                       g_pfnWinVerifyTrust                         = NULL;

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

BOOL LoadWinTrustDll()
{
    static BOOL bResult = FALSE;

    if(bResult)
    {
        return bResult;
    }

    HMODULE hModule = GetModuleHandle(SP_POLICY_PROVIDER_DLL_NAME);

    if(hModule == NULL)
    {
        hModule = LoadLibrary(SP_POLICY_PROVIDER_DLL_NAME);
    }

    if(hModule != NULL)
    {
        do 
        {
            (FARPROC&)g_pfnCryptCATAdminAcquireContext = GetProcAddress(hModule, "CryptCATAdminAcquireContext");
            if(g_pfnCryptCATAdminAcquireContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminReleaseContext = GetProcAddress(hModule, "CryptCATAdminReleaseContext");
            if(g_pfnCryptCATAdminReleaseContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminCalcHashFromFileHandle = GetProcAddress(hModule, "CryptCATAdminCalcHashFromFileHandle");
            if(g_pfnCryptCATAdminCalcHashFromFileHandle == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminEnumCatalogFromHash = GetProcAddress(hModule, "CryptCATAdminEnumCatalogFromHash");
            if(g_pfnCryptCATAdminEnumCatalogFromHash == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnCryptCATAdminReleaseCatalogContext = GetProcAddress(hModule, "CryptCATAdminReleaseCatalogContext");
            if(g_pfnCryptCATAdminReleaseCatalogContext == NULL)
            {
                break;
            }

            (FARPROC&)g_pfnWinVerifyTrust = GetProcAddress(hModule, "WinVerifyTrust");
            if(g_pfnWinVerifyTrust == NULL)
            {
                break;
            }

            bResult = TRUE;

        } while (FALSE);
    }

    return bResult;
}

DWORD SHSetTempValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData)
{
    HKEY hKey = NULL;
    DWORD dwRet = RegCreateKeyEx(hRootKey, pszSubKey, 0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if(ERROR_SUCCESS == dwRet)
    {
        dwRet = RegSetValueEx(hKey, pszValue, 0, dwType, (const unsigned char *)pvData, cbData);

        RegCloseKey(hKey);
    }

    return dwRet;
}

BOOL IsFileSignedBySelf(LPCTSTR lpFilePath)
{
    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_FILE_INFO sWintrustFileInfo;
    WINTRUST_DATA      sWintrustData;

    memset((void*)&sWintrustFileInfo, 0x00, sizeof(WINTRUST_FILE_INFO));
    memset((void*)&sWintrustData, 0x00, sizeof(WINTRUST_DATA));

    sWintrustFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    sWintrustFileInfo.pcwszFilePath = lpFilePath;
    sWintrustFileInfo.hFile = NULL;

    sWintrustData.cbStruct            = sizeof(WINTRUST_DATA);
    sWintrustData.dwUIChoice          = WTD_UI_NONE;
    sWintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    sWintrustData.dwUnionChoice       = WTD_CHOICE_FILE;
    sWintrustData.pFile               = &sWintrustFileInfo;
    sWintrustData.dwStateAction       = WTD_STATEACTION_VERIFY;

    return S_OK == WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);
}

BOOL GetFileHash(LPCTSTR lpFilePath, BYTE btHash[], DWORD *pcbSize)
{
    BOOL bResult = FALSE;
    HANDLE hFile = CreateFile(
        lpFilePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if(hFile != INVALID_HANDLE_VALUE)
    {
        __try
        {
            if(g_pfnCryptCATAdminCalcHashFromFileHandle(hFile, pcbSize, btHash, 0))
            {
                bResult = TRUE;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }

        CloseHandle(hFile);
    }

    return bResult;
}

BOOL IsFileSignedByCatlog(LPCTSTR lpFilePath)
{
    BOOL bResult = FALSE;

    if(LoadWinTrustDll())
    {
        BYTE btHash[100];
        DWORD dwHashSize = sizeof(btHash);

        if(GetFileHash(lpFilePath, btHash, &dwHashSize))
        {
            HANDLE hCatHandle = NULL;

            g_pfnCryptCATAdminAcquireContext(&hCatHandle, NULL, 0);

            if(hCatHandle != NULL)
            {
                __try
                {
                    HANDLE hCatalogContext = g_pfnCryptCATAdminEnumCatalogFromHash(hCatHandle, btHash, dwHashSize, 0, NULL);

                    if(hCatalogContext != NULL)
                    {
                        g_pfnCryptCATAdminReleaseCatalogContext(hCatHandle, hCatalogContext, 0);

                        bResult = TRUE;
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                }

                g_pfnCryptCATAdminReleaseContext(hCatHandle, 0);
            }
        }
    }

    return bResult;
}

BOOL IsFileSigned(LPCTSTR lpFilePath)
{
    return IsFileSignedBySelf(lpFilePath) || IsFileSignedByCatlog(lpFilePath);
}

BOOL SaveResourceToFile(LPCTSTR lpResType, LPCTSTR lpResName, LPCTSTR lpFilePath)
{
    BOOL bSucceed = FALSE;
    DWORD dwResSize = 0;

    HANDLE hFile = CreateFile(lpFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if(hFile != INVALID_HANDLE_VALUE)
    {
        HRSRC hRsrc = FindResource(g_hinstDll, lpResName, lpResType);

        if(hRsrc != NULL)
        {
            HGLOBAL hGlobal = LoadResource(g_hinstDll, hRsrc);

            if(hGlobal != NULL)
            {
                LPCSTR lpBuffer = (LPCSTR)LockResource(hGlobal);
                dwResSize = SizeofResource(g_hinstDll, hRsrc);

                if(lpBuffer != NULL && dwResSize > 0)
                {
                    DWORD dwBytesWritten = 0;

                    bSucceed = WriteFile(hFile, lpBuffer, dwResSize, &dwBytesWritten, NULL);
                }
            }
        }

        CloseHandle(hFile);
    }

    return bSucceed;
}

BOOL GetResultPath(LPCTSTR lpRarPath, LPTSTR lpResultPath, int nLength)
{
    if(nLength >= MAX_PATH)
    {
        TCHAR szJpgPath[MAX_PATH];

        lstrcpyn(szJpgPath, lpRarPath, MAX_PATH);
        PathRenameExtension(szJpgPath, TEXT(".jpg"));

        return PathYetAnotherMakeUniqueName(lpResultPath, szJpgPath, NULL, NULL);
    }

    return FALSE;
}

BOOL DoCombine(LPCTSTR lpFile1, LPCTSTR lpFile2, LPCTSTR lpFileNew)
{
    IStream *pStream1 = NULL;
    IStream *pStream2 = NULL;
    IStream *pStreamNew = NULL;

    if(S_OK == SHCreateStreamOnFile(lpFile1, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream1))
    {
        if(S_OK == SHCreateStreamOnFile(lpFile2, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream2))
        {
            if(S_OK == SHCreateStreamOnFile(lpFileNew, STGM_WRITE | STGM_SHARE_DENY_NONE | STGM_CREATE, &pStreamNew))
            {
                ULARGE_INTEGER uliSize1, uliSize2;
                STATSTG stat;

                pStream1->Stat(&stat, STATFLAG_NONAME);
                uliSize1.QuadPart = stat.cbSize.QuadPart;

                pStream2->Stat(&stat, STATFLAG_NONAME);
                uliSize2.QuadPart = stat.cbSize.QuadPart;

                pStream1->CopyTo(pStreamNew, uliSize1, NULL, NULL);
                pStream2->CopyTo(pStreamNew, uliSize2, NULL, NULL);

                pStreamNew->Release();
            }

            pStream2->Release();
        }

        pStream1->Release();
    }

    return TRUE;
}

BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength)
{
    //Get jpg file path
    TCHAR szJpgPath[MAX_PATH];

    if(GetFileAttributes(lpJpgPath) == INVALID_FILE_ATTRIBUTES)
    {
        GetModuleFileName(g_hinstDll, szJpgPath, MAX_PATH);
        PathAppend(szJpgPath, TEXT("\\..\\Default.jpg"));

        if(GetFileAttributes(szJpgPath) == INVALID_FILE_ATTRIBUTES)
        {
            SaveResourceToFile(TEXT("RT_FILE"), MAKEINTRESOURCE(IDR_DEFAULTJPG), szJpgPath);
        }
    }
    else
    {
        lstrcpyn(szJpgPath, lpJpgPath, MAX_PATH);
    }

    if(GetFileAttributes(szJpgPath) != INVALID_FILE_ATTRIBUTES)
    {
        if(GetResultPath(lpRarPath, lpResultPath, nLength))
        {
            return DoCombine(szJpgPath, lpRarPath, lpResultPath);
        }
    }

    return FALSE;
}

BOOL SetClipboardText(LPCTSTR lpText)
{
    BOOL bResult = FALSE;
    HGLOBAL hGlobal = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | GMEM_DDESHARE, (lstrlen(lpText) + 1) * sizeof(TCHAR));

    if(hGlobal != NULL)
    {
        LPTSTR lpBuffer = (LPTSTR)GlobalLock(hGlobal);

        if(lpBuffer != NULL)
        {
            lstrcpy(lpBuffer, lpText);

            GlobalUnlock(hGlobal);

            if(OpenClipboard(NULL))
            {
                EmptyClipboard();
#ifdef UNICODE
                SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
                SetClipboardData(CF_TEXT, hGlobal);
#endif
                CloseClipboard();

                bResult = TRUE;
            }
        }
    }

    if(!bResult && hGlobal != NULL)
    {
        GlobalFree(hGlobal);
    }

    return bResult;
}

BOOL RunCommand(LPTSTR lpCommandLine, LPCTSTR lpCurrentDirectory)
{
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.wShowWindow = SW_SHOW;   //no use

    if(CreateProcess(NULL, lpCommandLine, NULL, NULL, FALSE, 0, NULL, lpCurrentDirectory, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return TRUE;
    }

    return FALSE;
}

INT_PTR __stdcall RunCommandWithArgumentsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(hIcon == NULL)
        {
            hIcon = LoadIcon(g_hinstDll, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }

        SetDlgItemText(hwndDlg, IDC_FILE, (LPCTSTR)lParam);
        return FALSE;

    case WM_SYSCOMMAND:
        if(wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_COMMAND:
        if(LOWORD(wParam) == IDC_RUN)
        {
            if(IDYES == MessageBox(hwndDlg, TEXT("确定要尝试将此文件作为可执行文件运行吗？"), TEXT("请确认"), MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON3))
            {
                BOOL bSucceed = FALSE;
                INT_PTR nLength = 0;
                INT_PTR nOffset = 0;

                nLength += SendDlgItemMessage(hwndDlg, IDC_FILE, WM_GETTEXTLENGTH, 0, 0);
                nLength += SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, WM_GETTEXTLENGTH, 0, 0);
                nLength += 200;

                TCHAR *szCommand = new TCHAR[nLength];

                if(szCommand != NULL)
                {
                    lstrcpy(szCommand, TEXT("\""));
                    nOffset += 1;

                    nOffset += SendDlgItemMessage(hwndDlg, IDC_FILE, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    lstrcat(szCommand, TEXT("\" "));
                    nOffset += 2;

                    SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, WM_GETTEXT, nLength - nOffset, (LPARAM)(szCommand + nOffset));

                    bSucceed = RunCommand(szCommand);

                    delete []szCommand;
                }

                if(bSucceed)
                {
                    EndDialog(hwndDlg, 1);
                }
                else
                {
                    TCHAR szErrorMessage[200];

                    wnsprintf(szErrorMessage, sizeof(szErrorMessage) / sizeof(TCHAR), TEXT("无法启动进程，错误码%lu"), GetLastError());

                    MessageBox(hwndDlg, szErrorMessage, NULL, MB_ICONERROR);

                    SendDlgItemMessage(hwndDlg, IDC_FILE, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hwndDlg, IDC_FILE, EM_SETREADONLY, FALSE, 0);
                    SetFocus(GetDlgItem(hwndDlg, IDC_FILE));
                }
            }
            else
            {
                SendDlgItemMessage(hwndDlg, IDC_ARGUMENTS, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwndDlg, IDC_ARGUMENTS));
            }
        }
        else if(LOWORD(wParam) == IDC_CANCEL)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (BOOL)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL RunCommandWithArguments(LPCTSTR lpFile)
{
    return 0 != DialogBoxParam(g_hinstDll, MAKEINTRESOURCE(IDD_RUNWITHARGUMENTS), NULL, RunCommandWithArgumentsProc, (LPARAM)lpFile);
}

struct BrowserForRegPathParam
{
    HANDLE hProcess;
    HWND hRegEdit;
    HWND hTreeCtrl;
    LPCTSTR lpRegPath;
};

DWORD __stdcall BrowserForRegPathProc(LPVOID lpParam)
{
    BrowserForRegPathParam *pParam = (BrowserForRegPathParam *)lpParam;

    SendMessage(pParam->hRegEdit, WM_COMMAND, 0x10288, 0);
    WaitForInputIdle(pParam->hProcess, INFINITE);

    for (int i = 0; i < 40; i += 1)
    {
        SendMessage(pParam->hTreeCtrl, WM_KEYDOWN, VK_LEFT, 0);
        WaitForInputIdle(pParam->hProcess, INFINITE);
    }

    SendMessage(pParam->hTreeCtrl, WM_KEYDOWN, VK_RIGHT, 0);
    WaitForInputIdle(pParam->hProcess, INFINITE);

    LPCTSTR lpChar = pParam->lpRegPath;
    while (*lpChar != TEXT('\0'))
    {
        if (*lpChar == TEXT('\\'))
        {
            SendMessage(pParam->hTreeCtrl, WM_KEYDOWN, VK_RIGHT, 0);
        }
        else
        {
            SendMessage(pParam->hTreeCtrl, WM_CHAR, *lpChar, 0);
        }

        WaitForInputIdle(pParam->hProcess, INFINITE);
        lpChar += 1;
    }

    return 0;
}

BOOL BrowseForRegPath(LPCTSTR lpRegPath)
{
    if (lpRegPath == NULL || *lpRegPath == TEXT('\0'))
    {
        return FALSE;
    }

    HWND hRegEdit = NULL;
    DWORD dwRegEditProcessId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != NULL && hSnapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32 = {sizeof(pe32)};

        if (Process32First(hSnapshot, &pe32))
        {
            do 
            {
                if (pe32.th32ParentProcessID == GetCurrentProcessId())
                {
                    if (0 == lstrcmpi(pe32.szExeFile, TEXT("regedit.exe")))
                    {
                        dwRegEditProcessId = pe32.th32ProcessID;
                        break;
                    }
                }

            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }

    if (dwRegEditProcessId == 0)
    {
        TCHAR szCommand[] = TEXT("regedit -m");
        PROCESS_INFORMATION pi;
        STARTUPINFO si = {sizeof(si)};

        if (CreateProcess(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForInputIdle(pi.hProcess, 2212);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            dwRegEditProcessId = pi.dwProcessId;
        }
    }

    if (dwRegEditProcessId != 0)
    {
        LPCTSTR lpClassName = TEXT("RegEdit_RegEdit");
        HWND hWindow = FindWindowEx(NULL, NULL, lpClassName, NULL);

        while (IsWindow(hWindow))
        {
            DWORD dwProcessId = 0;

            GetWindowThreadProcessId(hWindow, &dwProcessId);

            if (dwProcessId == dwRegEditProcessId)
            {
                hRegEdit = hWindow;
                break;
            }

            hWindow = FindWindowEx(NULL, hWindow, lpClassName, NULL);
        }
    }

    if (IsWindow(hRegEdit))
    {
        HWND hTreeCtrl = FindWindowEx(hRegEdit, NULL, TEXT("SysTreeView32"), NULL);

        if (IsWindow(hTreeCtrl))
        {
            BrowserForRegPathParam param;

            param.hProcess = OpenProcess(SYNCHRONIZE, FALSE, dwRegEditProcessId);
            param.hRegEdit = hRegEdit;
            param.hTreeCtrl = hTreeCtrl;
            param.lpRegPath = lpRegPath;

            if (param.hProcess != NULL)
            {
                HANDLE hThread = CreateThread(NULL, 0, BrowserForRegPathProc, (LPVOID)&param, 0, NULL);

                if (WAIT_TIMEOUT == WaitForSingleObject(hThread, 1389))
                {
                    TerminateThread(hThread, 0);
                }

                CloseHandle(hThread);
                CloseHandle(param.hProcess);
            }
        }
    }

    return TRUE;
}

BOOL SHOpenFolderAndSelectItems(LPCTSTR lpFile)
{
    typedef HRESULT (__stdcall *PSHOpenFolderAndSelectItems)(LPCITEMIDLIST, UINT, LPCITEMIDLIST *, DWORD);

    BOOL bResult = FALSE;
    LPCTSTR lpDllName = TEXT("Shell32.dll");
    HMODULE hShell32 = GetModuleHandle(lpDllName);
    PSHOpenFolderAndSelectItems pSHOpenFolderAndSelectItems;

    if (hShell32 == NULL)
    {
        hShell32 = LoadLibrary(lpDllName);
    }

    if (hShell32 == NULL)
    {
        return bResult;
    }

    (PROC&)pSHOpenFolderAndSelectItems = GetProcAddress(hShell32, "SHOpenFolderAndSelectItems");

    if (pSHOpenFolderAndSelectItems != NULL)
    {
        IShellFolder* pDesktopFolder = NULL;

        if (SUCCEEDED(SHGetDesktopFolder(&pDesktopFolder)))
        {
            LPCITEMIDLIST pidlFolder = NULL;
            LPCITEMIDLIST pidlFile = NULL;
            LPITEMIDLIST pidl = NULL;
            WCHAR szFile[MAX_PATH] = {0};
            WCHAR szPath[MAX_PATH] = {0};

            lstrcpynW(szFile, TtoW(lpFile).c_str(), RTL_NUMBER_OF(szFile));
            lstrcpynW(szPath, szFile, RTL_NUMBER_OF(szPath));
            PathRemoveFileSpecW(szPath);

            if (SUCCEEDED(pDesktopFolder->ParseDisplayName(NULL, NULL, szPath, NULL, &pidl, NULL)))
            {
                pidlFolder = pidl;

                if (SUCCEEDED(pDesktopFolder->ParseDisplayName(NULL, NULL, szFile, NULL, &pidl, NULL)))
                {
                    pidlFile = pidl;

                    int nTryCount = 0;
                    while (!IsWindow(GetTrayNotifyWndInProcess()))
                    {
                        Sleep(107);
                        nTryCount += 1;

                        if (nTryCount > 17)
                        {
                            break;
                        }
                    }

                    CoInitialize(NULL);
                    bResult = SUCCEEDED(pSHOpenFolderAndSelectItems(pidlFolder, 1, &pidlFile, 0));
                }
            }

            if (pidlFolder != NULL)
            {
                CoTaskMemFree((void*)pidlFolder);
            }

            if (pidlFile != NULL)
            {
                CoTaskMemFree((void*)pidlFile);
            }

            pDesktopFolder->Release();
        }
    }

    return bResult;
}

BOOL BrowseForFile(LPCTSTR lpFile)
{
    if (SHOpenFolderAndSelectItems(lpFile))
    {
        return TRUE;
    }
    else
    {
        TCHAR szExplorerPath[MAX_PATH];
        TCHAR szCommandLine[MAX_PATH * 2 + 100];

        GetWindowsDirectory(szExplorerPath, MAX_PATH);
        PathAppend(szExplorerPath, TEXT("\\Explorer.exe"));
        wnsprintf(szCommandLine, sizeof(szCommandLine) / sizeof(TCHAR), TEXT("%s /n,/select,\"%s\""), szExplorerPath, lpFile);

        OutputDebugString(TEXT("BrowseForFile by starting new explorer.exe\r\n"));

        return RunCommand(szCommandLine, NULL);
    }
}

// void WINAPI ResetExplorer(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
// {
//     int nCmdLineLength = lstrlenA(lpszCmdLine);
// 
//     if(nCmdLineLength > 0)
//     {
//         WCHAR *szBuffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (nCmdLineLength + 100) * sizeof(WCHAR));
// 
//         if(szBuffer != NULL)
//         {
//             if(wnsprintf(szBuffer, (nCmdLineLength + 100), L"%S", lpszCmdLine) > 0)
//             {
//                 int nArgc = 0;
//                 LPWSTR *ppArgv = CommandLineToArgvW(szBuffer, &nArgc);
// 
//                 if(ppArgv != NULL)
//                 {
//                     if(nArgc >= 1)
//                     {
//                         DWORD dwProcessId = StrToInt(ppArgv[0]);
// 
//                         if(dwProcessId != 0)
//                         {
//                             do
//                             {
//                                 HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, dwProcessId);
// 
//                                 if(hProcess == NULL)
//                                 {
//                                     MessageBox(hwndStub, TEXT("打开进程失败！"), NULL, MB_ICONERROR | MB_TOPMOST);
// 
//                                     break;
//                                 }
// 
//                                 TerminateProcess(hProcess, 0);
// 
//                                 WaitForSingleObject(hProcess, 5000);
// 
//                                 DWORD dwExitCode = 0;
//                                 GetExitCodeProcess(hProcess, &dwExitCode);
// 
//                                 if(dwExitCode == STILL_ACTIVE)
//                                 {
//                                     MessageBox(hwndStub, TEXT("无法终止进程！"), NULL, MB_ICONERROR | MB_TOPMOST);
// 
//                                     break;
//                                 }
// 
//                                 CloseHandle(hProcess);
// 
//                                 STARTUPINFO si = {sizeof(si)};
//                                 PROCESS_INFORMATION pi;
//                                 TCHAR szExplorerPath[MAX_PATH];
// 
//                                 GetWindowsDirectory(szExplorerPath, MAX_PATH);
//                                 PathAppend(szExplorerPath, TEXT("\\Explorer.exe"));
// 
//                                 if(CreateProcess(NULL, szExplorerPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
//                                 {
//                                     if(nArgc >= 2 && lstrlenW(ppArgv[1]) < MAX_PATH)
//                                     {
//                                         WaitForInputIdle(pi.hProcess, 10 * 1000);
// 
//                                         TCHAR szFilePath[MAX_PATH];
//                                         DWORD dwFileAttributes = INVALID_FILE_ATTRIBUTES;
// 
//                                         wnsprintf(szFilePath, sizeof(szFilePath) / sizeof(TCHAR), TEXT("%ls"), ppArgv[1]);
// 
//                                         dwFileAttributes = GetFileAttributes(szFilePath);
// 
//                                         if(dwFileAttributes != INVALID_FILE_ATTRIBUTES)
//                                         {
//                                             if(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
//                                             {
//                                                 TCHAR szFindMark[MAX_PATH];
//                                                 WIN32_FIND_DATA wfd;
// 
//                                                 lstrcpyn(szFindMark, szFilePath, MAX_PATH);
//                                                 PathAppend(szFindMark, TEXT("\\*"));
// 
//                                                 HANDLE hFind = FindFirstFile(szFindMark, &wfd);
// 
//                                                 if(hFind != INVALID_HANDLE_VALUE)
//                                                 {
//                                                     do 
//                                                     {
//                                                         if(lstrcmp(wfd.cFileName, TEXT(".")) != 0 && lstrcmp(wfd.cFileName, TEXT("..")) != 0)
//                                                         {
//                                                             break;
//                                                         }
// 
//                                                     } while (FindNextFile(hFind, &wfd));
// 
//                                                     if(lstrcmp(wfd.cFileName, TEXT(".")) != 0 && lstrcmp(wfd.cFileName, TEXT("..")) != 0)
//                                                     {
//                                                         PathAddBackslash(szFilePath);
//                                                         lstrcat(szFilePath, wfd.cFileName);
//                                                     }
// 
//                                                     FindClose(hFind);
//                                                 }
//                                             }
// 
//                                             if(PathFileExists(szFilePath))
//                                             {
//                                                 BrowseForFile(szFilePath);
//                                             }
//                                         }
//                                     }
// 
//                                     CloseHandle(pi.hThread);
//                                     CloseHandle(pi.hProcess);
//                                 }
//                                 else
//                                 {
//                                     MessageBox(hwndStub, TEXT("无法启动Explorer！"), NULL, MB_ICONERROR | MB_TOPMOST);
// 
//                                     break;
//                                 }
//                             }
//                             while(FALSE);
//                         }
//                     }
// 
//                     LocalFree(ppArgv);
//                 }
//             }
// 
//             HeapFree(GetProcessHeap(), 0, szBuffer);
//         }
//     }
// }

BOOL IsExplorer()
{
    TCHAR szExePath[MAX_PATH] = TEXT("");

    GetModuleFileName(GetModuleHandle(NULL), szExePath, MAX_PATH);

    return lstrcmpi(PathFindFileName(szExePath), TEXT("explorer.exe")) == 0;
}

BOOL TryUnescapeFileName(LPCTSTR lpFilePath, TCHAR szUnescapedFilePath[], int nSize)
{
    BOOL bChanged = FALSE;

    if(lstrlen(lpFilePath) + 1 > nSize)
    {
        return bChanged;
    }

    lstrcpyn(szUnescapedFilePath, lpFilePath, nSize);

    // win7
    // (.+?\\)++.+?(?<kill>( \- 副本(( \(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\))|))+)\..+
    // xp
    // (.+?\\)++(?<kill>复件( \(([1-9][0-9]|[1-9])\))? )[^\.\\\n]+\.[^\.\\\n]+
    // 点前加(数字)或[数字]
    // (.+?\\)++.+?((?<kill>\(([1-9][0-9]|[1-9])\))|(?<kill>\[([1-9][0-9]|[1-9])\]))\..+
    static CRegexpT<TCHAR> regWin7    (TEXT("(.+?\\\\)++.+?(?<kill>( \\- 副本(( \\(([1-9][0-9]{2}|[1-9][0-9]|[1-9])\\))|))+)\\..+"));
    static CRegexpT<TCHAR> regXp      (TEXT("(.+?\\\\)++(?<kill>复件( \\(([1-9][0-9]|[1-9])\\))? )[^\\.\\\\\\n]+\\.[^\\.\\\\\\n]+"));
    static CRegexpT<TCHAR> regDownload(TEXT("(.+?\\\\)++.+?((?<kill>\\(([1-9][0-9]|[1-9])\\))|(?<kill>\\[([1-9][0-9]|[1-9])\\]))\\..+"));

    static int nGroupKillWin7     = regWin7.    GetNamedGroupNumber(TEXT("kill"));
    static int nGroupKillXp       = regXp.      GetNamedGroupNumber(TEXT("kill"));
    static int nGroupKillDownload = regDownload.GetNamedGroupNumber(TEXT("kill"));

    static CRegexpT<TCHAR>* pRegs[] = {
        &regWin7,
        &regXp,
        &regDownload,
        };
    static int pGroupKills[] = {
        nGroupKillWin7,
        nGroupKillXp,
        nGroupKillDownload,
    };

    for(int nIndex = 0; nIndex < sizeof(pRegs) / sizeof(pRegs[0]); nIndex += 1)
    {
        while(TRUE)
        {
            MatchResult result = pRegs[nIndex]->Match(szUnescapedFilePath);

            if(result.IsMatched())
            {
                int nGroupBegin = result.GetGroupStart(pGroupKills[nIndex]);
                int nGroupEnd = result.GetGroupEnd(pGroupKills[nIndex]);

                if(nGroupBegin >= 0 && nGroupEnd > nGroupBegin)
                {
                    MoveMemory(
                        szUnescapedFilePath + nGroupBegin,
                        szUnescapedFilePath + nGroupEnd,
                        (lstrlen(szUnescapedFilePath + nGroupEnd) + 1) * sizeof(TCHAR)
                        );

                    bChanged = TRUE;
                }
            }
            else
            {
                break;
            }
        }
    }

    TCHAR szTailMark[] = TEXT(".重命名");
    int nResultLength = lstrlen(szUnescapedFilePath);
    int nMarkLength = lstrlen(szTailMark);

    if(nResultLength > nMarkLength)
    {
        if(lstrcmp(szUnescapedFilePath + nResultLength - nMarkLength, szTailMark) == 0)
        {
            *(szUnescapedFilePath + nResultLength - nMarkLength) = TEXT('\0');

            bChanged = TRUE;
        }
    }

    return bChanged;
}

BOOL ResolveShortcut(LPCTSTR lpLinkFilePath, TCHAR szResolvedPath[], UINT nSize)
{
    WCHAR szLinkFilePath[MAX_PATH];
    IShellLink *pShellLink = NULL;
    IPersistFile *pPersistFile = NULL;
    WIN32_FIND_DATA wfd;
    BOOL bResult = FALSE;

    if(PathFileExists(lpLinkFilePath))
    {
#ifdef UNICODE
        wnsprintfW(szLinkFilePath, sizeof(szLinkFilePath) / sizeof(WCHAR), L"%s", lpLinkFilePath);
#else
        wnsprintfW(szLinkFilePath, sizeof(szLinkFilePath) / sizeof(WCHAR), L"%S", lpLinkFilePath);
#endif

        if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink)))
        {
            if(SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile)))
            {
                if(SUCCEEDED(pPersistFile->Load(szLinkFilePath, STGM_READ)))
                {
                    bResult = SUCCEEDED(pShellLink->GetPath(szResolvedPath, nSize, &wfd, 0));
                }

                pPersistFile->Release();
            }

            pShellLink->Release();
        }
    }

    return bResult;
}

#define APPPATH_PATH TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths")

BOOL ModifyAppPath_GetFileCommand(LPCTSTR lpFilePath, TCHAR szCommand[], DWORD dwSize)
{
    DWORD dwRegType = 0;

    dwSize *= sizeof(TCHAR);

    SHGetValue(
        HKEY_LOCAL_MACHINE,
        APPPATH_PATH,
        lpFilePath,
        &dwRegType,
        szCommand,
        &dwSize
        );

    return dwRegType == REG_SZ;
}

INT_PTR __stdcall ModifyAppPathProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    TCHAR szCommandInReg[MAX_PATH] = TEXT("");

    switch(uMsg)
    {
    case WM_INITDIALOG:
        if(hIcon == NULL)
        {
            hIcon = LoadIcon(g_hinstDll, MAKEINTRESOURCE(IDI_CONFIG_ICON));
        }

        if(hIcon != NULL)
        {
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }


        ModifyAppPath_GetFileCommand((LPCTSTR)lParam, szCommandInReg, sizeof(szCommandInReg) / sizeof(TCHAR));

        SetDlgItemText(hwndDlg, IDC_FILE, (LPCTSTR)lParam);
        SetDlgItemText(hwndDlg, IDC_KEY, szCommandInReg);

        SendDlgItemMessage(hwndDlg, IDC_KEY, EM_SETLIMITTEXT, sizeof(szCommandInReg) / sizeof(TCHAR), 0);

        return FALSE;

    case WM_SYSCOMMAND:
        if(wParam == SC_CLOSE)
        {
            EndDialog(hwndDlg, 0);
        }
        break;

    case WM_COMMAND:
        if(LOWORD(wParam) == IDOK)
        {
            TCHAR szRegPath[1000];
            TCHAR szFilePath[MAX_PATH] = TEXT("");
            TCHAR szCommand[MAX_PATH] = TEXT("");

            GetDlgItemText(hwndDlg, IDC_FILE, szFilePath, sizeof(szFilePath) / sizeof(TCHAR));
            GetDlgItemText(hwndDlg, IDC_KEY, szCommand, sizeof(szCommand) / sizeof(TCHAR));
            ModifyAppPath_GetFileCommand(szFilePath, szCommandInReg, sizeof(szCommandInReg) / sizeof(TCHAR));

            if (lstrlen(szCommandInReg) > 0)
            {
                wnsprintf(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(TCHAR),
                    TEXT("%s\\%s"),
                    APPPATH_PATH,
                    szCommandInReg
                    );

                SHDeleteKey(HKEY_LOCAL_MACHINE, szRegPath);
            }

            if (lstrlen(szCommand) <= 0)
            {
                SHDeleteValue(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath);
            }
            else
            {
                if (lstrcmpi(szCommand + lstrlen(szCommand) - 4, TEXT(".exe")) != 0)
                {
                    if (IDYES == MessageBox(
                        hwndDlg,
                        TEXT("您提供的快捷短语未以“.exe”结尾，是否自动添加？"),
                        TEXT("请确认"),
                        MB_ICONQUESTION | MB_YESNO
                        ))
                    {
                        StrCatBuff(szCommand, TEXT(".exe"), sizeof(szCommand) / sizeof(TCHAR));
                        SetDlgItemText(hwndDlg, IDC_KEY, szCommand);
                    }
                }

                SHSetValue(HKEY_LOCAL_MACHINE, APPPATH_PATH, szFilePath, REG_SZ, szCommand, (lstrlen(szCommand) + 1) * sizeof(TCHAR));

                wnsprintf(
                    szRegPath,
                    sizeof(szRegPath) / sizeof(TCHAR),
                    TEXT("%s\\%s"),
                    APPPATH_PATH,
                    szCommand
                    );

                SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, NULL, REG_SZ, szFilePath, (lstrlen(szFilePath) + 1) * sizeof(TCHAR));

                PathRemoveFileSpec(szFilePath);
                SHSetValue(HKEY_LOCAL_MACHINE, szRegPath, TEXT("Path"), REG_SZ, szFilePath, (lstrlen(szFilePath) + 1) * sizeof(TCHAR));
            }

            EndDialog(hwndDlg, IDOK);
        }
        else if(LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hwndDlg, IDCANCEL);
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (BOOL)GetStockObject(WHITE_BRUSH);

    default:
        break;
    }

    return FALSE;
}

BOOL ModifyAppPath(LPCTSTR lpFilePath)
{
    return 0 != DialogBoxParam(g_hinstDll, MAKEINTRESOURCE(IDD_APPPATH), NULL, ModifyAppPathProc, (LPARAM)lpFilePath);
}

VOID ShowErrorMessage(HWND hWindow, DWORD dwErrorCode)
{
    LPTSTR lpMessage = NULL;
    TCHAR szMessage[2000];
    UINT uType = MB_ICONERROR;

    FormatMessage(  
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMessage,
        0,
        NULL
        );

    if (lpMessage != NULL)
    {
        wnsprintf(
            szMessage,
            sizeof(szMessage) / sizeof(TCHAR),
            TEXT("错误码：%lu\r\n\r\n参考意义：%s"),
            dwErrorCode,
            lpMessage
            );

        LocalFree(lpMessage);
    }

    if (dwErrorCode == 0)
    {
        uType = MB_ICONINFORMATION;
    }

    MessageBox(hWindow, szMessage, TEXT("信息"), uType);
}

VOID DrvAction(HWND hWindow, LPCTSTR lpFilePath, DRIVER_ACTION daValue)
{
    if (daValue != DA_INSTALL   &&
        daValue != DA_UNINSTALL &&
        daValue != DA_START     &&
        daValue != DA_STOP
        )
    {
        return;
    }

    SC_HANDLE hService = NULL;
    SC_HANDLE hScManager = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);

    if (hScManager == NULL)
    {
        return;
    }

    TCHAR szServiceName[MAX_PATH] = TEXT("");

    lstrcpyn(szServiceName, PathFindFileName(lpFilePath), sizeof(szServiceName) / sizeof(TCHAR));
    PathRemoveExtension(szServiceName);

    if (daValue == DA_INSTALL)
    {
        TCHAR szImagePath[MAX_PATH * 4] = TEXT("\\??\\");

        StrCatBuff(szImagePath, lpFilePath, sizeof(szImagePath) / sizeof(TCHAR));

        hService = CreateService(
            hScManager,
            szServiceName,
            szServiceName,
            SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_IGNORE,
            szImagePath,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            );
    }
    else
    {
        hService = OpenService(hScManager, szServiceName, SERVICE_ALL_ACCESS);

        if (hService != NULL)
        {
            if (daValue == DA_UNINSTALL)
            {
                DeleteService(hService);
            }
            else if (daValue == DA_START)
            {
                StartService(hService, 0, NULL);
            }
            else if (daValue == DA_STOP)
            {
                SERVICE_STATUS status;
                ControlService(hService, SERVICE_CONTROL_STOP, &status);
            }
        }
    }

    ShowErrorMessage(hWindow, GetLastError());

    if (hService != NULL)
    {
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hScManager);
}

int MessageBoxFormat(HWND hWindow, LPCTSTR lpCaption, UINT uType, LPCTSTR lpFormat, ...)
{
    TCHAR szText[2000];
    va_list val;

    va_start(val, lpFormat);
    wvnsprintf(szText, sizeof(szText) / sizeof(TCHAR), lpFormat, val);
    va_end(val);

    return MessageBox(hWindow, szText, lpCaption, uType);
}

void WINAPI BrowserLinkFilePosition(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    TCHAR szLinkFilePath[MAX_PATH];
    TCHAR szTargetFilePath[MAX_PATH];

    CoInitialize(NULL);

    wnsprintf(szLinkFilePath, sizeof(szLinkFilePath) / sizeof(TCHAR), TEXT("%hs"), lpszCmdLine);
    PathUnquoteSpaces(szLinkFilePath);

    if(ResolveShortcut(szLinkFilePath, szTargetFilePath, sizeof(szTargetFilePath) / sizeof(TCHAR)))
    {
        BrowseForFile(szTargetFilePath);
    }
}

BOOL EnableDebugPrivilege(BOOL bEnable)
{
    BOOL bResult = FALSE;
    HANDLE hToken;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        TOKEN_PRIVILEGES tp;

        tp.PrivilegeCount = 1;
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
        tp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);

        bResult = GetLastError() == ERROR_SUCCESS;

        CloseHandle(hToken);
    }

    return bResult;
}

BOOL IsFileDenyed(LPCTSTR lpFilePath)
{
    HANDLE hFile = CreateFile(lpFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD dwLastError = GetLastError();

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
    }

    return dwLastError == ERROR_SHARING_VIOLATION;
}

BOOL CloseRemoteHandle(DWORD dwProcessId, HANDLE hRemoteHandle)
{
    BOOL bResult = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);

    if (hProcess != NULL)
    {
        HANDLE hLocalHandle = NULL;

        if (DuplicateHandle(
            hProcess,
            hRemoteHandle,
            GetCurrentProcess(),
            &hLocalHandle,
            0,
            FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE
            ))
        {
            bResult = TRUE;
            CloseHandle(hLocalHandle);
        }

        CloseHandle(hProcess);
    }

    return bResult;
}

BOOL IsSameFilePath(LPCTSTR lpFilePath1, LPCTSTR lpFilePath2)
{
    TCHAR szFilePath1[MAX_PATH];
    TCHAR szFilePath2[MAX_PATH];
    TCHAR szLongFilePath1[MAX_PATH];
    TCHAR szLongFilePath2[MAX_PATH];

    lstrcpyn(szFilePath1, lpFilePath1, sizeof(szFilePath1) / sizeof(TCHAR));
    lstrcpyn(szFilePath2, lpFilePath2, sizeof(szFilePath2) / sizeof(TCHAR));

    if (0 == GetLongPathName(szFilePath1, szLongFilePath1, sizeof(szLongFilePath1) / sizeof(TCHAR)))
    {
        lstrcpyn(szLongFilePath1, lpFilePath1, sizeof(szLongFilePath1) / sizeof(TCHAR));
    }

    if (0 == GetLongPathName(szFilePath2, szLongFilePath2, sizeof(szLongFilePath2) / sizeof(TCHAR)))
    {
        lstrcpyn(szLongFilePath2, lpFilePath2, sizeof(szLongFilePath2) / sizeof(TCHAR));
    }

    return lstrcmpi(szLongFilePath1, szLongFilePath2) == 0;
}

BOOL IsPathDesktop(LPCTSTR lpPath)
{
    TCHAR szDesktopPath[MAX_PATH];

    if (SHGetSpecialFolderPath(NULL, szDesktopPath, CSIDL_DESKTOP, FALSE))
    {
        return IsSameFilePath(lpPath, szDesktopPath);
    }

    return FALSE;
}

BOOL IsWow64ProcessHelper(HANDLE hProcess)
{
    BOOL bIsWow64 = FALSE;
    BOOL (WINAPI *pIsWow64Process)(HANDLE, PBOOL) = NULL;

    (PROC &)pIsWow64Process = GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),
        "IsWow64Process"
        );

    if (pIsWow64Process != NULL)
    {
        pIsWow64Process(hProcess, &bIsWow64);
    }

    return bIsWow64;
}

BOOL DisableWow64FsRedirection()
{
    BOOL bResult = TRUE;
    BOOL (WINAPI *pWow64DisableWow64FsRedirection)(PVOID *) = NULL;

    (PROC &)pWow64DisableWow64FsRedirection = GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),
        "Wow64DisableWow64FsRedirection"
        );

    if (pWow64DisableWow64FsRedirection != NULL)
    {
        PVOID pOldValue = NULL;
        bResult = pWow64DisableWow64FsRedirection(&pOldValue);
    }

    return bResult;
}

BOOL KillAllExplorers()
{
    set<DWORD> setExplorers;
    BOOL bKillSelf = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE && hSnapshot != NULL)
    {
        PROCESSENTRY32 pe32 = {sizeof(pe32)};

        if (Process32First(hSnapshot, &pe32))
        {
            do 
            {
                if (lstrcmpi(pe32.szExeFile, TEXT("explorer.exe")) == 0)
                {
                    if (pe32.th32ProcessID == GetCurrentProcessId())
                    {
                        bKillSelf = TRUE;
                    }
                    else
                    {
                        setExplorers.insert(pe32.th32ProcessID);
                    }
                }

            } while (Process32Next(hSnapshot, &pe32));
        }
    }

    set<DWORD>::iterator it = setExplorers.begin();
    for (; it != setExplorers.end(); ++it)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, *it);

        if (hProcess != NULL)
        {
            TerminateProcess(hProcess, 0);
        }
    }

    if (bKillSelf)
    {
        TerminateProcess(GetCurrentProcess(), 0);
    }

    return TRUE;
}

BOOL SetClipboardHtml(const char *html)
{
    static UINT nCbFormat = 0;
    BOOL bResult = FALSE;

    if (nCbFormat == 0)
    {
        nCbFormat = RegisterClipboardFormatA("HTML Format");

        if (nCbFormat == 0)
        {
            return bResult;
        }
    }

    if (html == NULL)
    {
        return bResult;
    }

    int nSize = lstrlenA(html) + 500;
    char *buffer = (char *)malloc(nSize);

    if (buffer == NULL)
    {
        return bResult;
    }

    int nStartHtml = 0;
    int nEndHtml = 0;
    int nStartFragment = 0;
    int nEndFragment = 0;
    const char *fmt =
        "Version:0.9\r\n"
        "StartHTML:%08d\r\n"
        "EndHTML:%08d\r\n"
        "StartFragment:%08d\r\n"
        "EndFragment:%08d\r\n"
        "<!doctype html><html><body>\r\n"
        "<!--StartFragment-->"
        "%s"
        "<!--EndFragment-->\r\n"
        "</body>\r\n"
        "</html>";

    nEndHtml = wnsprintfA(
        buffer,
        nSize,
        fmt,
        nStartHtml,
        nEndHtml,
        nStartFragment,
        nEndFragment,
        html
        );

    nStartHtml = (int)StrStrA(buffer, "<html") - (int)buffer;
    nStartFragment = (int)StrStrA(buffer, "<!--StartFrag") - (int)buffer;
    nEndFragment = (int)StrStrA(buffer, "<!--EndFragment") - (int)buffer;

    if (nStartHtml != 0 &&
        nEndHtml != 0 &&
        nStartFragment != 0 &&
        nEndFragment != 0
        )
    {
        nSize = wnsprintfA(
            buffer,
            nSize,
            fmt,
            nStartHtml,
            nEndHtml,
            nStartFragment,
            nEndFragment,
            html
            );

        if(OpenClipboard(0))
        {
            EmptyClipboard();

            HGLOBAL hText = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, nSize + 10);

            if (hText != NULL)
            {
                char *lpText = (char *)GlobalLock(hText);

                if (lpText != NULL)
                {
                    lstrcpynA(lpText, buffer, nSize + 10);

                    GlobalUnlock(hText);
                }

                bResult = SetClipboardData(nCbFormat, hText) != NULL;

                GlobalFree(hText);
            }

            CloseClipboard();
        }
    }

    free((void *)buffer);

    return bResult;
}

BOOL SetClipboardPicturePathsByHtml(LPCTSTR lpPaths)
{
    if (lpPaths == NULL)
    {
        return FALSE;
    }

    int nLength = lstrlen(lpPaths);

    stringstream ss;
    TCHAR szShortPath[MAX_PATH];

    ss<<"<DIV>"<<endl;

    while (nLength > 0)
    {
        if (GetShortPathName(lpPaths, szShortPath, RTL_NUMBER_OF(szShortPath)) > 0)
        {
            ss<<"<IMG src=\"file"<<":///"<<TtoU(szShortPath)<<"\" >"<<endl;
        }

        lpPaths += (nLength + 1);
        nLength = lstrlen(lpPaths);
    }

    ss<<"</DIV>";

    return SetClipboardHtml(ss.str().c_str());
}

void SafeDebugMessage(LPCTSTR pFormat, ...)
{
    TCHAR szBuffer[2000];
    va_list pArg;

    va_start(pArg, pFormat);
    wvnsprintf(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), pFormat, pArg);
    va_end(pArg);

    OutputDebugString(szBuffer);
}

HKEY ParseRegPath(LPCTSTR lpWholePath, LPCTSTR *lppRegPath)
{
    HKEY hResult = NULL;
    TCHAR szRootKey[100] = TEXT("");

    lstrcpyn(szRootKey, lpWholePath, RTL_NUMBER_OF(szRootKey));
    LPTSTR lpBackSlash = StrStr(szRootKey, TEXT("\\"));

    if (lpBackSlash != NULL)
    {
        *lpBackSlash = TEXT('\0');

        if (lstrcmpi(szRootKey, TEXT("HKEY_CLASSES_ROOT")) == 0 || lstrcmpi(szRootKey, TEXT("HKCR")) == 0)
        {
            hResult = HKEY_CLASSES_ROOT;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_CURRENT_USER")) == 0 || lstrcmpi(szRootKey, TEXT("HKCU")) == 0)
        {
            hResult = HKEY_CURRENT_USER;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_LOCAL_MACHINE")) == 0 || lstrcmpi(szRootKey, TEXT("HKLM")) == 0)
        {
            hResult = HKEY_LOCAL_MACHINE;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_USERS")) == 0 || lstrcmpi(szRootKey, TEXT("HKU")) == 0)
        {
            hResult = HKEY_USERS;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_CURRENT_CONFIG")) == 0 || lstrcmpi(szRootKey, TEXT("HKCC")) == 0)
        {
            hResult = HKEY_CURRENT_CONFIG;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_PERFORMANCE_DATA")) == 0)
        {
            hResult = HKEY_PERFORMANCE_DATA;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_PERFORMANCE_TEXT")) == 0)
        {
            hResult = HKEY_PERFORMANCE_TEXT;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_PERFORMANCE_NLSTEXT")) == 0)
        {
            hResult = HKEY_PERFORMANCE_NLSTEXT;
        }
        else if (lstrcmpi(szRootKey, TEXT("HKEY_DYN_DATA")) == 0)
        {
            hResult = HKEY_DYN_DATA;
        }
    }

    if (hResult != NULL)
    {
        lpBackSlash = StrStr(lpWholePath, TEXT("\\"));

        if (lpBackSlash != NULL)
        {
            while (*lpBackSlash == TEXT('\\'))
            {
                lpBackSlash += 1;
            }

            if (lppRegPath != NULL)
            {
                *lppRegPath = lpBackSlash;
            }
        }
    }

    return hResult;
}

BOOL IsRegPathExists(HKEY hRootKey, LPCTSTR lpRegPath)
{
    HKEY hKey = NULL;

    RegOpenKeyEx(hRootKey, lpRegPath, 0, READ_CONTROL, &hKey);

    if (hKey != NULL)
    {
        RegCloseKey(hKey);
    }

    return hKey != NULL;
}

BOOL TouchRegPath(HKEY hRootKey, LPCTSTR lpRegPath)
{
    HKEY hKey = NULL;

    RegCreateKeyEx(hRootKey, lpRegPath, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE, NULL, &hKey, NULL);

    if (hKey != NULL)
    {
        RegCloseKey(hKey);
    }

    return hKey != NULL;
}

HFONT GetMenuDefaultFont()
{
    HFONT hFont = NULL;
    NONCLIENTMETRICS ncm = {sizeof(ncm)};

    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
    {
        hFont = CreateFontIndirect(&ncm.lfMenuFont);
    }

    return hFont;
}

static UINT GetDrawTextSizeInDc(HDC hdc, LPCTSTR lpText, UINT *puHeight)
{
    SIZE size = {0};

    GetTextExtentPoint32(hdc, lpText, lstrlen(lpText), &size);

    if (puHeight != NULL)
    {
        *puHeight = (UINT)size.cy;
    }

    return (UINT)size.cx;
}

BOOL PathCompactPathHelper(HDC hdc, LPTSTR lpText, UINT dx)
{
    if (lpText == NULL || *lpText == TEXT('\0'))
    {
        return FALSE;
    }
    else if (GetDrawTextSizeInDc(hdc, lpText, NULL) <= dx)
    {
        return TRUE;
    }
    else if (dx == 0)
    {
        *lpText = TEXT('\0');
        return TRUE;
    }
    else
    {
        int nLength = lstrlen(lpText);
        BOOL bBackSlashExist = StrStr(lpText, TEXT("\\")) != NULL;
        BOOL bRet = PathCompactPath(hdc, lpText, dx);

        if (!bRet)
        {
            return FALSE;
        }
        else if (GetDrawTextSizeInDc(hdc, lpText, NULL) <= dx)
        {
            return TRUE;
        }
        else
        {
            LPTSTR lpEnd = NULL;

            lstrcpyn(lpText, TEXT(".........................."), nLength + 1);
            lpEnd = lpText + lstrlen(lpText);

            if (bBackSlashExist && GetDrawTextSizeInDc(hdc, TEXT("...\\..."), NULL) < dx)
            {
                lpText[3] = TEXT('\\');
            }

            while (lpEnd >= lpText)
            {
                if (GetDrawTextSizeInDc(hdc, lpText, NULL) < dx)
                {
                    return TRUE;
                }

                lpEnd -= 1;
                *lpEnd = TEXT('\0');
            }

            return FALSE;
        }
    }
}

HWND GetTrayNotifyWndInProcess()
{
    DWORD dwPid = GetCurrentProcessId();
    HWND hShell_TrayWnd = FindWindowEx(NULL, NULL, TEXT("Shell_TrayWnd"), NULL);

    while (IsWindow(hShell_TrayWnd))
    {
        DWORD dwWindowPid = 0;

        GetWindowThreadProcessId(hShell_TrayWnd, &dwWindowPid);

        if (dwWindowPid == dwPid)
        {
            HWND hTrayNotifyWnd = FindWindowEx(hShell_TrayWnd, NULL, TEXT("TrayNotifyWnd"), NULL);

            if (IsWindow(hTrayNotifyWnd))
            {
                return hTrayNotifyWnd;
            }
        }

        hShell_TrayWnd = FindWindowEx(NULL, hShell_TrayWnd, TEXT("Shell_TrayWnd"), NULL);
    }

    return NULL;
}

void WINAPI T2(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
    BrowseForFile(TEXT("C:\\Windows\\system32\\cmd.exe"));

    return ;
}
