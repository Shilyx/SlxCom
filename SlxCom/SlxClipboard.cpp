#include <Windows.h>
#include "resource.h"

extern HINSTANCE g_hinstDll; // SlxCom.cpp

#define DLG_CAPTION L"Clipboard viewer in SlxCom tools"

static HACCEL g_hAccel = NULL;
static HWND g_hwndDlg = NULL;

static void On_WM_INITDIALOG(HWND hwndDlg)
{
    g_hwndDlg = hwndDlg;

    HICON hIcon = LoadIconW(g_hinstDll, MAKEINTRESOURCEW(IDI_CONFIG_ICON));

    SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessageW(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    SetWindowTextW(hwndDlg, DLG_CAPTION);
}

static void On_WM_COMMAND(int id)
{
    switch (id)
    {
    case ID_REFRESH:
        MessageBoxW(g_hwndDlg, NULL, NULL, MB_ICONEXCLAMATION);
        break;

    default:
        break;
    }
}

static INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        On_WM_INITDIALOG(hwndDlg);
        return TRUE;

    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_COMMAND:
        On_WM_COMMAND(LOWORD(wParam));
        break;

    default:
        break;
    }

    return FALSE;
}

void CALLBACK SlxClipboard(HWND hwndStub, HINSTANCE hInstance, LPSTR lpCmd, int nCmdShow)
{
    if (IsWindow(FindWindowW(L"#32770", DLG_CAPTION)))
    {
        if (IDYES != MessageBoxW(hwndStub, DLG_CAPTION L"已在运行，是否要打开新实例？", L"信息", MB_ICONQUESTION | MB_DEFBUTTON2))
        {
            return;
        }
    }

    ACCEL accel[] =
    {
        { FVIRTKEY | FCONTROL, 'S',     ID_SAVE         },
        { FVIRTKEY,            VK_F5,   ID_REFRESH      },
        { FVIRTKEY | FCONTROL, 'H',     ID_HEXMODE      },
        { FVIRTKEY | FALT,     'Q',     ID_QUIT         },
    };

    HACCEL hAccel = CreateAcceleratorTable(accel, RTL_NUMBER_OF(accel));

    if (hAccel != NULL)
    {
        HWND hDialog = CreateDialogW(g_hinstDll, MAKEINTRESOURCEW(IDD_CLIPBOARD_DIALOG), NULL, DlgProc);

        if (IsWindow(hDialog))
        {
            ShowWindow(hDialog, SW_SHOW);

            MSG msg;

            while (TRUE)
            {
                int nRet = GetMessageW(&msg, NULL, 0, 0);

                if (nRet <= 0)
                {
                    break;
                }

                TranslateAccelerator(hDialog, hAccel, &msg);

                if (!IsDialogMessageW(hDialog, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }

        DestroyAcceleratorTable(hAccel);
    }
}
