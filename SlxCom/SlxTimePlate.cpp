#include "SlxTimePlate.h"
#include <Windows.h>
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "resource.h"

#define CLASS_NAME TEXT("__SlxTimePlateClass_20150912")

static volatile LONG g_bEnabled = FALSE;
static HANDLE g_hWorkThread = NULL;
static int gs_nIntervalMinutes = 60;        // 每几分钟显示一次

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

enum
{
    TI_MAIN = 12,
};

class CImageBuilder
{
#define MAX_PIXEL 2000
#define DEFALUT_BK_COLOR (RGB(91, 91, 91))
public:
    CImageBuilder(int nSize)
    {
        HDC hScreenDc = GetDC(NULL);

        m_hMemDc = CreateCompatibleDC(hScreenDc);
        m_hMemBmp = CreateCompatibleBitmap(hScreenDc, MAX_PIXEL, MAX_PIXEL);
        m_hOldBmp = SelectObject(m_hMemDc, m_hMemBmp);

        LOGFONT logfont;

        ZeroMemory(&logfont, sizeof(logfont));
        logfont.lfHeight = -MulDiv(nSize, GetDeviceCaps(m_hMemDc, LOGPIXELSY), 72);
        lstrcpyn(logfont.lfFaceName, TEXT("DS-Digital"), RTL_NUMBER_OF(logfont.lfFaceName));

        m_hMemFont = CreateFontIndirect(&logfont);
        m_hOldFont = SelectObject(m_hMemDc, m_hMemFont);

        SetBkColor(m_hMemDc, DEFALUT_BK_COLOR);
        SetTextColor(m_hMemDc, RGB(255, 255, 255));

        HBRUSH hBkBrush = CreateSolidBrush(DEFALUT_BK_COLOR);
        RECT rect = {0, 0, MAX_PIXEL, MAX_PIXEL};
        FillRect(m_hMemDc, &rect, hBkBrush);
        DeleteObject(hBkBrush);

        ReleaseDC(NULL, hScreenDc);
    }

    ~CImageBuilder()
    {
        SelectObject(m_hMemDc, m_hOldFont);
        SelectObject(m_hMemDc, m_hOldBmp);

        DeleteObject(m_hMemFont);
        DeleteObject(m_hMemBmp);
        DeleteDC(m_hMemDc);
    }

public:
    void DrawTextOnWindow(HWND hWnd, LPCTSTR lpText)
    {
        RECT rect;
        HDC hWindowDc = GetDC(hWnd);

        if (hWindowDc == NULL)
        {
            return;
        }

        TextOut(m_hMemDc, 0, 0, lpText, lstrlen(lpText));
        //StretchBlt(hWindowDc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, m_hMemDc, 0, 0, dwWidth, dwHeight, SRCCOPY);
        GetClientRect(hWnd, &rect);
        BitBlt(hWindowDc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, m_hMemDc, 0, 0, SRCCOPY);

        ReleaseDC(hWnd, hWindowDc);
    }

private:
    HDC m_hMemDc;
    HBITMAP m_hMemBmp;
    HGDIOBJ m_hOldBmp;
    HFONT m_hMemFont;
    HGDIOBJ m_hOldFont;
};

CImageBuilder *pImageBuilder = NULL;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        SetTimer(hWnd, TI_MAIN, 100, NULL);
        pImageBuilder = new CImageBuilder(72);
        return 0;

    case WM_RBUTTONDOWN:
    case WM_CLOSE:
        KillTimer(hWnd, TI_MAIN);
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        if (wParam == TI_MAIN)
        {
            SYSTEMTIME stNow;

            GetLocalTime(&stNow);

            // 控制是否显示窗口
            BOOL bShouldShow = g_bEnabled && (stNow.wMinute % gs_nIntervalMinutes == 0 || (stNow.wMinute + 1) % gs_nIntervalMinutes == 0);
            BOOL bIsWindowVisible = IsWindowVisible(hWnd);

            if (bIsWindowVisible && !bShouldShow)
            {
                ShowWindow(hWnd, SW_HIDE);
            }
            else if (!bIsWindowVisible && bShouldShow)
            {
                ShowWindow(hWnd, SW_SHOW);
            }

            // 控制展示时间
            if (IsWindowVisible(hWnd))
            {
                TCHAR szTimeString[128];
                TCHAR szTimeStringAdjusted[RTL_NUMBER_OF(szTimeString) * 2];
                TCHAR ch = TEXT(':');

                // 非等宽字体，闪烁冒号时字符串长度频繁变化
//                 if (stNow.wMilliseconds > 500)
//                 {
//                     ch = TEXT(' ');
//                 }

                wnsprintf(szTimeString, RTL_NUMBER_OF(szTimeString), TEXT(" %02u %c %02u %c %02u "), stNow.wHour, ch, stNow.wMinute, ch, stNow.wSecond);

                for (int i = 0, j = 0; i < RTL_NUMBER_OF(szTimeString); ++i, ++j)
                {
                    if (szTimeString[i] == TEXT('1'))
                    {
                        szTimeStringAdjusted[j++] = TEXT(' ');
                    }

                    szTimeStringAdjusted[j] = szTimeString[i];

                    if (szTimeStringAdjusted[j] == TEXT('\0'))
                    {
                        break;
                    }
                }

                pImageBuilder->DrawTextOnWindow(hWnd, szTimeStringAdjusted);
            }
        }
        break;

    case WM_SHOWWINDOW:
        if (wParam)
        {
            const int nDefaultDistance = 38;
            RECT rectWnd;
            int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
            int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

            GetWindowRect(hWnd, &rectWnd);

            SetWindowPos(hWnd, NULL, nScreenWidth - (rectWnd.right - rectWnd.left) - nDefaultDistance, nDefaultDistance, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        }
        break;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static DWORD CALLBACK WorkProc(LPVOID lpParam)
{
    DWORD dwResSize = 0;
    LPCVOID lpResBuffer = GetResourceBuffer(g_hinstDll, TEXT("RT_FILE"), MAKEINTRESOURCE(IDR_DIGIB_FONT), &dwResSize);

    if (lpResBuffer != NULL && dwResSize != 0)
    {
        DWORD dwCountInstalled = 0;
        HANDLE hFontHandle = AddFontMemResourceEx((LPVOID)lpResBuffer, dwResSize, NULL, &dwCountInstalled);

        if (hFontHandle == NULL)
        {
            return 0;
        }
    }

    WNDCLASSEX wcex = {sizeof(wcex)};

    wcex.lpszClassName  = CLASS_NAME;
    wcex.hInstance      = g_hinstDll;
    wcex.lpfnWndProc    = WndProc;
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);

    if (RegisterClassEx(&wcex))
    {
        HWND hWindow = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
            CLASS_NAME,
            CLASS_NAME,
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
            100,
            100,
            480,
            99,
            NULL,
            NULL,
            g_hinstDll,
            NULL
            );

        if (IsWindow(hWindow))
        {
            SetLayeredWindowAttributes(hWindow, 0, 144, LWA_ALPHA);

//             ShowWindow(hWindow, SW_SHOW);
//             UpdateWindow(hWindow);

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

    return 0;
}

void EnableTimePlate(bool bEnable)
{
    if (bEnable && g_hWorkThread == NULL)
    {
        g_hWorkThread = CreateThread(NULL, 0, WorkProc, NULL, 0, NULL);
    }

    InterlockedExchange(&g_bEnabled, !!bEnable);
}

void SetTimePlateOption(int nIntervalMinutes)
{
    gs_nIntervalMinutes = nIntervalMinutes;
}
