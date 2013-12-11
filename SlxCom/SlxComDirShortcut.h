#ifndef _SLXCOMDIRSHORTCUT_H
#define _SLXCOMDIRSHORTCUT_H

#include <Windows.h>

int ShowDirShotcutMgr(HWND hwndParent);
void AskForAddDirShotcut(HWND hwndParent, LPCTSTR lpDirPath, LPCTSTR lpNickName);

#endif /* _SLXCOMDIRSHORTCUT_H */