#include "Utf8Text.h"

#include <climits>
#include <cwchar>

bool TryDecodeUtf8(const std::string& asText, std::wstring& asDecoded)
{
	asDecoded.clear();
	for (std::string::size_type index = 0; index < asText.size();)
	{
		const unsigned char first = static_cast<unsigned char>(asText[index++]);
		unsigned int scalar = first;
		int continuationCount = 0;
		unsigned int minimum = 0;
		if (first >= 0xC2 && first <= 0xDF) { scalar = first & 0x1F; continuationCount = 1; minimum = 0x80; }
		else if (first >= 0xE0 && first <= 0xEF) { scalar = first & 0x0F; continuationCount = 2; minimum = 0x800; }
		else if (first >= 0xF0 && first <= 0xF4) { scalar = first & 0x07; continuationCount = 3; minimum = 0x10000; }
		else if (first >= 0x80) return false;
		if (index + continuationCount > asText.size()) return false;
		for (int continuation = 0; continuation < continuationCount; ++continuation)
		{
			const unsigned char next = static_cast<unsigned char>(asText[index++]);
			if ((next & 0xC0) != 0x80) return false;
			scalar = (scalar << 6) | (next & 0x3F);
		}
		if ((continuationCount && scalar < minimum) || scalar > 0x10FFFF ||
			(scalar >= 0xD800 && scalar <= 0xDFFF)) return false;
#if WCHAR_MAX <= 0xFFFF
		if (scalar > 0xFFFF)
		{
			scalar -= 0x10000;
			asDecoded += static_cast<wchar_t>(0xD800 + (scalar >> 10));
			asDecoded += static_cast<wchar_t>(0xDC00 + (scalar & 0x3FF));
		}
		else asDecoded += static_cast<wchar_t>(scalar);
#else
		asDecoded += static_cast<wchar_t>(scalar);
#endif
	}
	return true;
}

std::string EncodeUtf8(const std::wstring& asText)
{
	std::string encoded;
	for (std::wstring::size_type index = 0; index < asText.size(); ++index)
	{
		unsigned int scalar = static_cast<unsigned int>(asText[index]);
#if WCHAR_MAX <= 0xFFFF
		if (scalar >= 0xD800 && scalar <= 0xDBFF && index + 1 < asText.size())
		{
			const unsigned int low = static_cast<unsigned int>(asText[++index]);
			scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
		}
#endif
		if (scalar <= 0x7F) encoded += static_cast<char>(scalar);
		else if (scalar <= 0x7FF)
		{
			encoded += static_cast<char>(0xC0 | (scalar >> 6));
			encoded += static_cast<char>(0x80 | (scalar & 0x3F));
		}
		else if (scalar <= 0xFFFF)
		{
			encoded += static_cast<char>(0xE0 | (scalar >> 12));
			encoded += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
			encoded += static_cast<char>(0x80 | (scalar & 0x3F));
		}
		else
		{
			encoded += static_cast<char>(0xF0 | (scalar >> 18));
			encoded += static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
			encoded += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
			encoded += static_cast<char>(0x80 | (scalar & 0x3F));
		}
	}
	return encoded;
}
