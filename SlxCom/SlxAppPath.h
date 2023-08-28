#ifndef _SLXAPPPATH_H
#define _SLXAPPPATH_H

#include <Windows.h>
#include "lib/charconv.h"

BOOL ModifyAppPath(LPCWSTR lpFilePath);
BOOL ModifyAppPath_GetFileCommand(LPCWSTR lpFilePath, WCHAR szCommand[], DWORD dwSize);
std::wstring GetExistingAppPathCommand(LPCWSTR lpFilePath);

#endif /* _SLXAPPPATH_H */