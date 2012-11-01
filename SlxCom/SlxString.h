#ifndef _SLX_STRING_H
#define _SLX_STRING_H

#pragma warning(disable: 4786)
#include <string>

#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

#endif
