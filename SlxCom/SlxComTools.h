#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>
#pragma warning(disable: 4786)
#include "lib/charconv.h"

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
BOOL BrowseForRegPath(LPCTSTR lpRegPath);
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
BOOL IsPathDesktop(LPCTSTR lpPath);
BOOL IsWow64ProcessHelper(HANDLE hProcess);
BOOL DisableWow64FsRedirection();
BOOL ResetExplorer();
BOOL SetClipboardPicturePathsByHtml(LPCTSTR lpPaths);
void SafeDebugMessage(LPCTSTR pFormat, ...);
HKEY ParseRegPath(LPCTSTR lpWholePath, LPCTSTR *lppRegPath);
BOOL IsRegPathExists(HKEY hRootKey, LPCTSTR lpRegPath);
BOOL TouchRegPath(HKEY hRootKey, LPCTSTR lpRegPath);
HFONT GetMenuDefaultFont();
BOOL PathCompactPathHelper(HDC hdc, LPTSTR lpText, UINT dx);
HWND GetTrayNotifyWndInProcess();
BOOL GetVersionString(HINSTANCE hModule, TCHAR szVersionString[], int nSize);
BOOL IsAdminMode();
BOOL WriteFileHelper(HANDLE hFile, LPCVOID lpBuffer, DWORD dwBytesToWrite);
HICON GetWindowIcon(HWND hWindow);
HICON GetWindowIconSmall(HWND hWindow);
DWORD RegGetDWORD(HKEY hRootKey, LPCTSTR lpRegPath, LPCTSTR lpRegValue, DWORD dwDefault);
DWORD CheckMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bChecked);
DWORD EnableMenuItemHelper(HMENU hMenu, UINT uId, UINT uFlags, BOOL bEnabled);
BOOL IsWindowTopMost(HWND hWindow);
unsigned __int64 StrToInt64Def(LPCTSTR lpString, unsigned __int64 nDefault);
BOOL AdvancedSetForegroundWindow(HWND hWindow);
BOOL GetWindowImageFileName(HWND hWindow, LPTSTR lpBuffer, UINT uBufferSize);

std::tstring GetCurrentTimeString();
std::tstring &AssignString(std::tstring &str, LPCTSTR lpText);
std::tstring GetDesktopName(HDESK hDesktop);

#endif
