#ifndef GAME_INTERACTION_GATEWAY_H
#define GAME_INTERACTION_GATEWAY_H

#include <string>

enum eGameInteractionCommandType
{
	eGameInteractionCommand_Ping,
	eGameInteractionCommand_GetPosition,
	eGameInteractionCommand_GetRotation,
	eGameInteractionCommand_GetPositionRotation,
	eGameInteractionCommand_GetMap,
	eGameInteractionCommand_ExecuteScript
};

enum eGameInteractionCommandClassification
{
	eGameInteractionCommandClassification_Observational,
	eGameInteractionCommandClassification_StateChanging
};

class cGameInteractionCommand
{
public:
	explicit cGameInteractionCommand(eGameInteractionCommandType aType,
		const std::string& asData = std::string());
	eGameInteractionCommandType GetType() const { return mType; }
	eGameInteractionCommandClassification GetClassification() const;
	const std::string& GetData() const { return msData; }

private:
	eGameInteractionCommandType mType;
	std::string msData;
};

enum eGameInteractionResponseType
{
	eGameInteractionResponse_Pong,
	eGameInteractionResponse_Position,
	eGameInteractionResponse_Rotation,
	eGameInteractionResponse_PositionRotation,
	eGameInteractionResponse_Map,
	eGameInteractionResponse_ScriptExecuted
};

enum eGameInteractionCommandOutcome
{
	eGameInteractionCommandOutcome_Success,
	eGameInteractionCommandOutcome_MapNotLoaded
};

struct cGameInteractionPosition
{
	cGameInteractionPosition(float afX = 0.0f, float afY = 0.0f, float afZ = 0.0f)
		: mfX(afX), mfY(afY), mfZ(afZ) {}
	float mfX;
	float mfY;
	float mfZ;
};

struct cGameInteractionRotation
{
	cGameInteractionRotation(float afYawRadians = 0.0f, float afPitchRadians = 0.0f)
		: mfYawRadians(afYawRadians), mfPitchRadians(afPitchRadians) {}
	float mfYawRadians;
	float mfPitchRadians;
};

class iGameInteractionGameAdapter
{
public:
	virtual ~iGameInteractionGameAdapter() {}
	virtual bool IsMapLoaded() const = 0;
	virtual cGameInteractionPosition GetPosition() const = 0;
	virtual cGameInteractionRotation GetRotation() const = 0;
	virtual std::string GetMapFile() const = 0;
	virtual void RunScript(const std::string& asScript) = 0;
};

class cGameInteractionResponse
{
public:
	cGameInteractionResponse(eGameInteractionCommandType aCommandType, eGameInteractionResponseType aType,
		eGameInteractionCommandOutcome aOutcome = eGameInteractionCommandOutcome_Success);
	eGameInteractionCommandType GetCommandType() const { return mCommandType; }
	eGameInteractionResponseType GetType() const { return mType; }
	eGameInteractionCommandOutcome GetOutcome() const { return mOutcome; }
	const cGameInteractionPosition& GetPosition() const { return mPosition; }
	const cGameInteractionRotation& GetRotation() const { return mRotation; }
	const std::string& GetMapFile() const { return msMapFile; }
	void SetPosition(const cGameInteractionPosition& aPosition) { mPosition = aPosition; }
	void SetRotation(const cGameInteractionRotation& aRotation) { mRotation = aRotation; }
	void SetMapFile(const std::string& asMapFile) { msMapFile = asMapFile; }

private:
	eGameInteractionCommandType mCommandType;
	eGameInteractionResponseType mType;
	eGameInteractionCommandOutcome mOutcome;
	cGameInteractionPosition mPosition;
	cGameInteractionRotation mRotation;
	std::string msMapFile;
};

class cGameInteractionGateway
{
public:
	cGameInteractionGateway();
	void BeginLegacySession();
	void EndSession();
	bool CanStartQueuedCommand() const { return mbSessionActive; }
	cGameInteractionResponse Handle(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter) const;

private:
	bool mbSessionActive;
};

#endif
