#include "BuildVersion.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void ExpectLogLine(const std::string& asActual, const std::string& asExpected, const char* apDescription)
	{
		if (asActual == asExpected) return;
		std::cerr << "FAIL: " << apDescription << "\n  expected: " << asExpected << "\n  actual:   " << asActual << "\n";
		exit(1);
	}
}

int main()
{
	ExpectLogLine(
		MakeBuildVersionLogLine("v0.2.0", 2),
		"Amnesia TDD TCP v0.2.0 (Protocol Versions: legacy, 2)",
		"a build at a release tag names that tag");

	ExpectLogLine(
		MakeBuildVersionLogLine("v0.2.0-3-gff6db40-dirty", 2),
		"Amnesia TDD TCP v0.2.0-3-gff6db40-dirty (Protocol Versions: legacy, 2)",
		"a build after a tag with uncommitted changes names the commit and that it is dirty");

	ExpectLogLine(
		MakeBuildVersionLogLine("unknown", 2),
		"Amnesia TDD TCP unknown (Protocol Versions: legacy, 2)",
		"a build without git says its Build Version is unknown");

	ExpectLogLine(
		MakeBuildVersionLogLine("v1.0.0", 3),
		"Amnesia TDD TCP v1.0.0 (Protocol Versions: legacy, 3)",
		"names whatever negotiable Protocol Version it is given");

	std::cout << "Build Version cases passed\n";
	return 0;
}
