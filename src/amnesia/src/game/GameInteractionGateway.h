#ifndef GAME_INTERACTION_GATEWAY_H
#define GAME_INTERACTION_GATEWAY_H

#include "ChatModel.h"

#include <string>
#include <vector>

enum eGameInteractionEventType
{
	eGameInteractionEvent_MapChanged,
	eGameInteractionEvent_ScriptCallObserved,
	eGameInteractionEvent_LocalChatSubmitted,
	eGameInteractionEvent_CustomStoryStarted
};

class cGameInteractionEvent
{
public:
	cGameInteractionEvent(eGameInteractionEventType aType = eGameInteractionEvent_MapChanged,
		const std::string& asData = std::string());
	cGameInteractionEvent(eGameInteractionEventType aType, const std::wstring& asChatAuthor,
		const std::wstring& asChatMessage);
	cGameInteractionEvent(eGameInteractionEventType aType, const std::wstring& asCustomStoryIdentifier);
	eGameInteractionEventType GetType() const { return mType; }
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }
	const std::wstring& GetCustomStoryIdentifier() const { return msCustomStoryIdentifier; }

private:
	eGameInteractionEventType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
	std::wstring msCustomStoryIdentifier;
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
	eGameInteractionCommand_Chat,
	eGameInteractionCommand_GetCustomStories,
	eGameInteractionCommand_StartCustomStory,
	eGameInteractionCommand_NegotiateProtocol,
	eGameInteractionCommand_AvatarCreate,
	eGameInteractionCommand_AvatarRemove,
	eGameInteractionCommand_AvatarCollision,
	eGameInteractionCommand_AvatarPose,
	eGameInteractionCommand_LocalPose
};

enum eGameInteractionCapability
{
	eGameInteractionCapability_None = 0,
	eGameInteractionCapability_Avatars = 1 << 0,
	eGameInteractionCapability_LocalPose = 1 << 1
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
	cGameInteractionCommand(eGameInteractionCommandType aType, const std::wstring& asCustomStoryIdentifier);
	// A negotiation Command. Protocol Version 0 means the requested version was malformed; the
	// Capabilities are the recognized requested ones, as a set of eGameInteractionCapability flags.
	cGameInteractionCommand(eGameInteractionCommandType aType, unsigned int alProtocolVersion,
		unsigned int alCapabilities);
	eGameInteractionCommandType GetType() const { return mType; }
	eGameInteractionCommandClassification GetClassification() const;
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }
	const std::wstring& GetCustomStoryIdentifier() const { return msCustomStoryIdentifier; }
	unsigned int GetProtocolVersion() const { return mlProtocolVersion; }
	unsigned int GetCapabilities() const { return mlCapabilities; }

private:
	eGameInteractionCommandType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
	std::wstring msCustomStoryIdentifier;
	unsigned int mlProtocolVersion;
	unsigned int mlCapabilities;
};

enum eGameInteractionResponseType
{
	eGameInteractionResponse_Pong,
	eGameInteractionResponse_Position,
	eGameInteractionResponse_Rotation,
	eGameInteractionResponse_PositionRotation,
	eGameInteractionResponse_Map,
	eGameInteractionResponse_ScriptExecuted,
	eGameInteractionResponse_ChatDisplayed,
	eGameInteractionResponse_CustomStories,
	eGameInteractionResponse_CustomStoryStarting,
	eGameInteractionResponse_ProtocolNegotiated,
	eGameInteractionResponse_Rejected
};

enum eGameInteractionCommandOutcome
{
	eGameInteractionCommandOutcome_Success,
	eGameInteractionCommandOutcome_MapNotLoaded,
	eGameInteractionCommandOutcome_InvalidAuthor,
	eGameInteractionCommandOutcome_InvalidMessage,
	eGameInteractionCommandOutcome_Unavailable,
	eGameInteractionCommandOutcome_NotInMainMenu,
	eGameInteractionCommandOutcome_CustomStoryNotFound,
	eGameInteractionCommandOutcome_CustomStoryInvalid,
	eGameInteractionCommandOutcome_Invalid,
	eGameInteractionCommandOutcome_UnsupportedProtocolVersion,
	eGameInteractionCommandOutcome_AlreadyNegotiated,
	eGameInteractionCommandOutcome_NegotiationTooLate,
	eGameInteractionCommandOutcome_CapabilityNotGranted
};

enum eGameInteractionCustomStoryAvailability
{
	eGameInteractionCustomStoryAvailability_Available,
	eGameInteractionCustomStoryAvailability_NotInMainMenu,
	eGameInteractionCustomStoryAvailability_NotFound,
	eGameInteractionCustomStoryAvailability_Invalid
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

class cGameInteractionCustomStory
{
public:
	cGameInteractionCustomStory(const std::wstring& asIdentifier, const std::wstring& asName)
		: msIdentifier(asIdentifier), msName(asName) {}
	const std::wstring& GetIdentifier() const { return msIdentifier; }
	const std::wstring& GetName() const { return msName; }

private:
	std::wstring msIdentifier;
	std::wstring msName;
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
	virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const = 0;
	virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(
		const std::wstring& asIdentifier) const = 0;
	virtual void StartCustomStory(const std::wstring& asIdentifier) = 0;
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
	const std::vector<cGameInteractionCustomStory>& GetCustomStories() const { return mvCustomStories; }
	unsigned int GetCapabilities() const { return mlCapabilities; }
	void SetPosition(const cGameInteractionPosition& aPosition) { mPosition = aPosition; }
	void SetRotation(const cGameInteractionRotation& aRotation) { mRotation = aRotation; }
	void SetMapFile(const std::string& asMapFile) { msMapFile = asMapFile; }
	void SetCustomStories(const std::vector<cGameInteractionCustomStory>& avCustomStories)
	{
		mvCustomStories = avCustomStories;
	}
	void SetCapabilities(unsigned int alCapabilities) { mlCapabilities = alCapabilities; }

private:
	eGameInteractionCommandType mCommandType;
	eGameInteractionResponseType mType;
	eGameInteractionCommandOutcome mOutcome;
	cGameInteractionPosition mPosition;
	cGameInteractionRotation mRotation;
	std::string msMapFile;
	std::vector<cGameInteractionCustomStory> mvCustomStories;
	unsigned int mlCapabilities;
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
