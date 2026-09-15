#ifndef LEGACY_GAME_INTERACTION_PROTOCOL_H
#define LEGACY_GAME_INTERACTION_PROTOCOL_H

#include <string>

struct cLegacyPeerState
{
	bool mbMapLoaded;
	float mfPositionX;
	float mfPositionY;
	float mfPositionZ;
	float mfYawRadians;
	float mfPitchRadians;
	std::string msMapFile;
};

class iLegacyGameAdapter
{
public:
	virtual ~iLegacyGameAdapter() {}
	virtual bool IsMapLoaded() const = 0;
	virtual cLegacyPeerState GetPeerState() const = 0;
	virtual std::string GetMapFile() const = 0;
	virtual void RunScript(const std::string& asScript) = 0;
};

class cLegacyGameInteractionProtocol
{
public:
	explicit cLegacyGameInteractionProtocol(iLegacyGameAdapter& aGameAdapter);

	static const char* Greeting();
	static std::string MapChangedEvent(const std::string& asMapFile);
	static std::string ScriptCallObservation(const std::string& asScriptCall);
	static std::string ToWireLine(const std::string& asMessage);
	static std::string FirstCommandFromReceive(const std::string& asReceivedBytes);

	std::string HandleCommand(const std::string& asCommand);

private:
	iLegacyGameAdapter& mGameAdapter;
};

#endif
