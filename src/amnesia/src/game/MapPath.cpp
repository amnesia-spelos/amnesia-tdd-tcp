#include "MapPath.h"

#include <cctype>

namespace
{
	std::string WithForwardSlashes(std::string asPath)
	{
		for (size_t i = 0; i < asPath.size(); ++i)
		{
			if (asPath[i] == '\\') asPath[i] = '/';
		}
		return asPath;
	}

	// Windows folder names ignore letter case, so the install folder may be spelled differently.
	bool StartsWithIgnoringCase(const std::string& asText, const std::string& asPrefix)
	{
		if (asText.size() < asPrefix.size()) return false;
		for (size_t i = 0; i < asPrefix.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(asText[i])) !=
				std::tolower(static_cast<unsigned char>(asPrefix[i]))) return false;
		}
		return true;
	}
}

std::string MakeMapPath(const std::string& asResolvedFile, const std::string& asInstallFolder,
	const std::string& asMapFolder, const std::string& asRequestedFile)
{
	const std::string sResolvedFile = WithForwardSlashes(asResolvedFile);
	std::string sInstallFolder = WithForwardSlashes(asInstallFolder);
	if (sInstallFolder.empty() || sInstallFolder[sInstallFolder.size() - 1] != '/') sInstallFolder += "/";
	if (StartsWithIgnoringCase(sResolvedFile, sInstallFolder))
	{
		return sResolvedFile.substr(sInstallFolder.size());
	}
	const std::string sRequestedFile = WithForwardSlashes(asRequestedFile);
	return WithForwardSlashes(asMapFolder) + sRequestedFile.substr(sRequestedFile.find_last_of('/') + 1);
}
