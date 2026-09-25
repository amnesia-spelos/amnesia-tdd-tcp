#ifndef GAME_INTERACTION_GATEWAY_H
#define GAME_INTERACTION_GATEWAY_H

#include "ChatModel.h"

#include <string>
#include <vector>

struct cGameInteractionPosition
{
	cGameInteractionPosition(float afX = 0.0f, float afY = 0.0f, float afZ = 0.0f)
		: mfX(afX), mfY(afY), mfZ(afZ) {}
	float mfX;
	float mfY;
	float mfZ;
};

struct cGameInteractionVector
{
	cGameInteractionVector(float afX = 0.0f, float afY = 0.0f, float afZ = 0.0f)
		: mfX(afX), mfY(afY), mfZ(afZ) {}
	float mfX;
	float mfY;
	float mfZ;
};

struct cGameInteractionQuaternion
{
	cGameInteractionQuaternion(float afX = 0.0f, float afY = 0.0f, float afZ = 0.0f, float afW = 1.0f)
		: mfX(afX), mfY(afY), mfZ(afZ), mfW(afW) {}
	float mfX;
	float mfY;
	float mfZ;
	float mfW;
};

// Where a body is and how it moves, all in world space.
struct cGameInteractionBodyState
{
	cGameInteractionPosition mPosition;
	cGameInteractionQuaternion mOrientation;
	// World units per second.
	cGameInteractionVector mLinearVelocity;
	// Degrees per second.
	cGameInteractionVector mAngularVelocity;
};

// A body of a map-placed entity, named by the entity's ID in its map file and the body's ID in its
// entity file.
struct cGameInteractionBodySample
{
	cGameInteractionBodySample() : mlEntityId(0), mlBodyId(0) {}
	int mlEntityId;
	int mlBodyId;
	cGameInteractionBodyState mState;
};

// The local report, as a reportedbodies State Update carries it, or a Peer's batch of samples, as an
// entitybodies Command carries it.
struct cGameInteractionBodySamples
{
	cGameInteractionBodySamples() : mlTimeMs(0) {}
	// Milliseconds on the sending game's monotonic clock.
	unsigned long long mlTimeMs;
	std::vector<cGameInteractionBodySample> mvBodies;
	std::string msMapFile;
};

enum eGameInteractionEventType
{
	eGameInteractionEvent_MapChanged,
	eGameInteractionEvent_ScriptCallObserved,
	eGameInteractionEvent_LocalChatSubmitted,
	eGameInteractionEvent_CustomStoryStarted,
	// The interactions Events, which only a Session granted that Capability receives.
	eGameInteractionEvent_InteractionStarted,
	eGameInteractionEvent_InteractionEnded,
	eGameInteractionEvent_ReportContact,
	eGameInteractionEvent_ReportSettled,
	eGameInteractionEvent_ReportBroke
};

// How the local player's interaction with an entity ended.
enum eGameInteractionEnding
{
	eGameInteractionEnding_Released,
	eGameInteractionEnding_Thrown,
	// The body moved out of reach and was dropped.
	eGameInteractionEnding_TooFar,
	eGameInteractionEnding_Destroyed
};

// What an interactions Event reports. The body and ending apply to interaction Events, and the state
// to a break.
struct cGameInteractionEntityEvent
{
	cGameInteractionEntityEvent()
		: mlEntityId(0), mlBodyId(0), mEnding(eGameInteractionEnding_Released) {}
	int mlEntityId;
	int mlBodyId;
	eGameInteractionEnding mEnding;
	cGameInteractionBodyState mState;
	std::string msMapFile;
};

