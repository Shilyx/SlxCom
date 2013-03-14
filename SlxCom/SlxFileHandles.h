#ifndef _SLXFILEHANDLES_H
#define _SLXFILEHANDLES_H

#include <Windows.h>

struct FileHandleInfo
{
    DWORD               dwProcessId;
    HANDLE              hFile;
    TCHAR               szFilePath[MAX_PATH];
};

FileHandleInfo *GetFileHandleInfos(DWORD *pCount);
VOID FreeFileHandleInfos(FileHandleInfo *pInfos);

#endif // _SLXFILEHANDLES_H