#include "GameInteractionGateway.h"
#include "GameInteractionTransport.h"
#include "GameInteractionProtocolVersion2.h"
#include "LegacyGameInteractionProtocol.h"

#include <algorithm>
#include <set>
#include <map>
#include <vector>

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionEnemyRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mEnemyRequest(aRequest) {}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionEnemyAttackRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mEnemyAttackRequest(aRequest) {}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionSharedDeathRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mSharedDeathRequest(aRequest) {}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionSettingRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mSettingRequest(aRequest) {}

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

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const cGameInteractionEntityEvent& aEntityEvent)
	: mType(aType), mEntityEvent(aEntityEvent)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const cGameInteractionSharedCallback& aSharedCallback)
	: mType(aType), mSharedCallback(aSharedCallback)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const cGameInteractionEntityState& aState) : mType(aType), mEntityState(aState) {}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const cGameInteractionEnemyAttack& aAttack) : mType(aType), mEnemyAttack(aAttack) {}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionEntityStateRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mEntityStateRequest(aRequest) {}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionPlacementRequest& aRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mPlacementRequest(aRequest) {}

bool cGameInteractionEvent::IsInteractionsEvent() const
{
	return mType == eGameInteractionEvent_InteractionStarted || mType == eGameInteractionEvent_InteractionEnded ||
		mType == eGameInteractionEvent_ReportContact || mType == eGameInteractionEvent_ReportSettled ||
		mType == eGameInteractionEvent_ReportBroke || mType == eGameInteractionEvent_EntityReleased ||
		mType == eGameInteractionEvent_MapEntered;
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::string& asData)
	: mType(aType), msData(asData), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionChatRequest& aChatRequest)
	: mType(aType), mChatRequest(aChatRequest), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	unsigned int alProtocolVersion, unsigned int alCapabilities)
	: mType(aType), mlProtocolVersion(alProtocolVersion), mlCapabilities(alCapabilities),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	eGameInteractionSubscriptionRequest aRequest, unsigned int alSubscriptionRate)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0), mSubscriptionRequest(aRequest),
	  mlSubscriptionRate(alSubscriptionRate)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionEntityRequest& aEntityRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mEntityRequest(aEntityRequest)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionAvatarRequest& aAvatarRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0), mAvatarRequest(aAvatarRequest)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const cGameInteractionSharedCallbackRequest& aSharedCallbackRequest)
	: mType(aType), mlProtocolVersion(0), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mSharedCallbackRequest(aSharedCallbackRequest)
{
}

