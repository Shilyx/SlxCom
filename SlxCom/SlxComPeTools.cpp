#include "SlxComPeTools.h"

#define BETWEEN(i, a, b)            ((((i) <= (a)) && ((i) >= (b))) || (((i) >= (a)) && ((i) <= (b))))
#define EQUALA(a, b)                ((a) && (b) && (0 == lstrcmpA(a, b)))

typedef struct _FILE_MAPPING_STRUCT
{
    HANDLE hFile;
    HANDLE hMap;
    LPVOID lpView;
    LARGE_INTEGER fileSize;
    DWORD mappedSize;
} FILE_MAPPING_STRUCT, *PFILE_MAPPING_STRUCT;

static BOOL WINAPI _PeIsPeFile(LPVOID buffer, BOOL* b64)
{
    if (!buffer)
    {
        return FALSE;
    }

    IMAGE_DOS_HEADER* pDosHdr = (IMAGE_DOS_HEADER*)buffer;
    if (IMAGE_DOS_SIGNATURE != pDosHdr->e_magic)
    {
        return FALSE;
    }

    IMAGE_NT_HEADERS32* pNtHdr = (IMAGE_NT_HEADERS32*)((byte*)pDosHdr + pDosHdr->e_lfanew);
    if (IMAGE_NT_SIGNATURE != pNtHdr->Signature)
    {
        return FALSE;;
    }

    if (b64)
    {
        *b64 = (pNtHdr->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64));
    }

    return TRUE;
}

PFILE_MAPPING_STRUCT WINAPI GdFileMappingFileW(LPCWSTR fileName, BOOL bWrite, DWORD maxViewSize)
{
    PFILE_MAPPING_STRUCT pfms = NULL;

    do
    {
        if (!fileName)
        {
            break;
        }

        pfms = (PFILE_MAPPING_STRUCT)malloc(sizeof(FILE_MAPPING_STRUCT));
        if (!pfms)
        {
            break;
        }
        memset(pfms, 0, sizeof(FILE_MAPPING_STRUCT));

        pfms->hFile = CreateFileW(
            fileName,
            GENERIC_READ | (bWrite ? GENERIC_WRITE : 0),
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
        if (INVALID_HANDLE_VALUE == pfms->hFile)
        {
            break;
        }

        if (!GetFileSizeEx(pfms->hFile, &(pfms->fileSize)))
        {
            break;
        }
        if (pfms->fileSize.QuadPart < (sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS32)))
        {
            break;
        }

        // MAP的大小不能大于PE总大小
        if (!maxViewSize)
        {
            pfms->mappedSize= pfms->fileSize.QuadPart > 0xffffffff ? 0xffffffff : pfms->fileSize.LowPart;
        }
        else
        {
            pfms->mappedSize = pfms->fileSize.QuadPart > maxViewSize ? maxViewSize : pfms->fileSize.LowPart;
        }

        pfms->hMap = CreateFileMapping(pfms->hFile, NULL, bWrite ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
        if (!pfms->hMap)
        {
            break;
        }

        pfms->lpView = MapViewOfFile(pfms->hMap, FILE_MAP_READ | (bWrite ? FILE_MAP_WRITE : 0), 0, 0, pfms->mappedSize);
        if (!pfms->lpView)
        {
            break;
        }
    } while (FALSE);

    return pfms;
}

void WINAPI GdFileCloseFileMapping(PFILE_MAPPING_STRUCT pfms)
{
    if (pfms)
    {
        if (pfms->lpView)
        {
            UnmapViewOfFile(pfms->lpView);
        }

        if (pfms->hMap)
        {
            CloseHandle(pfms->hMap);
        }

        if (INVALID_HANDLE_VALUE != pfms->hFile)
        {
            CloseHandle(pfms->hFile);
        }

        free((void*)pfms);
    }
}

#define _PE_MAX_MAP_SIZE    (1024 * 1024 * 1024)

static BOOL WINAPI _PeCheckPeMapping(PFILE_MAPPING_STRUCT pfms, BOOL* b64)
{
    if (!pfms || !pfms->lpView || !pfms->fileSize.QuadPart)
    {
        return FALSE;
    }

    if (pfms->fileSize.QuadPart < (sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS32)))
    {
        return FALSE;
    }

    if (!_PeIsPeFile(pfms->lpView, b64))
    {
        return FALSE;
    }

    return TRUE;
}


static PFILE_MAPPING_STRUCT WINAPI _PeMappingFile(LPCWSTR fileName, BOOL bWrite, BOOL *b64)
{
    PFILE_MAPPING_STRUCT pfms = GdFileMappingFileW(fileName, bWrite, _PE_MAX_MAP_SIZE);
    if (!_PeCheckPeMapping(pfms, b64))
    {
        GdFileCloseFileMapping(pfms);
        return NULL;
    }

    return pfms;
}

