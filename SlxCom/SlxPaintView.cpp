#include <Windows.h>
#pragma warning(disable: 4786)
#include <list>

using namespace std;

#define PAINTVIEWWND_CLASS      TEXT("__slx_paintViewWnd_20131122")

static enum PaintMethod
{
    PM_LINE,
    PM_ECLIPSE,
    PM_RECTANGLE,
    PM_TEXT,
};

static enum PaintStatus
{
    PS_LINE,
    PS_ECLIPSE,
    PS_RECTANGLE,
    PS_TEXT,
};

class CEnhFile
{
private:
    CEnhFile(HDC hSrcDc)
    {
        m_hEnhDc = CreateEnhMetaFile(hSrcDc, NULL, NULL, NULL);
    }

public:
    static CEnhFile *Create(HDC hSrcDc)
    {
        return new CEnhFile(hSrcDc);
    }

    ~CEnhFile()
    {
        if (m_hEnhDc != NULL)
        {
            Save(NULL);
        }

        DeleteEnhMetaFile(m_hEnhFile);
    }

    HDC GetDc() const
    {
        return m_hEnhDc;
    }

    HENHMETAFILE Save(RECT *pRect)
    {
        m_hEnhFile = CloseEnhMetaFile(m_hEnhDc);
        m_hEnhDc = NULL;

        if (pRect != NULL)
        {
            m_rect = *pRect;
        }

        return GetFile();
    }

    HENHMETAFILE GetFile() const
    {
        return m_hEnhFile;
    }

    BOOL Play(HDC hDestDc) const
    {
        if (m_hEnhFile != NULL)
        {
            return PlayEnhMetaFile(hDestDc, m_hEnhFile, &m_rect);
        }

        return FALSE;
    }

private:
    HDC m_hEnhDc;
    HENHMETAFILE m_hEnhFile;
    RECT m_rect;
};

class CPaintViewWnd
{
public:
    static void Run(HDC hStartupDc, int nWidth, int nHeight)
    {
        CPaintViewWnd(hStartupDc, nWidth, nHeight);
    }

private:
    CPaintViewWnd(HDC hStartupDc, int nWidth, int nHeight)
        : m_hStartupDc(hStartupDc), m_nWidth(nWidth), m_nHeight(nHeight)
    {
        m_hBkDc = CreateCompatibleDC(hStartupDc);
        m_hBkBmp = CreateCompatibleBitmap(hStartupDc, nWidth, nHeight);
        m_hBkOldObj = SelectObject(m_hBkDc, (HGDIOBJ)m_hBkBmp);

//         RECT rect;
//         rect.left = 0;
//         rect.top = 0;
//         rect.right = 900;
//         rect.bottom = 900;
// 
// 
//         CEnhFile *pEnhFile = CEnhFile::Create(m_hMainDc);
//         delete pEnhFile;
// 
//         for (int i = 0; i < 100; i ++)
//         {
//             MoveToEx(pEnhFile->GetDc(), 0, i * 3, NULL);
//             LineTo(pEnhFile->GetDc(), 400, 400);
//             MoveToEx(pEnhFile->GetDc(), i, 0, NULL);
//             LineTo(pEnhFile->GetDc(), 900, 900);
//         }
// 
//         pEnhFile->Save(&rect);
// 
//         m_listEnhs.push_back(pEnhFile);

        BuildMainDc();
        Show();
    }

    ~CPaintViewWnd()
    {
        SelectObject(m_hBkDc, m_hBkOldObj);
        DeleteObject(m_hBkBmp);
        DeleteDC(m_hBkDc);
    }

    void Show() const
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
            WS_POPUP,
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

