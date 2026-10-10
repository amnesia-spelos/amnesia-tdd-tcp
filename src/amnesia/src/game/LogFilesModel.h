#ifndef LOG_FILES_MODEL_H
#define LOG_FILES_MODEL_H

#include <ctime>
#include <string>
#include <vector>

// The pair of log files one game run writes into the save folder (issue #70).
struct cLogFileNames
{
	std::wstring msLog;
	std::wstring msUpdateLog;
};

// What each kind of log's file name starts with, before its yyyyMMdd-HHmmss.log timestamp
extern const wchar_t* const kLogFilePrefix;
extern const wchar_t* const kUpdateLogFilePrefix;

// Names a run's logs hpl-yyyyMMdd-HHmmss.log and hpl_update-yyyyMMdd-HHmmss.log from the local time it started.
cLogFileNames MakeLogFileNames(const std::tm& aStartTime);

// Of the file names in the save folder, the ones to delete so that only the newest alNumberToKeep logs named
// <asPrefix>yyyyMMdd-HHmmss.log remain. Newest is decided by the timestamp in the name. The current run's log is
// never deleted and counts as one of those kept, so a count below 1 keeps 1.
std::vector<std::wstring> ChooseLogFilesToPrune(const std::vector<std::wstring>& avFileNames,
	const std::wstring& asPrefix, const std::wstring& asCurrentFile, int alNumberToKeep);

// The number of logs of each kind to keep, from the Main element's LogFilesToKeep value in main_settings.cfg
// (empty when the key is missing): 10 unless it is a whole number.
int ParseLogFilesToKeep(const std::string& asValue);

#endif // LOG_FILES_MODEL_H
