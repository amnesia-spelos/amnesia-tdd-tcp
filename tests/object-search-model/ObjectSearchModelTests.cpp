#include "ObjectSearch.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	const std::wstring gsAll = L"";

	void ExpectMatch(bool abActual, bool abExpected, const char* apDescription)
	{
		if (abActual == abExpected) return;
		std::cerr << "FAIL: " << apDescription << "\n  expected: " << (abExpected ? "match" : "no match")
			<< "\n  actual:   " << (abActual ? "match" : "no match") << "\n";
		exit(1);
	}
}

int main()
{
	// A one-word query anywhere in the Asset Path, whatever its letter case
	ExpectMatch(MatchesObjectSearch(L"furniture/chairs/chair_wood_01", gsAll, L"Wood"), true,
		"a word in the name matches ignoring letter case");

	// Several words, each anywhere in the Asset Path and in any order
	ExpectMatch(MatchesObjectSearch(L"furniture/chairs/chair_wood_01", gsAll, L"wood  chairs"), true,
		"every word found, in any order and across folders and name, matches");
	ExpectMatch(MatchesObjectSearch(L"furniture/chairs/chair_wood_01", gsAll, L"wood metal"), false,
		"a query with one word missing does not match");

	// A Category scope matches only that Category's assets
	ExpectMatch(MatchesObjectSearch(L"furniture/chairs/chair_wood_01", L"furniture", L"wood"), true,
		"an asset in the scoped Category matches");
	ExpectMatch(MatchesObjectSearch(L"church/benches/bench_wood", L"furniture", L"wood"), false,
		"an asset in another Category does not match");
	ExpectMatch(MatchesObjectSearch(L"furniture_old/chair_wood", L"furniture", L"wood"), false,
		"a Category whose name starts with the scope's name is another Category");

	// A query with no words leaves only the scope to decide
	ExpectMatch(MatchesObjectSearch(L"furniture/chairs/chair_wood_01", L"furniture", L"   "), true,
		"a blank query matches every asset in the scoped Category");

	std::cout << "Object Search cases passed\n";
	return 0;
}
