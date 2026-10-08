#ifndef MAP_PATH_H
#define MAP_PATH_H

#include <string>

// The Map Path that names a map to Peers (issue #64): the file the engine's file searcher resolved,
// relative to the game's install folder (its working folder), so it is the same on every installation
// whatever string a level door or script used to name the map. A file the searcher cannot resolve, or
// one outside the install folder, is named by its map folder and file name. Separators are always '/'.
// Engine-independent; cLuxMapHandler::LoadMap computes it once per map.
std::string MakeMapPath(const std::string& asResolvedFile, const std::string& asInstallFolder,
	const std::string& asMapFolder, const std::string& asRequestedFile);

#endif // MAP_PATH_H
