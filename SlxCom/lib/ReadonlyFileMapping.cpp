#include "ReadonlyFileMapping.h"

CReadonlyFileMapping::CReadonlyFileMapping(LPCWSTR lpFilePath)
    : m_hFile(INVALID_HANDLE_VALUE)
    , m_hFileMapping(NULL)
    , m_dwFileSize(0)
    , m_lpBuffer(NULL)
    , m_bLoadOk(false)
    , m_dwErrorCode(ERROR_SUCCESS)
{
    m_hFile = CreateFileW(lpFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        DWORD dwFileSizeHigh = 0;
        DWORD dwFileSize = GetFileSize(m_hFile, &dwFileSizeHigh);

        if (dwFileSizeHigh == 0)
        {
            if (dwFileSize == 0)
            {
                m_bLoadOk = true;
            }
            else
            {
                m_hFileMapping = CreateFileMappingW(m_hFile, NULL, PAGE_READONLY | SEC_COMMIT, 0, 0, NULL);

                if (m_hFileMapping != NULL)
                {
                    m_lpBuffer = MapViewOfFile(m_hFileMapping, FILE_MAP_READ, 0, 0, dwFileSize);

                    if (m_lpBuffer != NULL)
                    {
                        m_bLoadOk = true;
                        m_dwFileSize = dwFileSize;
                    }
                    else
                    {
                        m_dwErrorCode = GetLastError();
                    }
                }
                else
                {
                    m_dwErrorCode = GetLastError();
                }
            }
        }
        else
        {
            m_dwErrorCode = ERROR_BUFFER_OVERFLOW;
        }
    }
    else
    {
        m_dwErrorCode = GetLastError();
    }
}

CReadonlyFileMapping::~CReadonlyFileMapping()
{
    if (m_lpBuffer != NULL)
    {
        UnmapViewOfFile(m_lpBuffer);
    }

    if (m_hFileMapping != NULL)
    {
        CloseHandle(m_hFileMapping);
    }

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
    }
}

DWORD CReadonlyFileMapping::GetErrorCode() const
{
    return m_dwErrorCode;
}

bool CReadonlyFileMapping::IsMapOk() const
{
    return m_bLoadOk;
}

LPCVOID CReadonlyFileMapping::GetBuffer() const
{
    return m_lpBuffer;
}

DWORD CReadonlyFileMapping::GetSize() const
{
    return m_dwFileSize;
}