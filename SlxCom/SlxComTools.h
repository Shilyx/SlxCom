#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>

DWORD SHSetTempValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD dwType, LPCVOID pvData, DWORD cbData);
BOOL IsFileSigned(LPCTSTR lpFilePath);
BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength);
BOOL SaveResourceToFile(LPCTSTR lpResType, LPCTSTR lpResName, LPCTSTR lpFilePath);
LPCTSTR GetClipboardFiles(DWORD *pdwEffect, UINT *puFileCount = NULL);
BOOL SetClipboardText(LPCTSTR lpText);
BOOL RunCommand(LPTSTR lpCommandLine, LPCTSTR lpCurrentDirectory = NULL);
BOOL RunCommandWithArguments(LPCTSTR lpFile);

#endif
