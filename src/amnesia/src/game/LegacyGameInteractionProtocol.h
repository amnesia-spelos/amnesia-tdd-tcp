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

class iLegacyGameAdapter : public iGameInteractionGameAdapter
{
public:
	virtual ~iLegacyGameAdapter() {}
	virtual void RunScript(const std::string& asScript) = 0;
};

class cLegacyGameInteractionProtocol
{
public:
	cLegacyGameInteractionProtocol(cGameInteractionGateway& aGateway, iLegacyGameAdapter& aGameAdapter);

	static const char* Greeting();
	static std::string MapChangedEvent(const std::string& asMapFile);
	static std::string ScriptCallObservation(const std::string& asScriptCall);
	static std::string ToWireLine(const std::string& asMessage);
	static std::string FirstCommandFromReceive(const std::string& asReceivedBytes);

	std::string HandleCommand(const std::string& asCommand);

private:
	cGameInteractionGateway& mGateway;
	iLegacyGameAdapter& mGameAdapter;
};

#endif
