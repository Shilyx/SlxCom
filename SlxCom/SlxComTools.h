#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>
#include <CommCtrl.h>
#pragma warning(disable: 4786)
#include "lib/charconv.h"
#include <list>
#include <string>
#include <vector>

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
BOOL RunCommandEx(LPCTSTR lpApplication, LPCTSTR lpArguments, LPCTSTR lpCurrentDirectory, BOOL bElevate);
BOOL BrowseForRegPath(LPCTSTR lpRegPath);
BOOL BrowseForFile(LPCTSTR lpFile);
BOOL IsExplorer();
BOOL TryUnescapeFileName(LPCTSTR lpFilePath, TCHAR szUnescapedFilePath[], int nSize);
BOOL ResolveShortcut(LPCTSTR lpLinkFilePath, TCHAR szResolvedPath[], UINT nSize);
// BOOL RegisterClipboardFile(LPCTSTR lpFileList, BOOL bCopy);
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
UINT GetDrawTextSizeInDc(HDC hdc, LPCTSTR lpText, UINT *puHeight);
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
BOOL IsWindowFullScreen(HWND hWindow);
BOOL KillProcess(DWORD dwProcessId);
BOOL GetProcessNameById(DWORD dwProcessId, TCHAR szProcessName[], DWORD dwBufferSize);
LPCVOID GetResourceBuffer(HINSTANCE hInstance, LPCTSTR lpResType, LPCTSTR lpResName, LPDWORD lpResSize = NULL);
void ExpandTreeControlForLevel(HWND hControl, HTREEITEM htiBegin, int nLevel);

enum WindowUnalphaValue
{
    WUV100 = 255,
    WUV80 = 204,
    WUV60 = 153,
    WUV40 = 102,
    WUV20 = 51,
};

BOOL SetWindowUnalphaValue(HWND hWindow, WindowUnalphaValue nValue);

std::tstring GetCurrentTimeString();
std::tstring &AssignString(std::tstring &str, LPCTSTR lpText);
std::tstring GetDesktopName(HDESK hDesktop);
std::string GetFriendlyFileSizeA(unsigned __int64 nSize);
std::wstring fmtW(const wchar_t *fmt, ...);
LPSYSTEMTIME Time1970ToLocalTime(DWORD dwTime1970, LPSYSTEMTIME lpSt);

template <class T>
std::tstring AnyTypeToString(const T &value)
{
    std::tstringstream ss;

    ss<<value;

    return ss.str();
}

template <>
inline std::tstring AnyTypeToString(const std::tstring &value)
{
    return value;
}

// 功能：以patten为分隔符将字符串str分隔并置入result中，并返回result的大小，ignore_blank标识是否忽略空白部分
// 注意：只要函数被调用，result都将首先被置空
// 注意：原始字符串首尾的空白始终不被置入result中
// 例如：以*分隔“a*b”和“*a*b”和“a*b*”和“*a*b*”时无论ignore_blank为何得到的结果总是两部分
template <class T>
size_t SplitString(const std::basic_string<T> &str, const std::basic_string<T> &patten, std::vector<std::basic_string<T> > &result, bool ignore_blank = true)
{
    std::basic_string<T>::size_type pos = str.find(patten);
    std::basic_string<T>::size_type begin = 0;
    bool first = true;

    result.clear();

    while (true)
    {
        if (pos == str.npos)
        {
            if (begin < str.size())
            {
                result.push_back(str.substr(begin));
            }

            return result.size();
        }
        else
        {
            if (pos > begin || (!first && !ignore_blank))
            {
                result.push_back(str.substr(begin, pos - begin));
            }

            begin = pos + patten.size();
            pos = str.find(patten, begin);

            first = false;
        }
    }
}

std::list<HWND> GetDoubtfulDesktopWindowsInSelfProcess();

std::tstring RegGetString(HKEY hKey, LPCTSTR lpRegPath, LPCTSTR lpRegValue, LPCTSTR lpDefaultData = TEXT(""));

//////////////////////////////////////////////////////////////////////////
#define LOCAL_BEGIN class __Local           \
{                                           \
public:                                     \
    void __Temp()                           \
    {                                       \
        m___nTemp = GetTickCount();         \
    }                                       \
    volatile int m___nTemp

#define LOCAL_END }
#define LOCAL __Local::

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD (0x160c)
#endif

#endif
