#include "ObjectSearch.h"

#include <cwctype>

namespace
{
	std::wstring ToLower(std::wstring asText)
	{
		for (size_t i = 0; i < asText.size(); ++i)
		{
			asText[i] = static_cast<wchar_t>(std::towlower(asText[i]));
		}
		return asText;
	}
}

std::wstring GetAssetPathCategory(const std::wstring& asAssetPath)
{
	return asAssetPath.substr(0, asAssetPath.find(L'/'));
}

bool MatchesObjectSearch(const std::wstring& asAssetPath, const std::wstring& asCategory, const std::wstring& asQuery)
{
	if (!asCategory.empty() && GetAssetPathCategory(asAssetPath) != asCategory) return false;

	const std::wstring sAssetPath = ToLower(asAssetPath);
	const std::wstring sQuery = ToLower(asQuery);

	size_t lWordStart = 0;
	while (lWordStart < sQuery.size())
	{
		if (std::iswspace(sQuery[lWordStart])) { ++lWordStart; continue; }

		size_t lWordEnd = lWordStart;
		while (lWordEnd < sQuery.size() && !std::iswspace(sQuery[lWordEnd])) ++lWordEnd;

		if (sAssetPath.find(sQuery.substr(lWordStart, lWordEnd - lWordStart)) == std::wstring::npos) return false;
		lWordStart = lWordEnd;
	}
	return true;
}
