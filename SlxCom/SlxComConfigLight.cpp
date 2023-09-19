#include "SlxComConfigLight.h"
#include "SlxComTools.h"
#include <Shlwapi.h>

using namespace std;

#define REG_BASE_PATH L"Software\\Shilyx Studio\\SlxCom\\config\\"

bool BuildRegPath(LPCWSTR lpKey, wstring &strPath, wstring &strValue) {
    vector<wstring> vectorSections;

    SplitString<WCHAR>(lpKey, L".", vectorSections);

    if (vectorSections.size() == 0) {
        return false;
    }

    strPath = REG_BASE_PATH;
    strValue.erase(strValue.begin(), strValue.end());

    strValue = *vectorSections.rbegin();
    vectorSections.pop_back();

    for (vector<wstring>::const_iterator it = vectorSections.begin(); it != vectorSections.end(); ++it) {
        strPath += *it;
        strPath += L"\\";
    }

    return true;
}

bool cfglGetBool(LPCWSTR lpKey, bool bDefault) {
    return !!cfglGetLong(lpKey, bDefault);
}

void cfglSetBool(LPCWSTR lpKey, bool bValue) {
    cfglSetLong(lpKey, !!bValue);
}

DWORD cfglGetLong(LPCWSTR lpKey, DWORD dwDefault) {
    DWORD dwResult = dwDefault;
    DWORD dwSize = sizeof(dwResult);
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHGetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), NULL, &dwResult, &dwSize);
    }

    return dwResult;
}

void cfglSetLong(LPCWSTR lpKey, DWORD dwValue) {
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHSetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), REG_DWORD, &dwValue, sizeof(dwValue));
    }
}

unsigned __int64 cfglGetLongLong(LPCWSTR lpKey, unsigned __int64 qwDefault) {
    unsigned __int64 qwResult = qwDefault;
    DWORD dwSize = sizeof(qwResult);
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHGetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), NULL, &qwResult, &dwSize);
    }

    return qwResult;
}

void cfglSetLongLong(LPCWSTR lpKey, unsigned __int64 qwValue) {
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHSetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), REG_QWORD, &qwValue, sizeof(qwValue));
    }
}

std::wstring cfglGetString(LPCWSTR lpKey, const std::wstring &strDefault) {
    WCHAR szText[4096] = L"";
    DWORD dwSize = sizeof(szText);
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHGetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), NULL, szText, &dwSize);

        if (dwSize > 0) {
            return szText;
        }
    }

    return strDefault;
}

void cfglSetString(LPCWSTR lpKey, const std::wstring &strData) {
    wstring strPath;
    wstring strValue;

    if (BuildRegPath(lpKey, strPath, strValue)) {
        SHSetValueW(HKEY_CURRENT_USER, strPath.c_str(), strValue.c_str(), REG_SZ, strData.c_str(), ((DWORD)strData.size() + 1) * sizeof(WCHAR));
    }
}
