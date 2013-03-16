#include <Windows.h>
#include <Gdiplus.h>
#include <Shlwapi.h>
#include "resource.h"
#pragma warning(disable: 4786)
#include <vector>

#pragma comment(lib, "Gdiplus.lib")

using namespace std;
using namespace Gdiplus;

extern HINSTANCE g_hinstDll;

class CBitmapHandle
{
public:
    CBitmapHandle(HBITMAP hBmp);
    ~CBitmapHandle();

    CBitmapHandle &operator=(HBITMAP hBmp);
    HBITMAP operator()();

    HBITMAP             m_hBmp;
};

CBitmapHandle::CBitmapHandle(HBITMAP hBmp)
{
    m_hBmp = hBmp;
}

CBitmapHandle::~CBitmapHandle()
{
    if (m_hBmp != NULL)
    {
        DeleteObject(m_hBmp);
    }
}

CBitmapHandle &CBitmapHandle::operator=(HBITMAP hBmp)
{
    m_hBmp = hBmp;

    return *this;
}

HBITMAP CBitmapHandle::operator()()
{
    return m_hBmp;
}

class CBitmapRecord
{
public:
    CBitmapRecord(HDC hSrcDc, int nWidth, int nHeight);
    ~CBitmapRecord();

public:
    DWORD GetRecordCount;

private:
    VOID AllocBitmap();

    VOID AddBitmap(HBITMAP hBmp);
    static HBITMAP CopyBitmap(HDC hDc, HBITMAP hSrcBmp);

private:
    int                 m_nWidth;
    int                 m_nHeight;

    vector<CBitmapHandle> m_vectorBmps;
    DWORD               m_dwCurrentIndex;

    HDC                 m_hFirstDc;
    HBITMAP             m_hFirstBmp;
    HBITMAP             m_hFirstBmpTmp;
};

CBitmapRecord::CBitmapRecord(HDC hSrcDc, int nWidth, int nHeight)
{
    m_nWidth = nWidth;
    m_nHeight = nHeight;

    m_hFirstDc = CreateCompatibleDC(hSrcDc);
    m_hFirstBmp = CreateCompatibleBitmap(hSrcDc, nWidth, nHeight);
    m_hFirstBmpTmp = (HBITMAP)SelectObject(m_hFirstDc, m_hFirstBmp);

    BitBlt(m_hFirstDc, 0, 0, nWidth, nHeight, hSrcDc, 0, 0, SRCCOPY);
}

CBitmapRecord::~CBitmapRecord()
{
    SelectObject(m_hFirstDc, m_hFirstBmpTmp);

    DeleteDC(m_hFirstDc);
    DeleteObject(m_hFirstBmp);
}

VOID CBitmapRecord::AddBitmap(HBITMAP hBmp)
{
    //m_vectorBmps.
}

HBITMAP CBitmapRecord::CopyBitmap(HDC hDc, HBITMAP hSrcBmp)
{
    BITMAP bmp;

    GetObject(hSrcBmp, sizeof(bmp), &bmp);

    HDC hSrcDc = CreateCompatibleDC(hDc);
    HDC hDstDc = CreateCompatibleDC(hDc);
    HBITMAP hDstBmp = CreateCompatibleBitmap(hDc, bmp.bmWidth, bmp.bmHeight);

    HBITMAP hSrcBmpTmp = (HBITMAP)SelectObject(hSrcDc, hSrcBmp);
    HBITMAP hDstBmpTmp = (HBITMAP)SelectObject(hDstDc, hDstBmp);

    BitBlt(hDstDc, 0, 0, bmp.bmWidth, bmp.bmHeight, hSrcDc, 0, 0, SRCCOPY);

    SelectObject(hSrcDc, hSrcBmpTmp);
    SelectObject(hDstDc, hDstBmpTmp);

    DeleteDC(hSrcDc);
    DeleteDC(hDstDc);

    return hDstBmp;
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

BOOL SaveHBitmapAsJpgFile(HBITMAP hBitmap, LPCTSTR lpJpgFilePath, DWORD dwQuality)
{
    BOOL bResult = FALSE;
    IStream *pFileStream = NULL;
    CLSID clsidJpeg;
    EncoderParameters encoderParameters;
    Bitmap *pBitmap = Bitmap::FromHBITMAP(hBitmap, (HPALETTE)GetStockObject(DEFAULT_PALETTE));

    if (pBitmap != NULL)
    {
        encoderParameters.Count = 1;
        encoderParameters.Parameter[0].Guid = EncoderQuality;
        encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParameters.Parameter[0].NumberOfValues = 1;
        encoderParameters.Parameter[0].Value = &dwQuality;

        SHCreateStreamOnFile(lpJpgFilePath, STGM_WRITE | STGM_SHARE_DENY_NONE | STGM_CREATE, &pFileStream);

        if(pFileStream != NULL)
        {
            GetEncoderClsid(L"image/jpeg", &clsidJpeg);
            bResult = Ok == pBitmap->Save(pFileStream, &clsidJpeg, &encoderParameters);

            pFileStream->Release();
        }

        delete pBitmap;
    }

    return bResult;
}

static INT_PTR __stdcall PaintDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_INITDIALOG)
    {
        SetWindowPos(
            hwndDlg,
            HWND_TOPMOST,
            0,
            0,
//             GetSystemMetrics(SM_CXSCREEN),
//             GetSystemMetrics(SM_CYSCREEN),
800,
500,
            0
            );

        return TRUE;
    }
    else if (uMsg == WM_RBUTTONUP)
    {
        RECT rect;

        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);

        HDC hScreenDc = GetDC(NULL);
        HDC hMetaFile = CreateEnhMetaFile(hScreenDc, TEXT("D:\\administrator\\Desktop\\a.emf"), &rect, NULL);

        TextOut(hMetaFile, 0, 0, TEXT("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 20);

        CloseEnhMetaFile(hMetaFile);
        ReleaseDC(NULL, hScreenDc);

    }
#ifdef _DEBUG
    else if (uMsg == WM_MBUTTONDOWN)
    {
        EndDialog(hwndDlg, 0);
    }
#endif

    return FALSE;
}

VOID SlxPaint(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpComLine, int nShow)
{

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    DialogBox(g_hinstDll, MAKEINTRESOURCE(IDD_PAINT_DIALOG), hwndStub, PaintDlgProc);

    GdiplusShutdown(gdiplusToken);

}