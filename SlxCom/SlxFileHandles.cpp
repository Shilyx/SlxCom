#include "SlxFileHandles.h"
#include <Shlwapi.h>
#include <TlHelp32.h>

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation,
    SystemProcessorInformation, // obsolete...delete
    SystemPerformanceInformation,
    SystemTimeOfDayInformation,
    SystemPathInformation,
    SystemProcessInformation,
    SystemCallCountInformation,
    SystemDeviceInformation,
    SystemProcessorPerformanceInformation,
    SystemFlagsInformation,
    SystemCallTimeInformation,
    SystemModuleInformation,
    SystemLocksInformation,
    SystemStackTraceInformation,
    SystemPagedPoolInformation,
    SystemNonPagedPoolInformation,
    SystemHandleInformation,
    SystemObjectInformation,
    SystemPageFileInformation,
    SystemVdmInstemulInformation,
    SystemVdmBopInformation,
    SystemFileCacheInformation,
    SystemPoolTagInformation,
    SystemInterruptInformation,
    SystemDpcBehaviorInformation,
    SystemFullMemoryInformation,
    SystemLoadGdiDriverInformation,
    SystemUnloadGdiDriverInformation,
    SystemTimeAdjustmentInformation,
    SystemSummaryMemoryInformation,
    SystemMirrorMemoryInformation,
    SystemPerformanceTraceInformation,
    SystemObsolete0,
    SystemExceptionInformation,
    SystemCrashDumpStateInformation,
    SystemKernelDebuggerInformation,
    SystemContextSwitchInformation,
    SystemRegistryQuotaInformation,
    SystemExtendServiceTableInformation,
    SystemPrioritySeperation,
    SystemVerifierAddDriverInformation,
    SystemVerifierRemoveDriverInformation,
    SystemProcessorIdleInformation,
    SystemLegacyDriverInformation,
    SystemCurrentTimeZoneInformation,
    SystemLookasideInformation,
    SystemTimeSlipNotification,
    SystemSessionCreate,
    SystemSessionDetach,
    SystemSessionInformation,
    SystemRangeStartInformation,
    SystemVerifierInformation,
    SystemVerifierThunkExtend,
    SystemSessionProcessInformation,
    SystemLoadGdiDriverInSystemSpace,
    SystemNumaProcessorMap,
    SystemPrefetcherInformation,
    SystemExtendedProcessInformation,
    SystemRecommendedSharedDataAlignment,
    SystemComPlusPackage,
    SystemNumaAvailableMemory,
    SystemProcessorPowerInformation,
    SystemEmulationBasicInformation,
    SystemEmulationProcessorInformation,
    SystemExtendedHandleInformation,
    SystemLostDelayedWriteInformation,
    SystemBigPoolInformation,
    SystemSessionPoolTagInformation,
    SystemSessionMappedViewInformation,
    SystemHotpatchInformation,
    SystemObjectSecurityMode,
    SystemWatchdogTimerHandler,
    SystemWatchdogTimerInformation,
    SystemLogicalProcessorInformation,
    SystemWow64SharedInformation,
    SystemRegisterFirmwareTableInformationHandler,
    SystemFirmwareTableInformation,
    SystemModuleInformationEx,
    SystemVerifierTriageInformation,
    SystemSuperfetchInformation,
    SystemMemoryListInformation,
    SystemFileCacheInformationEx,
    MaxSystemInfoClass // MaxSystemInfoClass should always be the last enum
} SYSTEM_INFORMATION_CLASS;

typedef LONG NTSTATUS;
#define NT_SUCCESS(Status)				((LONG)(Status) >= 0) 
#define NT_ERROR(Status)				((ULONG)(Status) >> 30 == 3) 

//  The specified information record length does not match the length required for the specified information class.
#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)

typedef NTSTATUS (__stdcall *NTQUERYSYSTEMINFORMATION)(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    IN OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    );

typedef struct system_handle_entry
{
    ULONG		uIdProcess;		//进程ID
    UCHAR		ObjectType;		//句柄类型
    UCHAR		Flags;			//标志
    USHORT		Handle;			//句柄的数值
    PVOID		ObjectPointer;	//句柄所指向的内核对象地址
    ACCESS_MASK	GrantedAccess;	//访问权限
} SYSTEM_HANDLE_ENTRY, *LPSYSTEM_HANDLE_ENTRY;

