#include "GameInteractionGateway.h"
#include "GameInteractionTransport.h"
#include "GameInteractionProtocolVersion2.h"
#include "LegacyGameInteractionProtocol.h"

#include <algorithm>
#include <set>
#include <vector>

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::string& asData)
	: mType(aType), msData(asData)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::wstring& asChatAuthor, const std::wstring& asChatMessage)
	: mType(aType), msChatAuthor(asChatAuthor), msChatMessage(asChatMessage)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::string& asData)
	: mType(aType), msData(asData), mlProtocolVersion(0), mlCapabilities(0),
	  mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asChatAuthor, const std::wstring& asChatMessage)
	: mType(aType), msChatAuthor(asChatAuthor), msChatMessage(asChatMessage), mlProtocolVersion(0),
	  mlCapabilities(0), mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier), mlProtocolVersion(0), mlCapabilities(0),
	  mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	unsigned int alProtocolVersion, unsigned int alCapabilities)
	: mType(aType), mlProtocolVersion(alProtocolVersion), mlCapabilities(alCapabilities),
	  mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	eGameInteractionLocalPoseRequest aRequest, unsigned int alLocalPoseRate)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0), mLocalPoseRequest(aRequest),
	  mlLocalPoseRate(alLocalPoseRate)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionAvatarRequest& aAvatarRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0), mAvatarRequest(aAvatarRequest)
{
}

eGameInteractionCommandClassification cGameInteractionCommand::GetClassification() const
{
	return mType == eGameInteractionCommand_ExecuteScript || mType == eGameInteractionCommand_Chat ||
		mType == eGameInteractionCommand_StartCustomStory || mType == eGameInteractionCommand_AvatarCreate ||
		mType == eGameInteractionCommand_AvatarRemove || mType == eGameInteractionCommand_AvatarCollision ||
		mType == eGameInteractionCommand_AvatarPose ?
		eGameInteractionCommandClassification_StateChanging :
		eGameInteractionCommandClassification_Observational;
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome), mlCapabilities(0),
	  mLocalPoseRequest(eGameInteractionLocalPoseRequest_Invalid), mlLocalPoseRate(0)
{
}

namespace
{
	struct cPendingCustomStoryStart
	{
		cPendingCustomStoryStart() : mbPending(false) {}
		bool mbPending;
		std::wstring msIdentifier;
	};

	const unsigned int kSupportedProtocolVersion = 2;
	const unsigned int kSupportedCapabilities =
		eGameInteractionCapability_Avatars | eGameInteractionCapability_LocalPose;

	const unsigned int kMinimumLocalPoseRate = 1;
	const unsigned int kMaximumLocalPoseRate = 60;

	struct cLocalPoseSubscription
	{
		cLocalPoseSubscription() : mbSubscribed(false), mlRate(0), mbScheduled(false), mfNextSampleTimeMs(0.0) {}
		bool mbSubscribed;
		unsigned int mlRate;
		bool mbScheduled;
		double mfNextSampleTimeMs;
		// The newest sampled State Update not yet handed to the transport, or empty. A newer Pose
		// replaces it, so a Peer that reads slowly never accumulates Poses.
		std::string msUndeliveredStateUpdate;
		// The last sampled Pose serialized without its time, or empty after the Pose was unavailable.
		std::string msLastSampledPose;
	};

	const char* const kDefaultAvatarEntityFile = "entities/multiplayer/skeleton_spelos/TheSkeletonSpelos.ent";
	const size_t kMaximumAvatarCount = 16;

	struct cSessionAvatars
	{
		cSessionAvatars() : mbUnidentifiedPoseFailing(false) {}
		// In creation order, so the game removes them in a predictable order.
		std::vector<std::string> mvIdentifiers;
		// Avatar Identifiers whose avatarpose failure streak was already reported.
		std::set<std::string> msetFailingPoseIdentifiers;
		// The one streak shared by avatarpose lines without a valid Avatar Identifier.
		bool mbUnidentifiedPoseFailing;

