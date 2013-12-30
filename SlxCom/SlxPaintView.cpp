#include <Windows.h>
#pragma warning(disable: 4786)
#include <list>

using namespace std;

#define PAINTVIEWWND_CLASS      TEXT("__slx_paintViewWnd_20131122")

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
    CPaintViewWnd(HDC hStartupDc, int nWidth, int nHeight)
        : m_hStartupDc(hStartupDc), m_nWidth(nWidth), m_nHeight(nHeight)
    {
        m_hMainDc = CreateCompatibleDC(hStartupDc);
        m_hMainBmp = CreateCompatibleBitmap(hStartupDc, nWidth, nHeight);
        m_hMainOldObj = SelectObject(m_hMainDc, (HGDIOBJ)m_hMainBmp);

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
    }

    ~CPaintViewWnd()
    {
        SelectObject(m_hMainDc, m_hMainOldObj);
        DeleteObject(m_hMainBmp);
        DeleteDC(m_hMainDc);
    }

public:
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
    }

private:
    static CPaintViewWnd *GetClass(HWND hWnd)
    {
        return (CPaintViewWnd *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_CREATE:
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
            return TRUE;

        case WM_ERASEBKGND:
            return 0;

        case WM_PAINT:
            GetClass(hWnd)->WndProc_Paint(hWnd);
            break;

        case WM_CLOSE:
        case WM_RBUTTONUP:
            DestroyWindow(hWnd);
            break;

        case WM_LBUTTONDOWN:
            SetCapture(hWnd);
            break;

        case WM_LBUTTONUP:
            ReleaseCapture();
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            break;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void WndProc_Paint(HWND hWnd) const
    {
        PAINTSTRUCT ps;
        HDC hPaintDc = BeginPaint(hWnd, &ps);

        BitBlt(hPaintDc, 0, 0, m_nWidth, m_nHeight, m_hMainDc, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }

private:
    void BuildMainDc()
    {
        BitBlt(m_hMainDc, 0, 0, m_nWidth, m_nHeight, m_hStartupDc, 0, 0, SRCCOPY);

        list<CEnhFile *>::const_iterator it = m_listEnhs.begin();
        for (; it != m_listEnhs.end(); ++it)
        {
            (*it)->Play(m_hMainDc);
        }
    }

private:
    list<CEnhFile *> m_listEnhs;    //动作序列
    HDC m_hStartupDc;               //初始状态的dc
    int m_nWidth;                   //初始状态的位图宽度
    int m_nHeight;                  //初始状态的位图高度

    HDC m_hMainDc;                  //窗口的缓冲dc
    HBITMAP m_hMainBmp;             //窗口的缓冲dc中的位图
    HGDIOBJ m_hMainOldObj;          //
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
        MessageBox(hwndStub, TEXT("截取桌面背景失败"), NULL, MB_ICONERROR | MB_SYSTEMMODAL);
    }

    SelectObject(hMemDc, hOldBmp);
    DeleteObject(hScreenBmp);
    DeleteDC(hMemDc);
}
