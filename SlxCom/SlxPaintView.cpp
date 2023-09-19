#include <Windows.h>
#pragma warning(disable: 4786)
#include <list>

using namespace std;

#define PAINTVIEWWND_CLASS      L"__slx_paintViewWnd_20131122"

enum PaintMethod {
    PM_LINE,
    PM_ECLIPSE,
    PM_RECTANGLE,
    PM_TEXT,
};

enum PaintStatus {
    PS_LINE,
    PS_ECLIPSE,
    PS_RECTANGLE,
    PS_TEXT,
};

class CFullScreenEnhFile {
private:
    CFullScreenEnhFile(HDC hSrcDc, int nWidth, int nHeight, int nWidthMM, int nHeightMM)
        : m_nWidth(nWidth), m_nHeight(nHeight) {
        RECT rect;

        rect.left = 0;
        rect.top = 0;
        rect.right = nWidthMM;
        rect.bottom = nHeightMM;

        m_hEnhDc = CreateEnhMetaFile(hSrcDc, NULL, &rect, NULL);
    }

public:
    static CFullScreenEnhFile *Create(HDC hSrcDc, int nWidth, int nHeight, int nWidthMM, int nHeightMM) {
        return new CFullScreenEnhFile(hSrcDc, nWidth, nHeight, nWidthMM, nHeightMM);
    }

    ~CFullScreenEnhFile() {
        if (m_hEnhDc != NULL) {
            Save();
        }

        DeleteEnhMetaFile(m_hEnhFile);
    }

    HDC GetDc() const {
        return m_hEnhDc;
    }

    HENHMETAFILE Save() {
        m_hEnhFile = CloseEnhMetaFile(m_hEnhDc);
        m_hEnhDc = NULL;

        return GetFile();
    }

    HENHMETAFILE GetFile() const {
        return m_hEnhFile;
    }

    BOOL Play(HDC hDestDc) const {
        if (m_hEnhFile != NULL) {
            RECT rect;

            rect.left = 0;
            rect.top = 0;
            rect.right = m_nWidth;
            rect.bottom = m_nHeight;

            return PlayEnhMetaFile(hDestDc, m_hEnhFile, &rect);
        }

        return FALSE;
    }

private:
    HDC m_hEnhDc;
    HENHMETAFILE m_hEnhFile;
    int m_nWidth;
    int m_nHeight;
};