		bool Contains(const std::string& asIdentifier) const
		{
			return std::find(mvIdentifiers.begin(), mvIdentifiers.end(), asIdentifier) != mvIdentifiers.end();
		}

		bool Remove(const std::string& asIdentifier)
		{
			std::vector<std::string>::iterator avatar = std::find(mvIdentifiers.begin(), mvIdentifiers.end(), asIdentifier);
			if (avatar == mvIdentifiers.end()) return false;
			mvIdentifiers.erase(avatar);
			return true;
		}

		// True only for the failure that starts a streak; an empty identifier is the shared streak.
		bool StartsPoseFailureStreak(const std::string& asIdentifier)
		{
			if (!asIdentifier.empty()) return msetFailingPoseIdentifiers.insert(asIdentifier).second;
			const bool starts = !mbUnidentifiedPoseFailing;
			mbUnidentifiedPoseFailing = true;
			return starts;
		}

		void EndPoseFailureStreak(const std::string& asIdentifier) { msetFailingPoseIdentifiers.erase(asIdentifier); }
	};

	struct cSessionProtocol
	{
		cSessionProtocol()
			: mbNegotiated(false), mbCommandProcessed(false), mlGrantedCapabilities(eGameInteractionCapability_None) {}
		bool mbNegotiated;
		// Set by any Command other than a failed negotiation, after which negotiating is too late.
		bool mbCommandProcessed;
		unsigned int mlGrantedCapabilities;
		cLocalPoseSubscription mLocalPoseSubscription;
		cSessionAvatars mAvatars;
	};

	eGameInteractionCommandOutcome NegotiationOutcomeFor(const cGameInteractionCommand& aCommand,
		const cSessionProtocol& aSession)
	{
		if (aSession.mbNegotiated) return eGameInteractionCommandOutcome_AlreadyNegotiated;
		if (aSession.mbCommandProcessed) return eGameInteractionCommandOutcome_NegotiationTooLate;
		if (aCommand.GetProtocolVersion() == 0) return eGameInteractionCommandOutcome_Invalid;
		if (aCommand.GetProtocolVersion() != kSupportedProtocolVersion)
			return eGameInteractionCommandOutcome_UnsupportedProtocolVersion;
		return eGameInteractionCommandOutcome_Success;
	}

