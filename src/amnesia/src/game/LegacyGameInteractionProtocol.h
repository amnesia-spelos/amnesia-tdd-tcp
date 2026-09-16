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
	cLegacyGameInteractionProtocol(cGameInteractionGateway& aGateway, iGameInteractionGameAdapter& aGameAdapter);

	static const char* Greeting();
	static std::string SerializeEvent(const cGameInteractionEvent& aEvent);
	static std::string ToWireLine(const std::string& asMessage);
	static std::string FirstCommandFromReceive(const std::string& asReceivedBytes);

	std::string HandleCommand(const std::string& asCommand);

private:
	cGameInteractionGateway& mGateway;
	iGameInteractionGameAdapter& mGameAdapter;
};

#endif