eGameInteractionCommandClassification cGameInteractionCommand::GetClassification() const
{
	return mType == eGameInteractionCommand_ExecuteScript || mType == eGameInteractionCommand_Chat ||
		mType == eGameInteractionCommand_StartCustomStory || mType == eGameInteractionCommand_AvatarCreate ||
		mType == eGameInteractionCommand_AvatarEnemySight || mType == eGameInteractionCommand_AvatarRemove || mType == eGameInteractionCommand_AvatarCollision ||
		mType == eGameInteractionCommand_AvatarPose || mType == eGameInteractionCommand_EntityDrive ||
		mType == eGameInteractionCommand_EntityBodies || mType == eGameInteractionCommand_EntityInteracting ||
		mType == eGameInteractionCommand_EntityBreak || mType == eGameInteractionCommand_EntityRelease ||
		mType == eGameInteractionCommand_ButtonState || mType == eGameInteractionCommand_LampState ||
		mType == eGameInteractionCommand_EnemyDrive || mType == eGameInteractionCommand_EnemyPose ||
		mType == eGameInteractionCommand_EnemyRelease || mType == eGameInteractionCommand_EnemyAttack ||
		mType == eGameInteractionCommand_EnemyDoorBreak ||
		mType == eGameInteractionCommand_SharedDeath ||
		mType == eGameInteractionCommand_SharedCallback || mType == eGameInteractionCommand_PlayerPlace ?
		eGameInteractionCommandClassification_StateChanging :
		eGameInteractionCommandClassification_Observational;
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome), mlCapabilities(0),
	  mSubscriptionRequest(eGameInteractionSubscriptionRequest_Invalid), mlSubscriptionRate(0),
	  mbNamesEntity(false), mlEntityId(0)
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

	const unsigned int kSupportedCapabilities =
		eGameInteractionCapability_Avatars | eGameInteractionCapability_LocalPose |
		eGameInteractionCapability_Interactions | eGameInteractionCapability_SharedCallbacks |
		eGameInteractionCapability_PlayerPlace | eGameInteractionCapability_Enemies | eGameInteractionCapability_Lean |
		eGameInteractionCapability_Settings;

	const unsigned int kMinimumSubscriptionRate = 1;
	const unsigned int kMaximumSubscriptionRate = 60;

	// A localpose or reportedbodies subscription.
	struct cStateUpdateSubscription
	{
		cStateUpdateSubscription() : mbSubscribed(false), mlRate(0), mbScheduled(false), mfNextSampleTimeMs(0.0) {}
		bool mbSubscribed;
		unsigned int mlRate;
		bool mbScheduled;
		double mfNextSampleTimeMs;
		// The newest sampled State Update not yet handed to the transport, or empty. A newer sample
		// replaces it, so a Peer that reads slowly never accumulates State Updates.
		std::string msUndeliveredStateUpdate;
		// The last sample serialized without its time, or empty after there was nothing to sample.
		std::string msLastSample;
	};

	struct cSessionEntities
	{
		cSessionEntities() : mbDroveEntity(false), mbBodiesFailing(false) {}
		// Whether the game may still drive an entity for this Session when it ends.
		bool mbDroveEntity;
		// Whether the entitybodies failure streak was already reported.
		bool mbBodiesFailing;
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
			: mbNegotiated(false), mbCommandProcessed(false), mlGrantedCapabilities(eGameInteractionCapability_None), mbDroveEnemy(false), mbEnemyPoseFailing(false) {}
		bool mbNegotiated;
		// Set by any Command other than a failed negotiation, after which negotiating is too late.
		bool mbCommandProcessed;
		unsigned int mlGrantedCapabilities;
		bool mbDroveEnemy;
		bool mbEnemyPoseFailing;
		cStateUpdateSubscription mEnemySubscription;
		std::map<int, std::string> mEnemyUpdates;
		cStateUpdateSubscription mLocalPoseSubscription;
		cStateUpdateSubscription mReportedBodiesSubscription;
		cSessionAvatars mAvatars;
		cSessionEntities mEntities;
		// The last checkpoint Event sent, or empty when the next sample must be reported.
		std::string msLastCheckPoint;
		cSharedDeathModel mSharedDeaths;
	};

	eGameInteractionCommandOutcome NegotiationOutcomeFor(const cGameInteractionCommand& aCommand,
		const cSessionProtocol& aSession)
	{
		if (aSession.mbNegotiated) return eGameInteractionCommandOutcome_AlreadyNegotiated;
		if (aSession.mbCommandProcessed) return eGameInteractionCommandOutcome_NegotiationTooLate;
		if (aCommand.GetProtocolVersion() == 0) return eGameInteractionCommandOutcome_Invalid;
		if (aCommand.GetProtocolVersion() != cGameInteractionGateway::kSupportedProtocolVersion)
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
	cGameInteractionResponse ChangeSubscription(const cGameInteractionCommand& aCommand,
		cStateUpdateSubscription& aSubscription)
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Subscription);
		switch (aCommand.GetSubscriptionRequest())
		{
		case eGameInteractionSubscriptionRequest_Subscribe:
		{
			unsigned int rate = aCommand.GetSubscriptionRate();
			if (rate < kMinimumSubscriptionRate) rate = kMinimumSubscriptionRate;
			if (rate > kMaximumSubscriptionRate) rate = kMaximumSubscriptionRate;
			aSubscription = cStateUpdateSubscription();
			aSubscription.mbSubscribed = true;
			aSubscription.mlRate = rate;
			response.SetSubscription(eGameInteractionSubscriptionRequest_Subscribe, rate);
			return response;
		}
		case eGameInteractionSubscriptionRequest_Unsubscribe:
			aSubscription = cStateUpdateSubscription();
			response.SetSubscription(eGameInteractionSubscriptionRequest_Unsubscribe, 0);
			return response;
		default:
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Subscription,
				eGameInteractionCommandOutcome_Invalid);
		}
	}

	// A subscription samples at most once per rate interval of its sample's own clock.
	bool IsSampleDue(const cStateUpdateSubscription& aSubscription, unsigned long long alTimeMs)
	{
		return !aSubscription.mbScheduled || static_cast<double>(alTimeMs) >= aSubscription.mfNextSampleTimeMs;
	}

	// While play is suspended, a sample unchanged at wire precision is not kept again, because a Peer
	// could not observe the difference.
	void KeepSample(cStateUpdateSubscription& aSubscription, eGameInteractionLocalPoseAvailability aAvailability,
		unsigned long long alTimeMs, const std::string& asStateUpdate, const std::string& asTimelessSample)
	{
		if (aAvailability == eGameInteractionLocalPoseAvailability_Suspended &&
			asTimelessSample == aSubscription.msLastSample) return;

		// Keeping to the schedule holds the average rate even when updates do not land on it exactly.
		const double now = static_cast<double>(alTimeMs);
		const double interval = 1000.0 / aSubscription.mlRate;
		double next = aSubscription.mfNextSampleTimeMs + interval;
		if (!aSubscription.mbScheduled || next <= now) next = now + interval;
		aSubscription.mbScheduled = true;
		aSubscription.mfNextSampleTimeMs = next;
		aSubscription.msUndeliveredStateUpdate = asStateUpdate;
		aSubscription.msLastSample = asTimelessSample;
	}

	void SampleLocalPose(iGameInteractionGameAdapter& aGameAdapter, cStateUpdateSubscription& aSubscription,
		unsigned int alGrantedCapabilities)
	{
		if (!aSubscription.mbSubscribed) return;
		const eGameInteractionLocalPoseAvailability availability = aGameAdapter.GetLocalPoseAvailability();
		if (availability == eGameInteractionLocalPoseAvailability_Unavailable)
		{
			aSubscription.msUndeliveredStateUpdate.clear();
			aSubscription.msLastSample.clear();
			return;
		}

		cGameInteractionPose pose = aGameAdapter.GetLocalPose();
		const unsigned long long timeMs = pose.mlTimeMs;
		if (!IsSampleDue(aSubscription, timeMs)) return;
		const std::string stateUpdate = cGameInteractionProtocolVersion2::SerializeLocalPose(pose, alGrantedCapabilities);
		pose.mlTimeMs = 0;
		KeepSample(aSubscription, availability, timeMs, stateUpdate,
			cGameInteractionProtocolVersion2::SerializeLocalPose(pose, alGrantedCapabilities));
	}

	void SampleEnemies(iGameInteractionGameAdapter& aGameAdapter, cSessionProtocol& aSession)
	{
		cStateUpdateSubscription& subscription = aSession.mEnemySubscription;
		if (!subscription.mbSubscribed) { aSession.mEnemyUpdates.clear(); return; }
		if (aGameAdapter.GetLocalPoseAvailability() == eGameInteractionLocalPoseAvailability_Unavailable)
		{ aSession.mEnemyUpdates.clear(); subscription.mbScheduled = false; return; }
		const unsigned long long now = aGameAdapter.GetLocalPose().mlTimeMs;
		if (!IsSampleDue(subscription, now)) return;
		const std::vector<cGameInteractionEnemyPose> enemies = aGameAdapter.GetEnemyPoses();
		// Replace the complete set so inactive enemies and previous maps cannot leave stale slots.
		aSession.mEnemyUpdates.clear();
		for (size_t index = 0; index < enemies.size(); ++index)
			aSession.mEnemyUpdates[enemies[index].mlEntityId] =
				cGameInteractionProtocolVersion2::SerializeEnemyPose(enemies[index]);
		KeepSample(subscription, eGameInteractionLocalPoseAvailability_Live, now, "", "");
	}

	// Reports the current checkpoint registration whenever it differs from the last one reported.
	void SampleCheckPoint(iGameInteractionGameAdapter& aGameAdapter, cSessionProtocol& aSession,
		cGameInteractionTransport& aTransport)
	{
		cGameInteractionCheckPoint checkPoint;
		if (!(aSession.mlGrantedCapabilities & eGameInteractionCapability_Enemies) ||
			!aGameAdapter.GetCheckPoint(checkPoint)) return;
		const std::string event = cGameInteractionProtocolVersion2::SerializeCheckPoint(checkPoint);
		if (event == aSession.msLastCheckPoint) return;
		aSession.msLastCheckPoint = event;
		aTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(event));
	}

	// Follows the local Pose's availability, because the report exists only while the local player is
	// on a map. An empty report is not sampled, but an undelivered earlier one is still delivered.
	void SampleReportedBodies(iGameInteractionGameAdapter& aGameAdapter, cStateUpdateSubscription& aSubscription)
	{
		if (!aSubscription.mbSubscribed) return;
		const eGameInteractionLocalPoseAvailability availability = aGameAdapter.GetLocalPoseAvailability();
		if (availability == eGameInteractionLocalPoseAvailability_Unavailable)
		{
			aSubscription.msUndeliveredStateUpdate.clear();
			aSubscription.msLastSample.clear();
			return;
		}

		cGameInteractionBodySamples report = aGameAdapter.GetReportedBodies();
		if (report.mvBodies.empty())
		{
			aSubscription.msLastSample.clear();
			return;
		}
		const unsigned long long timeMs = report.mlTimeMs;
		if (!IsSampleDue(aSubscription, timeMs)) return;
		const std::string stateUpdate = cGameInteractionProtocolVersion2::SerializeReportedBodies(report);
		report.mlTimeMs = 0;
		KeepSample(aSubscription, availability, timeMs, stateUpdate,
			cGameInteractionProtocolVersion2::SerializeReportedBodies(report));
	}

	bool IsCurrentMap(const iGameInteractionGameAdapter& aGameAdapter, const std::string& asMapFile)
	{
		return aGameAdapter.IsMapLoaded() && aGameAdapter.GetMapFile() == asMapFile;
	}

	// Only driving can be not-holdable; the contract answers every other entity Command with not-found
	// when the Session does not drive the entity.
	eGameInteractionCommandOutcome CommandOutcomeFor(eGameInteractionCommandType aType,
		eGameInteractionEntityOutcome aOutcome)
	{
		if (aOutcome == eGameInteractionEntityOutcome_Success) return eGameInteractionCommandOutcome_Success;
		if (aOutcome == eGameInteractionEntityOutcome_NotHoldable && aType == eGameInteractionCommand_EntityDrive)
			return eGameInteractionCommandOutcome_EntityNotHoldable;
		return eGameInteractionCommandOutcome_EntityNotFound;
	}

	// Checked before the game sees the Command, so a late Command for another map never reaches an
	// entity that shares its identifier in the current one.
	eGameInteractionCommandOutcome EntityRequestOutcome(const cGameInteractionEntityRequest& aRequest,
		const iGameInteractionGameAdapter& aGameAdapter)
	{
		if (!aRequest.mbValid) return eGameInteractionCommandOutcome_Invalid;
		if (!IsCurrentMap(aGameAdapter, aRequest.msMapFile)) return eGameInteractionCommandOutcome_WrongMap;
		return eGameInteractionCommandOutcome_Success;
	}

	// A malformed line's identifier is not echoed.
	cGameInteractionResponse EntityResponse(eGameInteractionCommandType aType, eGameInteractionCommandOutcome aOutcome,
		int alEntityId)
	{
		cGameInteractionResponse response(aType, eGameInteractionResponse_Entity, aOutcome);
		if (aOutcome != eGameInteractionCommandOutcome_Invalid) response.SetEntityId(alEntityId);
		return response;
	}

	cGameInteractionResponse ExecuteEnemyCommand(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionProtocol& aSession)
	{
		const cGameInteractionEnemyRequest& request = aCommand.GetEnemyRequest();
		const int id = request.mEnemy.mlEntityId;
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!IsCurrentMap(aGameAdapter, request.mEnemy.mPose.msMapFile)) outcome = eGameInteractionCommandOutcome_WrongMap;
		else if (aCommand.GetType() == eGameInteractionCommand_EnemyDrive)
		{
			outcome = aGameAdapter.DriveEnemy(id);
			if (outcome == eGameInteractionCommandOutcome_Success) aSession.mbDroveEnemy = true;
		}
		else if (aCommand.GetType() == eGameInteractionCommand_EnemyRelease) outcome = aGameAdapter.ReleaseEnemy(id);
		else if (aCommand.GetType() == eGameInteractionCommand_EnemyDoorBreak) outcome = aGameAdapter.BreakEnemyDoor(id);
		else outcome = aGameAdapter.PoseEnemy(request.mEnemy);
		if (aCommand.GetType() == eGameInteractionCommand_EnemyPose)
		{
			const bool failed = outcome != eGameInteractionCommandOutcome_Success;
			const bool report = failed && !aSession.mbEnemyPoseFailing;
			aSession.mbEnemyPoseFailing = failed;
			if (!report) return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_None);
		}
		return EntityResponse(aCommand.GetType(), outcome, id);
	}

	cGameInteractionResponse ExecuteEnemyAttack(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter)
	{
		const cGameInteractionEnemyAttackRequest& request = aCommand.GetEnemyAttackRequest();
		const cGameInteractionEnemyAttack& attack = request.mAttack;
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!IsCurrentMap(aGameAdapter, attack.msMapFile)) outcome = eGameInteractionCommandOutcome_WrongMap;
		else outcome = aGameAdapter.ResolveEnemyAttack(attack);
		return EntityResponse(aCommand.GetType(), outcome, attack.mlEntityId);
	}

	cGameInteractionResponse ExecuteEntityCommand(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionEntities& aEntities)
	{
		const cGameInteractionEntityRequest& request = aCommand.GetEntityRequest();
		const eGameInteractionCommandOutcome requestOutcome = EntityRequestOutcome(request, aGameAdapter);
		if (requestOutcome != eGameInteractionCommandOutcome_Success)
			return EntityResponse(aCommand.GetType(), requestOutcome, request.mlEntityId);

		eGameInteractionEntityOutcome outcome;
		switch (aCommand.GetType())
		{
		case eGameInteractionCommand_EntityDrive:
			outcome = aGameAdapter.DriveEntity(request.mlEntityId);
			if (outcome == eGameInteractionEntityOutcome_Success) aEntities.mbDroveEntity = true;
			break;
		case eGameInteractionCommand_EntityInteracting:
			outcome = aGameAdapter.SetDrivenEntityInteracting(request.mlEntityId, request.mbInteracting);
			break;
		case eGameInteractionCommand_EntityBreak:
			outcome = aGameAdapter.BreakDrivenEntity(request.mlEntityId, request.mState);
			break;
		case eGameInteractionCommand_EntityRelease:
			outcome = aGameAdapter.ReleaseDrivenEntity(request.mlEntityId);
			break;
		default:
			return EntityResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, request.mlEntityId);
		}
		return EntityResponse(aCommand.GetType(), CommandOutcomeFor(aCommand.GetType(), outcome), request.mlEntityId);
	}

	// Bodies stream at network rate, so a success is not answered and a failure is answered only when
	// it starts the Session's one failure streak.
	cGameInteractionResponse DriveEntityBodies(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSessionEntities& aEntities)
	{
		const cGameInteractionEntityRequest& request = aCommand.GetEntityRequest();
		eGameInteractionCommandOutcome outcome = EntityRequestOutcome(request, aGameAdapter);
		int failedEntityId = 0;
		if (outcome == eGameInteractionCommandOutcome_Success)
			outcome = CommandOutcomeFor(aCommand.GetType(), aGameAdapter.DriveEntityBodies(request.mBodies, failedEntityId));
		const bool startsStreak = outcome != eGameInteractionCommandOutcome_Success && !aEntities.mbBodiesFailing;
		aEntities.mbBodiesFailing = outcome != eGameInteractionCommandOutcome_Success;
		if (!startsStreak) return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_None);
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Entity, outcome);
		if (outcome == eGameInteractionCommandOutcome_EntityNotFound) response.SetEntityId(failedEntityId);
		return response;
	}

	// Reject late state Commands for another map.
	cGameInteractionResponse ApplyEntityState(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter)
	{
		const cGameInteractionEntityStateRequest& request = aCommand.GetEntityStateRequest();
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!aGameAdapter.IsMapLoaded() || aGameAdapter.GetMapFile() != request.mState.msMapFile)
			outcome = eGameInteractionCommandOutcome_WrongMap;
		else outcome = aGameAdapter.ApplyEntityState(request.mState);
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_EntityState, outcome);
	}

	// An episode names its map, so a late one never kills the player in another map.
	cGameInteractionResponse ApplySharedDeath(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cSharedDeathModel& aSharedDeaths)
	{
		const cGameInteractionSharedDeathRequest& request = aCommand.GetSharedDeathRequest();
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!IsCurrentMap(aGameAdapter, request.mEpisode.msMapFile)) outcome = eGameInteractionCommandOutcome_WrongMap;
		else
		{
			const eSharedDeathOutcome applied = aSharedDeaths.ApplyEpisode(request.mEpisode);
			if (applied == eSharedDeathOutcome_Duplicate) outcome = eGameInteractionCommandOutcome_Duplicate;
			else if (applied == eSharedDeathOutcome_Stale) outcome = eGameInteractionCommandOutcome_Stale;
			// The death the kill starts comes back through BeginPlayerDeath, where the episode claims it.
			else if (applied == eSharedDeathOutcome_Kill && !aGameAdapter.KillPlayer())
			{
				aSharedDeaths.AbandonKill();
				outcome = eGameInteractionCommandOutcome_Unavailable;
			}
		}
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_SharedDeath, outcome);
	}

	// A placement names its map, so a late Command never places the player in another map.
	cGameInteractionResponse PlacePlayer(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter)
	{
		const cGameInteractionPlacementRequest& request = aCommand.GetPlacementRequest();
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!IsCurrentMap(aGameAdapter, request.mPlacement.msMapFile))
			outcome = eGameInteractionCommandOutcome_WrongMap;
		else if (!aGameAdapter.PlacePlayer(request.mPlacement)) outcome = eGameInteractionCommandOutcome_Unavailable;
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_PlayerPlace, outcome);
	}

	// A Shared Callback names its map, so a late Command never runs a registration of another map.
	cGameInteractionResponse RunSharedCallback(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter)
	{
		const cGameInteractionSharedCallbackRequest& request = aCommand.GetSharedCallbackRequest();
		eGameInteractionCommandOutcome outcome = eGameInteractionCommandOutcome_Success;
		if (!request.mbValid) outcome = eGameInteractionCommandOutcome_Invalid;
		else if (!IsCurrentMap(aGameAdapter, request.mCallback.msMapFile)) outcome = eGameInteractionCommandOutcome_WrongMap;
		else if (!aGameAdapter.RunSharedCallback(request.mCallback)) outcome = eGameInteractionCommandOutcome_CallbackNotFound;
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_SharedCallback, outcome);
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

	cGameInteractionResponse SetAvatarOption(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, const cSessionAvatars& aAvatars)
	{
		const cGameInteractionAvatarRequest& request = aCommand.GetAvatarRequest();
		if (!request.mbValid)
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, std::string());
		if (!aAvatars.Contains(request.msIdentifier))
			return AvatarResponse(aCommand.GetType(), eGameInteractionCommandOutcome_AvatarNotFound, request.msIdentifier);
		if (aCommand.GetType() == eGameInteractionCommand_AvatarEnemySight)
			aGameAdapter.SetAvatarEnemySight(request.msIdentifier, request.mbEnemySight);
		else aGameAdapter.SetAvatarCollision(request.msIdentifier, request.mbCollides);
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

	eGameInteractionCommandOutcome CommandOutcomeFor(ePeerSettingOutcome aOutcome)
	{
		switch (aOutcome)
		{
		case ePeerSettingOutcome_Success: return eGameInteractionCommandOutcome_Success;
		case ePeerSettingOutcome_Duplicate: return eGameInteractionCommandOutcome_Duplicate;
		case ePeerSettingOutcome_Unavailable: return eGameInteractionCommandOutcome_Unavailable;
		default: return eGameInteractionCommandOutcome_SettingNotFound;
		}
	}

	// Names the key unless the line was invalid, and carries the effective value on success.
	cGameInteractionResponse SettingResponse(eGameInteractionCommandType aType, eGameInteractionCommandOutcome aOutcome,
		const std::string& asKey, const std::string& asValue)
	{
		cGameInteractionResponse response(aType, eGameInteractionResponse_Setting, aOutcome);
		if (aOutcome != eGameInteractionCommandOutcome_Invalid) response.SetSetting(asKey, asValue);
		return response;
	}

	cGameInteractionResponse ExecuteSettingCommand(const cGameInteractionCommand& aCommand,
		cPeerSettingModel& aPeerSettings, unsigned int alSession)
	{
		const cGameInteractionSettingRequest& request = aCommand.GetSettingRequest();
		const std::string& key = request.mAnnouncement.msKey;
		if (!request.mbValid)
			return SettingResponse(aCommand.GetType(), eGameInteractionCommandOutcome_Invalid, key, std::string());
		std::string value;
		const ePeerSettingOutcome outcome = aCommand.GetType() == eGameInteractionCommand_SettingAnnounce ?
			aPeerSettings.Announce(alSession, request.mAnnouncement, value) :
			aPeerSettings.ReadValue(alSession, key, value);
		return SettingResponse(aCommand.GetType(), CommandOutcomeFor(outcome), key, value);
	}

	unsigned int RequiredCapability(eGameInteractionCommandType aType)
	{
		switch (aType)
		{
		case eGameInteractionCommand_AvatarCreate:
		case eGameInteractionCommand_AvatarRemove:
		case eGameInteractionCommand_AvatarEnemySight:
		case eGameInteractionCommand_AvatarCollision:
		case eGameInteractionCommand_AvatarPose:
			return eGameInteractionCapability_Avatars;
		case eGameInteractionCommand_EnemyPoses:
		case eGameInteractionCommand_EnemyDrive:
		case eGameInteractionCommand_EnemyPose:
		case eGameInteractionCommand_EnemyRelease:
		case eGameInteractionCommand_EnemyAttack:
		case eGameInteractionCommand_SharedDeath:
		case eGameInteractionCommand_EnemyDoorBreak:
			return eGameInteractionCapability_Enemies;
		case eGameInteractionCommand_LocalPose:
			return eGameInteractionCapability_LocalPose;
		case eGameInteractionCommand_ReportedBodies:
		case eGameInteractionCommand_EntityDrive:
		case eGameInteractionCommand_EntityBodies:
		case eGameInteractionCommand_EntityInteracting:
		case eGameInteractionCommand_EntityBreak:
		case eGameInteractionCommand_EntityRelease:
			return eGameInteractionCapability_Interactions;
		case eGameInteractionCommand_ButtonState:
		case eGameInteractionCommand_LampState:
		case eGameInteractionCommand_SharedCallback:
			return eGameInteractionCapability_SharedCallbacks;
		case eGameInteractionCommand_PlayerPlace:
			return eGameInteractionCapability_PlayerPlace;
		case eGameInteractionCommand_SettingAnnounce:
		case eGameInteractionCommand_SettingValue:
			return eGameInteractionCapability_Settings;
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
		cPendingCustomStoryStart& aPendingStart, cPeerSettingModel& aPeerSettings, unsigned int alSession)
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
		case eGameInteractionCommand_EnemyDrive:
		case eGameInteractionCommand_EnemyPose:
		case eGameInteractionCommand_EnemyRelease:
		case eGameInteractionCommand_EnemyDoorBreak:
			return ExecuteEnemyCommand(aCommand, aGameAdapter, aSession);
		case eGameInteractionCommand_EnemyAttack:
			return ExecuteEnemyAttack(aCommand, aGameAdapter);
		case eGameInteractionCommand_SharedDeath:
			return ApplySharedDeath(aCommand, aGameAdapter, aSession.mSharedDeaths);
		case eGameInteractionCommand_EnemyPoses:
			return ChangeSubscription(aCommand, aSession.mEnemySubscription);
		case eGameInteractionCommand_LocalPose:
			return ChangeSubscription(aCommand, aSession.mLocalPoseSubscription);
		case eGameInteractionCommand_ReportedBodies:
			return ChangeSubscription(aCommand, aSession.mReportedBodiesSubscription);
		case eGameInteractionCommand_EntityBodies:
			return DriveEntityBodies(aCommand, aGameAdapter, aSession.mEntities);
		case eGameInteractionCommand_EntityDrive:
		case eGameInteractionCommand_EntityInteracting:
		case eGameInteractionCommand_EntityBreak:
		case eGameInteractionCommand_EntityRelease:
			return ExecuteEntityCommand(aCommand, aGameAdapter, aSession.mEntities);
		case eGameInteractionCommand_ButtonState:
		case eGameInteractionCommand_LampState:
			return ApplyEntityState(aCommand, aGameAdapter);
		case eGameInteractionCommand_PlayerPlace:
			return PlacePlayer(aCommand, aGameAdapter);
		case eGameInteractionCommand_SettingAnnounce:
		case eGameInteractionCommand_SettingValue:
			return ExecuteSettingCommand(aCommand, aPeerSettings, alSession);
		case eGameInteractionCommand_SharedCallback:
			return RunSharedCallback(aCommand, aGameAdapter);
		case eGameInteractionCommand_AvatarCreate:
			return CreateAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarRemove:
			return RemoveAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarPose:
			return PoseAvatar(aCommand, aGameAdapter, aSession.mAvatars);
		case eGameInteractionCommand_AvatarEnemySight:
		case eGameInteractionCommand_AvatarCollision:
			return SetAvatarOption(aCommand, aGameAdapter, aSession.mAvatars);
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
			const cGameInteractionChatRequest& request = aCommand.GetChatRequest();
			if (!request.mbValid)
				return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
					eGameInteractionCommandOutcome_Invalid);
			cChatEntry entry;
			const eChatEntryValidation validation = cChatModel::TryCreateEntry(request.msAuthor,
				request.msMessage, entry, request.mAuthorColor, request.mMessageColor);
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
	// Whether an ended Session may have left Peer-Driven Entities that the game has not released yet.
	bool mbEndedSessionDroveEntities;

	bool mbEndedSessionDroveEnemies;
	// Outlives every Session, so a later Session can announce a key an ended one owned.
	cPeerSettingModel mPeerSettings;
	// Numbers the current Session for the Peer Setting model.
	unsigned int mlSessionNumber;
	cImplementation() : mbEndedSessionDroveEntities(false), mbEndedSessionDroveEnemies(false), mlSessionNumber(1) {}

	void EndSession()
	{
		mPeerSettings.EndSession(mlSessionNumber);
		++mlSessionNumber;
		const std::vector<std::string>& avatars = mSession.mAvatars.mvIdentifiers;
		mvEndedSessionAvatars.insert(mvEndedSessionAvatars.end(), avatars.begin(), avatars.end());
		mbEndedSessionDroveEntities = mbEndedSessionDroveEntities || mSession.mEntities.mbDroveEntity;
		mbEndedSessionDroveEnemies = mbEndedSessionDroveEnemies || mSession.mbDroveEnemy;
		mSession = cSessionProtocol();
	}

	void CleanUpEndedSessions(iGameInteractionGameAdapter& aGameAdapter)
	{
		std::vector<std::string> avatars;
		avatars.swap(mvEndedSessionAvatars);
		for (std::vector<std::string>::const_iterator avatar = avatars.begin(); avatar != avatars.end(); ++avatar)
			aGameAdapter.RemoveAvatar(*avatar);
		if (mbEndedSessionDroveEnemies) aGameAdapter.ReleaseDrivenEnemies();
		mbEndedSessionDroveEnemies = false;
		if (!mbEndedSessionDroveEntities) return;
		mbEndedSessionDroveEntities = false;
		aGameAdapter.ReleaseDrivenEntities();
	}

	// The legacy adapter serves a Session until it negotiates; its negotiation Command is the only
	// Protocol Version 2 line a legacy Session understands.
	cGameInteractionCommand ParseCommand(const std::string& asLine) const
	{
		if (mSession.mbNegotiated || cGameInteractionProtocolVersion2::IsNegotiation(asLine))
			return cGameInteractionProtocolVersion2::ParseCommand(asLine, mSession.mlGrantedCapabilities);
		return cLegacyGameInteractionProtocol::ParseCommand(asLine);
	}

	std::string SerializeResponse(const cGameInteractionResponse& aResponse) const
	{
		if (mSession.mbNegotiated || aResponse.GetCommandType() == eGameInteractionCommand_NegotiateProtocol)
			return cGameInteractionProtocolVersion2::SerializeResponse(aResponse);
		return cLegacyGameInteractionProtocol::SerializeResponse(aResponse);
	}

	// State Updates are handed to the transport only once everything queued before them was sent, so
	// at most one undelivered State Update of each kind exists and it is always the newest one.
	void DeliverStateUpdates()
	{
		std::string& localPose = mSession.mLocalPoseSubscription.msUndeliveredStateUpdate;
		std::string& reportedBodies = mSession.mReportedBodiesSubscription.msUndeliveredStateUpdate;
		if ((localPose.empty() && reportedBodies.empty() && mSession.mEnemyUpdates.empty()) || !mTransport.HasPeer() ||
			mTransport.GetPendingDeliveryByteCount() != 0) return;
		if (!localPose.empty()) mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(localPose));
		if (!reportedBodies.empty()) mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(reportedBodies));
		for (std::map<int, std::string>::const_iterator it = mSession.mEnemyUpdates.begin();
			it != mSession.mEnemyUpdates.end(); ++it)
			mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(it->second));
		mSession.mEnemyUpdates.clear();
		localPose.clear();
		reportedBodies.clear();
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

	mpImplementation->CleanUpEndedSessions(aGameAdapter);
	mpImplementation->PerformPendingCustomStoryStart(aGameAdapter);
	if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		aGameAdapter.SetPeerConnected(false);
		return;
	}

	for (std::vector<std::string>::const_iterator bytes = receivedBytes.begin();
		bytes != receivedBytes.end(); ++bytes)
		mpImplementation->mInboundLines.Append(bytes->data(), bytes->size());

	std::string commandText;
	while (mpImplementation->mTransport.HasPeer() &&
		mpImplementation->mInboundLines.TryPopLine(commandText))
	{
		const cGameInteractionCommand command = mpImplementation->ParseCommand(commandText);
		const cGameInteractionResponse response = ExecuteCommand(command, aGameAdapter,
			mpImplementation->mSession, mpImplementation->mPendingCustomStoryStart, mpImplementation->mPeerSettings,
			mpImplementation->mlSessionNumber);
		if (response.GetType() == eGameInteractionResponse_None) continue;
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			mpImplementation->SerializeResponse(response)));
	}
	if (mpImplementation->mTransport.HasPeer())
		SampleCheckPoint(aGameAdapter, mpImplementation->mSession, mpImplementation->mTransport);
	// Delivering Responses now rather than on the next update removes a tick of latency.
	mpImplementation->mTransport.Flush();
	SampleEnemies(aGameAdapter, mpImplementation->mSession);
	SampleLocalPose(aGameAdapter, mpImplementation->mSession.mLocalPoseSubscription,
		mpImplementation->mSession.mlGrantedCapabilities);
	SampleReportedBodies(aGameAdapter, mpImplementation->mSession.mReportedBodiesSubscription);
	mpImplementation->DeliverStateUpdates();

	if (mpImplementation->mInboundLines.HasExceededLineLimit())
	{
		mpImplementation->mTransport.DisconnectPeer("Peer exceeded the inbound line length limit");
		mpImplementation->mInboundLines.Clear();
		mpImplementation->EndSession();
		mpImplementation->CleanUpEndedSessions(aGameAdapter);
	}
	aGameAdapter.SetPeerConnected(mpImplementation->mTransport.HasPeer());
}

