#ifndef HPLEDITOR_OBJECT_SEARCH_H
#define HPLEDITOR_OBJECT_SEARCH_H

#include <string>

// The Category an Asset Path begins with.
std::wstring GetAssetPathCategory(const std::wstring& asAssetPath);

// Whether the Object Browser's search lists an asset (issue #105). asCategory is the search scope:
// empty means All, otherwise only assets in that Category match. A query matches when every
// whitespace-separated word appears, ignoring letter case, somewhere in the Asset Path.
// Engine-independent; cEditorWindowObjectBrowser runs it over the Asset Paths it walked.
bool MatchesObjectSearch(const std::wstring& asAssetPath, const std::wstring& asCategory, const std::wstring& asQuery);

#endif // HPLEDITOR_OBJECT_SEARCH_H
