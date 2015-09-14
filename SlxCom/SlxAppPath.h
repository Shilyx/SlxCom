#ifndef _SLXAPPPATH_H
#define _SLXAPPPATH_H

#include <Windows.h>
#include "lib/charconv.h"

BOOL ModifyAppPath(LPCTSTR lpFilePath);
BOOL ModifyAppPath_GetFileCommand(LPCTSTR lpFilePath, TCHAR szCommand[], DWORD dwSize);
std::tstring GetExistingAppPathCommand(LPCTSTR lpFilePath);

#endif /* _SLXAPPPATH_H */