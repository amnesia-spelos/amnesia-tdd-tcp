#include "LogFilesModel.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	typedef std::vector<std::wstring> tNames;

	std::string Narrow(const std::wstring& asText)
	{
		std::string sText;
		for (size_t i = 0; i < asText.size(); ++i) sText += static_cast<char>(asText[i]);
		return sText;
	}

	void Fail(const char* apDescription, const std::string& asExpected, const std::string& asActual)
	{
		std::cerr << "FAIL: " << apDescription << "\n  expected: " << asExpected << "\n  actual:   " << asActual << "\n";
		exit(1);
	}

	void ExpectName(const std::wstring& asActual, const std::wstring& asExpected, const char* apDescription)
	{
		if (asActual == asExpected) return;
		Fail(apDescription, Narrow(asExpected), Narrow(asActual));
	}

	tNames Names(const wchar_t* a0 = NULL, const wchar_t* a1 = NULL, const wchar_t* a2 = NULL,
		const wchar_t* a3 = NULL, const wchar_t* a4 = NULL, const wchar_t* a5 = NULL, const wchar_t* a6 = NULL,
		const wchar_t* a7 = NULL)
	{
		const wchar_t* vNames[] = { a0, a1, a2, a3, a4, a5, a6, a7 };
		tNames names;
		for (size_t i = 0; i < sizeof(vNames) / sizeof(vNames[0]) && vNames[i] != NULL; ++i) names.push_back(vNames[i]);
		return names;
	}

	std::string Describe(tNames avNames)
	{
		std::sort(avNames.begin(), avNames.end());
		std::string sText = "[";
		for (size_t i = 0; i < avNames.size(); ++i) sText += (i == 0 ? "" : ", ") + Narrow(avNames[i]);
		return sText + "]";
	}

	// The order of the names to delete does not matter
	void ExpectNames(const tNames& avActual, const tNames& avExpected, const char* apDescription)
	{
		if (Describe(avActual) == Describe(avExpected)) return;
		Fail(apDescription, Describe(avExpected), Describe(avActual));
	}

	void ExpectCount(int alActual, int alExpected, const char* apDescription)
	{
		if (alActual == alExpected) return;
		Fail(apDescription, std::to_string(alExpected), std::to_string(alActual));
	}

	std::tm StartTime(int alYear, int alMonth, int alDay, int alHour, int alMinute, int alSecond)
	{
		std::tm time = std::tm();
		time.tm_year = alYear - 1900;
		time.tm_mon = alMonth - 1;
		time.tm_mday = alDay;
		time.tm_hour = alHour;
		time.tm_min = alMinute;
		time.tm_sec = alSecond;
		return time;
	}
}

int main()
{
	// Each run's pair of logs is named by the local time the game started (issue #70)
	cLogFileNames names = MakeLogFileNames(StartTime(2026, 3, 7, 9, 5, 2));
	ExpectName(names.msLog, L"hpl-20260307-090502.log", "the game log is named hpl-yyyyMMdd-HHmmss.log");
	ExpectName(names.msUpdateLog, L"hpl_update-20260307-090502.log",
		"the update log is named hpl_update-yyyyMMdd-HHmmss.log with the same timestamp");

	// The newest N logs are kept, decided by the timestamp in their names, whatever order the folder lists them
	ExpectNames(
		ChooseLogFilesToPrune(
			Names(L"hpl-20260301-120000.log", L"hpl-20260310-080000.log", L"hpl-20251231-235959.log",
				L"hpl-20260305-000000.log", L"hpl-20260309-235959.log"),
			L"hpl-", L"hpl-20260310-080000.log", 3),
		Names(L"hpl-20260301-120000.log", L"hpl-20251231-235959.log"),
		"pruning deletes all but the newest 3 logs");

	// Only hpl-yyyyMMdd-HHmmss.log names are logs this game rotates; the original game's hpl.log is never touched
	ExpectNames(
		ChooseLogFilesToPrune(
			Names(L"hpl.log", L"hpl-notes.log", L"hpl-2026030-120000.log", L"hpl-20260301-1200000.log",
				L"hpl-20260301-120000.txt", L"hpl-20260301-120000.log", L"hpl-20260310-080000.log"),
			L"hpl-", L"hpl-20260310-080000.log", 1),
		Names(L"hpl-20260301-120000.log"),
		"pruning ignores files that do not match the log name pattern");

	// The clock moved back since earlier runs, so this run's log is not the newest by name
	ExpectNames(
		ChooseLogFilesToPrune(
			Names(L"hpl-20260310-080000.log", L"hpl-20260309-080000.log", L"hpl-20260308-080000.log",
				L"hpl-20260101-080000.log"),
			L"hpl-", L"hpl-20260101-080000.log", 2),
		Names(L"hpl-20260309-080000.log", L"hpl-20260308-080000.log"),
		"pruning never deletes the current run's log, which counts as one of the logs kept");

	// A count below 1 still keeps the current run's log
	ExpectNames(
		ChooseLogFilesToPrune(Names(L"hpl-20260310-080000.log", L"hpl-20260309-080000.log"),
			L"hpl-", L"hpl-20260310-080000.log", 0),
		Names(L"hpl-20260309-080000.log"),
		"a count of 0 is treated as 1");
	ExpectNames(
		ChooseLogFilesToPrune(Names(L"hpl-20260310-080000.log", L"hpl-20260309-080000.log"),
			L"hpl-", L"hpl-20260310-080000.log", -5),
		Names(L"hpl-20260309-080000.log"),
		"a negative count is treated as 1");

	// Game logs and update logs share the folder but are pruned separately
	tNames vMixedFolder = Names(L"hpl-20260310-080000.log", L"hpl_update-20260310-080000.log",
		L"hpl-20260309-080000.log", L"hpl_update-20260309-080000.log",
		L"hpl-20260308-080000.log", L"hpl_update-20260308-080000.log", L"hpl_update-20260307-080000.log");
	ExpectNames(
		ChooseLogFilesToPrune(vMixedFolder, L"hpl-", L"hpl-20260310-080000.log", 2),
		Names(L"hpl-20260308-080000.log"),
		"pruning game logs leaves update logs alone");
	ExpectNames(
		ChooseLogFilesToPrune(vMixedFolder, L"hpl_update-", L"hpl_update-20260310-080000.log", 2),
		Names(L"hpl_update-20260308-080000.log", L"hpl_update-20260307-080000.log"),
		"pruning update logs leaves game logs alone");

	// LogFilesToKeep as written in main_settings.cfg, or empty when the key is missing
	ExpectCount(ParseLogFilesToKeep("3"), 3, "a number is the count of logs to keep");
	ExpectCount(ParseLogFilesToKeep(""), 10, "a missing count keeps 10 logs");
	ExpectCount(ParseLogFilesToKeep("lots"), 10, "a count that is not a number keeps 10 logs");
	ExpectCount(ParseLogFilesToKeep("3 logs"), 10, "a count with trailing text is not a number");
	ExpectCount(ParseLogFilesToKeep("-4"), -4, "a negative count is a number, which pruning treats as 1");

	std::cout << "Log Files model cases passed\n";
	return 0;
}
