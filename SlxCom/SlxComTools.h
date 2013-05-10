#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>

enum DRIVER_ACTION
{
    DA_INSTALL,
    DA_START,
    DA_STOP,
    DA_UNINSTALL,
};

struct ListCtrlSortStruct
{
    int nRet;
    HWND hListCtrl;
    int nSubItem;
};

VOID DrvAction(HWND hWindow, LPCTSTR lpFilePath, DRIVER_ACTION daValue);
DWORD SHSetTempValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData);
BOOL IsFileSigned(LPCTSTR lpFilePath);
BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength);
BOOL SaveResourceToFile(LPCTSTR lpResType, LPCTSTR lpResName, LPCTSTR lpFilePath);
LPCTSTR GetClipboardFiles(DWORD *pdwEffect, UINT *puFileCount = NULL);
BOOL SetClipboardText(LPCTSTR lpText);
BOOL RunCommand(LPTSTR lpCommandLine, LPCTSTR lpCurrentDirectory = NULL);
BOOL RunCommandWithArguments(LPCTSTR lpFile);
BOOL BrowseForFile(LPCTSTR lpFile);
BOOL IsExplorer();
BOOL TryUnescapeFileName(LPCTSTR lpFilePath, TCHAR szUnescapedFilePath[], int nSize);
BOOL ResolveShortcut(LPCTSTR lpLinkFilePath, TCHAR szResolvedPath[], UINT nSize);
// BOOL RegisterClipboardFile(LPCTSTR lpFileList, BOOL bCopy);
BOOL ModifyAppPath(LPCTSTR lpFilePath);
BOOL ModifyAppPath_GetFileCommand(LPCTSTR lpFilePath, TCHAR szCommand[], DWORD dwSize);
int MessageBoxFormat(HWND hWindow, LPCTSTR lpCaption, UINT uType, LPCTSTR lpFormat, ...);
BOOL EnableDebugPrivilege(BOOL bEnable);
BOOL IsFileDenyed(LPCTSTR lpFilePath);
BOOL CloseRemoteHandle(DWORD dwProcessId, HANDLE hRemoteHandle);
BOOL IsSameFilePath(LPCTSTR lpFilePath1, LPCTSTR lpFilePath2);
BOOL IsWow64ProcessHelper(HANDLE hProcess);
BOOL DisableWow64FsRedirection();
BOOL KillAllExplorers();

#endif