    static CPaintViewWnd *GetClass(HWND hWnd)
    {
        return (CPaintViewWnd *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_CREATE:
        {
            CPaintViewWnd *pClass = (CPaintViewWnd *)((LPCREATESTRUCT)lParam)->lpCreateParams;

            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pClass);
            pClass->m_hPaintView = hWnd;
            pClass->m_hPaintViewDc = GetDC(hWnd);

            return TRUE;
        }

        case WM_ERASEBKGND:
            return 0;

        case WM_PAINT:
            GetClass(hWnd)->WndProc_PAINT(hWnd);
            break;

        case WM_CLOSE:
        case WM_RBUTTONUP:
            DestroyWindow(hWnd);
            break;

        case WM_LBUTTONDOWN:
            GetClass(hWnd)->WndProc_LBUTTONDOWN(wParam, lParam);
            break;

        case WM_LBUTTONUP:
            GetClass(hWnd)->WndProc_LBUTTONUP(wParam, lParam);
            break;

        case WM_MOUSEMOVE:
            GetClass(hWnd)->WndProc_MOUSEMOVE(wParam, lParam);
            break;

        case WM_DESTROY:
            ReleaseDC(hWnd, GetClass(hWnd)->m_hPaintViewDc);
            PostQuitMessage(0);
            break;

        default:
            break;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void WndProc_LBUTTONDOWN(WPARAM wParam, LPARAM lParam)
    {
        int xPos = LOWORD(lParam); 
        int yPos = HIWORD(lParam); 

        switch (m_pmValue)
        {
        case PM_LINE:
            SetCapture(m_hPaintView);
            MoveToEx(m_hPaintViewDc, xPos, yPos, NULL);
            MoveToEx(m_hPaintViewDc, xPos, yPos, NULL);

            break;

        default:
            break;
        }
    }

    void WndProc_LBUTTONUP(WPARAM wParam, LPARAM lParam)
    {
        switch (m_pmValue)
        {
        case PM_LINE:
            ReleaseCapture();
            break;

        default:
            break;
        }
    }

    void WndProc_MOUSEMOVE(WPARAM wParam, LPARAM lParam)
    {
        if (m_bPainting)
        {
            switch (m_pmValue)
            {
            case PM_LINE:

                break;

            default:
                break;
            }
        }
    }

    void WndProc_PAINT(HWND hWnd) const
    {
        PAINTSTRUCT ps;
        HDC hPaintDc = BeginPaint(hWnd, &ps);

        BitBlt(hPaintDc, 0, 0, m_nWidth, m_nHeight, m_hBkDc, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }

    void BuildMainDc()
    {
        BitBlt(m_hBkDc, 0, 0, m_nWidth, m_nHeight, m_hStartupDc, 0, 0, SRCCOPY);

        list<CEnhFile *>::const_iterator it = m_listEnhs.begin();
        for (; it != m_listEnhs.end(); ++it)
        {
            (*it)->Play(m_hBkDc);
        }
    }

private:
    list<CEnhFile *> m_listEnhs;    //动作序列
    HDC m_hStartupDc;               //初始状态的dc
    int m_nWidth;                   //初始状态的位图宽度
    int m_nHeight;                  //初始状态的位图高度

    HDC m_hBkDc;                    //窗口的缓冲dc
    HBITMAP m_hBkBmp;               //窗口的缓冲dc中的位图
    HGDIOBJ m_hBkOldObj;            //窗口的缓冲dc中的原位图

    PaintMethod m_pmValue;          //绘图方式
    BOOL m_bPainting;               //绘图状态
    PaintStatus m_psValue;          //绘图状态

    HWND m_hPaintView;              //主窗口
    HDC m_hPaintViewDc;             //主窗口dc
    CEnhFile *pCurrentEnhFile;      //当前CEnhFile
};

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
        CPaintViewWnd::Run(hMemDc, nScreenWidth, nScreenHeight);
    }
    else
    {
        MessageBox(hwndStub, TEXT("截取桌面背景失败"), NULL, MB_ICONERROR | MB_SYSTEMMODAL);
    }

    SelectObject(hMemDc, hOldBmp);
    DeleteObject(hScreenBmp);
    DeleteDC(hMemDc);
}