class CPaintViewWnd {
public:
    static void Run(HDC hStartupDc, int nWidth, int nHeight, int nWidthMM, int nHeightMM) {
        CPaintViewWnd(hStartupDc, nWidth, nHeight, nWidthMM, nHeightMM);
    }

private:
    CPaintViewWnd(HDC hStartupDc, int nWidth, int nHeight, int nWidthMM, int nHeightMM)
        : m_hStartupDc(hStartupDc), m_nWidth(nWidth), m_nHeight(nHeight), m_nWidthMM(nWidthMM), m_nHeightMM(nHeightMM) {
        m_hBkDc = CreateCompatibleDC(hStartupDc);
        m_hBkBmp = CreateCompatibleBitmap(hStartupDc, nWidth, nHeight);
        m_hBkOldObj = SelectObject(m_hBkDc, (HGDIOBJ)m_hBkBmp);

        m_pmValue = PM_LINE;
        m_bPainting = FALSE;

        m_hWindow = NULL;
        m_hWindowDc = NULL;
        m_pCurrentEnhFile = NULL;

        NewEnhFile();
        HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 199));
        HGDIOBJ hOldPen = SelectObject(m_pCurrentEnhFile->GetDc(), (HGDIOBJ)hPen);
        for (int i = 0; i < 100; i += 1) {
            MoveToEx(m_pCurrentEnhFile->GetDc(), i * 9, 0, NULL);
            LineTo(m_pCurrentEnhFile->GetDc(), 900, 900);
        }
        SelectObject(m_pCurrentEnhFile->GetDc(), hOldPen);
        DeleteObject(hPen);

        m_pCurrentEnhFile->Save();
        m_listEnhs.push_back(m_pCurrentEnhFile);
        m_pCurrentEnhFile = NULL;

        BuildMainDc();
        Show();
    }

    ~CPaintViewWnd() {
        SelectObject(m_hBkDc, m_hBkOldObj);
        DeleteObject(m_hBkBmp);
        DeleteDC(m_hBkDc);

        list<CFullScreenEnhFile *>::const_iterator it = m_listEnhs.begin();
        for (; it != m_listEnhs.end(); ++it) {
            delete *it;
        }
    }

    void Show() const {
        extern HINSTANCE g_hinstDll; //SlxCom.cpp
        WNDCLASSEXW wcex = {sizeof(wcex)};
        HWND hSameWnd = FindWindowW(PAINTVIEWWND_CLASS, PAINTVIEWWND_CLASS);

        if (IsWindow(hSameWnd)) {
            return;
        }

        wcex.lpszClassName = PAINTVIEWWND_CLASS;
        wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wcex.hInstance = g_hinstDll;
        wcex.lpfnWndProc = WndProc;
        wcex.style = CS_HREDRAW | CS_VREDRAW;

        RegisterClassExW(&wcex);

        HWND hPaintViewWnd = CreateWindowExW(
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

        while (TRUE) {
            int nRet = GetMessageW(&msg, NULL, 0, 0);

            if (nRet <= 0) {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    static CPaintViewWnd *GetClass(HWND hWnd) {
        return (CPaintViewWnd *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE: {
            CPaintViewWnd *pClass = (CPaintViewWnd *)((LPCREATESTRUCT)lParam)->lpCreateParams;

            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pClass);
            pClass->m_hWindow = hWnd;
            pClass->m_hWindowDc = GetDC(hWnd);

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
            ReleaseDC(hWnd, GetClass(hWnd)->m_hWindowDc);
            PostQuitMessage(0);
            break;

        default:
            break;
        }

        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    void WndProc_LBUTTONDOWN(WPARAM wParam, LPARAM lParam) {
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);

        switch (m_pmValue) {
        case PM_LINE:
            m_bPainting = TRUE;
            SetCapture(m_hWindow);
            MoveToEx(m_hWindowDc, xPos, yPos, NULL);
            NewEnhFile();
            MoveToEx(m_pCurrentEnhFile->GetDc(), xPos, yPos, NULL);
            break;

        default:
            break;
        }
    }

    void WndProc_MOUSEMOVE(WPARAM wParam, LPARAM lParam) {
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);

        if (m_bPainting) {
            switch (m_pmValue) {
            case PM_LINE:
                LineTo(m_hWindowDc, xPos, yPos);
                LineTo(m_pCurrentEnhFile->GetDc(), xPos, yPos);
                break;

            default:
                break;
            }
        }
    }

    void WndProc_LBUTTONUP(WPARAM wParam, LPARAM lParam) {
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);

        switch (m_pmValue) {
        case PM_LINE:
            LineTo(m_hWindowDc, xPos, yPos);
            LineTo(m_pCurrentEnhFile->GetDc(), xPos, yPos);
            ReleaseCapture();
            m_bPainting = FALSE;

            m_pCurrentEnhFile->Save();
            m_listEnhs.push_back(m_pCurrentEnhFile);
            BuildMainDc();
            InvalidateRect(m_hWindow, NULL, TRUE);

            break;

        default:
            break;
        }
    }

    void WndProc_PAINT(HWND hWnd) const {
        PAINTSTRUCT ps;
        HDC hPaintDc = BeginPaint(hWnd, &ps);

        BitBlt(hPaintDc, 0, 0, m_nWidth, m_nHeight, m_hBkDc, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }

    void BuildMainDc() {
        BitBlt(m_hBkDc, 0, 0, m_nWidth, m_nHeight, m_hStartupDc, 0, 0, SRCCOPY);

        list<CFullScreenEnhFile *>::const_iterator it = m_listEnhs.begin();
        for (; it != m_listEnhs.end(); ++it) {
            (*it)->Play(m_hBkDc);
        }
    }

    void NewEnhFile() {
        m_pCurrentEnhFile = CFullScreenEnhFile::Create(m_hBkDc, m_nWidth, m_nHeight, m_nWidthMM * 100, m_nHeightMM * 100);
    }

private:
    list<CFullScreenEnhFile *> m_listEnhs;  //动作序列
    HDC m_hStartupDc;                       //初始状态的dc
    int m_nWidth;                           //初始状态的位图宽度（像素）
    int m_nHeight;                          //初始状态的位图高度（像素）
    int m_nWidthMM;                         //初始状态的位图宽度（毫米）
    int m_nHeightMM;                        //初始状态的位图高度（毫米）

    HDC m_hBkDc;                            //窗口的缓冲dc
    HBITMAP m_hBkBmp;                       //窗口的缓冲dc中的位图
    HGDIOBJ m_hBkOldObj;                    //窗口的缓冲dc中的原位图

    PaintMethod m_pmValue;                  //绘图方式
    BOOL m_bPainting;                       //绘图状态
    PaintStatus m_psValue;                  //绘图状态

    HWND m_hWindow;                         //主窗口
    HDC m_hWindowDc;                        //主窗口dc
    CFullScreenEnhFile *m_pCurrentEnhFile;  //当前CEnhFile
};

void WINAPI SlxPaintView(HWND hwndStub, HINSTANCE hAppInstance, LPCWSTR lpszCmdLine, int nCmdShow) {
    HDC hMemDc = NULL;
    HBITMAP hScreenBmp = NULL;
    HGDIOBJ hOldBmp = NULL;

    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (nScreenWidth > 0 && nScreenHeight > 0) {
        HDC hScreenDc = GetDC(NULL);

        if (hScreenDc != NULL) {
            hScreenBmp = CreateCompatibleBitmap(hScreenDc, nScreenWidth, nScreenHeight);
            hMemDc = CreateCompatibleDC(hScreenDc);
            hOldBmp = SelectObject(hMemDc, (HGDIOBJ)hScreenBmp);

            BitBlt(hMemDc, 0, 0, nScreenWidth, nScreenHeight, hScreenDc, 0, 0, SRCCOPY);

            ReleaseDC(NULL, hScreenDc);
        }
    }

    if (hMemDc != NULL) {
        CPaintViewWnd::Run(
            hMemDc,
            nScreenWidth,
            nScreenHeight,
            GetDeviceCaps(hMemDc, HORZSIZE),
            GetDeviceCaps(hMemDc, VERTSIZE)
            );
    } else {
        MessageBoxW(hwndStub, L"截取桌面背景失败", NULL, MB_ICONERROR | MB_SYSTEMMODAL);
    }

    SelectObject(hMemDc, hOldBmp);
    DeleteObject(hScreenBmp);
    DeleteDC(hMemDc);
}
