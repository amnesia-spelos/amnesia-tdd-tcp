#include "MapPath.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void ExpectMapPath(const std::string& asActual, const std::string& asExpected, const char* apDescription)
	{
		if (asActual == asExpected) return;
		std::cerr << "FAIL: " << apDescription << "\n  expected: " << asExpected << "\n  actual:   " << asActual << "\n";
		exit(1);
	}
}

int main()
{
	// A level door that names its map by the author's absolute path (issue #64)
	ExpectMapPath(
		MakeMapPath(
			"C:/Games/Amnesia/custom_stories/mp-test-cs/maps/alt-map.map",
			"C:\\Games\\Amnesia",
			"custom_stories/mp-test-cs/maps/",
			"D:/SteamLibrary/steamapps/common/Amnesia The Dark Descent/custom_stories/mp-test-cs/maps/alt-map.map"),
		"custom_stories/mp-test-cs/maps/alt-map.map",
		"a map under the install folder is named relative to it, whatever the level door wrote");

	// The install folder ends in a separator only when it is a drive root
	ExpectMapPath(
		MakeMapPath("C:/maps/main/00_rainy_hall.map", "C:\\", "maps/main/",
			"D:/Amnesia/maps/main/00_rainy_hall.map"),
		"maps/main/00_rainy_hall.map",
		"an install folder at a drive root is stripped with its one separator");

	// Windows folder names match whatever their letter case
	ExpectMapPath(
		MakeMapPath("C:/Games/Amnesia/custom_stories/Story/maps/a.map", "c:\\games\\amnesia", "custom_stories/Story/maps/",
			"D:/Elsewhere/custom_stories/Story/maps/a.map"),
		"custom_stories/Story/maps/a.map",
		"the install folder matches the resolved file ignoring letter case, and the rest keeps its case");

	// A file the install folder does not contain
	ExpectMapPath(
		MakeMapPath("E:/Mods/Story/maps/alt-map.map", "C:\\Games\\Amnesia", "custom_stories/Story/maps/",
			"D:\\Author\\custom_stories\\Story\\maps\\alt-map.map"),
		"custom_stories/Story/maps/alt-map.map",
		"a map outside the install folder is named by its map folder and file name");
	ExpectMapPath(
		MakeMapPath("C:/Games/Amnesia2/maps/alt-map.map", "C:\\Games\\Amnesia", "maps/", "alt-map.map"),
		"maps/alt-map.map",
		"a sibling folder that shares the install folder's name is outside it");

	// The searcher finds no .map when the map ships only compressed (.cmap)
	ExpectMapPath(
		MakeMapPath("", "C:\\Games\\Amnesia", "custom_stories\\Story\\maps\\", "alt-map.map"),
		"custom_stories/Story/maps/alt-map.map",
		"a map the searcher cannot resolve is named by its map folder and file name, with / separators");

	std::cout << "Map Path cases passed\n";
	return 0;
}
