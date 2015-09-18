#include "lib/GeneralTreeItemW.h"
#include "lib/ReadonlyFileMapping.h"
#include <Shlwapi.h>
#include "SlxPeInformation.h"
#include "SlxComTools.h"
#include <vector>
#include <memory>
#include <sstream>
#include <map>

using namespace std;
using namespace tr1;

class PeInformationAnalyzer
{
#define HALT throw __LINE__
public:
    PeInformationAnalyzer(const wchar_t *lpPeFilePath)
        : m_szFilePath(lpPeFilePath)
        , m_lpBuffer(NULL)
        , m_dwBufferSize(0)
        , m_nNumberOfSections(0)
        , m_pImageSectionHeader(NULL)
    {
        Analyze();
    }

    shared_ptr<GeneralTreeItemW> GetResult() const
    {
        return m_pResult;
    }

private:
    wstring GetByte2String(WORD nValue) const
    {
        return fmtW(L"0x%04x", nValue);
    }

    wstring GetByte4String(DWORD nValue) const
    {
        return fmtW(L"0x%08x", nValue);
    }

    wstring GetByte8String(ULONGLONG nValue) const
    {
        return fmtW(L"0x%016x", nValue);
    }

    wstring GetTimeStampString(DWORD dwTimeStamp) const
    {
        SYSTEMTIME stTime;

        Time1970ToLocalTime(dwTimeStamp, &stTime);

        return fmtW(
            L"%04u-%02u-%02u %02u:%02u:%02u(0x%08x)",
            stTime.wYear,
            stTime.wMonth,
            stTime.wDay,
            stTime.wHour,
            stTime.wMinute,
            stTime.wSecond,
            dwTimeStamp);
    }

    wstring GetAnsiString(const char *lpString) const
    {
        wchar_t szNewString[8192];

        if (lpString != NULL)
        {
            for (int i = 0; i < RTL_NUMBER_OF(szNewString); ++i)
            {
                szNewString[i] = lpString[i];

                if (szNewString[i] == L'\0')
                {
                    break;
                }
            }
        }
        else
        {
            szNewString[0] = L'\0';
        }

        szNewString[RTL_NUMBER_OF(szNewString) - 1] = L'\0';

        return szNewString;
    }

