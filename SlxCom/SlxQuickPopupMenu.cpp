#include "SlxQuickPopupMenu.h"
#include <stack>

using namespace std;

static stack<HMENU>* _getStack(void* param)
{
    return (stack<HMENU>*)param;
}

CSlxQuickPopupMenu::CSlxQuickPopupMenu()
{
    m_hMenu = CreatePopupMenu();
    m_stackCurrent = new stack<HMENU>;
    _getStack(m_stackCurrent)->push(m_hMenu);
}

CSlxQuickPopupMenu::~CSlxQuickPopupMenu()
{
    delete _getStack(m_stackCurrent);
    DestroyMenu(m_hMenu);
}

int CSlxQuickPopupMenu::Popup(HWND hWnd, int x, int y) const
{
    return TrackPopupMenu(
        m_hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        x, y,
        0,
        hWnd,
        NULL);
}

CSlxQuickPopupMenu& CSlxQuickPopupMenu::AddItemIf(LPCWSTR lpText, int nID, BOOL bChecked, BOOL bDisabled, BOOL bGray, BOOL bExecute)
{
    if (!bExecute) {
        return *this;
    }

    AppendMenuW(
        _getStack(m_stackCurrent)->top(),
        MF_BYCOMMAND | 
        (bChecked ? MF_CHECKED : 0) |
        (bDisabled ? MF_DISABLED : 0) |
        (bGray ? MF_GRAYED : 0),
        nID,
        lpText);
    return *this;
}

CSlxQuickPopupMenu& CSlxQuickPopupMenu::AddItem(LPCWSTR lpText, int nID)
{
    return AddItemIf(lpText, nID, FALSE, FALSE, FALSE, TRUE);
}

CSlxQuickPopupMenu& CSlxQuickPopupMenu::AddSubMenu(LPCWSTR lpText)
{
    HMENU hMenu = CreateMenu();
    AppendMenuW(_getStack(m_stackCurrent)->top(), MF_POPUP, (ULONG_PTR)hMenu, lpText);
    _getStack(m_stackCurrent)->push(hMenu);
    return *this;
}

CSlxQuickPopupMenu& CSlxQuickPopupMenu::ExitSubMenu()
{
    _getStack(m_stackCurrent)->pop();
    return *this;
}

CSlxQuickPopupMenu& CSlxQuickPopupMenu::AddSeparator()
{
    AppendMenuW(_getStack(m_stackCurrent)->top(), MF_SEPARATOR, 0, NULL);
    return *this;
}
