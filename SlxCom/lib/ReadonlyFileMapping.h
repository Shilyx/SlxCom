#ifndef _READONLYFILEMAPPING_H
#define _READONLYFILEMAPPING_H

#include <Windows.h>

class CReadonlyFileMapping
{
public:
    CReadonlyFileMapping(LPCWSTR lpFilePath);
    virtual ~CReadonlyFileMapping();

    // 判断是否映射成功
    bool IsMapOk() const;

    // 获取映射失败的错误码
    DWORD GetErrorCode() const;

    // 获取映射成功的缓冲区
    LPCVOID GetBuffer() const;

    // 获取映射成功的缓冲区大小
    DWORD GetSize() const;

protected:
    DWORD m_dwErrorCode;
    bool m_bLoadOk;
    HANDLE m_hFile;
    HANDLE m_hFileMapping;
    DWORD m_dwFileSize;
    LPVOID m_lpBuffer;
};

#endif /* _READONLYFILEMAPPING_H */
