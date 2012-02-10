#include "SlxComTools.h"
#include <wintrust.h>
#include <Softpub.h>
#include <shlwapi.h>
#include <GdiPlus.h>
#include <shlobj.h>

#pragma comment(lib, "GdiPlus.lib")
#pragma comment(lib, "Wintrust.lib")

using namespace Gdiplus;

extern HINSTANCE g_hinstDll;

BOOL IsFileSigned(LPCTSTR lpFilePath)
{
    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_FILE_INFO sWintrustFileInfo;
    WINTRUST_DATA      sWintrustData;

    memset((void*)&sWintrustFileInfo, 0x00, sizeof(WINTRUST_FILE_INFO));
    memset((void*)&sWintrustData, 0x00, sizeof(WINTRUST_DATA));

    sWintrustFileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    sWintrustFileInfo.pcwszFilePath = lpFilePath;
    sWintrustFileInfo.hFile = NULL;

    sWintrustData.cbStruct            = sizeof(WINTRUST_DATA);
    sWintrustData.dwUIChoice          = WTD_UI_NONE;
    sWintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    sWintrustData.dwUnionChoice       = WTD_CHOICE_FILE;
    sWintrustData.pFile               = &sWintrustFileInfo;
    sWintrustData.dwStateAction       = WTD_STATEACTION_VERIFY;

    return S_OK == WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &guidAction, &sWintrustData);
}