void cGameInteractionGateway::Report(const cGameInteractionEvent& aEvent)
{
	if (aEvent.GetType() == eGameInteractionEvent_MapEntered || aEvent.GetType() == eGameInteractionEvent_MapChanged)
	{
		mpImplementation->mSession.mEnemyUpdates.clear();
		mpImplementation->mSession.mEnemySubscription.mbScheduled = false;
		mpImplementation->mSession.mbEnemyPoseFailing = false;
	}
	if (aEvent.GetType() == eGameInteractionEvent_MapEntered)
	{
		mpImplementation->mSession.msLastCheckPoint.clear();
		mpImplementation->mSession.mSharedDeaths.EndMapVisit();
	}
	if (!mpImplementation->mTransport.HasPeer()) return;
	if (aEvent.GetType() == eGameInteractionEvent_EnemyAttack || aEvent.GetType() == eGameInteractionEvent_EnemyDoorBreak)
	{
		if (mpImplementation->mSession.mlGrantedCapabilities & eGameInteractionCapability_Enemies)
			mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
				cGameInteractionProtocolVersion2::SerializeEvent(aEvent)));
		return;
	}
	if (aEvent.GetType() == eGameInteractionEvent_SharedCallback ||
		aEvent.GetType() == eGameInteractionEvent_ButtonState || aEvent.GetType() == eGameInteractionEvent_LampState)
	{
		if (mpImplementation->mSession.mlGrantedCapabilities & eGameInteractionCapability_SharedCallbacks)
			mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
				cGameInteractionProtocolVersion2::SerializeEvent(aEvent)));
		return;
	}
	if (!aEvent.IsInteractionsEvent())
	{
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cLegacyGameInteractionProtocol::SerializeEvent(aEvent)));
		return;
	}
	if (mpImplementation->mSession.mlGrantedCapabilities & eGameInteractionCapability_Interactions)
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cGameInteractionProtocolVersion2::SerializeEvent(aEvent)));
}