typedef struct system_handle_info
{
    ULONG		uCount;			//系统句柄数
    SYSTEM_HANDLE_ENTRY aSH[1];	//句柄信息
}SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_NAME_INFORMATION
{
    UNICODE_STRING ObjectName;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef enum _OBJECT_INFORMATION_CLASS
{
    ObjectBasicInformation,	// Result is OBJECT_BASIC_INFORMATION structure
    ObjectNameInformation,	// Result is OBJECT_NAME_INFORMATION structure
    ObjectTypeInformation,	// Result is OBJECT_TYPE_INFORMATION structure
    ObjectAllInformation,	// Result is OBJECT_ALL_INFORMATION structure
    ObjectDataInformation	// Result is OBJECT_DATA_INFORMATION structure
} OBJECT_INFORMATION_CLASS, *POBJECT_INFORMATION_CLASS;

typedef NTSTATUS (__stdcall* NTQUERYOBJECT)(
    IN HANDLE						ObjectHandle,
    IN OBJECT_INFORMATION_CLASS		ObjectInformationClass,
    OUT PVOID						ObjectInformation,
    IN ULONG						ObjectInformationLength,
    OUT PULONG						ReturnLength OPTIONAL
    );

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS  Status;
        PVOID  Pointer;
    };
    ULONG_PTR  Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _FILE_NAME_INFORMATION {
    ULONG  FileNameLength;
    WCHAR  FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

typedef enum _FILE_INFORMATION_CLASS { 
    FileDirectoryInformation                 = 1,
    FileFullDirectoryInformation,
    FileBothDirectoryInformation,
    FileBasicInformation,
    FileStandardInformation,
    FileInternalInformation,
    FileEaInformation,
    FileAccessInformation,
    FileNameInformation,
    FileRenameInformation,
    FileLinkInformation,
    FileNamesInformation,
    FileDispositionInformation,
    FilePositionInformation,
    FileFullEaInformation,
    FileModeInformation,
    FileAlignmentInformation,
    FileAllInformation,
    FileAllocationInformation,
    FileEndOfFileInformation,
    FileAlternateNameInformation,
    FileStreamInformation,
    FilePipeInformation,
    FilePipeLocalInformation,
    FilePipeRemoteInformation,
    FileMailslotQueryInformation,
    FileMailslotSetInformation,
    FileCompressionInformation,
    FileObjectIdInformation,
    FileCompletionInformation,
    FileMoveClusterInformation,
    FileQuotaInformation,
    FileReparsePointInformation,
    FileNetworkOpenInformation,
    FileAttributeTagInformation,
    FileTrackingInformation,
    FileIdBothDirectoryInformation,
    FileIdFullDirectoryInformation,
    FileValidDataLengthInformation,
    FileShortNameInformation,
    FileIoCompletionNotificationInformation,
    FileIoStatusBlockRangeInformation,
    FileIoPriorityHintInformation,
    FileSfioReserveInformation,
    FileSfioVolumeInformation,
    FileHardLinkInformation,
    FileProcessIdsUsingFileInformation,
    FileNormalizedNameInformation,
    FileNetworkPhysicalNameInformation,
    FileIdGlobalTxDirectoryInformation,
    FileIsRemoteDeviceInformation,
    FileAttributeCacheInformation,
    FileNumaNodeInformation,
    FileStandardLinkInformation,
    FileRemoteProtocolInformation,
    FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef NTSTATUS (__stdcall *NTQUERYINFORMATIONFILE)(
    IN HANDLE  FileHandle,
    OUT PIO_STATUS_BLOCK  IoStatusBlock,
    OUT PVOID  FileInformation,
    IN ULONG  Length,
    IN FILE_INFORMATION_CLASS  FileInformationClass
    );

static NTQUERYINFORMATIONFILE NtQueryInformationFile = NULL;
static NTQUERYSYSTEMINFORMATION NtQuerySystemInformation = NULL;

PVOID GetInfoTable(IN ULONG uTableType)
{
    ULONG mSize = 0x8000;
    PVOID mPtr;
    NTSTATUS status;

    if (NtQuerySystemInformation == NULL)
    {
        (PROC &)NtQuerySystemInformation = GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQuerySystemInformation");
    }

    do
    {
        mPtr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, mSize);

        if(!mPtr)
            return NULL;

        status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)uTableType, mPtr, mSize, NULL);

        if(status == STATUS_INFO_LENGTH_MISMATCH)
        {
            HeapFree(GetProcessHeap(), HEAP_ZERO_MEMORY, mPtr);
            mSize = mSize * 2;
        }

    }while(status == STATUS_INFO_LENGTH_MISMATCH);

    if(NT_SUCCESS(status))
        return mPtr;

    HeapFree(GetProcessHeap(), 0, mPtr);

    return NULL;
}

