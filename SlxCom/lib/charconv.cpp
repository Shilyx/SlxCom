#include "charconv.h"
#include <Windows.h>
#include <tchar.h>

using namespace std;

strutf8 AtoU(const string &str)
{
    return WtoU(AtoW(str));
}

strutf8 WtoU(const wstring &str)
{
    strutf8 ret;

    int in_len = (int)str.length();
    int count = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), in_len, NULL, 0, NULL, NULL);

    if (count > 0)
    {
        char *buffer = (char *)malloc(count * sizeof(char));

        if (buffer != 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, str.c_str(), in_len, buffer, count, NULL, NULL);
            ret = strutf8(buffer, count);

            free((void *)buffer);
        }
    }

    return ret;
}

string UtoA(const strutf8 &str)
{
    return WtoA(UtoW(str));
}

wstring UtoW(const strutf8 &str)
{
    wstring ret;

    int in_len = (int)str.length();
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), in_len, NULL, 0);

    if (count > 0)
    {
        wchar_t *buffer = (wchar_t *)malloc(count * sizeof(wchar_t));

        if (buffer != 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), in_len, buffer, count);
            ret = wstring(buffer, count);

            free((void *)buffer);
        }
    }

    return ret;
}

string WtoA(const wstring &str)
{
    string ret;

    int in_len = (int)str.length();
    int count = WideCharToMultiByte(CP_ACP, 0, str.c_str(), in_len, NULL, 0, NULL, NULL);

    if (count > 0)
    {
        char *buffer = (char *)malloc(count * sizeof(char));

        if (buffer != 0)
        {
            WideCharToMultiByte(CP_ACP, 0, str.c_str(), in_len, buffer, count, NULL, NULL);
            ret = string(buffer, count);

            free((void *)buffer);
        }
    }

    return ret;
}

wstring AtoW(const string &str)
{
    wstring ret;

    int in_len = (int)str.length();
    int count = MultiByteToWideChar(CP_ACP, 0, str.c_str(), in_len, NULL, 0);

    if (count > 0)
    {
        wchar_t *buffer = (wchar_t *)malloc(count * sizeof(wchar_t));

        if (buffer != 0)
        {
            MultiByteToWideChar(CP_ACP, 0, str.c_str(), in_len, buffer, count);
            ret = wstring(buffer, count);

            free((void *)buffer);
        }
    }

    return ret;
}
