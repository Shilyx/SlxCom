#ifndef _PEINFORMATION_H
#define _PEINFORMATION_H

#include <string>

std::string GetPeInformation(const wchar_t *lpPeFilePath);
std::wstring GetPeInformationW(const wchar_t *lpPeFilePath);
bool IsFileLikePeFile(const wchar_t *lpPeFilePath);

#endif /* _PEINFORMATION_H */
