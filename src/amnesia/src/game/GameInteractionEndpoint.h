#ifndef GAME_INTERACTION_ENDPOINT_H
#define GAME_INTERACTION_ENDPOINT_H

#include <string>

// The address and TCP port the game listens on for Game Interaction Protocol Peers (issue #109).
struct cGameInteractionEndpoint
{
	std::string msHost;
	int mlPort;
};

// Reads an endpoint back from text, as the options tab and the main config hold it. An empty host, or a
// port that is not a whole number from 1 to 65535, falls back to the default 127.0.0.1:5150.
// Engine-independent; cLuxSocketServer and the options tab both read through it.
cGameInteractionEndpoint ParseGameInteractionEndpoint(const std::string& asHost, const std::string& asPort);

#endif // GAME_INTERACTION_ENDPOINT_H
