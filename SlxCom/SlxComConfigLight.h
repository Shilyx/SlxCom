#ifndef _SLXCOMCONFIGLIGHT_H
#define _SLXCOMCONFIGLIGHT_H

#include <Windows.h>
#include <string>
#include <vector>

bool cfglGetBool(LPCWSTR lpKey, bool bDefault = FALSE);
void cfglSetBool(LPCWSTR lpKey, bool bValue);

DWORD cfglGetLong(LPCWSTR lpKey, DWORD dwDefault = 0);
void cfglSetLong(LPCWSTR lpKey, DWORD dwValue);

unsigned __int64 cfglGetLongLong(LPCWSTR lpKey, unsigned __int64 qwDefault = 0);
void cfglSetLongLong(LPCWSTR lpKey, unsigned __int64 qwValue);

std::wstring cfglGetString(LPCWSTR lpKey, const std::wstring &strDefault = L"");
void cfglSetString(LPCWSTR lpKey, const std::wstring &strValue);

#endif /* _SLXCOMCONFIGLIGHT_H */