	// A failed first negotiation leaves the Session on the legacy protocol and may be retried, so a
	// Peer can fall back from a Protocol Version this game does not support.
	cGameInteractionResponse NegotiateProtocol(const cGameInteractionCommand& aCommand, cSessionProtocol& aSession)
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_ProtocolNegotiated,
			NegotiationOutcomeFor(aCommand, aSession));
		if (response.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
		aSession.mbNegotiated = true;
		aSession.mlGrantedCapabilities = aCommand.GetCapabilities() & kSupportedCapabilities;
		response.SetCapabilities(aSession.mlGrantedCapabilities);
		return response;
	}

	// Subscribing again restarts the subscription at the new rate.
	cGameInteractionResponse ChangeLocalPoseSubscription(const cGameInteractionCommand& aCommand,
		cLocalPoseSubscription& aSubscription)
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_LocalPoseSubscription);
		switch (aCommand.GetLocalPoseRequest())
		{
		case eGameInteractionLocalPoseRequest_Subscribe:
		{
			unsigned int rate = aCommand.GetLocalPoseRate();
			if (rate < kMinimumLocalPoseRate) rate = kMinimumLocalPoseRate;
			if (rate > kMaximumLocalPoseRate) rate = kMaximumLocalPoseRate;
			aSubscription = cLocalPoseSubscription();
			aSubscription.mbSubscribed = true;
			aSubscription.mlRate = rate;
			response.SetLocalPoseSubscription(eGameInteractionLocalPoseRequest_Subscribe, rate);
			return response;
		}
		case eGameInteractionLocalPoseRequest_Unsubscribe:
			aSubscription = cLocalPoseSubscription();
			response.SetLocalPoseSubscription(eGameInteractionLocalPoseRequest_Unsubscribe, 0);
			return response;
		default:
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_LocalPoseSubscription,
				eGameInteractionCommandOutcome_Invalid);
		}
	}

	// Samples at most once per rate interval of the Pose's own clock. While play is suspended, an
	// unchanged Pose is not sampled again.
	void SampleLocalPose(iGameInteractionGameAdapter& aGameAdapter, cLocalPoseSubscription& aSubscription)
	{
		if (!aSubscription.mbSubscribed) return;
		const eGameInteractionLocalPoseAvailability availability = aGameAdapter.GetLocalPoseAvailability();
		if (availability == eGameInteractionLocalPoseAvailability_Unavailable)
		{
			aSubscription.msUndeliveredStateUpdate.clear();
			aSubscription.msLastSampledPose.clear();
			return;
		}

		cGameInteractionPose pose = aGameAdapter.GetLocalPose();
		const double now = static_cast<double>(pose.mlTimeMs);
		if (aSubscription.mbScheduled && now < aSubscription.mfNextSampleTimeMs) return;
		const std::string stateUpdate = cGameInteractionProtocolVersion2::SerializeLocalPose(pose);
		// Comparing at wire precision ignores changes a Peer could not observe.
		pose.mlTimeMs = 0;
		const std::string timelessPose = cGameInteractionProtocolVersion2::SerializeLocalPose(pose);
		if (availability == eGameInteractionLocalPoseAvailability_Suspended &&
			timelessPose == aSubscription.msLastSampledPose) return;

		// Keeping to the schedule holds the average rate even when updates do not land on it exactly.
		const double interval = 1000.0 / aSubscription.mlRate;
		double next = aSubscription.mfNextSampleTimeMs + interval;
		if (!aSubscription.mbScheduled || next <= now) next = now + interval;
		aSubscription.mbScheduled = true;
		aSubscription.mfNextSampleTimeMs = next;
		aSubscription.msUndeliveredStateUpdate = stateUpdate;
		aSubscription.msLastSampledPose = timelessPose;
	}

	cGameInteractionResponse AvatarResponse(eGameInteractionCommandType aType, eGameInteractionCommandOutcome aOutcome,
		const std::string& asIdentifier)
	{
		cGameInteractionResponse response(aType, eGameInteractionResponse_Avatar, aOutcome);
		response.SetAvatarIdentifier(asIdentifier);
		return response;
	}

	cGameInteractionResponse CreateAvatar(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionAvatars& aAvatars)
	{
		const cGameInteractionAvatarRequest& request = aCommand.GetAvatarRequest();
		if (!request.mbValid)
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, std::string());
		if (aAvatars.Contains(request.msIdentifier))
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarExists, request.msIdentifier);
		if (aAvatars.mvIdentifiers.size() >= kMaximumAvatarCount)
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarLimitReached,
				request.msIdentifier);
		const std::string entityFile = request.msEntityFile.empty() ? kDefaultAvatarEntityFile : request.msEntityFile;
		if (!aGameAdapter.CreateAvatar(request.msIdentifier, entityFile))
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarModelNotFound,
				request.msIdentifier);
		aAvatars.mvIdentifiers.push_back(request.msIdentifier);
		return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Success, request.msIdentifier);
	}

	cGameInteractionResponse RemoveAvatar(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionAvatars& aAvatars)
	{
		const cGameInteractionAvatarRequest& request = aCommand.GetAvatarRequest();
		if (!request.mbValid)
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, std::string());
		if (!aAvatars.Remove(request.msIdentifier))
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarNotFound, request.msIdentifier);
		aGameAdapter.RemoveAvatar(request.msIdentifier);
		return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Success, request.msIdentifier);
	}

	cGameInteractionResponse SetAvatarCollision(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, const cSessionAvatars& aAvatars)
	{
		const cGameInteractionAvatarRequest& request = aCommand.GetAvatarRequest();
		if (!request.mbValid)
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, std::string());
		if (!aAvatars.Contains(request.msIdentifier))
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarNotFound, request.msIdentifier);
		aGameAdapter.SetAvatarCollision(request.msIdentifier, request.mbCollides);
		return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Success, request.msIdentifier);
	}

	// Poses stream at network rate, so a success is not answered and a failure is answered only
	// when it starts its Avatar's failure streak.
	cGameInteractionResponse PoseAvatar(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionAvatars& aAvatars)
	{
		const cGameInteractionAvatarRequest& request = aCommand.GetAvatarRequest();
		const eGameInteractionCommandOutcome outcome = !request.mbValid ? eGameInteractionCommandOutcome_Invalid :
			!aAvatars.Contains(request.msIdentifier) ? eGameInteractionCommandOutcome_AvatarNotFound :
			eGameInteractionCommandOutcome_Success;
		if (outcome == eGameInteractionCommandOutcome_Success)
		{
			aAvatars.EndPoseFailureStreak(request.msIdentifier);
			aGameAdapter.PoseAvatar(request.msIdentifier, request.mPose);
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_None);
		}
		if (!aAvatars.StartsPoseFailureStreak(request.msIdentifier))
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_None);
		return AvatarResponse(aCommand.GetType(), outcome, request.msIdentifier);
	}

	unsigned int RequiredCapability(eGameInteractionCommandType aType)
	{
		switch (aType)
		{
		case eGameInteractionCommand_AvatarCreate:
		case eGameInteractionCommand_AvatarRemove:
		case eGameInteractionCommand_AvatarCollision:
		case eGameInteractionCommand_AvatarPose:
			return eGameInteractionCapability_Avatars;
		case eGameInteractionCommand_LocalPose:
			return eGameInteractionCapability_LocalPose;
		default:
			return eGameInteractionCapability_None;
		}
	}

	eGameInteractionCommandOutcome OutcomeFor(eGameInteractionCustomStoryAvailability aAvailability)
	{
		switch (aAvailability)
		{
		case eGameInteractionCustomStoryAvailability_Available: return eGameInteractionCommandOutcome_Success;
		case eGameInteractionCustomStoryAvailability_NotFound: return eGameInteractionCommandOutcome_CustomStoryNotFound;
		case eGameInteractionCustomStoryAvailability_Invalid: return eGameInteractionCommandOutcome_CustomStoryInvalid;
		default: return eGameInteractionCommandOutcome_NotInMainMenu;
		}
	}

	cGameInteractionResponse ExecuteCommand(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionProtocol& aSession,
		cPendingCustomStoryStart& aPendingStart)
	{
		if (aCommand.GetType() != eGameInteractionCommand_NegotiateProtocol) aSession.mbCommandProcessed = true;
		const unsigned int requiredCapability = RequiredCapability(aCommand.GetType());
		if ((aSession.mlGrantedCapabilities & requiredCapability) != requiredCapability)
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Rejected,
				eGameInteractionCommandOutcome_CapabilityNotGranted);
		switch (aCommand.GetType())
		{
		case eGameInteractionCommand_NegotiateProtocol:
			return NegotiateProtocol(aCommand, aSession);
		case eGameInteractionCommand_LocalPose:
			return ChangeLocalPoseSubscription(aCommand, aSession.mLocalPoseSubscription);
		case eGameInteractionCommand_AvatarCreate:
			return CreateAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarRemove:
			return RemoveAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarPose:
			return PoseAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarCollision:
			return SetAvatarCollision(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_GetPosition:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Position,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetPosition(aGameAdapter.GetPosition());
			return response;
		}
		case eGameInteractionCommand_GetRotation:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Rotation,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetRotation(aGameAdapter.GetRotation());
			return response;
		}
		case eGameInteractionCommand_GetPositionRotation:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_PositionRotation,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
			response.SetPosition(aGameAdapter.GetPosition());
			response.SetRotation(aGameAdapter.GetRotation());
			return response;
		}
		case eGameInteractionCommand_GetMap:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Map,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetMapFile(aGameAdapter.GetMapFile());
			return response;
		}
		case eGameInteractionCommand_ExecuteScript:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_ScriptExecuted,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				aGameAdapter.RunScript(aCommand.GetData());
			return response;
		}
		case eGameInteractionCommand_Chat:
		{
			cChatEntry entry;
			const eChatEntryValidation validation = cChatModel::TryCreateEntry(
				aCommand.GetChatAuthor(), aCommand.GetChatMessage(), entry);
			if (validation == eChatEntryValidation_InvalidAuthor)
				return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
					eGameInteractionCommandOutcome_InvalidAuthor);
			if (validation == eChatEntryValidation_InvalidMessage)
				return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
					eGameInteractionCommandOutcome_InvalidMessage);
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
				aGameAdapter.DisplayChatEntry(entry) ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_Unavailable);
		}
		case eGameInteractionCommand_GetCustomStories:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_CustomStories);
			response.SetCustomStories(aGameAdapter.GetCustomStories());
			return response;
		}
		case eGameInteractionCommand_StartCustomStory:
		{
			// A pending start will leave the main menu, so a second start cannot be accepted.
			const eGameInteractionCommandOutcome outcome = aPendingStart.mbPending ?
				eGameInteractionCommandOutcome_NotInMainMenu :
				OutcomeFor(aGameAdapter.GetCustomStoryAvailability(aCommand.GetCustomStoryIdentifier()));
			if (outcome == eGameInteractionCommandOutcome_Success)
			{
				aPendingStart.mbPending = true;
				aPendingStart.msIdentifier = aCommand.GetCustomStoryIdentifier();
			}
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_CustomStoryStarting, outcome);
		}
		default:
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Pong);
		}
	}
}

