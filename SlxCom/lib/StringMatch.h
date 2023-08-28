#ifndef _STRINGMATCH_H
#define _STRINGMATCH_H

#include "deelx.h"
#include <string>
#include <vector>

template <class Ch>
bool SmMatchExact(const Ch *str, const Ch *patten, bool bEgnoreCase = false)
{
    return !!CRegexpT<Ch>(patten, bEgnoreCase ? IGNORECASE : 0).MatchExact(str).IsMatched();
}

template <class Ch>
std::basic_string<Ch> SmSearch(const Ch *str, const Ch *patten, bool bEgnoreCase = false)
{
    std::basic_string<Ch> subStr;
    MatchResult result = CRegexpT<Ch>(patten, bEgnoreCase ? IGNORECASE : 0).Match(str);

    if (result.IsMatched())
    {
        int start = result.GetStart();
        int end = result.GetEnd();

        if (start >= 0 && end >= start)
        {
            subStr.assign(str + start, end - start);
        }
    }

    return subStr;
}

template <class Ch>
std::vector<std::basic_string<Ch>> SmSearchAll(const Ch *str, const Ch *patten, bool bEgnoreCase = false)
{
    std::vector<std::basic_string<Ch>> vectorResult;
    CRegexpT<Ch> reg(patten, bEgnoreCase ? IGNORECASE : 0);
    CContext *context = reg.PrepareMatch(str);
    MatchResult result = reg.Match(context);

    while ( result.IsMatched())
    {
        int start = result.GetStart();
        int end = result.GetEnd();

        if (start >= 0 && end >= start)
        {
            vectorResult.push_back(std::basic_string<Ch>(str + start, end - start));
        }

        result = reg.Match(context);
    }

    return vectorResult;
}

#endif /* _STRINGMATCH_H */
