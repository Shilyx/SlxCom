#ifndef _READONLYFILEMAPPING_H
#define _READONLYFILEMAPPING_H

#include <Windows.h>

class CReadonlyFileMapping
{
public:
    CReadonlyFileMapping(LPCWSTR lpFilePath);
    virtual ~CReadonlyFileMapping();

    // �ж��Ƿ�ӳ��ɹ�
    bool IsMapOk() const;

    // ��ȡӳ��ʧ�ܵĴ�����
    DWORD GetErrorCode() const;

    // ��ȡӳ��ɹ��Ļ�����
    LPCVOID GetBuffer() const;

    // ��ȡӳ��ɹ��Ļ�������С
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