    DWORD RvaToVa(DWORD dwRva) const
    {
        if (m_pImageSectionHeader == NULL || m_nNumberOfSections == 0)
        {
            return 0;
        }

        for (int i = 0; i < m_nNumberOfSections; ++i)
        {
            if (dwRva >= m_pImageSectionHeader[i].VirtualAddress &&
                dwRva <= m_pImageSectionHeader[i].VirtualAddress + m_pImageSectionHeader[i].SizeOfRawData)
            {
                return (dwRva - m_pImageSectionHeader[i].VirtualAddress) + m_pImageSectionHeader[i].PointerToRawData;
            }
        }

        return 0;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeDosHeader(PIMAGE_DOS_HEADER pDosHeader)
    {
        shared_ptr<GeneralTreeItemW> pDosInfo(new GeneralTreeItemW(L"DOS头部"));

        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        {
            HALT;
        }

        pDosInfo->AppendItem(L"签名")->SetValue(fmtW(L"%c%c", ((const char *)&pDosHeader->e_magic)[0], ((const char *)&pDosHeader->e_magic)[1]));
        pDosInfo->AppendItem(L"额外字节")->SetValue(GetByte2String(pDosHeader->e_cblp));
        pDosInfo->AppendItem(L"页")->SetValue(GetByte2String(pDosHeader->e_cp));
        pDosInfo->AppendItem(L"重定位项目")->SetValue(GetByte2String(pDosHeader->e_crlc));
        pDosInfo->AppendItem(L"标头大小")->SetValue(GetByte2String(pDosHeader->e_cparhdr));
        pDosInfo->AppendItem(L"最小允许")->SetValue(GetByte2String(pDosHeader->e_minalloc));
        pDosInfo->AppendItem(L"最大允许")->SetValue(GetByte2String(pDosHeader->e_maxalloc));
        pDosInfo->AppendItem(L"初始SS")->SetValue(GetByte2String(pDosHeader->e_ss));
        pDosInfo->AppendItem(L"初始SP")->SetValue(GetByte2String(pDosHeader->e_sp));
        pDosInfo->AppendItem(L"校验码")->SetValue(GetByte2String(pDosHeader->e_csum));
        pDosInfo->AppendItem(L"初始IP")->SetValue(GetByte2String(pDosHeader->e_ss));
        pDosInfo->AppendItem(L"初始CS")->SetValue(GetByte2String(pDosHeader->e_cs));
        pDosInfo->AppendItem(L"重定位表")->SetValue(GetByte2String(pDosHeader->e_lfarlc));
        pDosInfo->AppendItem(L"覆盖")->SetValue(GetByte2String(pDosHeader->e_ovno));
        pDosInfo->AppendItem(L"PE头偏移")->SetValue(GetByte4String(pDosHeader->e_lfanew));

        return pDosInfo;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeNtHeaderWithoutOptHeader(PIMAGE_NT_HEADERS32 pNtHeader)
    {
        struct
        {
            WORD nMachine;
            LPCWSTR lpMachineName;
        } machineNames[] = {
            {0x0, L"适用于任何类型处理器"},
            {0x1d3, L"Matsushita AM33处理器"},
            {0x8664, L"x64处理器"},
            {0x1c0, L"ARM小尾处理器"},
            {0xebc, L"EFI字节码处理器"},
            {0x14c, L"Intel 386或后继处理器及其兼容处理器"},
            {0x200, L"Intel Itanium处理器"},
            {0x9041, L"Mitsubishi M32R小尾处理器"},
            {0x266, L"MIPS16处理器"},
            {0x366, L"带FPU的MIPS处理器"},
            {0x466, L"带FPU的MIPS16处理器"},
            {0x1f0, L"PowerPC小尾处理器"},
            {0x1f1, L"带符点运算支持的PowerPC处理器"},
            {0x166, L"MIPS小尾处理器"},
            {0x1a2, L"Hitachi SH3处理器"},
            {0x1a3, L"Hitachi SH3 DSP处理器"},
            {0x1a6, L"Hitachi SH4处理器"},
            {0x1a6, L"Hitachi SH5处理器"},
            {0x1c2, L"Thumb处理器"},
            {0x169, L"MIPS小尾WCE v2处理器"},
        };

        shared_ptr<GeneralTreeItemW> pNtInfo(new GeneralTreeItemW(L"NT头部"));
        PIMAGE_FILE_HEADER pFileHeader = &pNtHeader->FileHeader;
        char szSignature[128] = "";
        wstring strMachineName;

        if (pNtHeader->Signature != IMAGE_NT_SIGNATURE)
        {
            HALT;
        }

        m_nNumberOfSections = pFileHeader->NumberOfSections;

        *(DWORD *)szSignature = pNtHeader->Signature;

        for (int i = 0; i < RTL_NUMBER_OF(machineNames); ++i)
        {
            if (machineNames[i].nMachine == pFileHeader->Machine)
            {
                strMachineName = machineNames[i].lpMachineName;
                break;
            }
        }

        pNtInfo->AppendItem(L"签名")->SetValue(fmtW(L"%hs", szSignature));
        pNtInfo->AppendItem(L"机器类型")->SetValue(fmtW(L"%ls(0x%04x)", strMachineName.c_str(), pFileHeader->Machine));
        pNtInfo->AppendItem(L"区段数")->SetValue(GetByte2String(pFileHeader->NumberOfSections));
        pNtInfo->AppendItem(L"时间戳")->SetValue(GetTimeStampString(pFileHeader->TimeDateStamp));
        m_pBasicInfo->AppendItem(L"构建时间")->SetValue(GetTimeStampString(pFileHeader->TimeDateStamp));
        pNtInfo->AppendItem(L"符号表")->SetValue(GetByte4String(pFileHeader->PointerToSymbolTable));
        pNtInfo->AppendItem(L"符号数")->SetValue(GetByte4String(pFileHeader->NumberOfSymbols));
        pNtInfo->AppendItem(L"可选标头大小")->SetValue(GetByte2String(pFileHeader->SizeOfOptionalHeader));

        // 特性和具体特性
        shared_ptr<GeneralTreeItemW> pCharacteristicsInfo(new GeneralTreeItemW(L"特性"));
        WORD wBit = 1;
        LPCWSTR lpCharacteristicsName[] = {
            L"文件不包含基址重定位信息",
            L"文件是可执行的",
            L"不存在行信息",
            L"不存在符号信息",
            L"Agressively trim working set",
            L"应用程序可以处理大于2GB的地址",
            L"保留",
            L"Bytes of machine word are reversed",
            L"机器类型基于32位体系结构",
            L"不包含调试信息",
            L"不能从可移动盘运行",
            L"不能从网络运行",
            L"系统文件",
            L"动态链接库",
            L"文件不能在多处理器上运行",
            L"Bytes of machine word are reversed",
        };

        pNtInfo->AppendItem(pCharacteristicsInfo);
        pCharacteristicsInfo->SetValue(GetByte2String(pFileHeader->Characteristics));

        for (int i = 0; i < RTL_NUMBER_OF(lpCharacteristicsName); ++i)
        {
            if (pFileHeader->Characteristics & (wBit << i))
            {
                pCharacteristicsInfo->AppendItem(L"具体特性")->SetValue(fmtW(L"(0x%04x)%ls", wBit << i, lpCharacteristicsName[i]));
            }
        }

        return pNtInfo;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeOptHeader(PIMAGE_OPTIONAL_HEADER32 pOptHeader32, BOOL bIsX64)
    {
        shared_ptr<GeneralTreeItemW> pOptInfo(new GeneralTreeItemW(L"可选头部"));
        PIMAGE_OPTIONAL_HEADER64 pOptHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptHeader32;
        PIMAGE_DATA_DIRECTORY pImageDataDirectory = NULL;
        DWORD dwCountOfDataDirectorys = 0;
        wstring strMagic = L"未知的";
        wstring strSubSystemName;

        if (pOptHeader32->Magic == 0x10B)
        {
            strMagic = L"32位可执行文件";
        }
        else if (pOptHeader32->Magic == 0x107)
        {
            strMagic = L"ROM镜像文件";
        }
        else if (pOptHeader32->Magic == 0x20B)
        {
            strMagic = L"64位可执行文件";
        }

        switch (pOptHeader32->Subsystem)
        {
        default:
        case 0:
            strSubSystemName = L"未知子系统";
            break;

        case 1:
            strSubSystemName = L"设备驱动程序和Native Windows进程";
            break;

        case 2:
            strSubSystemName = L"Windows图形用户界面（GUI）子系统（一般程序）";
            break;

        case 3:
            strSubSystemName = L"Windows字符模式（CUI）子系统（从命令提示符启动的）";
            break;

        case 7:
            strSubSystemName = L"Posix字符模式子系统";
            break;

        case 9:
            strSubSystemName = L"Windows CE";
            break;

        case 10:
            strSubSystemName = L"可扩展固件接口（EFI）应用程序";
            break;

        case 11:
            strSubSystemName = L"带引导服务的EFI驱动程序";
            break;

        case 12:
            strSubSystemName = L"带运行时服务的EFI驱动程序";
            break;

        case 13:
            strSubSystemName = L"EFI ROM镜像";
            break;

        case 14:
            strSubSystemName = L"XBOX";
            break;
        }

        pOptInfo->AppendItem(L"魔数")->SetValue(fmtW(L"(0x%04x)%ls", pOptHeader32->Magic, strMagic.c_str()));
        m_pBasicInfo->AppendItem(L"镜像类型")->SetValue(strMagic);
        pOptInfo->AppendItem(L"链接器主版本号")->SetValue(fmtW(L"%u", (unsigned)pOptHeader32->MajorLinkerVersion));
        pOptInfo->AppendItem(L"链接器副版本号")->SetValue(fmtW(L"%u", (unsigned)pOptHeader32->MinorLinkerVersion));
        m_pBasicInfo->AppendItem(L"链接器版本")->SetValue(fmtW(L"%u.%u", (unsigned)pOptHeader32->MajorLinkerVersion, (unsigned)pOptHeader32->MinorLinkerVersion));
        pOptInfo->AppendItem(L"代码段大小")->SetValue(GetByte4String(pOptHeader32->SizeOfCode));
        pOptInfo->AppendItem(L"已初始化数大小")->SetValue(GetByte4String(pOptHeader32->SizeOfInitializedData));
        pOptInfo->AppendItem(L"未初始化数大小")->SetValue(GetByte4String(pOptHeader32->SizeOfUninitializedData));
        pOptInfo->AppendItem(L"入口点")->SetValue(GetByte4String(pOptHeader32->AddressOfEntryPoint));
        pOptInfo->AppendItem(L"代码基址")->SetValue(GetByte4String(pOptHeader32->BaseOfCode));

        if (bIsX64)
        {
            pOptInfo->AppendItem(L"镜像基址")->SetValue(GetByte8String(pOptHeader64->ImageBase));
        }
        else
        {
            pOptInfo->AppendItem(L"数据基址")->SetValue(GetByte4String(pOptHeader32->BaseOfData));
            pOptInfo->AppendItem(L"镜像基址")->SetValue(GetByte4String(pOptHeader32->ImageBase));
        }

        pOptInfo->AppendItem(L"内存对齐")->SetValue(GetByte4String(pOptHeader32->SectionAlignment));
        pOptInfo->AppendItem(L"文件对齐")->SetValue(GetByte4String(pOptHeader32->FileAlignment));
        pOptInfo->AppendItem(L"主系统的主版本号")->SetValue(GetByte2String(pOptHeader32->MajorOperatingSystemVersion));
        pOptInfo->AppendItem(L"主系统的副版本号")->SetValue(GetByte2String(pOptHeader32->MinorOperatingSystemVersion));
        m_pBasicInfo->AppendItem(L"最低操作系统版本")->SetValue(fmtW(L"%u.%u", (unsigned)pOptHeader32->MajorOperatingSystemVersion, (unsigned)pOptHeader32->MinorOperatingSystemVersion));
        pOptInfo->AppendItem(L"镜像的主版本号")->SetValue(GetByte2String(pOptHeader32->MajorImageVersion));
        pOptInfo->AppendItem(L"镜像的副版本号")->SetValue(GetByte2String(pOptHeader32->MinorImageVersion));
        pOptInfo->AppendItem(L"子系统的主版本号")->SetValue(GetByte2String(pOptHeader32->MajorSubsystemVersion));
        pOptInfo->AppendItem(L"子系统的副版本号")->SetValue(GetByte2String(pOptHeader32->MinorSubsystemVersion));
        pOptInfo->AppendItem(L"保留")->SetValue(GetByte4String(pOptHeader32->Win32VersionValue));
        pOptInfo->AppendItem(L"镜像大小")->SetValue(GetByte4String(pOptHeader32->SizeOfImage));
        pOptInfo->AppendItem(L"头大小")->SetValue(GetByte4String(pOptHeader32->SizeOfHeaders));
        pOptInfo->AppendItem(L"校验和")->SetValue(GetByte4String(pOptHeader32->CheckSum));
        pOptInfo->AppendItem(L"子系统类型")->SetValue(fmtW(L"(0x%04x)%ls", pOptHeader32->Subsystem, strSubSystemName.c_str()));
        m_pBasicInfo->AppendItem(L"子系统类型")->SetValue(strSubSystemName.c_str());

        shared_ptr<GeneralTreeItemW> pDllCharacteristics = pOptInfo->AppendItem(L"DLL特性");

        pDllCharacteristics->SetValue(GetByte2String(pOptHeader32->DllCharacteristics));

        // dll具体特性
        WORD wBit = 1;
        LPCWSTR lpCharacteristicsName[] = {
            L"保留",
            L"保留",
            L"保留",
            L"保留",
            L"保留",
            L"保留",
            L"DLL可以在加载时被重定位。",
            L"强制进行代码完整性校验。",
            L"镜像兼容于NX。",
            L"可以隔离，但并不隔离此镜像。",
            L"不使用结构化异常处理。",
            L"不绑定镜像。",
            L"保留",
            L"WDM驱动程序。",
            L"保留",
            L"可以用于终端服务器。",
        };

        for (int i = 0; i < RTL_NUMBER_OF(lpCharacteristicsName); ++i)
        {
            if (pOptHeader32->DllCharacteristics & (wBit << i))
            {
                pDllCharacteristics->AppendItem(L"具体特性")->SetValue(fmtW(L"(0x%04x)%ls", wBit << i, lpCharacteristicsName[i]));
            }
        }

        if (bIsX64)
        {
            pOptInfo->AppendItem(L"栈保留大小")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfStackReserve).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfStackReserve).c_str()));
            pOptInfo->AppendItem(L"栈提交大小")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfStackCommit).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfStackCommit).c_str()));
            pOptInfo->AppendItem(L"堆保留大小")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfHeapReserve).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfHeapReserve).c_str()));
            pOptInfo->AppendItem(L"堆提交大小")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfHeapCommit).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfHeapCommit).c_str()));

            pOptInfo->AppendItem(L"保留")->SetValue(GetByte4String(pOptHeader64->LoaderFlags));

            pImageDataDirectory = pOptHeader64->DataDirectory;
            dwCountOfDataDirectorys = pOptHeader64->NumberOfRvaAndSizes;
        }
        else
        {
            pOptInfo->AppendItem(L"栈保留大小")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfStackReserve).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfStackReserve).c_str()));
            pOptInfo->AppendItem(L"栈提交大小")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfStackCommit).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfStackCommit).c_str()));
            pOptInfo->AppendItem(L"堆保留大小")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfHeapReserve).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfHeapReserve).c_str()));
            pOptInfo->AppendItem(L"堆提交大小")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfHeapCommit).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfHeapCommit).c_str()));

            pOptInfo->AppendItem(L"保留")->SetValue(GetByte4String(pOptHeader32->LoaderFlags));

            pImageDataDirectory = pOptHeader32->DataDirectory;
            dwCountOfDataDirectorys = pOptHeader32->NumberOfRvaAndSizes;
        }

        memcpy(&m_dataDirectory, pImageDataDirectory, sizeof(m_dataDirectory));

        shared_ptr<GeneralTreeItemW> pDataDirectoryInfo = pOptInfo->AppendItem(L"数据目录表");
        LPCWSTR lpDataDirctoryNames[] = {
            L"导出表",
            L"导入表",
            L"资源表",
            L"异常表",
            L"属性证书表",
            L"基址重定位表",
            L"调试数据表",
            L"保留",
            L"全局指针",
            L"线程局部存储表",
            L"加载配置表",
            L"绑定导入查找表",
            L"导入地址表",
            L"延迟导入描述符表",
            L"CLR运行时头部",
            L"保留",
        };

        pDataDirectoryInfo->SetValue(fmtW(L"共%lu个", dwCountOfDataDirectorys));

        for (DWORD i = 0; i < min(dwCountOfDataDirectorys, RTL_NUMBER_OF(lpDataDirctoryNames)); ++i, ++pImageDataDirectory)
        {
            shared_ptr<GeneralTreeItemW> p = pDataDirectoryInfo->AppendItem(lpDataDirctoryNames[i]);

            p->AppendItem(L"地址")->SetValue(GetByte4String(pImageDataDirectory->VirtualAddress));
            p->AppendItem(L"大小")->SetValue(GetByte4String(pImageDataDirectory->Size));
        }

        return pOptInfo;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeImageSectionHeader(PIMAGE_SECTION_HEADER pImageSectionHeader)
    {
        WCHAR szSectionName[IMAGE_SIZEOF_SHORT_NAME + 1] = L"";

        for (int i = 0; i < RTL_NUMBER_OF(pImageSectionHeader->Name); ++i)
        {
            szSectionName[i] = pImageSectionHeader->Name[i];
        }

        shared_ptr<GeneralTreeItemW> pSectionInfo(new GeneralTreeItemW(L"节"));

        pSectionInfo->SetValue(szSectionName);
        pSectionInfo->AppendItem(L"名字")->SetValue(szSectionName);
        pSectionInfo->AppendItem(L"内存中节的大小")->SetValue(GetByte4String(pImageSectionHeader->Misc.VirtualSize));
        pSectionInfo->AppendItem(L"内存中节的偏移量")->SetValue(GetByte4String(pImageSectionHeader->VirtualAddress));
        pSectionInfo->AppendItem(L"文件中已初始化的大小")->SetValue(GetByte4String(pImageSectionHeader->SizeOfRawData));
        pSectionInfo->AppendItem(L"数据起始的文件偏移")->SetValue(GetByte4String(pImageSectionHeader->PointerToRawData));
        pSectionInfo->AppendItem(L"重定位项开头的文件指针")->SetValue(GetByte4String(pImageSectionHeader->PointerToRelocations));
        pSectionInfo->AppendItem(L"保留")->SetValue(GetByte4String(pImageSectionHeader->PointerToLinenumbers));
        pSectionInfo->AppendItem(L"重定位项的个数")->SetValue(GetByte2String(pImageSectionHeader->NumberOfRelocations));
        pSectionInfo->AppendItem(L"保留")->SetValue(GetByte2String(pImageSectionHeader->NumberOfLinenumbers));

        shared_ptr<GeneralTreeItemW> pSectionCharacteristics = pSectionInfo->AppendItem(L"节特性");

        pSectionCharacteristics->SetValue(GetByte4String(pImageSectionHeader->Characteristics));

        struct
        {
            int nBitIndex;
            LPCWSTR lpCharacteristic;
        } sectionCharacteristics[] = {
            {6, L"代码块", },
            {7, L"已初始化", },
            {8, L"未初始化", },
            {16, L"此节包含通过全局指针来引用的数据", },
            {25, L"包含扩展的重定位信息", },
            {26, L"可丢弃", },
            {27, L"不可缓存", },
            {28, L"不可交换", },
            {29, L"已共享", },
            {30, L"可执行", },
            {31, L"可读", },
            {32, L"可写", },
        };

        for (int i = 0; i < RTL_NUMBER_OF(sectionCharacteristics); ++i)
        {
            DWORD dwMask = 1 << (sectionCharacteristics[i].nBitIndex - 1);

            if (pImageSectionHeader->Characteristics & dwMask)
            {
                pSectionCharacteristics->AppendItem(L"具体特性")->SetValue(fmtW(L"(0x%08x)%ls", dwMask, sectionCharacteristics[i].lpCharacteristic).c_str());
            }
        }

        return pSectionInfo;
    }

    shared_ptr<GeneralTreeItemW> GetExportDirectoryDetailInformation(PIMAGE_EXPORT_DIRECTORY pExportDirectory)
    {
        shared_ptr<GeneralTreeItemW> pInfo(new GeneralTreeItemW(L"原始信息"));

        pInfo->AppendItem(L"特性")->SetValue(GetByte4String(pExportDirectory->Characteristics));
        pInfo->AppendItem(L"时间戳")->SetValue(GetTimeStampString(pExportDirectory->TimeDateStamp));
        pInfo->AppendItem(L"主版本")->SetValue(GetByte2String(pExportDirectory->MajorVersion));
        pInfo->AppendItem(L"副版本")->SetValue(GetByte2String(pExportDirectory->MinorVersion));
        pInfo->AppendItem(L"模块名称地址")->SetValue(GetByte4String(pExportDirectory->Name));
        pInfo->AppendItem(L"导出函数的起始序数值")->SetValue(fmtW(L"%lu", pExportDirectory->Base));
        pInfo->AppendItem(L"导出函数数量")->SetValue(fmtW(L"%lu", pExportDirectory->NumberOfFunctions));
        pInfo->AppendItem(L"按名字导出的函数的数量")->SetValue(fmtW(L"%lu", pExportDirectory->NumberOfNames));
        pInfo->AppendItem(L"函数地址表地址")->SetValue(GetByte4String(pExportDirectory->AddressOfFunctions));
        pInfo->AppendItem(L"函数名字表地址")->SetValue(GetByte4String(pExportDirectory->AddressOfNames));
        pInfo->AppendItem(L"函数序号表地址")->SetValue(GetByte4String(pExportDirectory->AddressOfNameOrdinals));

        return pInfo;
    }

    struct ExportEntry
    {
        wstring strName;
        WORD wOrd;
        DWORD dwAddress;
        wstring strForword;
    };

    shared_ptr<GeneralTreeItemW> GetExportDirectoryFunctionList(PIMAGE_EXPORT_DIRECTORY pExportDirectory)
    {
        map<WORD, ExportEntry> mapExportEntries;
        shared_ptr<GeneralTreeItemW> pInfo(new GeneralTreeItemW(GetAnsiString(m_lpBuffer + RvaToVa(pExportDirectory->Name))));

        DWORD dwAddressOfFunctions = RvaToVa(pExportDirectory->AddressOfFunctions);
        DWORD dwAddressOfNames = RvaToVa(pExportDirectory->AddressOfNames);
        DWORD dwAddressOfNameOrdinals = RvaToVa(pExportDirectory->AddressOfNameOrdinals);

        LPDWORD lpAddressOfFunctions = (LPDWORD)(m_lpBuffer + dwAddressOfFunctions);            // 全部函数地址
        LPDWORD lpAddressOfNames = (LPDWORD)(m_lpBuffer + dwAddressOfNames);                    // 名字地址
        LPWORD lpAddressOfNameOrdinals = (LPWORD)(m_lpBuffer + dwAddressOfNameOrdinals);        // 名字地址中每一项对应在全部函数地址的index

        // 初始化map，并确认ord
        for (WORD i = 0; i < pExportDirectory->NumberOfFunctions; ++i)
        {
            DWORD dwAddress = lpAddressOfFunctions[i];

            if (dwAddress != 0)
            {
                mapExportEntries[i].dwAddress = dwAddress;
                mapExportEntries[i].wOrd = i + (WORD)pExportDirectory->Base;
            }
        }

        // 给具名的函数命名
        for (DWORD i = 0; i < pExportDirectory->NumberOfNames; ++i)
        {
            DWORD dwNameAddress = RvaToVa(lpAddressOfNames[i]);
            const char *lpName = m_lpBuffer + dwNameAddress;
            WORD wIndex = lpAddressOfNameOrdinals[i];

            if (mapExportEntries.count(wIndex) > 0)
            {
                mapExportEntries[wIndex].strName = GetAnsiString(lpName);
            }
        }

        // 找出forword的函数
        for (map<WORD, ExportEntry>::iterator it = mapExportEntries.begin(); it != mapExportEntries.end(); ++it)
        {
            // if (it->second.dwAddress >= m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
        }

        // 生成树
        map<WORD, shared_ptr<GeneralTreeItemW> > mapAllFunctions;   // 全部节点
        map<wstring, WORD> mapExportEntriesByName;                  // 以名字排序的
        map<WORD, WORD> mapExportEntriesByOrd;                      // 以序号排序的

        for (map<WORD, ExportEntry>::iterator it = mapExportEntries.begin(); it != mapExportEntries.end(); ++it)
        {
            const ExportEntry &entry = it->second;
            shared_ptr<GeneralTreeItemW> pFunction;

            if (!entry.strName.empty())
            {
                mapExportEntriesByName[entry.strName] = it->first;
                pFunction.reset(new GeneralTreeItemW(entry.strName));
            }
            else
            {
                mapExportEntriesByOrd[entry.wOrd] = it->first;
                pFunction.reset(new GeneralTreeItemW(fmtW(L"#%u", entry.wOrd)));
            }

            mapAllFunctions[it->first] = pFunction;

            pFunction->AppendItem(L"序号")->SetValue(fmtW(L"%u", entry.wOrd));
            pFunction->AppendItem(L"地址")->SetValue(GetByte4String(entry.dwAddress));

            if (!entry.strForword.empty())
            {
                pFunction->AppendItem(L"重定向至")->SetValue(entry.strForword);
            }
        }

        for (map<WORD, WORD>::const_iterator it = mapExportEntriesByOrd.begin(); it != mapExportEntriesByOrd.end(); ++it)
        {
            pInfo->AppendItem(mapAllFunctions[it->second]);
        }

        for (map<wstring, WORD>::const_iterator it = mapExportEntriesByName.begin(); it != mapExportEntriesByName.end(); ++it)
        {
            pInfo->AppendItem(mapAllFunctions[it->second]);
        }

        return pInfo;
    }

    shared_ptr<GeneralTreeItemW> GetPeInformationFromBuffer()
    {
        shared_ptr<GeneralTreeItemW> pPeInfo(new GeneralTreeItemW(L"pe信息"));

        LPCSTR lpRestBuffer = m_lpBuffer;
        DWORD dwRestSize = m_dwBufferSize;

        if (dwRestSize < sizeof(IMAGE_DOS_HEADER))
        {
            HALT;
        }

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpRestBuffer;
        pPeInfo->AppendItem(AnalyzeDosHeader(pDosHeader));

        if (dwRestSize < (DWORD)pDosHeader->e_lfanew)
        {
            HALT;
        }

        // nt头
        lpRestBuffer += pDosHeader->e_lfanew;
        dwRestSize -= pDosHeader->e_lfanew;

        PIMAGE_NT_HEADERS32 pNtHeader32 = (PIMAGE_NT_HEADERS32)lpRestBuffer;
        PIMAGE_NT_HEADERS64 pNtHeader64 = (PIMAGE_NT_HEADERS64)lpRestBuffer;

        if (dwRestSize < min(sizeof(IMAGE_NT_HEADERS32), sizeof(IMAGE_NT_HEADERS64)))
        {
            HALT;
        }

        pPeInfo->AppendItem(AnalyzeNtHeaderWithoutOptHeader(pNtHeader32));

        // 可选头
        if (pNtHeader32->FileHeader.Machine == 0x14c)
        {
            pPeInfo->AppendItem(AnalyzeOptHeader(&pNtHeader32->OptionalHeader, FALSE));
        }
        else if (pNtHeader32->FileHeader.Machine == 0x8664)
        {
            pPeInfo->AppendItem(AnalyzeOptHeader(&pNtHeader32->OptionalHeader, TRUE));
        }
        else
        {
            // do nothing
        }

        // 节
        m_pImageSectionHeader = IMAGE_FIRST_SECTION(pNtHeader32);

        if ((ULONG_PTR)(m_pImageSectionHeader + m_nNumberOfSections) > (ULONG_PTR)(m_lpBuffer + m_dwBufferSize))
        {
            HALT;
        }

        shared_ptr<GeneralTreeItemW> pSections = pPeInfo->AppendItem(L"节");

        for (WORD i = 0; i < m_nNumberOfSections; ++i)
        {
            pSections->AppendItem(AnalyzeImageSectionHeader(m_pImageSectionHeader + i));
        }

        // 导出表 IMAGE_DIRECTORY_ENTRY_EXPORT
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size > 0)
        {
            UINT dwOffset = RvaToVa(m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
            DWORD dwSize = m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

            if (dwOffset != 0 && dwSize >= sizeof(IMAGE_EXPORT_DIRECTORY))
            {
                PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(m_lpBuffer + dwOffset);
                shared_ptr<GeneralTreeItemW> pExportTable = pPeInfo->AppendItem(L"导出表");

                pExportTable->AppendItem(GetExportDirectoryDetailInformation(pExportDirectory));
                pExportTable->AppendItem(GetExportDirectoryFunctionList(pExportDirectory));
            }
        }

        // 导入表 IMAGE_DIRECTORY_ENTRY_IMPORT
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0)
        {
            
        }

        // 数字签名区域 IMAGE_DIRECTORY_ENTRY_SECURITY
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size > 0)
        {
            m_pBasicInfo->AppendItem(L"数字签名区域")->SetValue(L"存在");
        }

        // 调试信息区域 IMAGE_DIRECTORY_ENTRY_DEBUG
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size > 0)
        {
            m_pBasicInfo->AppendItem(L"调试信息区域")->SetValue(L"存在");
        }

        return pPeInfo;
    }

    void Analyze()
    {
        m_pResult = shared_ptr<GeneralTreeItemW>(new GeneralTreeItemW(m_szFilePath.c_str()));

        if (PathFileExistsW(m_szFilePath.c_str()))
        {
            CReadonlyFileMapping fm(m_szFilePath.c_str());

            m_lpBuffer = (LPCSTR)fm.GetBuffer();
            m_dwBufferSize = fm.GetSize();

            if (m_lpBuffer != NULL && m_dwBufferSize > 0)
            {
                try
                {
                    m_pBasicInfo = m_pResult->AppendItem(L"摘要信息");
                    m_pBasicInfo->AppendItem(L"文件大小")->SetValue(GetAnsiString(GetFriendlyFileSizeA(m_dwBufferSize).c_str()));

                    m_pResult->AppendItem(GetPeInformationFromBuffer());
                }
                catch (int)
                {
                    m_pResult->SetValue(L"解析失败");
                }
            }
            else
            {
                m_pResult->SetValue(L"文件映射失败");
            }
        }
        else
        {
            m_pResult->SetValue(L"文件不存在");
        }
    }