class cGameInteractionEvent
{
public:
	cGameInteractionEvent(eGameInteractionEventType aType = eGameInteractionEvent_MapChanged,
		const std::string& asData = std::string());
	cGameInteractionEvent(eGameInteractionEventType aType, const std::wstring& asChatAuthor,
		const std::wstring& asChatMessage);
	cGameInteractionEvent(eGameInteractionEventType aType, const std::wstring& asCustomStoryIdentifier);
	cGameInteractionEvent(eGameInteractionEventType aType, const cGameInteractionEntityEvent& aEntityEvent);
	eGameInteractionEventType GetType() const { return mType; }
	bool IsInteractionsEvent() const;
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }
	const std::wstring& GetCustomStoryIdentifier() const { return msCustomStoryIdentifier; }
	const cGameInteractionEntityEvent& GetEntityEvent() const { return mEntityEvent; }

private:
	eGameInteractionEventType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
	std::wstring msCustomStoryIdentifier;
	cGameInteractionEntityEvent mEntityEvent;
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
	eGameInteractionCommand_LocalPose,
	eGameInteractionCommand_ReportedBodies,
	eGameInteractionCommand_EntityDrive,
	eGameInteractionCommand_EntityBodies,
	eGameInteractionCommand_EntityInteracting,
	eGameInteractionCommand_EntityBreak,
	eGameInteractionCommand_EntityRelease
};

enum eGameInteractionCapability
{
	eGameInteractionCapability_None = 0,
	eGameInteractionCapability_Avatars = 1 << 0,
	eGameInteractionCapability_LocalPose = 1 << 1,
	eGameInteractionCapability_Interactions = 1 << 2
};

struct cGameInteractionRotation
{
	cGameInteractionRotation(float afYawRadians = 0.0f, float afPitchRadians = 0.0f)
		: mfYawRadians(afYawRadians), mfPitchRadians(afPitchRadians) {}
	float mfYawRadians;
	float mfPitchRadians;
};

// The Pose of the local player, as a localpose State Update reports it, or of an Avatar, as an
// avatarpose Command sets it.
struct cGameInteractionPose
{
	cGameInteractionPose()
		: mlTimeMs(0), mlTeleportCounter(0), mfBodyYawDegrees(0.0f), mfCameraPitchDegrees(0.0f),
		  mbCrouching(false), mbLanternRaised(false) {}
	// Milliseconds on the sending game's monotonic clock.
	unsigned long long mlTimeMs;
	// Changes whenever the player is placed rather than moved.
	unsigned int mlTeleportCounter;
	cGameInteractionPosition mFeetPosition;
	float mfBodyYawDegrees;
	float mfCameraPitchDegrees;
	bool mbCrouching;
	// Whether the lantern is raised. Its oil level and flicker are not part of the Pose.
	bool mbLanternRaised;
	std::string msMapFile;
};

// What an Avatar Command asks for. The Avatar Identifier is kept whenever its field is valid, even
// on an otherwise malformed line, so failures can name the Avatar. An empty entity file means the
// Peer named no model.
struct cGameInteractionAvatarRequest
{
	cGameInteractionAvatarRequest() : mbValid(false), mbCollides(false) {}
	bool mbValid;
	std::string msIdentifier;
	std::string msEntityFile;
	cGameInteractionPose mPose;
	// Whether an avatarcollision Command turns the Avatar's collision with the local player on.
	bool mbCollides;
};

// What a localpose or reportedbodies Command asks for. Invalid means the line was malformed.
enum eGameInteractionSubscriptionRequest
{
	eGameInteractionSubscriptionRequest_Invalid,
	eGameInteractionSubscriptionRequest_Subscribe,
	eGameInteractionSubscriptionRequest_Unsubscribe
};

