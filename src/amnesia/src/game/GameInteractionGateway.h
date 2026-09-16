#ifndef GAME_INTERACTION_GATEWAY_H
#define GAME_INTERACTION_GATEWAY_H

#include "ChatModel.h"

#include <string>

enum eGameInteractionEventType
{
	eGameInteractionEvent_MapChanged,
	eGameInteractionEvent_ScriptCallObserved,
	eGameInteractionEvent_LocalChatSubmitted
};

class cGameInteractionEvent
{
public:
	cGameInteractionEvent(eGameInteractionEventType aType = eGameInteractionEvent_MapChanged,
		const std::string& asData = std::string());
	cGameInteractionEvent(eGameInteractionEventType aType, const std::wstring& asChatAuthor,
		const std::wstring& asChatMessage);
	eGameInteractionEventType GetType() const { return mType; }
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }

private:
	eGameInteractionEventType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
};

enum eGameInteractionCommandType
{
	eGameInteractionCommand_Unknown,
	eGameInteractionCommand_Ping,
	eGameInteractionCommand_GetPosition,
	eGameInteractionCommand_GetRotation,
	eGameInteractionCommand_GetPositionRotation,
	eGameInteractionCommand_GetMap,
	eGameInteractionCommand_ExecuteScript,
	eGameInteractionCommand_Chat
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
	cGameInteractionCommand(eGameInteractionCommandType aType, const std::wstring& asChatAuthor,
		const std::wstring& asChatMessage);
	eGameInteractionCommandType GetType() const { return mType; }
	eGameInteractionCommandClassification GetClassification() const;
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }

private:
	eGameInteractionCommandType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
};

enum eGameInteractionResponseType
{
	eGameInteractionResponse_Pong,
	eGameInteractionResponse_Position,
	eGameInteractionResponse_Rotation,
	eGameInteractionResponse_PositionRotation,
	eGameInteractionResponse_Map,
	eGameInteractionResponse_ScriptExecuted,
	eGameInteractionResponse_ChatDisplayed
};

enum eGameInteractionCommandOutcome
{
	eGameInteractionCommandOutcome_Success,
	eGameInteractionCommandOutcome_MapNotLoaded,
	eGameInteractionCommandOutcome_InvalidAuthor,
	eGameInteractionCommandOutcome_InvalidMessage,
	eGameInteractionCommandOutcome_Unavailable
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
	virtual bool DisplayChatEntry(const cChatEntry& aEntry) = 0;
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
	~cGameInteractionGateway();

	bool Listen(const std::string& asHost, int alPort);
	void Shutdown();
	void Update(iGameInteractionGameAdapter& aGameAdapter);
	void Report(const cGameInteractionEvent& aEvent);

	int GetPort() const;
	const std::string& GetDiagnostic() const;

private:
	class cImplementation;
	cImplementation* mpImplementation;

	cGameInteractionGateway(const cGameInteractionGateway&);
	cGameInteractionGateway& operator=(const cGameInteractionGateway&);
};

#endif
