#ifndef UTF8_TEXT_H
#define UTF8_TEXT_H

#include <string>

// Conversions between UTF-8 and wide strings, which hold UTF-16 where wchar_t is 16 bits.
// Rejects malformed UTF-8, including overlong forms and encoded surrogates.
bool TryDecodeUtf8(const std::string& asText, std::wstring& asDecoded);
std::string EncodeUtf8(const std::wstring& asText);

#endif
