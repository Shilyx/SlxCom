#ifndef _SLX_COM_TOOLS_H
#define _SLX_COM_TOOLS_H

#include <Windows.h>

BOOL IsFileSigned(LPCTSTR lpFilePath);
BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength);
BOOL SaveResourceToFile(LPCTSTR lpResType, LPCTSTR lpResName, LPCTSTR lpFilePath);
LPCTSTR GetClipboardFiles(DWORD *pdwEffect);
BOOL SetClipboardText(LPCTSTR lpText);

#endif