// What a Peer-Driven Entity Command asks for. Every one names the map the Peer means; entitybodies
// names its entities in its samples instead of the entity field.
struct cGameInteractionEntityRequest
{
	cGameInteractionEntityRequest() : mbValid(false), mlEntityId(0), mbInteracting(false) {}
	bool mbValid;
	int mlEntityId;
	// Whether an entityinteracting Command marks the entity as being interacted with.
	bool mbInteracting;
	// The final state an entitybreak Command snaps the entity to.
	cGameInteractionBodyState mState;
	cGameInteractionBodySamples mBodies;
	std::string msMapFile;
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
	// A localpose or reportedbodies Command. The rate is the requested State Update rate in Hz before
	// clamping.
	cGameInteractionCommand(eGameInteractionCommandType aType, eGameInteractionSubscriptionRequest aRequest,
		unsigned int alSubscriptionRate);
	cGameInteractionCommand(eGameInteractionCommandType aType, const cGameInteractionAvatarRequest& aAvatarRequest);
	cGameInteractionCommand(eGameInteractionCommandType aType, const cGameInteractionEntityRequest& aEntityRequest);
	eGameInteractionCommandType GetType() const { return mType; }
	eGameInteractionCommandClassification GetClassification() const;
	const std::string& GetData() const { return msData; }
	const std::wstring& GetChatAuthor() const { return msChatAuthor; }
	const std::wstring& GetChatMessage() const { return msChatMessage; }
	const std::wstring& GetCustomStoryIdentifier() const { return msCustomStoryIdentifier; }
	unsigned int GetProtocolVersion() const { return mlProtocolVersion; }
	unsigned int GetCapabilities() const { return mlCapabilities; }
	eGameInteractionSubscriptionRequest GetSubscriptionRequest() const { return mSubscriptionRequest; }
	unsigned int GetSubscriptionRate() const { return mlSubscriptionRate; }
	const cGameInteractionAvatarRequest& GetAvatarRequest() const { return mAvatarRequest; }
	const cGameInteractionEntityRequest& GetEntityRequest() const { return mEntityRequest; }

private:
	eGameInteractionCommandType mType;
	std::string msData;
	std::wstring msChatAuthor;
	std::wstring msChatMessage;
	std::wstring msCustomStoryIdentifier;
	unsigned int mlProtocolVersion;
	unsigned int mlCapabilities;
	eGameInteractionSubscriptionRequest mSubscriptionRequest;
	unsigned int mlSubscriptionRate;
	cGameInteractionAvatarRequest mAvatarRequest;
	cGameInteractionEntityRequest mEntityRequest;
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
	eGameInteractionResponse_Subscription,
	eGameInteractionResponse_Avatar,
	eGameInteractionResponse_Entity,
	eGameInteractionResponse_Rejected,
	// The Command is not answered, such as a successful avatarpose.
	eGameInteractionResponse_None
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
	eGameInteractionCommandOutcome_CapabilityNotGranted,
	eGameInteractionCommandOutcome_AvatarExists,
	eGameInteractionCommandOutcome_AvatarLimitReached,
	eGameInteractionCommandOutcome_AvatarModelNotFound,
	eGameInteractionCommandOutcome_AvatarNotFound,
	// The Command names a map other than the current one, or no map is loaded.
	eGameInteractionCommandOutcome_WrongMap,
	eGameInteractionCommandOutcome_EntityNotFound,
	eGameInteractionCommandOutcome_EntityNotHoldable
};

// What the game did with a Peer-Driven Entity operation on the current map.
enum eGameInteractionEntityOutcome
{
	eGameInteractionEntityOutcome_Success,
	// Driving: the map has no such entity. Otherwise: the Session does not drive it, or it has no such
	// body.
	eGameInteractionEntityOutcome_NotFound,
	// The entity was created at runtime or is not a type a player holds.
	eGameInteractionEntityOutcome_NotHoldable
};

enum eGameInteractionCustomStoryAvailability
{
	eGameInteractionCustomStoryAvailability_Available,
	eGameInteractionCustomStoryAvailability_NotInMainMenu,
	eGameInteractionCustomStoryAvailability_NotFound,
	eGameInteractionCustomStoryAvailability_Invalid
};