class cGameInteractionGateway::cImplementation
{
public:
	cGameInteractionTransport mTransport;
	cGameInteractionLineBuffer mInboundLines;
	cPendingCustomStoryStart mPendingCustomStoryStart;
	cSessionProtocol mSession;
	// Avatars of ended Sessions that the game has not removed yet. A Session can end without a game
	// adapter at hand, such as on Shutdown, so they are removed on the next update.
	std::vector<std::string> mvEndedSessionAvatars;

	void EndSession()
	{
		const std::vector<std::string>& avatars = mSession.mAvatars.mvIdentifiers;
		mvEndedSessionAvatars.insert(mvEndedSessionAvatars.end(), avatars.begin(), avatars.end());
		mSession = cSessionProtocol();
	}

	void RemoveEndedSessionAvatars(iGameInteractionGameAdapter& aGameAdapter)
	{
		std::vector<std::string> avatars;
		avatars.swap(mvEndedSessionAvatars);
		for (std::vector<std::string>::const_iterator avatar = avatars.begin(); avatar != avatars.end(); ++avatar)
			aGameAdapter.RemoveAvatar(*avatar);
	}

	// The legacy adapter serves a Session until it negotiates; its negotiation Command is the only
	// Protocol Version 2 line a legacy Session understands.
	cGameInteractionCommand ParseCommand(const std::string& asLine) const
	{
		if (mSession.mbNegotiated || cGameInteractionProtocolVersion2::IsNegotiation(asLine))
			return cGameInteractionProtocolVersion2::ParseCommand(asLine);
		return cLegacyGameInteractionProtocol::ParseCommand(asLine);
	}

