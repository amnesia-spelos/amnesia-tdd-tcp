#include "LogFilesModel.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <functional>

const wchar_t* const kLogFilePrefix = L"hpl-";
const wchar_t* const kUpdateLogFilePrefix = L"hpl_update-";

namespace
{
	// <asPrefix>yyyyMMdd-HHmmss.log
	bool IsLogFileName(const std::wstring& asFileName, const std::wstring& asPrefix)
	{
		const std::wstring sStampPattern = L"########-######.log";
		if (asFileName.size() != asPrefix.size() + sStampPattern.size()) return false;
		if (asFileName.compare(0, asPrefix.size(), asPrefix) != 0) return false;

		for (size_t i = 0; i < sStampPattern.size(); ++i)
		{
			wchar_t c = asFileName[asPrefix.size() + i];
			bool bMatches = sStampPattern[i] == L'#' ? (c >= L'0' && c <= L'9') : c == sStampPattern[i];
			if (bMatches == false) return false;
		}
		return true;
	}
}

cLogFileNames MakeLogFileNames(const std::tm& aStartTime)
{
	wchar_t sStamp[32];
	std::wcsftime(sStamp, sizeof(sStamp) / sizeof(sStamp[0]), L"%Y%m%d-%H%M%S", &aStartTime);

	cLogFileNames names;
	names.msLog = std::wstring(kLogFilePrefix) + sStamp + L".log";
	names.msUpdateLog = std::wstring(kUpdateLogFilePrefix) + sStamp + L".log";
	return names;
}

std::vector<std::wstring> ChooseLogFilesToPrune(const std::vector<std::wstring>& avFileNames,
	const std::wstring& asPrefix, const std::wstring& asCurrentFile, int alNumberToKeep)
{
	std::vector<std::wstring> vLogs;
	for (size_t i = 0; i < avFileNames.size(); ++i)
	{
		if (avFileNames[i] != asCurrentFile && IsLogFileName(avFileNames[i], asPrefix)) vLogs.push_back(avFileNames[i]);
	}

	// Under one prefix, the yyyyMMdd-HHmmss timestamp makes name order the order the runs started
	std::sort(vLogs.begin(), vLogs.end(), std::greater<std::wstring>());

	// The current run's log is always kept, and counts as one of the logs kept
	size_t lOthersToKeep = alNumberToKeep < 1 ? 0 : static_cast<size_t>(alNumberToKeep) - 1;
	std::vector<std::wstring> vToDelete;
	for (size_t i = lOthersToKeep; i < vLogs.size(); ++i) vToDelete.push_back(vLogs[i]);
	return vToDelete;
}

int ParseLogFilesToKeep(const std::string& asValue)
{
	const int lDefault = 10;
	if (asValue.empty()) return lDefault;

	// cConfigFile::GetInt would read a value that is not a number as 0
	char* pEnd = NULL;
	long lValue = std::strtol(asValue.c_str(), &pEnd, 10);
	if (pEnd != asValue.c_str() + asValue.size()) return lDefault;

	return static_cast<int>(lValue);
}