HANDLE DuplicateHandleHelp(DWORD dwProcessId, HANDLE hObject)
{
    HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);

    if (hProcess == NULL)
    {
        return NULL;
    }

    HANDLE hResult = NULL;

    DuplicateHandle(hProcess, hObject, GetCurrentProcess(), &hResult, 0, FALSE, DUPLICATE_SAME_ACCESS);

    CloseHandle(hProcess);

    return hResult;
}

#define FILE_NAME_INFORMATION_BUFFER_LENGTH 2000

struct QueryFileNameThreadParam
{
    DWORD dwSerials['Z' - 'A' + 1];
    HANDLE hDubHandle;
    TCHAR szFilePath[MAX_PATH];
};

DWORD __stdcall QueryFileNameProc(LPVOID lpParam)
{
    QueryFileNameThreadParam *pThreadParam = (QueryFileNameThreadParam *)lpParam;
    BOOL bDriveLetterSucceed = FALSE;
    TCHAR chDriveLetter = TEXT('A');
    BY_HANDLE_FILE_INFORMATION bhfi;

    if (GetFileInformationByHandle(pThreadParam->hDubHandle, &bhfi))
    {
        if (bhfi.dwVolumeSerialNumber != 0 && bhfi.dwVolumeSerialNumber != -1)
        {
            unsigned char ucDriveIndex = 0;

            for (; ucDriveIndex < sizeof(pThreadParam->dwSerials) / sizeof(DWORD); ucDriveIndex += 1)
            {
                if (bhfi.dwVolumeSerialNumber == pThreadParam->dwSerials[ucDriveIndex])
                {
                    chDriveLetter += ucDriveIndex;
                    bDriveLetterSucceed = TRUE;
                    break;
                }
            }
        }
    }

    if (bDriveLetterSucceed)
    {
        DWORD dwRet = 0;
        char szBuffer[3000] = "";
        PFILE_NAME_INFORMATION pFni = (PFILE_NAME_INFORMATION)szBuffer;
        IO_STATUS_BLOCK iob;

        if (NtQueryInformationFile == NULL)
        {
            (PROC &)NtQueryInformationFile = GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQueryInformationFile");
        }

        if (NtQueryInformationFile != NULL)
        {
            NtQueryInformationFile(pThreadParam->hDubHandle, &iob, pFni, sizeof(szBuffer), FileNameInformation);
        }

        if (pFni->FileNameLength > 0)
        {
            wnsprintf(
                pThreadParam->szFilePath,
                sizeof(pThreadParam->szFilePath) / sizeof(TCHAR),
                TEXT("%c:%ls"),
                chDriveLetter,
                pFni->FileName
                );
        }
    }

    return 0;
}

VOID InitDriveSerials(DWORD dwSerials[])
{
    DWORD dwDrives = GetLogicalDrives();
    DWORD dwDriveIndex = 0;
    TCHAR szDriveRoot[] = TEXT("1:\\");

    for (; dwDriveIndex < 'Z' - 'A' + 1; dwDriveIndex += 1)
    {
        if ((1 << dwDriveIndex) & dwDrives)
        {
            szDriveRoot[0] = TEXT('A') + (unsigned char)dwDriveIndex;

            if (!GetVolumeInformation(
                szDriveRoot,
                NULL,
                0,
                &dwSerials[dwDriveIndex],
                NULL,
                NULL,
                NULL,
                NULL
                ))
            {
                dwSerials[dwDriveIndex] = -1;
            }
        }
    }
}

BOOL QueryProcessNameFromSnapshot(HANDLE hSnapshot, DWORD dwProcessId, TCHAR szProcessName[], DWORD dwSize)
{
    PROCESSENTRY32 pe32 = {sizeof(pe32)};

    if (Process32First(hSnapshot, &pe32))
    {
        do 
        {
            if (pe32.th32ProcessID == dwProcessId)
            {
                lstrcpyn(szProcessName, pe32.szExeFile, dwSize);
                return TRUE;
            }

        } while (Process32Next(hSnapshot, &pe32));
    }

    return FALSE;
}