enum eGameInteractionLocalPoseAvailability
{
	// No map is loaded, or one is loading, so there is no local Pose to report.
	eGameInteractionLocalPoseAvailability_Unavailable,
	// The local player is in play.
	eGameInteractionLocalPoseAvailability_Live,
	// A map is loaded but play is suspended, such as while paused or in the inventory.
	eGameInteractionLocalPoseAvailability_Suspended
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
	virtual eGameInteractionLocalPoseAvailability GetLocalPoseAvailability() const = 0;
	// Called only while the local Pose is available.
	virtual cGameInteractionPose GetLocalPose() const = 0;
	// Avatars belong to the Session, so the gateway creates, poses, and removes each one it accepted,
	// whether or not a map is loaded. Creation fails only when the model cannot be found.
	virtual bool CreateAvatar(const std::string& asIdentifier, const std::string& asEntityFile) = 0;
	virtual void RemoveAvatar(const std::string& asIdentifier) = 0;
	virtual void PoseAvatar(const std::string& asIdentifier, const cGameInteractionPose& aPose) = 0;
	// Whether the local player collides with the Avatar while it is awake. It is on until turned off.
	virtual void SetAvatarCollision(const std::string& asIdentifier, bool abCollides) = 0;
	// The bodies whose motion the local player decides, on the current map. Called only while the
	// local Pose is available.
	virtual cGameInteractionBodySamples GetReportedBodies() const = 0;
	// Peer-Driven Entities are the game's to track, because it ends them itself on a map change or save
	// load. The gateway calls these only for the current map.
	virtual eGameInteractionEntityOutcome DriveEntity(int alEntityId) = 0;
	// Applies every sample it can. On failure, names the entity of the first sample it could not apply.
	virtual eGameInteractionEntityOutcome DriveEntityBodies(const cGameInteractionBodySamples& aSamples,
		int& alFailedEntityId) = 0;
	virtual eGameInteractionEntityOutcome SetDrivenEntityInteracting(int alEntityId, bool abInteracting) = 0;
	virtual eGameInteractionEntityOutcome BreakDrivenEntity(int alEntityId,
		const cGameInteractionBodyState& aFinalState) = 0;
	virtual eGameInteractionEntityOutcome ReleaseDrivenEntity(int alEntityId) = 0;
	// Called when a Session that drove an entity ends, whatever map is loaded.
	virtual void ReleaseDrivenEntities() = 0;
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
	eGameInteractionSubscriptionRequest GetSubscriptionRequest() const { return mSubscriptionRequest; }
	unsigned int GetSubscriptionRate() const { return mlSubscriptionRate; }
	void SetPosition(const cGameInteractionPosition& aPosition) { mPosition = aPosition; }
	void SetRotation(const cGameInteractionRotation& aRotation) { mRotation = aRotation; }
	void SetMapFile(const std::string& asMapFile) { msMapFile = asMapFile; }
	void SetCustomStories(const std::vector<cGameInteractionCustomStory>& avCustomStories)
	{
		mvCustomStories = avCustomStories;
	}
	void SetCapabilities(unsigned int alCapabilities) { mlCapabilities = alCapabilities; }
	void SetSubscription(eGameInteractionSubscriptionRequest aRequest, unsigned int alRate)
	{
		mSubscriptionRequest = aRequest;
		mlSubscriptionRate = alRate;
	}
	// The Avatar a Response names, or empty when it names none.
	const std::string& GetAvatarIdentifier() const { return msAvatarIdentifier; }
	void SetAvatarIdentifier(const std::string& asIdentifier) { msAvatarIdentifier = asIdentifier; }
	// The entity a Response names, if it names one.
	bool NamesEntity() const { return mbNamesEntity; }
	int GetEntityId() const { return mlEntityId; }
	void SetEntityId(int alEntityId)
	{
		mbNamesEntity = true;
		mlEntityId = alEntityId;
	}

private:
	eGameInteractionCommandType mCommandType;
	eGameInteractionResponseType mType;
	eGameInteractionCommandOutcome mOutcome;
	cGameInteractionPosition mPosition;
	cGameInteractionRotation mRotation;
	std::string msMapFile;
	std::vector<cGameInteractionCustomStory> mvCustomStories;
	unsigned int mlCapabilities;
	eGameInteractionSubscriptionRequest mSubscriptionRequest;
	unsigned int mlSubscriptionRate;
	std::string msAvatarIdentifier;
	bool mbNamesEntity;
	int mlEntityId;
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
