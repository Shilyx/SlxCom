#include <Windows.h>
#include <Gdiplus.h>
#include <Shlwapi.h>
#include "resource.h"
#pragma warning(disable: 4786)
#include <vector>

#pragma comment(lib, "Gdiplus.lib")

using namespace std;
using namespace Gdiplus;

#define WNDCLASS_NAME   TEXT("SLX_PAINT_WINDOW_20130318")
extern HINSTANCE g_hinstDll;



struct PaintDlgData
{
    vector<HBITMAP>     vectorBmps;
    DWORD               dwCurrentIndex;
    HDC                 hMemDc;
    int                 nWidth;
    int                 nHeight;
    HBITMAP             hBmpTmp;

    VOID LoadBitmap();
    VOID UnloadBitmap();
};

VOID PaintDlgData::LoadBitmap()
{
    hBmpTmp = (HBITMAP)SelectObject(hMemDc, vectorBmps.at(dwCurrentIndex));
}

VOID PaintDlgData::UnloadBitmap()
{
    SelectObject(hMemDc, hBmpTmp);
}

HBITMAP CopyBitmap(HDC hDc, const HBITMAP hSrcBmp)
{
    BITMAP bmp;

    GlobalLock(hSrcBmp);
    GetObject(hSrcBmp, sizeof(bmp), &bmp);
    GlobalUnlock(hSrcBmp);

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

static PaintDlgData *GetPaintDlgDataPointer(HWND hWindow)
{
    PaintDlgData *pData = (PaintDlgData *)GetWindowLongPtr(hWindow, GWLP_USERDATA);

    if (pData == NULL)
    {
        pData = new PaintDlgData;

        if (pData != NULL)
        {
            HDC hScreenDc = GetDC(NULL);

            pData->nWidth = GetSystemMetrics(SM_CXSCREEN);
            pData->nHeight = GetSystemMetrics(SM_CYSCREEN);
            pData->dwCurrentIndex = 0;
            pData->hMemDc = CreateCompatibleDC(hScreenDc);
            pData->vectorBmps.push_back(CreateCompatibleBitmap(hScreenDc, pData->nWidth, pData->nHeight));

            SelectObject(pData->hMemDc, GetStockObject(NULL_BRUSH));

            pData->LoadBitmap();
            BitBlt(pData->hMemDc, 0, 0, pData->nWidth, pData->nHeight, hScreenDc, 0, 0, SRCCOPY);
            pData->UnloadBitmap();

            ReleaseDC(NULL, hScreenDc);
        }

        SetWindowLong(hWindow, GWLP_USERDATA, (LONG_PTR)pData);
        return GetPaintDlgDataPointer(hWindow);
    }
    else
    {
        return pData;
    }
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

static HRESULT __stdcall WndProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_PAINT)
    {
        PaintDlgData *pData = GetPaintDlgDataPointer(hWindow);

        PAINTSTRUCT ps;
        HDC hPaintDc = BeginPaint(hWindow, &ps);

        pData->LoadBitmap();
        BitBlt(hPaintDc, 0, 0, pData->nWidth, pData->nHeight, pData->hMemDc, 0, 0, SRCCOPY);
        pData->UnloadBitmap();

#ifdef _DEBUG
        HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
        HPEN hPenTmp = (HPEN)SelectObject(hPaintDc, hPen);
        HBRUSH hBrushTmp = (HBRUSH)SelectObject(hPaintDc, GetStockObject(NULL_BRUSH));
        Rectangle(hPaintDc, 3, 3, 30, 30);
        SelectObject(hPaintDc, hBrushTmp);
        SelectObject(hPaintDc, hPenTmp);
        DeleteObject(hPen);
#endif

        EndPaint(hWindow, &ps);
    }
    else if (uMsg == WM_CREATE)
    {
        GetPaintDlgDataPointer(hWindow);
    }
    else if (uMsg == WM_CLOSE)
    {
        DestroyWindow(hWindow);
    }
    else if (uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
    }
    else if (uMsg == WM_MBUTTONUP)
    {
        SendMessage(hWindow, WM_SYSCOMMAND, SC_CLOSE, 0);
    }
    else if (uMsg == WM_RBUTTONUP)
    {
        HDC hDc = GetWindowDC(hWindow);
        POINT pt;

        GetCursorPos(&pt);

        Rectangle(hDc, pt.x, pt.y, 100, 100);
        ReleaseDC(hWindow, hDc);
    }

    return DefWindowProc(hWindow, uMsg, wParam, lParam);
}

VOID SlxPaint(HWND hwndStub, HINSTANCE hAppInstance, LPCSTR lpComLine, int nShow)
{
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSEX wcex = {sizeof(wcex)};

    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.hInstance = g_hinstDll;
    wcex.lpszClassName = WNDCLASS_NAME;
    wcex.hCursor = LoadCursor(g_hinstDll, MAKEINTRESOURCE(IDC_POINTER));
    wcex.lpfnWndProc = WndProc;

    if (RegisterClassEx(&wcex))
    {
        HWND hWindow = CreateWindowEx(
            WS_EX_TOOLWINDOW,
            WNDCLASS_NAME,
            NULL,
            WS_POPUP,
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            NULL,
            NULL,
            NULL,
            NULL
            );

        if (IsWindow(hWindow))
        {
            ShowWindow(hWindow, SW_SHOW);
            UpdateWindow(hWindow);

            MSG msg;

            while (TRUE)
            {
                int nRet = GetMessage(&msg, NULL, 0, 0);

                if (nRet <= 0)
                {
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    GdiplusShutdown(gdiplusToken);
}