FileHandleInfo *GetFileHandleInfos(DWORD *pCount)
{
    TCHAR szTempFile[MAX_PATH];
    TCHAR szTempPath[MAX_PATH];
    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    DWORD dwFileIndex = 0;
    QueryFileNameThreadParam threadParam;

    InitDriveSerials(threadParam.dwSerials);

    GetTempPath(sizeof(szTempPath) / sizeof(TCHAR), szTempPath);
    lstrcpyn(szTempFile, szTempPath, sizeof(szTempFile) / sizeof(TCHAR));
    StrCatBuff(szTempFile, TEXT("__SlxComTestTempFile.txt"), sizeof(szTempFile) / sizeof(TCHAR));

    while (TRUE)
    {
        hTempFile = CreateFile(
            szTempFile,
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );

        if (hTempFile != INVALID_HANDLE_VALUE)
        {
            break;
        }

        wnsprintf(szTempFile,
            sizeof(szTempFile) / sizeof(TCHAR),
            TEXT("%s__SlxComTestTempFile_%lu.txt"),
            szTempPath,
            ++dwFileIndex
            );
    }

    if (hTempFile == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    FileHandleInfo *pResult = NULL;
    *pCount = 0;

    PSYSTEM_HANDLE_INFORMATION pHandles = (PSYSTEM_HANDLE_INFORMATION)GetInfoTable(SystemHandleInformation);

    if (pHandles != NULL)
    {
        UINT uIndex = 0;
        UCHAR uFileType = -1;
        DWORD dwCurrentProcessId = GetCurrentProcessId();

        for (uIndex = 0; uIndex < pHandles->uCount; uIndex++)
        {
            if (pHandles->aSH[uIndex].uIdProcess == dwCurrentProcessId && (HANDLE)pHandles->aSH[uIndex].Handle == hTempFile)
            {
                uFileType = pHandles->aSH[uIndex].ObjectType;
                break;
            }
        }

        for (uIndex = 0; uIndex < pHandles->uCount; uIndex++)
        {
            if (pHandles->aSH[uIndex].uIdProcess != dwCurrentProcessId)
            {
                if (pHandles->aSH[uIndex].ObjectType == uFileType)
                {
                    HANDLE hDubHandle = DuplicateHandleHelp(pHandles->aSH[uIndex].uIdProcess, (HANDLE)pHandles->aSH[uIndex].Handle);

                    if (hDubHandle != NULL)
                    {
                        threadParam.hDubHandle = hDubHandle;
                        lstrcpyn(threadParam.szFilePath, TEXT(""), sizeof(threadParam.szFilePath) / sizeof(TCHAR));

                        HANDLE hThread = CreateThread(NULL, 0, QueryFileNameProc, &threadParam, 0, NULL);

                        if (hThread != NULL)
                        {
                            if (WAIT_OBJECT_0 == WaitForSingleObject(hThread, 500))
                            {
                                if (lstrlen(threadParam.szFilePath) > 0)
                                {
                                    *pCount += 1;

                                    if (pResult == NULL)
                                    {
                                        pResult = (FileHandleInfo *)HeapAlloc(GetProcessHeap(), 0, *pCount * sizeof(FileHandleInfo));
                                    }
                                    else
                                    {
                                        pResult = (FileHandleInfo *)HeapReAlloc(GetProcessHeap(), 0, pResult, *pCount * sizeof(FileHandleInfo));
                                    }

                                    pResult[*pCount - 1].dwProcessId = pHandles->aSH[uIndex].uIdProcess;
                                    pResult[*pCount - 1].hFile = (HANDLE)pHandles->aSH[uIndex].Handle;
                                    lstrcpyn(pResult[*pCount - 1].szFilePath, threadParam.szFilePath, sizeof(pResult[*pCount - 1].szFilePath) / sizeof(TCHAR));

                                    if (!QueryProcessNameFromSnapshot(
                                        hSnapshot,
                                        pResult[*pCount - 1].dwProcessId,
                                        pResult[*pCount - 1].szProcessName,
                                        sizeof(pResult[*pCount - 1].szProcessName) / sizeof(TCHAR)
                                        ))
                                    {
                                        lstrcpyn(
                                            pResult[*pCount - 1].szProcessName,
                                            TEXT(""),
                                            sizeof(pResult[*pCount - 1].szProcessName) / sizeof(TCHAR)
                                            );
                                    }
                                }
                            }
                            else
                            {
                                TerminateThread(hThread, 0);
                            }

                            CloseHandle(hThread);
                        }

                        CloseHandle(hDubHandle);
                    }
                }
            }
        }
    }

    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSnapshot);
    }

    CloseHandle(hTempFile);

    return pResult;
}

VOID FreeFileHandleInfos(FileHandleInfo *pInfos)
{
    HeapFree(GetProcessHeap(), 0, pInfos);
}