	std::string SerializeResponse(const cGameInteractionResponse& aResponse) const
	{
		if (mSession.mbNegotiated || aResponse.GetCommandType() == eGameInteractionCommand_NegotiateProtocol)
			return cGameInteractionProtocolVersion2::SerializeResponse(aResponse);
		return cLegacyGameInteractionProtocol::SerializeResponse(aResponse);
	}

	// A State Update is handed to the transport only once everything queued before it was sent, so
	// at most one undelivered local Pose exists and it is always the newest one.
	void DeliverLocalPose()
	{
		std::string& stateUpdate = mSession.mLocalPoseSubscription.msUndeliveredStateUpdate;
		if (stateUpdate.empty() || !mTransport.HasPeer() || mTransport.GetPendingDeliveryByteCount() != 0) return;
		mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(stateUpdate));
		stateUpdate.clear();
		mTransport.Flush();
	}

	// Runs after the transport has flushed the accepting Response and outside Command processing,
	// because starting a Custom Story blocks while its start map loads.
	void PerformPendingCustomStoryStart(iGameInteractionGameAdapter& aGameAdapter)
	{
		if (!mPendingCustomStoryStart.mbPending) return;
		const std::wstring identifier = mPendingCustomStoryStart.msIdentifier;
		mPendingCustomStoryStart = cPendingCustomStoryStart();
		aGameAdapter.StartCustomStory(identifier);
	}
};

