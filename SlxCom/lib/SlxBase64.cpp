#include "SlxBase64.hxx"

using namespace std;

static const char _base64_encode_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char _base64_decode_chars[128] =
{
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	 -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	 -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

string Base64Encode(const void *buffer, size_t size)
{
	string ret;

    const char *str = (const char *)buffer;
	unsigned char c1, c2, c3;
    size_t i = 0;

	while(i < size)
	{
		// read the first byte
		c1 = str[i++];
		if (i == size)	   // pad with "="
		{
			ret += _base64_encode_chars[c1 >> 2];
			ret += _base64_encode_chars[(c1 & 0x3) << 4];
			ret += "==";
			break;
		}

		// read the second byte
		c2 = str[i++];
		if (i == size)	   // pad with "="
		{
			ret += _base64_encode_chars[c1 >> 2];
			ret += _base64_encode_chars[((c1 & 0x3) << 4) | ((c2 & 0xF0) >> 4)];
			ret += _base64_encode_chars[(c2 & 0xF) << 2];
			ret += "=";
			break;
		}

		// read the third byte
		c3 = str[i++];
		// convert into four bytes string
		ret += _base64_encode_chars[c1 >> 2];
		ret += _base64_encode_chars[((c1 & 0x3) << 4) | ((c2 & 0xF0) >> 4)];
		ret += _base64_encode_chars[((c2 & 0xF) << 2) | ((c3 & 0xC0) >> 6)];
		ret += _base64_encode_chars[c3 & 0x3F];
	}

    return ret;
}

string Base64Decode(const char *str)
{
	char c1, c2, c3, c4;
	size_t i = 0;
	size_t len = strlen(str);
	string ret;

	while (i < len)
	{
		// read the first byte
		do {
			c1 = _base64_decode_chars[str[i++]];
		} while(i < len && c1 == -1);

		if (c1 == -1)
		{
			break;
		}

		// read the second byte
		do {
			c2 = _base64_decode_chars[str[i++]];
		} while(i < len && c2 == -1);

		if (c2 == -1)
		{
			break;
		}

		// assamble the first byte
		ret += char((c1 << 2) | ((c2 & 0x30) >> 4));

		// read the third byte
		do {
			c3 = str[i++];
			if (c3 == 61)	   // meet with "=", break
			{
				return ret;
			}
			c3 = _base64_decode_chars[c3];
		} while(i < len && c3 == -1);

		if (c3 == -1)
		{
			break;
		}

		// assamble the second byte
		ret += char(((c2 & 0XF) << 4) | ((c3 & 0x3C) >> 2));

		// read the fourth byte
		do {
			c4 = str[i++];
			if (c4 == 61)	   // meet with "=", break
			{
				return ret;
			}
			c4 = _base64_decode_chars[c4];
		} while(i < len && c4 == -1);

		if (c4 == -1)
		{
			break;
		}

		// assamble the third byte
		ret += char(((c3 & 0x03) << 6) | c4);
	}

	return ret;
}

std::string Base64Encode(const std::string &buffer)
{
    return Base64Encode(buffer.c_str(), buffer.size());
}

std::string Base64Decode(const std::string &buffer)
{
    return Base64Decode(buffer.c_str());
}