eSharedDeathPolicy cGameInteractionGateway::BeginPlayerDeath(const cGameInteractionCheckPoint& aCheckPoint,
	bool abOtherPlayerOnMap)
{
	cSessionProtocol& session = mpImplementation->mSession;
	const bool shares = mpImplementation->mTransport.HasPeer() &&
		(session.mlGrantedCapabilities & eGameInteractionCapability_Enemies) != 0;
	const eSharedDeathPolicy policy = cSharedDeathModel::SelectPolicy(shares && abOtherPlayerOnMap,
		aCheckPoint.mbCallback);
	const int death = session.mSharedDeaths.BeginDeath(policy);
	if (shares && death != 0)
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cGameInteractionProtocolVersion2::SerializePlayerDeath(death, aCheckPoint)));
	return policy;
}

bool cGameInteractionGateway::HoldsCheckPoint(double afTimeMs)
{
	return mpImplementation->mSession.mSharedDeaths.HoldsCheckPoint(afTimeMs);
}

cSharedDeathCheckPoint cGameInteractionGateway::CompleteCheckPoint(const cGameInteractionCheckPoint& aCheckPoint)
{
	// A name no episode can carry never matches one, even the - every such name is reported as.
	const std::string name = cGameInteractionProtocolVersion2::IsValidItemName(aCheckPoint.msName) ?
		aCheckPoint.msName : std::string();
	return mpImplementation->mSession.mSharedDeaths.CompleteCheckPoint(name, aCheckPoint.mbCallback,
		aCheckPoint.mlCount);
}