Status CallGdiplusStartup()
{
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;

    return GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

static BOOL GetEncoderClsid(LPCWSTR lpFormat, CLSID *pClsid)
{
    UINT uNum = 0;
    UINT uIndex = 0;
    UINT uSize = 0;
    HANDLE hHeap = NULL;
    ImageCodecInfo *pImageCodecInfo = NULL;

    GetImageEncodersSize(&uNum, &uSize);

    if(uSize != 0)
    {
        hHeap = GetProcessHeap();
        pImageCodecInfo = (ImageCodecInfo *)HeapAlloc(hHeap, 0, uSize);

        if(pImageCodecInfo != NULL)
        {
            GetImageEncoders(uNum, uSize, pImageCodecInfo);

            for(; uIndex < uNum; uIndex += 1)
            {
                if(lstrcmpW(pImageCodecInfo[uIndex].MimeType, lpFormat) == 0)
                {
                    *pClsid = pImageCodecInfo[uIndex].Clsid;

                    break;
                }
            }

            HeapFree(hHeap, 0, pImageCodecInfo);
        }
    }

    return uIndex < uNum;
}

BOOL GetTempJpgPath(LPCTSTR lpDescription, LPTSTR lpTempJpgPath, int nLength)
{
    static Status __unused = CallGdiplusStartup();

    Bitmap bmp(416, 355, PixelFormat32bppRGB);
    Graphics graphics(&bmp);
    SolidBrush solidBrush(Color::Blue); 
    FontFamily fontFamily(L"楷体_GB2312");
    Font font(&fontFamily, 16, FontStyleRegular, UnitPoint);

    StringFormat stringFormat;
    stringFormat.SetFormatFlags(StringFormatFlagsNoFitBlackBox);
    stringFormat.SetAlignment(StringAlignmentCenter);

    graphics.DrawString(L"AAAA", 4, &font, RectF(30, 30, 150, 200), &stringFormat, &solidBrush);

    {
        {
            CLSID clsidJpeg;
            EncoderParameters encoderParameters;
            LONG quality = 80;

            {
                GetEncoderClsid(L"image/jpeg", &clsidJpeg);

                encoderParameters.Count = 1;
                encoderParameters.Parameter[0].Guid = EncoderQuality;
                encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
                encoderParameters.Parameter[0].NumberOfValues = 1;

                encoderParameters.Parameter[0].Value = &quality;

                bmp.Save(L"C:\\a.jpg", &clsidJpeg, &encoderParameters);
            }
        }
    }

    return TRUE;
}

BOOL GetResultPath(LPCTSTR lpRarPath, LPTSTR lpResultPath, int nLength)
{
    if(nLength >= MAX_PATH)
    {
        TCHAR szJpgPath[MAX_PATH];

        lstrcpyn(szJpgPath, lpRarPath, MAX_PATH);
        PathRenameExtension(szJpgPath, TEXT(".jpg"));

        return PathYetAnotherMakeUniqueName(lpResultPath, szJpgPath, NULL, NULL);
    }

    return FALSE;
}

BOOL Combine(LPCTSTR lpFile1, LPCTSTR lpFile2, LPCTSTR lpFileNew)
{
    IStream *pStream1 = NULL;
    IStream *pStream2 = NULL;
    IStream *pStreamNew = NULL;

    if(S_OK == SHCreateStreamOnFile(lpFile1, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream1))
    {
        if(S_OK == SHCreateStreamOnFile(lpFile2, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream2))
        {
            if(S_OK == SHCreateStreamOnFile(lpFileNew, STGM_WRITE | STGM_SHARE_DENY_NONE | STGM_CREATE, &pStreamNew))
            {
                ULARGE_INTEGER uliSize1, uliSize2;
                STATSTG stat;

                pStream1->Stat(&stat, STATFLAG_NONAME);
                uliSize1.QuadPart = stat.cbSize.QuadPart;

                pStream2->Stat(&stat, STATFLAG_NONAME);
                uliSize2.QuadPart = stat.cbSize.QuadPart;

                pStream1->CopyTo(pStreamNew, uliSize1, NULL, NULL);
                pStream2->CopyTo(pStreamNew, uliSize2, NULL, NULL);

                pStreamNew->Release();
            }

            pStream2->Release();
        }

        pStream1->Release();
    }

    return TRUE;
}

BOOL CombineFile(LPCTSTR lpRarPath, LPCTSTR lpJpgPath, LPTSTR lpResultPath, int nLength)
{
    //Get jpg file path
    TCHAR szJpgPath[MAX_PATH];
    BOOL bShouldDeleteJpg = FALSE;

    if(GetFileAttributes(lpJpgPath) == INVALID_FILE_ATTRIBUTES)
    {
        GetModuleFileName(g_hinstDll, szJpgPath, MAX_PATH);
        PathAppend(szJpgPath, TEXT("\\..\\Default.jpg"));

        if(GetFileAttributes(szJpgPath) == INVALID_FILE_ATTRIBUTES)
        {
            GetTempJpgPath(PathFindFileName(lpRarPath), szJpgPath, MAX_PATH);
            bShouldDeleteJpg = TRUE;
        }
    }
    else
    {
        lstrcpyn(szJpgPath, lpJpgPath, MAX_PATH);
    }

    if(GetFileAttributes(szJpgPath) != INVALID_FILE_ATTRIBUTES)
    {
        //Generate result path
        if(GetResultPath(lpRarPath, lpResultPath, nLength))
        {
            Combine(szJpgPath, lpRarPath, lpResultPath);
        }
    }

    if(bShouldDeleteJpg)
    {
        DeleteFile(szJpgPath);
    }

    return TRUE;
}

void WINAPI T(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpszCmdLine, int nCmdShow)
{
//     for(int nIndex = 0; nIndex < 10; nIndex +=1)
//     {
//         TCHAR szPath[MAX_PATH];
// 
//         if(PathMakeUniqueName(szPath, MAX_PATH, TEXT("C:\\n\\新建abcv.rar.txt"), NULL, NULL))
//         {
//             HANDLE hFile = CreateFile(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
//             CloseHandle(hFile);
// 
//             PathUndecorate(szPath);
//             MessageBox(hwndStub, szPath, NULL, MB_TOPMOST);
//         }
//     }

    TCHAR szResultPath[MAX_PATH];
    CombineFile(TEXT("D:\\桌面\\StatusStr.rar"), NULL, szResultPath, MAX_PATH);

    MessageBox(hwndStub, NULL, NULL, MB_TOPMOST);
}