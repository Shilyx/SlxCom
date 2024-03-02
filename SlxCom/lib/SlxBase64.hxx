#ifndef _SLXBASE64_HXX
#define _SLXBASE64_HXX

#include <string>

std::string Base64Encode(const void *buffer, size_t size);
std::string Base64Decode(const char *encoded);

std::string Base64Encode(const std::string &buffer);
std::string Base64Decode(const std::string &buffer);

#endif /* _SLXBASE64_HXX */