static UINT WINAPI _PeRVAToFileOffset(void* lpMem, UINT rva, LPSTR sectionName, UINT bufSize)
{
    UINT offset = 0xffffffff;

    do
    {
        if (!lpMem || !rva || (rva > 0x7fffffff))
        {
            break;
        }

        if (!_PeIsPeFile(lpMem, NULL))
        {
            break;
        }

        IMAGE_DOS_HEADER* pDosHdr = (IMAGE_DOS_HEADER*)lpMem;
        IMAGE_NT_HEADERS32* pNtHdr = (IMAGE_NT_HEADERS32*)((byte*)pDosHdr + pDosHdr->e_lfanew);

        int sectionCounts = pNtHdr->FileHeader.NumberOfSections;
        if (!sectionCounts || (sectionCounts > 4096))
        {
            break;
        }

        int secHdrOffset = (pNtHdr->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64)) ? sizeof(IMAGE_NT_HEADERS64) : sizeof(IMAGE_NT_HEADERS32);
        IMAGE_SECTION_HEADER* pSecHdr = (IMAGE_SECTION_HEADER*)((byte*)pNtHdr + secHdrOffset);

        int i;
        for (i = 0; i < pNtHdr->FileHeader.NumberOfSections; ++i)
        {
            // 这里是严格按照某本教程上的实现，为什么节起始RVA要加 SizeOfRawData ？
            // 感觉上加映射后的大小更合理，不过应该是无所谓的，因为 RawData 和 VirtualData 大小不同的部分
            // 要么是不映射进内存，要么是不存在于原始文件上，不在此函数的考虑范围内
            if (BETWEEN(rva, pSecHdr[i].VirtualAddress, pSecHdr[i].VirtualAddress + pSecHdr[i].SizeOfRawData))
            {
                if (sectionName)
                {
                    lstrcpynA(sectionName, (LPCSTR)pSecHdr[i].Name, bufSize > 8 ? 8 : bufSize);
                }

                // 文件偏移等于在内存中地址和所属节的相对偏移加上文件起始位置
                offset = (rva - pSecHdr[i].VirtualAddress) + pSecHdr[i].PointerToRawData;
                break;
            }
        }
    } while (FALSE);

    return offset;
}

BOOL PeIsCOMModule(LPCTSTR fileName)
{
    BOOL bRet = FALSE;
    PFILE_MAPPING_STRUCT pfms = NULL;

    do
    {
        BOOL b64 = FALSE;
        pfms = _PeMappingFile(fileName, FALSE, &b64);
        if (!pfms)
        {
            break;
        }

        // 来自 _PeMappingFile 信誉保障，不再多检查
        PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)pfms->lpView;
        PIMAGE_NT_HEADERS32 pNtHdr32 = (PIMAGE_NT_HEADERS32)((byte*)pfms->lpView + pDosHdr->e_lfanew);
        PIMAGE_NT_HEADERS64 pNtHdr64 = (PIMAGE_NT_HEADERS64)((byte*)pfms->lpView + pDosHdr->e_lfanew);

        UINT rva = b64 ? 
            pNtHdr64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress :
        pNtHdr32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

        if (!rva)
        {
            // 说明没有导出表
            break;
        }

        IMAGE_EXPORT_DIRECTORY* pExpDir = (IMAGE_EXPORT_DIRECTORY*)((byte*)pDosHdr + _PeRVAToFileOffset(pfms->lpView, rva, NULL, 0));
        DWORD* offsetNames = (DWORD*)((byte*)pDosHdr + _PeRVAToFileOffset(pfms->lpView, pExpDir->AddressOfNames, NULL, 0));
        UINT i;
        int score = 0;
        for (i = 0; i < pExpDir->NumberOfNames; ++i)
        {
            char* funcName = (char*)((byte*)pDosHdr + _PeRVAToFileOffset(pfms->lpView, offsetNames[i], NULL, 0));

#define _HADDSCORE(func)                    \
    if (EQUALA(funcName, func))     \
            {                               \
            score++;                    \
            }                               \

            _HADDSCORE("DllUnregisterServer");
            _HADDSCORE("DllRegisterServer");
            // _HADDSCORE("DllCanUnloadNow");
            // _HADDSCORE("DllGetClassObject");
            // _HADDSCORE("DllInstall");
#undef _HADDSCORE

            if (score >= 4)
            {
                break;
            }
        }

        bRet = (score >= 2);
    } while (FALSE);

    GdFileCloseFileMapping(pfms);

    return bRet;
}
