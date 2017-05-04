#ifndef _SLXCOMEXPANDDIRFILES_H
#define _SLXCOMEXPANDDIRFILES_H

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

BOOL EdfCanBeExpanded(LPCWSTR lpDir);

BOOL EdfCanBeUnexpanded(LPCWSTR lpDir);

BOOL EdfDoExpand(LPCWSTR lpDir);

BOOL EdfDoUnexpand(LPCWSTR lpDir);

#ifdef __cplusplus
}
#endif

#endif /* _SLXCOMEXPANDDIRFILES_H */
