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
        shared_ptr<GeneralTreeItemW> pDosInfo(new GeneralTreeItemW(L"DOSͷ��"));

        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        {
            HALT;
        }

        pDosInfo->AppendItem(L"ǩ��")->SetValue(fmtW(L"%c%c", ((const char *)&pDosHeader->e_magic)[0], ((const char *)&pDosHeader->e_magic)[1]));
        pDosInfo->AppendItem(L"�����ֽ�")->SetValue(GetByte2String(pDosHeader->e_cblp));
        pDosInfo->AppendItem(L"ҳ")->SetValue(GetByte2String(pDosHeader->e_cp));
        pDosInfo->AppendItem(L"�ض�λ��Ŀ")->SetValue(GetByte2String(pDosHeader->e_crlc));
        pDosInfo->AppendItem(L"��ͷ��С")->SetValue(GetByte2String(pDosHeader->e_cparhdr));
        pDosInfo->AppendItem(L"��С����")->SetValue(GetByte2String(pDosHeader->e_minalloc));
        pDosInfo->AppendItem(L"�������")->SetValue(GetByte2String(pDosHeader->e_maxalloc));
        pDosInfo->AppendItem(L"��ʼSS")->SetValue(GetByte2String(pDosHeader->e_ss));
        pDosInfo->AppendItem(L"��ʼSP")->SetValue(GetByte2String(pDosHeader->e_sp));
        pDosInfo->AppendItem(L"У����")->SetValue(GetByte2String(pDosHeader->e_csum));
        pDosInfo->AppendItem(L"��ʼIP")->SetValue(GetByte2String(pDosHeader->e_ss));
        pDosInfo->AppendItem(L"��ʼCS")->SetValue(GetByte2String(pDosHeader->e_cs));
        pDosInfo->AppendItem(L"�ض�λ��")->SetValue(GetByte2String(pDosHeader->e_lfarlc));
        pDosInfo->AppendItem(L"����")->SetValue(GetByte2String(pDosHeader->e_ovno));
        pDosInfo->AppendItem(L"PEͷƫ��")->SetValue(GetByte4String(pDosHeader->e_lfanew));

        return pDosInfo;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeNtHeaderWithoutOptHeader(PIMAGE_NT_HEADERS32 pNtHeader)
    {
        struct
        {
            WORD nMachine;
            LPCWSTR lpMachineName;
        } machineNames[] = {
            {0x0, L"�������κ����ʹ�����"},
            {0x1d3, L"Matsushita AM33������"},
            {0x8664, L"x64������"},
            {0x1c0, L"ARMСβ������"},
            {0xebc, L"EFI�ֽ��봦����"},
            {0x14c, L"Intel 386���̴�����������ݴ�����"},
            {0x200, L"Intel Itanium������"},
            {0x9041, L"Mitsubishi M32RСβ������"},
            {0x266, L"MIPS16������"},
            {0x366, L"��FPU��MIPS������"},
            {0x466, L"��FPU��MIPS16������"},
            {0x1f0, L"PowerPCСβ������"},
            {0x1f1, L"����������֧�ֵ�PowerPC������"},
            {0x166, L"MIPSСβ������"},
            {0x1a2, L"Hitachi SH3������"},
            {0x1a3, L"Hitachi SH3 DSP������"},
            {0x1a6, L"Hitachi SH4������"},
            {0x1a6, L"Hitachi SH5������"},
            {0x1c2, L"Thumb������"},
            {0x169, L"MIPSСβWCE v2������"},
        };

        shared_ptr<GeneralTreeItemW> pNtInfo(new GeneralTreeItemW(L"NTͷ��"));
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

        pNtInfo->AppendItem(L"ǩ��")->SetValue(fmtW(L"%hs", szSignature));
        pNtInfo->AppendItem(L"��������")->SetValue(fmtW(L"%ls(0x%04x)", strMachineName.c_str(), pFileHeader->Machine));
        pNtInfo->AppendItem(L"������")->SetValue(GetByte2String(pFileHeader->NumberOfSections));
        pNtInfo->AppendItem(L"ʱ���")->SetValue(GetTimeStampString(pFileHeader->TimeDateStamp));
        m_pBasicInfo->AppendItem(L"����ʱ��")->SetValue(GetTimeStampString(pFileHeader->TimeDateStamp));
        pNtInfo->AppendItem(L"���ű�")->SetValue(GetByte4String(pFileHeader->PointerToSymbolTable));
        pNtInfo->AppendItem(L"������")->SetValue(GetByte4String(pFileHeader->NumberOfSymbols));
        pNtInfo->AppendItem(L"��ѡ��ͷ��С")->SetValue(GetByte2String(pFileHeader->SizeOfOptionalHeader));

        // ���Ժ;�������
        shared_ptr<GeneralTreeItemW> pCharacteristicsInfo(new GeneralTreeItemW(L"����"));
        WORD wBit = 1;
        LPCWSTR lpCharacteristicsName[] = {
            L"�ļ���������ַ�ض�λ��Ϣ",
            L"�ļ��ǿ�ִ�е�",
            L"����������Ϣ",
            L"�����ڷ�����Ϣ",
            L"Agressively trim working set",
            L"Ӧ�ó�����Դ������2GB�ĵ�ַ",
            L"����",
            L"Bytes of machine word are reversed",
            L"�������ͻ���32λ��ϵ�ṹ",
            L"������������Ϣ",
            L"���ܴӿ��ƶ�������",
            L"���ܴ���������",
            L"ϵͳ�ļ�",
            L"��̬���ӿ�",
            L"�ļ������ڶദ����������",
            L"Bytes of machine word are reversed",
        };

        pNtInfo->AppendItem(pCharacteristicsInfo);
        pCharacteristicsInfo->SetValue(GetByte2String(pFileHeader->Characteristics));

        for (int i = 0; i < RTL_NUMBER_OF(lpCharacteristicsName); ++i)
        {
            if (pFileHeader->Characteristics & (wBit << i))
            {
                pCharacteristicsInfo->AppendItem(L"��������")->SetValue(fmtW(L"(0x%04x)%ls", wBit << i, lpCharacteristicsName[i]));
            }
        }

        return pNtInfo;
    }

    shared_ptr<GeneralTreeItemW> AnalyzeOptHeader(PIMAGE_OPTIONAL_HEADER32 pOptHeader32, BOOL bIsX64)
    {
        shared_ptr<GeneralTreeItemW> pOptInfo(new GeneralTreeItemW(L"��ѡͷ��"));
        PIMAGE_OPTIONAL_HEADER64 pOptHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptHeader32;
        PIMAGE_DATA_DIRECTORY pImageDataDirectory = NULL;
        DWORD dwCountOfDataDirectorys = 0;
        wstring strMagic = L"δ֪��";
        wstring strSubSystemName;

        if (pOptHeader32->Magic == 0x10B)
        {
            strMagic = L"32λ��ִ���ļ�";
        }
        else if (pOptHeader32->Magic == 0x107)
        {
            strMagic = L"ROM�����ļ�";
        }
        else if (pOptHeader32->Magic == 0x20B)
        {
            strMagic = L"64λ��ִ���ļ�";
        }

        switch (pOptHeader32->Subsystem)
        {
        default:
        case 0:
            strSubSystemName = L"δ֪��ϵͳ";
            break;

        case 1:
            strSubSystemName = L"�豸���������Native Windows����";
            break;

        case 2:
            strSubSystemName = L"Windowsͼ���û����棨GUI����ϵͳ��һ�����";
            break;

        case 3:
            strSubSystemName = L"Windows�ַ�ģʽ��CUI����ϵͳ����������ʾ�������ģ�";
            break;

        case 7:
            strSubSystemName = L"Posix�ַ�ģʽ��ϵͳ";
            break;

        case 9:
            strSubSystemName = L"Windows CE";
            break;

        case 10:
            strSubSystemName = L"����չ�̼��ӿڣ�EFI��Ӧ�ó���";
            break;

        case 11:
            strSubSystemName = L"�����������EFI��������";
            break;

        case 12:
            strSubSystemName = L"������ʱ�����EFI��������";
            break;

        case 13:
            strSubSystemName = L"EFI ROM����";
            break;

        case 14:
            strSubSystemName = L"XBOX";
            break;
        }

        pOptInfo->AppendItem(L"ħ��")->SetValue(fmtW(L"(0x%04x)%ls", pOptHeader32->Magic, strMagic.c_str()));
        m_pBasicInfo->AppendItem(L"��������")->SetValue(strMagic);
        pOptInfo->AppendItem(L"���������汾��")->SetValue(fmtW(L"%u", (unsigned)pOptHeader32->MajorLinkerVersion));
        pOptInfo->AppendItem(L"���������汾��")->SetValue(fmtW(L"%u", (unsigned)pOptHeader32->MinorLinkerVersion));
        m_pBasicInfo->AppendItem(L"�������汾")->SetValue(fmtW(L"%u.%u", (unsigned)pOptHeader32->MajorLinkerVersion, (unsigned)pOptHeader32->MinorLinkerVersion));
        pOptInfo->AppendItem(L"����δ�С")->SetValue(GetByte4String(pOptHeader32->SizeOfCode));
        pOptInfo->AppendItem(L"�ѳ�ʼ������С")->SetValue(GetByte4String(pOptHeader32->SizeOfInitializedData));
        pOptInfo->AppendItem(L"δ��ʼ������С")->SetValue(GetByte4String(pOptHeader32->SizeOfUninitializedData));
        pOptInfo->AppendItem(L"��ڵ�")->SetValue(GetByte4String(pOptHeader32->AddressOfEntryPoint));
        pOptInfo->AppendItem(L"�����ַ")->SetValue(GetByte4String(pOptHeader32->BaseOfCode));

        if (bIsX64)
        {
            pOptInfo->AppendItem(L"�����ַ")->SetValue(GetByte8String(pOptHeader64->ImageBase));
        }
        else
        {
            pOptInfo->AppendItem(L"���ݻ�ַ")->SetValue(GetByte4String(pOptHeader32->BaseOfData));
            pOptInfo->AppendItem(L"�����ַ")->SetValue(GetByte4String(pOptHeader32->ImageBase));
        }

        pOptInfo->AppendItem(L"�ڴ����")->SetValue(GetByte4String(pOptHeader32->SectionAlignment));
        pOptInfo->AppendItem(L"�ļ�����")->SetValue(GetByte4String(pOptHeader32->FileAlignment));
        pOptInfo->AppendItem(L"��ϵͳ�����汾��")->SetValue(GetByte2String(pOptHeader32->MajorOperatingSystemVersion));
        pOptInfo->AppendItem(L"��ϵͳ�ĸ��汾��")->SetValue(GetByte2String(pOptHeader32->MinorOperatingSystemVersion));
        m_pBasicInfo->AppendItem(L"��Ͳ���ϵͳ�汾")->SetValue(fmtW(L"%u.%u", (unsigned)pOptHeader32->MajorOperatingSystemVersion, (unsigned)pOptHeader32->MinorOperatingSystemVersion));
        pOptInfo->AppendItem(L"��������汾��")->SetValue(GetByte2String(pOptHeader32->MajorImageVersion));
        pOptInfo->AppendItem(L"����ĸ��汾��")->SetValue(GetByte2String(pOptHeader32->MinorImageVersion));
        pOptInfo->AppendItem(L"��ϵͳ�����汾��")->SetValue(GetByte2String(pOptHeader32->MajorSubsystemVersion));
        pOptInfo->AppendItem(L"��ϵͳ�ĸ��汾��")->SetValue(GetByte2String(pOptHeader32->MinorSubsystemVersion));
        pOptInfo->AppendItem(L"����")->SetValue(GetByte4String(pOptHeader32->Win32VersionValue));
        pOptInfo->AppendItem(L"�����С")->SetValue(GetByte4String(pOptHeader32->SizeOfImage));
        pOptInfo->AppendItem(L"ͷ��С")->SetValue(GetByte4String(pOptHeader32->SizeOfHeaders));
        pOptInfo->AppendItem(L"У���")->SetValue(GetByte4String(pOptHeader32->CheckSum));
        pOptInfo->AppendItem(L"��ϵͳ����")->SetValue(fmtW(L"(0x%04x)%ls", pOptHeader32->Subsystem, strSubSystemName.c_str()));
        m_pBasicInfo->AppendItem(L"��ϵͳ����")->SetValue(strSubSystemName.c_str());

        shared_ptr<GeneralTreeItemW> pDllCharacteristics = pOptInfo->AppendItem(L"DLL����");

        pDllCharacteristics->SetValue(GetByte2String(pOptHeader32->DllCharacteristics));

        // dll��������
        WORD wBit = 1;
        LPCWSTR lpCharacteristicsName[] = {
            L"����",
            L"����",
            L"����",
            L"����",
            L"����",
            L"����",
            L"DLL�����ڼ���ʱ���ض�λ��",
            L"ǿ�ƽ��д���������У�顣",
            L"���������NX��",
            L"���Ը��룬����������˾���",
            L"��ʹ�ýṹ���쳣����",
            L"���󶨾���",
            L"����",
            L"WDM��������",
            L"����",
            L"���������ն˷�������",
        };

        for (int i = 0; i < RTL_NUMBER_OF(lpCharacteristicsName); ++i)
        {
            if (pOptHeader32->DllCharacteristics & (wBit << i))
            {
                pDllCharacteristics->AppendItem(L"��������")->SetValue(fmtW(L"(0x%04x)%ls", wBit << i, lpCharacteristicsName[i]));
            }
        }

        if (bIsX64)
        {
            pOptInfo->AppendItem(L"ջ������С")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfStackReserve).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfStackReserve).c_str()));
            pOptInfo->AppendItem(L"ջ�ύ��С")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfStackCommit).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfStackCommit).c_str()));
            pOptInfo->AppendItem(L"�ѱ�����С")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfHeapReserve).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfHeapReserve).c_str()));
            pOptInfo->AppendItem(L"���ύ��С")->SetValue(fmtW(L"(%ls) %hs", GetByte8String(pOptHeader64->SizeOfHeapCommit).c_str(), GetFriendlyFileSizeA(pOptHeader64->SizeOfHeapCommit).c_str()));

            pOptInfo->AppendItem(L"����")->SetValue(GetByte4String(pOptHeader64->LoaderFlags));

            pImageDataDirectory = pOptHeader64->DataDirectory;
            dwCountOfDataDirectorys = pOptHeader64->NumberOfRvaAndSizes;
        }
        else
        {
            pOptInfo->AppendItem(L"ջ������С")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfStackReserve).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfStackReserve).c_str()));
            pOptInfo->AppendItem(L"ջ�ύ��С")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfStackCommit).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfStackCommit).c_str()));
            pOptInfo->AppendItem(L"�ѱ�����С")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfHeapReserve).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfHeapReserve).c_str()));
            pOptInfo->AppendItem(L"���ύ��С")->SetValue(fmtW(L"(%ls) %hs", GetByte4String(pOptHeader32->SizeOfHeapCommit).c_str(), GetFriendlyFileSizeA(pOptHeader32->SizeOfHeapCommit).c_str()));

            pOptInfo->AppendItem(L"����")->SetValue(GetByte4String(pOptHeader32->LoaderFlags));

            pImageDataDirectory = pOptHeader32->DataDirectory;
            dwCountOfDataDirectorys = pOptHeader32->NumberOfRvaAndSizes;
        }

        memcpy(&m_dataDirectory, pImageDataDirectory, sizeof(m_dataDirectory));

        shared_ptr<GeneralTreeItemW> pDataDirectoryInfo = pOptInfo->AppendItem(L"����Ŀ¼��");
        LPCWSTR lpDataDirctoryNames[] = {
            L"������",
            L"�����",
            L"��Դ��",
            L"�쳣��",
            L"����֤���",
            L"��ַ�ض�λ��",
            L"�������ݱ�",
            L"����",
            L"ȫ��ָ��",
            L"�ֲ߳̾��洢��",
            L"�������ñ�",
            L"�󶨵�����ұ�",
            L"�����ַ��",
            L"�ӳٵ�����������",
            L"CLR����ʱͷ��",
            L"����",
        };

        pDataDirectoryInfo->SetValue(fmtW(L"��%lu��", dwCountOfDataDirectorys));

        for (DWORD i = 0; i < min(dwCountOfDataDirectorys, RTL_NUMBER_OF(lpDataDirctoryNames)); ++i, ++pImageDataDirectory)
        {
            shared_ptr<GeneralTreeItemW> p = pDataDirectoryInfo->AppendItem(lpDataDirctoryNames[i]);

            p->AppendItem(L"��ַ")->SetValue(GetByte4String(pImageDataDirectory->VirtualAddress));
            p->AppendItem(L"��С")->SetValue(GetByte4String(pImageDataDirectory->Size));
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

        shared_ptr<GeneralTreeItemW> pSectionInfo(new GeneralTreeItemW(L"��"));

        pSectionInfo->SetValue(szSectionName);
        pSectionInfo->AppendItem(L"����")->SetValue(szSectionName);
        pSectionInfo->AppendItem(L"�ڴ��нڵĴ�С")->SetValue(GetByte4String(pImageSectionHeader->Misc.VirtualSize));
        pSectionInfo->AppendItem(L"�ڴ��нڵ�ƫ����")->SetValue(GetByte4String(pImageSectionHeader->VirtualAddress));
        pSectionInfo->AppendItem(L"�ļ����ѳ�ʼ���Ĵ�С")->SetValue(GetByte4String(pImageSectionHeader->SizeOfRawData));
        pSectionInfo->AppendItem(L"������ʼ���ļ�ƫ��")->SetValue(GetByte4String(pImageSectionHeader->PointerToRawData));
        pSectionInfo->AppendItem(L"�ض�λ�ͷ���ļ�ָ��")->SetValue(GetByte4String(pImageSectionHeader->PointerToRelocations));
        pSectionInfo->AppendItem(L"����")->SetValue(GetByte4String(pImageSectionHeader->PointerToLinenumbers));
        pSectionInfo->AppendItem(L"�ض�λ��ĸ���")->SetValue(GetByte2String(pImageSectionHeader->NumberOfRelocations));
        pSectionInfo->AppendItem(L"����")->SetValue(GetByte2String(pImageSectionHeader->NumberOfLinenumbers));

        shared_ptr<GeneralTreeItemW> pSectionCharacteristics = pSectionInfo->AppendItem(L"������");

        pSectionCharacteristics->SetValue(GetByte4String(pImageSectionHeader->Characteristics));

        struct
        {
            int nBitIndex;
            LPCWSTR lpCharacteristic;
        } sectionCharacteristics[] = {
            {6, L"�����", },
            {7, L"�ѳ�ʼ��", },
            {8, L"δ��ʼ��", },
            {16, L"�˽ڰ���ͨ��ȫ��ָ�������õ�����", },
            {25, L"������չ���ض�λ��Ϣ", },
            {26, L"�ɶ���", },
            {27, L"���ɻ���", },
            {28, L"���ɽ���", },
            {29, L"�ѹ���", },
            {30, L"��ִ��", },
            {31, L"�ɶ�", },
            {32, L"��д", },
        };

        for (int i = 0; i < RTL_NUMBER_OF(sectionCharacteristics); ++i)
        {
            DWORD dwMask = 1 << (sectionCharacteristics[i].nBitIndex - 1);

            if (pImageSectionHeader->Characteristics & dwMask)
            {
                pSectionCharacteristics->AppendItem(L"��������")->SetValue(fmtW(L"(0x%08x)%ls", dwMask, sectionCharacteristics[i].lpCharacteristic).c_str());
            }
        }

        return pSectionInfo;
    }

    shared_ptr<GeneralTreeItemW> GetExportDirectoryDetailInformation(PIMAGE_EXPORT_DIRECTORY pExportDirectory)
    {
        shared_ptr<GeneralTreeItemW> pInfo(new GeneralTreeItemW(L"ԭʼ��Ϣ"));

        pInfo->AppendItem(L"����")->SetValue(GetByte4String(pExportDirectory->Characteristics));
        pInfo->AppendItem(L"ʱ���")->SetValue(GetTimeStampString(pExportDirectory->TimeDateStamp));
        pInfo->AppendItem(L"���汾")->SetValue(GetByte2String(pExportDirectory->MajorVersion));
        pInfo->AppendItem(L"���汾")->SetValue(GetByte2String(pExportDirectory->MinorVersion));
        pInfo->AppendItem(L"ģ�����Ƶ�ַ")->SetValue(GetByte4String(pExportDirectory->Name));
        pInfo->AppendItem(L"������������ʼ����ֵ")->SetValue(fmtW(L"%lu", pExportDirectory->Base));
        pInfo->AppendItem(L"������������")->SetValue(fmtW(L"%lu", pExportDirectory->NumberOfFunctions));
        pInfo->AppendItem(L"�����ֵ����ĺ���������")->SetValue(fmtW(L"%lu", pExportDirectory->NumberOfNames));
        pInfo->AppendItem(L"������ַ���ַ")->SetValue(GetByte4String(pExportDirectory->AddressOfFunctions));
        pInfo->AppendItem(L"�������ֱ��ַ")->SetValue(GetByte4String(pExportDirectory->AddressOfNames));
        pInfo->AppendItem(L"������ű��ַ")->SetValue(GetByte4String(pExportDirectory->AddressOfNameOrdinals));

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

        LPDWORD lpAddressOfFunctions = (LPDWORD)(m_lpBuffer + dwAddressOfFunctions);            // ȫ��������ַ
        LPDWORD lpAddressOfNames = (LPDWORD)(m_lpBuffer + dwAddressOfNames);                    // ���ֵ�ַ
        LPWORD lpAddressOfNameOrdinals = (LPWORD)(m_lpBuffer + dwAddressOfNameOrdinals);        // ���ֵ�ַ��ÿһ���Ӧ��ȫ��������ַ��index

        // ��ʼ��map����ȷ��ord
        for (WORD i = 0; i < pExportDirectory->NumberOfFunctions; ++i)
        {
            DWORD dwAddress = lpAddressOfFunctions[i];

            if (dwAddress != 0)
            {
                mapExportEntries[i].dwAddress = dwAddress;
                mapExportEntries[i].wOrd = i + (WORD)pExportDirectory->Base;
            }
        }

        // �������ĺ�������
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

        // �ҳ�forword�ĺ���
        for (map<WORD, ExportEntry>::iterator it = mapExportEntries.begin(); it != mapExportEntries.end(); ++it)
        {
            // if (it->second.dwAddress >= m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress)
        }

        // ������
        map<WORD, shared_ptr<GeneralTreeItemW> > mapAllFunctions;   // ȫ���ڵ�
        map<wstring, WORD> mapExportEntriesByName;                  // �����������
        map<WORD, WORD> mapExportEntriesByOrd;                      // ����������

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

            pFunction->AppendItem(L"���")->SetValue(fmtW(L"%u", entry.wOrd));
            pFunction->AppendItem(L"��ַ")->SetValue(GetByte4String(entry.dwAddress));

            if (!entry.strForword.empty())
            {
                pFunction->AppendItem(L"�ض�����")->SetValue(entry.strForword);
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
        shared_ptr<GeneralTreeItemW> pPeInfo(new GeneralTreeItemW(L"pe��Ϣ"));

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

        // ntͷ
        lpRestBuffer += pDosHeader->e_lfanew;
        dwRestSize -= pDosHeader->e_lfanew;

        PIMAGE_NT_HEADERS32 pNtHeader32 = (PIMAGE_NT_HEADERS32)lpRestBuffer;
        PIMAGE_NT_HEADERS64 pNtHeader64 = (PIMAGE_NT_HEADERS64)lpRestBuffer;

        if (dwRestSize < min(sizeof(IMAGE_NT_HEADERS32), sizeof(IMAGE_NT_HEADERS64)))
        {
            HALT;
        }

        pPeInfo->AppendItem(AnalyzeNtHeaderWithoutOptHeader(pNtHeader32));

        // ��ѡͷ
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

        // ��
        m_pImageSectionHeader = IMAGE_FIRST_SECTION(pNtHeader32);

        if ((ULONG_PTR)(m_pImageSectionHeader + m_nNumberOfSections) > (ULONG_PTR)(m_lpBuffer + m_dwBufferSize))
        {
            HALT;
        }

        shared_ptr<GeneralTreeItemW> pSections = pPeInfo->AppendItem(L"��");

        for (WORD i = 0; i < m_nNumberOfSections; ++i)
        {
            pSections->AppendItem(AnalyzeImageSectionHeader(m_pImageSectionHeader + i));
        }

        // ������ IMAGE_DIRECTORY_ENTRY_EXPORT
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size > 0)
        {
            UINT dwOffset = RvaToVa(m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
            DWORD dwSize = m_dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

            if (dwOffset != 0 && dwSize >= sizeof(IMAGE_EXPORT_DIRECTORY))
            {
                PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)(m_lpBuffer + dwOffset);
                shared_ptr<GeneralTreeItemW> pExportTable = pPeInfo->AppendItem(L"������");

                pExportTable->AppendItem(GetExportDirectoryDetailInformation(pExportDirectory));
                pExportTable->AppendItem(GetExportDirectoryFunctionList(pExportDirectory));
            }
        }

        // ����� IMAGE_DIRECTORY_ENTRY_IMPORT
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0)
        {
            
        }

        // ����ǩ������ IMAGE_DIRECTORY_ENTRY_SECURITY
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size > 0)
        {
            m_pBasicInfo->AppendItem(L"����ǩ������")->SetValue(L"����");
        }

        // ������Ϣ���� IMAGE_DIRECTORY_ENTRY_DEBUG
        if (m_dataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size > 0)
        {
            m_pBasicInfo->AppendItem(L"������Ϣ����")->SetValue(L"����");
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
                    m_pBasicInfo = m_pResult->AppendItem(L"ժҪ��Ϣ");
                    m_pBasicInfo->AppendItem(L"�ļ���С")->SetValue(GetAnsiString(GetFriendlyFileSizeA(m_dwBufferSize).c_str()));

                    m_pResult->AppendItem(GetPeInformationFromBuffer());
                }
                catch (int)
                {
                    m_pResult->SetValue(L"����ʧ��");
                }
            }
            else
            {
                m_pResult->SetValue(L"�ļ�ӳ��ʧ��");
            }
        }
        else
        {
            m_pResult->SetValue(L"�ļ�������");
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