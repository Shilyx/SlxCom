// charconv是Windows平台下的字符集转换函数库
// 作者: Shilyx
// email: shilyx@yeah.net
// 
// 四种类型，四种简写如下：
// A: string   mbcs字符串
// W: wstring  unicode字符串
// T: tstring  定义了_UNICODE时是unicode字符串，其余情况下是mbcs字符串
// U: strutf8  utf-8字符串
// tstring和strutf8都是std命名空间中的类型
// 
// 十六种变换调用如下（括号内的为别名）：
// AtoA: string  -> string 
// WtoW: wstring -> wstring
// TtoT: tstring -> tstring
// UtoU: strutf8 -> strutf8
// AtoU: string  -> strutf8
// UtoA: strutf8 -> string 
// WtoU: wstring -> strutf8
// UtoW: strutf8 -> wstring
// AtoW: string  -> wstring
// WtoA: wstring -> string 
// AtoT: string  -> tstring
// TtoA: tstring -> string 
// WtoT: wstring -> tstring
// TtoW: tstring -> wstring
// UtoT: strutf8 -> tstring
// TtoU: tstring -> strutf8

#ifndef _CHARCONV_H
#define _CHARCONV_H

#ifndef __cplusplus
#  error charconv requires C++ compilation (use a .cpp suffix)
#endif

#pragma warning(disable: 4786)
#include <string>
#include <sstream>

_STD_BEGIN
//strutf8类型定义，同string同类型
typedef string strutf8;
//tstring类型定义
#ifdef _UNICODE
typedef wstring tstring;
typedef wstringstream tstringstream;
#else
typedef string tstring;
typedef stringstream tstringstream;
#endif
_STD_END

// 
template <class _Self> _Self _StoS(const _Self &_self)
{
    return _self;
}

// string -> string
#define AtoA _StoS<std::string>

// wstring -> wstring
#define WtoW _StoS<std::wstring>

// tstring -> tstring
#define TtoT _StoS<std::tstring>

// strutf8 -> strutf8
#define UtoU _StoS<std::strutf8>

//string -> strutf8
std::strutf8 AtoU(const std::string &str);

//strutf8 -> string
std::string UtoA(const std::strutf8 &str);

//wstring -> strutf8
std::strutf8 WtoU(const std::wstring &str);

//strutf8 -> wstring
std::wstring UtoW(const std::strutf8 &str);

//string -> wstring 
std::wstring AtoW(const std::string &str);

//wstring -> string
std::string WtoA(const std::wstring &str);

// string -> tstring
#ifdef _UNICODE
#  define AtoT AtoW
#else
#  define AtoT AtoA
#endif

// tstring -> string
#ifdef _UNICODE
#  define TtoA WtoA
#else
#  define TtoA AtoA
#endif

// wstring -> tstring
#ifdef _UNICODE
#  define WtoT WtoW
#else
#  define WtoT WtoA
#endif

// tstring -> wstring
#ifdef _UNICODE
#  define TtoW WtoW
#else
#  define TtoW AtoW
#endif

// strutf8 - > tstring
#ifdef _UNICODE
#  define UtoT UtoW
#else
#  define UtoT UtoA
#endif

// tstring - > strutf8
#ifdef _UNICODE
#  define TtoU WtoU
#else
#  define TtoU AtoU
#endif

#endif