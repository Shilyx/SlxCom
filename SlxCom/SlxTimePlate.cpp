#include "SlxTimePlate.h"
#include <Windows.h>
#include <Shlwapi.h>
#include "SlxComTools.h"
#include "resource.h"

#define CLASS_NAME L"__SlxTimePlateClass_20150912"

static volatile LONG g_bEnabled = FALSE;
static HANDLE g_hWorkThread = NULL;
static int gs_nIntervalMinutes = 60;        // 每几分钟显示一次

extern HINSTANCE g_hinstDll;    // SlxCom.cpp

enum
{
    TI_MAIN = 12,
    TI_MOUSE_MONITOR,
};

class CImageBuilder
{
#define MAX_PIXEL 2000
#define DEFALUT_BK_COLOR (RGB(91, 91, 91))
public:
    CImageBuilder(int nSize)
        : m_nHour(100)
        , m_nMinute(100)
        , m_nSecond(100)
        , m_nTextWidth(488)
        , m_nTextHeight(99)
    {
        HDC hScreenDc = GetDC(NULL);

        m_hMemDc = CreateCompatibleDC(hScreenDc);
        m_hMemBmp = CreateCompatibleBitmap(hScreenDc, MAX_PIXEL, MAX_PIXEL);
        m_hOldBmp = SelectObject(m_hMemDc, m_hMemBmp);

        LOGFONTW logfont;

        ZeroMemory(&logfont, sizeof(logfont));
        logfont.lfHeight = -MulDiv(nSize, GetDeviceCaps(m_hMemDc, LOGPIXELSY), 72);
        lstrcpynW(logfont.lfFaceName, L"DS-Digital", RTL_NUMBER_OF(logfont.lfFaceName));

        m_hMemFont = CreateFontIndirectW(&logfont);
        m_hOldFont = SelectObject(m_hMemDc, m_hMemFont);

        SetBkColor(m_hMemDc, DEFALUT_BK_COLOR);
        SetTextColor(m_hMemDc, RGB(255, 255, 255));

        HBRUSH hBkBrush = CreateSolidBrush(DEFALUT_BK_COLOR);
        RECT rect = {0, 0, MAX_PIXEL, MAX_PIXEL};
        FillRect(m_hMemDc, &rect, hBkBrush);
        DeleteObject(hBkBrush);

        ReleaseDC(NULL, hScreenDc);

        CalcTextSize();
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
    // 传入当前时间，对比，更新，处理时间字符串，绘制到内存dc，更新窗口
    void UpdateMemText(HWND hWnd, int nHour, int nMinute, int nSecond)
    {
        if (m_nSecond != nSecond || m_nMinute != nMinute || m_nHour != nHour)
        {
            m_nHour = nHour;
            m_nMinute = m_nMinute;
            m_nSecond = m_nSecond;

            WCHAR szTimeString[128];
            WCHAR szTimeStringAdjusted[RTL_NUMBER_OF(szTimeString) * 2];
            WCHAR ch = L':';

            wnsprintfW(szTimeString, RTL_NUMBER_OF(szTimeString), L" %02u %c %02u %c %02u ", nHour, ch, nMinute, ch, nSecond);

            for (int i = 0, j = 0; i < RTL_NUMBER_OF(szTimeString); ++i, ++j)
            {
                if (szTimeString[i] == L'1')
                {
                    szTimeStringAdjusted[j++] = L' ';
                }

                szTimeStringAdjusted[j] = szTimeString[i];

                if (szTimeStringAdjusted[j] == L'\0')
                {
                    break;
                }
            }

            TextOutW(m_hMemDc, 0, 0, szTimeStringAdjusted, lstrlenW(szTimeStringAdjusted));
            InvalidateRect(hWnd, NULL, TRUE);
        }
    }

    HDC GetDc() const
    {
        return m_hMemDc;
    }

    int GetTextWidth() const
    {
        return m_nTextWidth;
    }

    int GetTextHeight() const
    {
        return m_nTextHeight;
    }

private:
    void CalcTextSize()
    {
        m_nTextWidth = (int)GetDrawTextSizeInDc(m_hMemDc, L" 88 : 88 : 88 ", (UINT *)&m_nTextHeight);
    }

private:
    HDC m_hMemDc;
    HBITMAP m_hMemBmp;
    HGDIOBJ m_hOldBmp;
    HFONT m_hMemFont;
    HGDIOBJ m_hOldFont;
    int m_nHour;
    int m_nMinute;
    int m_nSecond;
    int m_nTextWidth;           // 88:88:88的宽度
    int m_nTextHeight;          // 88:88:88的高度
};

static CImageBuilder *g_pImageBuilder = NULL;

static void UpdateWindowLayeredValue(HWND hWnd, BOOL bForceUpdate = FALSE)
{
    static int s_nValues[] = {155, 35};
    static bool s_bPtInRect = false;

    POINT pt;
    RECT rect;

    GetCursorPos(&pt);
    GetWindowRect(hWnd, &rect);

    bool bBtInRectNow = !!PtInRect(&rect, pt);

    if (bForceUpdate)
    {
        s_bPtInRect = bBtInRectNow;
        SetLayeredWindowAttributes(hWnd, 0, s_nValues[!!s_bPtInRect], LWA_ALPHA);
    }
    else if (!bBtInRectNow ^ !s_bPtInRect)
    {
        s_bPtInRect = !s_bPtInRect;
        SetLayeredWindowAttributes(hWnd, 0, s_nValues[!!s_bPtInRect], LWA_ALPHA);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        SetTimer(hWnd, TI_MAIN, 31, NULL);
        SetTimer(hWnd, TI_MOUSE_MONITOR, 333, NULL);
        return 0;

    case WM_RBUTTONDOWN:
    case WM_CLOSE:
        KillTimer(hWnd, TI_MAIN);
        KillTimer(hWnd, TI_MOUSE_MONITOR);
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:{
        PAINTSTRUCT ps;
        RECT rect;
        HDC hPaintDc = BeginPaint(hWnd, &ps);

        GetClientRect(hWnd, &rect);
        BitBlt(hPaintDc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, g_pImageBuilder->GetDc(), 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
        break;}

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
                g_pImageBuilder->UpdateMemText(hWnd, stNow.wHour, stNow.wMinute, stNow.wSecond);
            }
        }
        else if (wParam == TI_MOUSE_MONITOR && IsWindowVisible(hWnd))
        {
            UpdateWindowLayeredValue(hWnd);
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
            UpdateWindowLayeredValue(hWnd, TRUE);
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
    LPCVOID lpResBuffer = GetResourceBuffer(g_hinstDll, L"RT_FILE", MAKEINTRESOURCEW(IDR_DIGIB_FONT), &dwResSize);

    if (lpResBuffer != NULL && dwResSize != 0)
    {
        DWORD dwCountInstalled = 0;
        HANDLE hFontHandle = AddFontMemResourceEx((LPVOID)lpResBuffer, dwResSize, NULL, &dwCountInstalled);

        if (hFontHandle == NULL)
        {
            return 0;
        }
    }

    g_pImageBuilder = new CImageBuilder(72);

    WNDCLASSEXW wcex = {sizeof(wcex)};

    wcex.lpszClassName  = CLASS_NAME;
    wcex.hInstance      = g_hinstDll;
    wcex.lpfnWndProc    = WndProc;
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);

    if (RegisterClassExW(&wcex))
    {
        HWND hWindow = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            CLASS_NAME,
            CLASS_NAME,
            WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
            100,
            100,
            g_pImageBuilder->GetTextWidth(),
            g_pImageBuilder->GetTextHeight(),
            NULL,
            NULL,
            g_hinstDll,
            NULL
            );

        if (IsWindow(hWindow))
        {
            MSG msg;

            while (TRUE)
            {
                int nRet = GetMessageW(&msg, NULL, 0, 0);

                if (nRet <= 0)
                {
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessageW(&msg);
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
