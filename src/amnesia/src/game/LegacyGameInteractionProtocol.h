#ifndef LEGACY_GAME_INTERACTION_PROTOCOL_H
#define LEGACY_GAME_INTERACTION_PROTOCOL_H

#include "GameInteractionGateway.h"

#include <string>

class cGameInteractionLineBuffer
{
public:
	cGameInteractionLineBuffer();
	void Append(const char* apBytes, std::string::size_type aLength);
	bool TryPopLine(std::string& asLine);
	void Clear();

private:
	std::string msPendingBytes;
	std::string::size_type mSearchStart;
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
