#include "QuickPopupMenu.h"
#include <stack>

using namespace std;

static stack<HMENU>* _getStack(void* param) {
    return (stack<HMENU>*)param;
}

CQuickPopupMenu::CQuickPopupMenu() {
    m_hMenu = CreatePopupMenu();
    m_stackCurrent = new stack<HMENU>;
    _getStack(m_stackCurrent)->push(m_hMenu);
}

CQuickPopupMenu::~CQuickPopupMenu() {
    delete _getStack(m_stackCurrent);
    DestroyMenu(m_hMenu);
}

int CQuickPopupMenu::Popup(HWND hWnd, int x, int y) const {
    return TrackPopupMenu(
        m_hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        x, y,
        0,
        hWnd,
        NULL);
}

CQuickPopupMenu& CQuickPopupMenu::AddItemIf(LPCWSTR lpText, int nID, BOOL bChecked, BOOL bDisabled, BOOL bGray, BOOL bExecute) {
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

CQuickPopupMenu& CQuickPopupMenu::AddItem(LPCWSTR lpText, int nID) {
    return AddItemIf(lpText, nID, FALSE, FALSE, FALSE, TRUE);
}

CQuickPopupMenu& CQuickPopupMenu::AddSubMenu(LPCWSTR lpText) {
    HMENU hMenu = CreateMenu();
    AppendMenuW(_getStack(m_stackCurrent)->top(), MF_POPUP, (ULONG_PTR)hMenu, lpText);
    _getStack(m_stackCurrent)->push(hMenu);
    return *this;
}

CQuickPopupMenu& CQuickPopupMenu::ExitSubMenu() {
    _getStack(m_stackCurrent)->pop();
    return *this;
}

CQuickPopupMenu& CQuickPopupMenu::AddSeparator() {
    AppendMenuW(_getStack(m_stackCurrent)->top(), MF_SEPARATOR, 0, NULL);
    return *this;
}