cGameInteractionGateway::cGameInteractionGateway()
	: mpImplementation(new cImplementation)
{
}

cGameInteractionGateway::~cGameInteractionGateway()
{
	delete mpImplementation;
}

bool cGameInteractionGateway::Listen(const std::string& asHost, int alPort)
{
	mpImplementation->mInboundLines.Clear();
	return mpImplementation->mTransport.Listen(asHost, alPort);
}

void cGameInteractionGateway::Shutdown()
{
	mpImplementation->mInboundLines.Clear();
	mpImplementation->mTransport.Shutdown();
	mpImplementation->EndSession();
}

void cGameInteractionGateway::Update(iGameInteractionGameAdapter& aGameAdapter)
{
	std::vector<std::string> receivedBytes;
	const eGameInteractionTransportEvent event = mpImplementation->mTransport.Update(receivedBytes);
	if (event == eGameInteractionTransportEvent_PeerConnected)
	{
		mpImplementation->mInboundLines.Clear();
		mpImplementation->EndSession();
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cLegacyGameInteractionProtocol::Greeting()));
	}
	else if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		mpImplementation->mInboundLines.Clear();
		mpImplementation->EndSession();
	}

	mpImplementation->RemoveEndedSessionAvatars(aGameAdapter);
	mpImplementation->PerformPendingCustomStoryStart(aGameAdapter);
	if (event == eGameInteractionTransportEvent_PeerDisconnected) return;

	for (std::vector<std::string>::const_iterator bytes = receivedBytes.begin();
		bytes != receivedBytes.end(); ++bytes)
		mpImplementation->mInboundLines.Append(bytes->data(), bytes->size());

	std::string commandText;
	while (mpImplementation->mTransport.HasPeer() &&
		mpImplementation->mInboundLines.TryPopLine(commandText))
	{
		const cGameInteractionCommand command = mpImplementation->ParseCommand(commandText);
		const cGameInteractionResponse response = ExecuteCommand(command, aGameAdapter,
			mpImplementation->mSession, mpImplementation->mPendingCustomStoryStart);
		if (response.GetType() == eGameInteractionResponse_None) continue;
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			mpImplementation->SerializeResponse(response)));
	}
	// Delivering Responses now rather than on the next update removes a tick of latency.
	mpImplementation->mTransport.Flush();
	SampleLocalPose(aGameAdapter, mpImplementation->mSession.mLocalPoseSubscription);
	mpImplementation->DeliverLocalPose();

	if (mpImplementation->mInboundLines.HasExceededLineLimit())
	{
		mpImplementation->mTransport.DisconnectPeer("Peer exceeded the inbound line length limit");
		mpImplementation->mInboundLines.Clear();
		mpImplementation->EndSession();
		mpImplementation->RemoveEndedSessionAvatars(aGameAdapter);
	}
}

void cGameInteractionGateway::Report(const cGameInteractionEvent& aEvent)
{
	if (!mpImplementation->mTransport.HasPeer()) return;
	mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
		cLegacyGameInteractionProtocol::SerializeEvent(aEvent)));
}

int cGameInteractionGateway::GetPort() const
{
	return mpImplementation->mTransport.GetPort();
}

const std::string& cGameInteractionGateway::GetDiagnostic() const
{
	return mpImplementation->mTransport.GetDiagnostic();
}
