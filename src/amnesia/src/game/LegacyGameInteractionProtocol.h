#ifndef LEGACY_GAME_INTERACTION_PROTOCOL_H
#define LEGACY_GAME_INTERACTION_PROTOCOL_H

#include "GameInteractionGateway.h"

#include <string>

class cGameInteractionLineBuffer
{
public:
	// Generous enough for long legacy exec: scripts while bounding what a Peer can make the game buffer.
	static const std::string::size_type kDefaultMaximumLineLength = 64 * 1024;

	explicit cGameInteractionLineBuffer(std::string::size_type aMaximumLineLength = kDefaultMaximumLineLength);
	void Append(const char* apBytes, std::string::size_type aLength);
	// Stops extracting once a line longer than the maximum is found, complete or not.
	bool TryPopLine(std::string& asLine);
	bool HasExceededLineLimit() const { return mbExceededLineLimit; }
	void Clear();

private:
	std::string msPendingBytes;
	std::string::size_type mSearchStart;
	std::string::size_type mMaximumLineLength;
	bool mbExceededLineLimit;
};

class cLegacyGameInteractionProtocol
{
public:
	static const char* Greeting();
	static std::string SerializeEvent(const cGameInteractionEvent& aEvent);
	static cGameInteractionCommand ParseCommand(const std::string& asCommand);
	static std::string SerializeResponse(const cGameInteractionResponse& aResponse);
	static std::string ToWireLine(const std::string& asMessage);
};

#endif