bool cGameInteractionGateway::PreservesSharedEnemies() const
{
	return mpImplementation->mSession.mSharedDeaths.PreservesEnemies();
}

bool cGameInteractionGateway::SharesEnemies() const
{
	return mpImplementation->mTransport.HasPeer() &&
		(mpImplementation->mSession.mlGrantedCapabilities & eGameInteractionCapability_Enemies) != 0;
}

void cGameInteractionGateway::SetPeerSettingStore(iPeerSettingStore* apStore)
{
	mpImplementation->mPeerSettings.SetStore(apStore);
}

const std::vector<cPeerSetting>& cGameInteractionGateway::GetPeerSettings() const
{
	return mpImplementation->mPeerSettings.GetSettings();
}

unsigned int cGameInteractionGateway::GetPeerSettingsRevision() const
{
	return mpImplementation->mPeerSettings.GetRevision();
}

// Only the current Session can own a key, so every change goes to its Peer; the next update delivers it.
void cGameInteractionGateway::ConfirmPeerSettings(const std::vector<cPeerSettingEdit>& avEdits)
{
	const std::vector<cPeerSettingChange> changes = mpImplementation->mPeerSettings.Confirm(avEdits);
	for (size_t index = 0; index < changes.size(); ++index)
	{
		if (changes[index].mlSession != mpImplementation->mlSessionNumber || !mpImplementation->mTransport.HasPeer())
			continue;
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cGameInteractionProtocolVersion2::SerializeSettingChanged(changes[index].msKey, changes[index].msValue)));
	}
}

int cGameInteractionGateway::GetPort() const
{
	return mpImplementation->mTransport.GetPort();
}

bool cGameInteractionGateway::IsListening() const
{
	return mpImplementation->mTransport.IsListening();
}

const std::string& cGameInteractionGateway::GetDiagnostic() const
{
	return mpImplementation->mTransport.GetDiagnostic();
}
