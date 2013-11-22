#include <Windows.h>

#define PAINTVIEWWND_CLASS      TEXT("__slx_paintViewWnd_20131122")

class CPaintViewWnd
{
public:
    CPaintViewWnd(HDC hStartupDc, int nWidth, int nHeight);
    ~CPaintViewWnd();

public:
    void Show();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void WndProc_Paint(HWND hWnd);

private:
    HDC m_hStartupDc;
    int m_nWidth;
    int m_nHeight;
};

CPaintViewWnd::CPaintViewWnd(HDC hStartupDc, int nWidth, int nHeight)
    : m_hStartupDc(hStartupDc), m_nWidth(nWidth), m_nHeight(nHeight)
{

}

CPaintViewWnd::~CPaintViewWnd()
{

}

void CPaintViewWnd::Show()
{
    extern HINSTANCE g_hinstDll; //SlxCom.cpp
    WNDCLASSEX wcex = {sizeof(wcex)};
    HWND hSameWnd = FindWindow(PAINTVIEWWND_CLASS, PAINTVIEWWND_CLASS);

    if (IsWindow(hSameWnd))
    {
        return;
    }

    wcex.lpszClassName = PAINTVIEWWND_CLASS;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hInstance = g_hinstDll;
    wcex.lpfnWndProc = WndProc;
    wcex.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&wcex);

    HWND hPaintViewWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        PAINTVIEWWND_CLASS,
        PAINTVIEWWND_CLASS,
        WS_POPUP | WS_BORDER,
        0,
        0,
        m_nWidth,
        m_nHeight,
        NULL,
        NULL,
        g_hinstDll,
        (LPVOID)this
        );

    ShowWindow(hPaintViewWnd, SW_SHOW);
    UpdateWindow(hPaintViewWnd);
}

LRESULT CALLBACK CPaintViewWnd::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static CPaintViewWnd *pThis = NULL;

    switch (uMsg)
    {
    case WM_CREATE:
        pThis = (CPaintViewWnd *)((LPCREATESTRUCT)lParam)->lpCreateParams;
        return TRUE;

    case WM_PAINT:
        pThis->WndProc_Paint(hWnd);
        break;

    case WM_CLOSE:
    case WM_LBUTTONDOWN:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void CPaintViewWnd::WndProc_Paint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hPaintDc = BeginPaint(hWnd, &ps);

    BitBlt(hPaintDc, 0, 0, m_nWidth, m_nHeight, m_hStartupDc, 0, 0, SRCCOPY);

    EndPaint(hWnd, &ps);
}

void WINAPI SlxPaintView(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow)
{
    HDC hMemDc = NULL;
    HBITMAP hScreenBmp = NULL;
    HGDIOBJ hOldBmp = NULL;

    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (nScreenWidth > 0 && nScreenHeight > 0)
    {
        HDC hScreenDc = GetDC(NULL);

        if (hScreenDc != NULL)
        {
            hScreenBmp = CreateCompatibleBitmap(hScreenDc, nScreenWidth, nScreenHeight);
            hMemDc = CreateCompatibleDC(hScreenDc);
            hOldBmp = SelectObject(hMemDc, (HGDIOBJ)hScreenBmp);

            BitBlt(hMemDc, 0, 0, nScreenWidth, nScreenHeight, hScreenDc, 0, 0, SRCCOPY);

            ReleaseDC(NULL, hScreenDc);
        }
    }

    if (hMemDc != NULL)
    {
        CPaintViewWnd(hMemDc, nScreenWidth, nScreenHeight).Show();

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
    else
    {
        MessageBox(NULL, TEXT("½ØÈ¡×ÀÃæ±³¾°Ê§°Ü"), NULL, MB_ICONERROR | MB_SYSTEMMODAL);
    }

    SelectObject(hMemDc, hOldBmp);
    DeleteObject(hScreenBmp);
    DeleteDC(hMemDc);
}