private:
    wstring m_szFilePath;
    shared_ptr<GeneralTreeItemW> m_pResult;
    shared_ptr<GeneralTreeItemW> m_pBasicInfo;
    LPCSTR m_lpBuffer;
    DWORD m_dwBufferSize;
    PIMAGE_SECTION_HEADER m_pImageSectionHeader;
    WORD m_nNumberOfSections;

    IMAGE_DATA_DIRECTORY m_dataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};

std::string GetPeInformation(const wchar_t *lpPeFilePath)
{
    return PeInformationAnalyzer(lpPeFilePath).GetResult()->GenerateXmlUtf8();
}

////////////////////////////////////////////////////////////////////////////////////////////
#include "resource.h"

class CTestGetPeInformationDialog
{
#define TESTGETPEINFORMATION_OBJECT_PROP_NAME TEXT("__TestGetPeInformationObject")

public:
    CTestGetPeInformationDialog(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hParent)
        : m_hInstance(hInstance)
        , m_lpTemplate(lpTemplate)
        , m_hwndDlg(NULL)
        , m_hParent(hParent)
    {
    }

    INT_PTR DoModel()
    {
        return DialogBoxParam(m_hInstance, m_lpTemplate, m_hParent, TestGetPeInformationDialogProc, (LPARAM)this);
    }

private:
    static INT_PTR CALLBACK TestGetPeInformationDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_INITDIALOG)
        {
            SetProp(hwndDlg, TESTGETPEINFORMATION_OBJECT_PROP_NAME, (HANDLE)lParam);
        }

        CTestGetPeInformationDialog *pTestGetPeInformationDialog = (CTestGetPeInformationDialog *)GetProp(hwndDlg, TESTGETPEINFORMATION_OBJECT_PROP_NAME);

        if (pTestGetPeInformationDialog != NULL)
        {
            return pTestGetPeInformationDialog->TestGetPeInformationDialogPrivateProc(hwndDlg, uMsg, wParam, lParam);
        }

        switch (uMsg)
        {
        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            break;

        default:
            break;
        }

        return FALSE;
    }

    INT_PTR CALLBACK TestGetPeInformationDialogPrivateProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_INITDIALOG:{
            m_hwndDlg = hwndDlg;

            WCHAR szSelfPath[MAX_PATH];

            GetModuleFileName(m_hInstance, szSelfPath, RTL_NUMBER_OF(szSelfPath));
            LoadPeFile(szSelfPath);

            break;}

        case WM_CLOSE:
            RemoveProp(hwndDlg, TESTGETPEINFORMATION_OBJECT_PROP_NAME);
            EndDialog(hwndDlg, 0);
            break;

        case WM_DROPFILES:
            if (wParam != 0)
            {
                HDROP hDrop = (HDROP)wParam;
                WCHAR szFilePath[MAX_PATH] = L"";

                DragQueryFileW(hDrop, 0, szFilePath, RTL_NUMBER_OF(szFilePath));
                DragFinish(hDrop);

                if (PathFileExistsW(szFilePath))
                {
                    LoadPeFile(szFilePath);
                }
            }
            return TRUE;

        case WM_SHOWWINDOW:
            if (!wParam)
            {
                break;
            }

        case WM_SIZE:
            ResizeTreeControl();
            break;

        default:
            break;
        }

        return FALSE;
    }

    void ResizeTreeControl()
    {
        RECT rectClient;

        GetClientRect(m_hwndDlg, &rectClient);
        InflateRect(&rectClient, -7, -7);

        if (IsWindow(GetDlgItem(m_hwndDlg, IDC_TREE)))
        {
            MoveWindow(GetDlgItem(m_hwndDlg, IDC_TREE), rectClient.left, rectClient.top, rectClient.right - rectClient.left, rectClient.bottom - rectClient.top, TRUE);
        }
    }

    void ExpandTreeControlForLevel(HWND hControl, HTREEITEM htiBegin, int nLevel)
    {
        if (nLevel < 1)
        {
            return;
        }

        if (htiBegin == NULL)
        {
            htiBegin = (HTREEITEM)SendMessage(hControl, TVM_GETNEXTITEM, TVGN_ROOT, NULL);
        }

        SendMessage(hControl, TVM_EXPAND, TVE_EXPAND, (LPARAM)htiBegin);

        if (nLevel > 1)
        {
            HTREEITEM hChild = (HTREEITEM)SendMessage(hControl, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)htiBegin);

            while (hChild != NULL)
            {
                ExpandTreeControlForLevel(hControl, hChild, nLevel - 1);
                hChild = (HTREEITEM)SendMessage(hControl, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hChild);
            }
        }
    }

    void LoadPeFile(LPCWSTR lpFilePath)
    {
        SendDlgItemMessage(m_hwndDlg, IDC_TREE, TVM_DELETEITEM, 0, 0);
        AttachToTreeCtrlUtf8(GetPeInformation(lpFilePath), GetDlgItem(m_hwndDlg, IDC_TREE));
        ExpandTreeControlForLevel(GetDlgItem(m_hwndDlg, IDC_TREE), NULL, 2);
    }

private:
    HWND m_hwndDlg;
    HWND m_hParent;
    HINSTANCE m_hInstance;
    LPCTSTR m_lpTemplate;
};

extern HINSTANCE g_hinstDll;    //SlxCom.cpp

void CALLBACK TestShowPeInformationW(HWND hwndStub, HINSTANCE hInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    CTestGetPeInformationDialog(g_hinstDll, MAKEINTRESOURCE(IDD_TEST_PEINFORMATION_DIALOG), NULL).DoModel();
}