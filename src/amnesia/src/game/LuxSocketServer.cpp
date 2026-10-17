#include "LuxSocketServer.h"
#include "LuxAvatarHandler.h"
#include "LuxInteractionReportHandler.h"
#include "LuxPeerDrivenEntityHandler.h"
#include "LuxSharedCallbackHandler.h"
#include "LuxMap.h"
#include "LuxEnemy.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxChatHandler.h"
#include "LuxConfigHandler.h"
#include "LuxInputHandler.h"
#include "LuxMainMenu.h"
#include "LuxMoveState_Normal.h"

#include <set>

namespace
{
	cGameInteractionCheckPoint CurrentCheckPoint()
	{
		cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
		cGameInteractionCheckPoint checkPoint;
		checkPoint.mlCount = pMap->GetCheckPointCount();
		checkPoint.mbCallback = pMap->HasCheckPointCallback();
		checkPoint.msName = pMap->GetCheckPointName();
		checkPoint.msMapFile = pMap->GetMapPath();
		return checkPoint;
	}

	class cLuxGameInteractionGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		virtual bool IsMapLoaded() const
		{
			return gpBase->mpMapHandler && gpBase->mpMapHandler->GetCurrentMap();
		}

		virtual cGameInteractionPosition GetPosition() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			const cVector3f position = pCharBody->GetFeetPosition();
			return cGameInteractionPosition(position.x, position.y, position.z);
		}

		virtual cGameInteractionRotation GetRotation() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			return cGameInteractionRotation(pCharBody->GetYaw(), pCharBody->GetPitch());
		}

		virtual std::string GetMapFile() const
		{
			return gpBase->mpMapHandler->GetCurrentMap()->GetMapPath();
		}

		virtual void RunScript(const std::string& asScript)
		{
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(asScript);
		}

		virtual bool DisplayChatEntry(const cChatEntry& aEntry)
		{
			return gpBase->mpChatHandler && gpBase->mpChatHandler->DisplayChatEntry(aEntry);
		}

		virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const
		{
			std::vector<cGameInteractionCustomStory> vStories;
			std::set<tWString> setIdentifiers;
			tWStringList lstStoryFolders;
			cLuxCustomStorySettings::FindInstalledStoryFolders(lstStoryFolders);
			for(tWStringListIt it = lstStoryFolders.begin(); it != lstStoryFolders.end(); ++it)
			{
				// Like starting, the menu's earlier location wins when an identifier is installed twice.
				const tWString sIdentifier = cString::GetFileNameW(*it);
				if(!setIdentifiers.insert(sIdentifier).second) continue;

				cLuxCustomStorySettings story;
				// Peers may poll the listing, so broken stories are only logged when started.
				if(story.CreateFromPath(*it, false))
					vStories.push_back(cGameInteractionCustomStory(sIdentifier, story.msName));
			}
			return vStories;
		}

		virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(
			const std::wstring& asIdentifier) const
		{
			cLuxCustomStorySettings story;
			return FindCustomStory(asIdentifier, story);
		}

		virtual void StartCustomStory(const std::wstring& asIdentifier)
		{
			// The game may have left the main menu since the Command was accepted.
			cLuxCustomStorySettings story;
			if(FindCustomStory(asIdentifier, story) != eGameInteractionCustomStoryAvailability_Available)
				return;

			if(gpBase->mpUserConfig==NULL)
			{
				gpBase->CreateProfile(gpBase->msDefaultProfileName);
				gpBase->SetProfile(gpBase->msDefaultProfileName);
				gpBase->InitUserConfig();
			}

			story.SetActive();
			gpBase->StartCustomStory();
		}

		virtual eGameInteractionLocalPoseAvailability GetLocalPoseAvailability() const
		{
			if(!IsMapLoaded()) return eGameInteractionLocalPoseAvailability_Unavailable;
			switch(gpBase->mpInputHandler->GetState())
			{
			case eLuxInputState_LoadScreen: return eGameInteractionLocalPoseAvailability_Unavailable;
			case eLuxInputState_Game: return eGameInteractionLocalPoseAvailability_Live;
			// The in-game main menu is the pause menu.
			default: return eGameInteractionLocalPoseAvailability_Suspended;
			}
		}

		virtual cGameInteractionPose GetLocalPose() const
		{
			cLuxPlayer *pPlayer = gpBase->mpPlayer;
			iCharacterBody* pCharBody = pPlayer->GetCharacterBody();
			const cVector3f vFeet = pCharBody->GetFeetPosition();
			cLuxMoveState_Normal *pMoveNormal =
				static_cast<cLuxMoveState_Normal*>(pPlayer->GetMoveStateData(eLuxMoveState_Normal));

			cGameInteractionPose pose;
			pose.mlTimeMs = static_cast<unsigned long long>(gpBase->mpEngine->GetGameTime() * 1000.0);
			pose.mlTeleportCounter = pPlayer->GetTeleportCounter();
			pose.mFeetPosition = cGameInteractionPosition(vFeet.x, vFeet.y, vFeet.z);
			pose.mfBodyYawDegrees = cMath::ToDeg(pCharBody->GetYaw());
			pose.mfCameraPitchDegrees = cMath::ToDeg(pPlayer->GetCamera()->GetPitch());
			pose.mbCrouching = pMoveNormal->IsCrouching();
			pose.mbLanternRaised = pPlayer->GetHelperLantern()->IsActive();
			pose.msMapFile = GetMapFile();
			return pose;
		}

		virtual std::vector<cGameInteractionEnemyPose> GetEnemyPoses() const
		{
			std::vector<cGameInteractionEnemyPose> poses;
			if(!IsMapLoaded()) return poses;
			cLuxEnemyIterator it = gpBase->mpMapHandler->GetCurrentMap()->GetEnemyIterator();
			while(it.HasNext())
			{
				iLuxEnemy* enemy = it.Next();
				if(enemy->GetEnemyType() == eLuxEnemyType_Grunt && enemy->IsActive() &&
					!enemy->GetDestroyMe()) poses.push_back(enemy->GetEnemyPose());
			}
			return poses;
		}

		virtual eGameInteractionCommandOutcome DriveEnemy(int alEntityId)
		{
			iLuxEnemy* enemy = NULL;
			const eGameInteractionCommandOutcome outcome = FindEnemy(alEntityId, enemy);
			if(outcome == eGameInteractionCommandOutcome_Success) enemy->SetEnemyDriven(true);
			return outcome;
		}

		virtual eGameInteractionCommandOutcome PoseEnemy(const cGameInteractionEnemyPose& aPose)
		{
			iLuxEnemy* enemy = NULL;
			const eGameInteractionCommandOutcome outcome = FindEnemy(aPose.mlEntityId, enemy);
			if(outcome != eGameInteractionCommandOutcome_Success) return outcome;
			if(!enemy->IsEnemyDriven()) return eGameInteractionCommandOutcome_EntityNotFound;
			return enemy->AddEnemyPose(aPose) ? eGameInteractionCommandOutcome_Success : eGameInteractionCommandOutcome_Invalid;
		}

		virtual eGameInteractionCommandOutcome ReleaseEnemy(int alEntityId)
		{
			iLuxEnemy* enemy = NULL;
			const eGameInteractionCommandOutcome outcome = FindEnemy(alEntityId, enemy);
			if(outcome != eGameInteractionCommandOutcome_Success) return outcome;
			if(!enemy->IsEnemyDriven()) return eGameInteractionCommandOutcome_EntityNotFound;
			enemy->SetEnemyDriven(false);
			return eGameInteractionCommandOutcome_Success;
		}

		virtual bool GetCheckPoint(cGameInteractionCheckPoint& aCheckPoint) const
		{
			if(!IsMapLoaded()) return false;
			aCheckPoint = CurrentCheckPoint();
			return true;
		}

		virtual bool KillPlayer()
		{
			cLuxPlayer *pPlayer = gpBase->mpPlayer;
			if(GetLocalPoseAvailability() == eGameInteractionLocalPoseAvailability_Unavailable || !pPlayer->IsActive() ||
				pPlayer->IsDead() || pPlayer->GetHelperFlashback()->IsActive())
			{
				Log("Game Interaction Protocol: cannot kill the player for a Shared Death: not in play\n");
				return false;
			}
			pPlayer->SetHealth(0);
			return true;
		}

		virtual eGameInteractionCommandOutcome ResolveEnemyAttack(const cGameInteractionEnemyAttack& aAttack)
		{
			iLuxEnemy* enemy = NULL;
			const eGameInteractionCommandOutcome outcome = FindEnemy(aAttack.mlEntityId, enemy);
			if(outcome != eGameInteractionCommandOutcome_Success) return outcome;
			if(!enemy->IsEnemyDriven()) return eGameInteractionCommandOutcome_EntityNotFound;
			return enemy->ResolveEnemyAttack(aAttack) ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_Invalid;
		}

		virtual void ReleaseDrivenEnemies()
		{
			if(!IsMapLoaded()) return;
			cLuxEnemyIterator it = gpBase->mpMapHandler->GetCurrentMap()->GetEnemyIterator();
			while(it.HasNext()) it.Next()->SetEnemyDriven(false);
		}

		virtual bool CreateAvatar(const std::string& asIdentifier, const std::string& asEntityFile)
		{
			return gpBase->mpAvatarHandler && gpBase->mpAvatarHandler->CreateAvatar(asIdentifier, asEntityFile);
		}

		virtual void RemoveAvatar(const std::string& asIdentifier)
		{
			if(gpBase->mpAvatarHandler) gpBase->mpAvatarHandler->RemoveAvatar(asIdentifier);
		}

		virtual void PoseAvatar(const std::string& asIdentifier, const cGameInteractionPose& aPose)
		{
			if(gpBase->mpAvatarHandler) gpBase->mpAvatarHandler->PoseAvatar(asIdentifier, aPose);
		}

		virtual void SetAvatarEnemySight(const std::string& asIdentifier, bool abEnabled)
		{
			if(gpBase->mpAvatarHandler) gpBase->mpAvatarHandler->SetAvatarEnemySight(asIdentifier, abEnabled);
		}

		virtual void SetAvatarCollision(const std::string& asIdentifier, bool abCollides)
		{
			if(gpBase->mpAvatarHandler) gpBase->mpAvatarHandler->SetAvatarCollision(asIdentifier, abCollides);
		}

		virtual cGameInteractionBodySamples GetReportedBodies() const
		{
			if(gpBase->mpInteractionReportHandler==NULL) return cGameInteractionBodySamples();
			return gpBase->mpInteractionReportHandler->GetReportedBodies();
		}

		virtual eGameInteractionEntityOutcome DriveEntity(int alEntityId)
		{
			if(gpBase->mpPeerDrivenEntityHandler==NULL) return eGameInteractionEntityOutcome_NotHoldable;
			return gpBase->mpPeerDrivenEntityHandler->DriveEntity(alEntityId);
		}

		virtual eGameInteractionEntityOutcome DriveEntityBodies(const cGameInteractionBodySamples& aSamples,
			int& alFailedEntityId)
		{
			if(gpBase->mpPeerDrivenEntityHandler==NULL) return eGameInteractionEntityOutcome_NotFound;
			return gpBase->mpPeerDrivenEntityHandler->DriveEntityBodies(aSamples, alFailedEntityId);
		}

		virtual eGameInteractionEntityOutcome SetDrivenEntityInteracting(int alEntityId, bool abInteracting)
		{
			if(gpBase->mpPeerDrivenEntityHandler==NULL) return eGameInteractionEntityOutcome_NotFound;
			return gpBase->mpPeerDrivenEntityHandler->SetEntityInteracting(alEntityId, abInteracting);
		}

		virtual eGameInteractionEntityOutcome BreakDrivenEntity(int alEntityId,
			const cGameInteractionBodyState& aFinalState)
		{
			if(gpBase->mpPeerDrivenEntityHandler==NULL) return eGameInteractionEntityOutcome_NotFound;
			return gpBase->mpPeerDrivenEntityHandler->BreakEntity(alEntityId, aFinalState);
		}

		virtual eGameInteractionEntityOutcome ReleaseDrivenEntity(int alEntityId)
		{
			if(gpBase->mpPeerDrivenEntityHandler==NULL) return eGameInteractionEntityOutcome_NotFound;
			return gpBase->mpPeerDrivenEntityHandler->ReleaseEntity(alEntityId);
		}

		virtual void ReleaseDrivenEntities()
		{
			if(gpBase->mpPeerDrivenEntityHandler) gpBase->mpPeerDrivenEntityHandler->ReleaseEntities();
		}

		virtual eGameInteractionCommandOutcome ApplyEntityState(const cGameInteractionEntityState& aState)
		{
			return gpBase->mpSharedCallbackHandler ? gpBase->mpSharedCallbackHandler->ApplyEntityState(aState) :
				eGameInteractionCommandOutcome_Unavailable;
		}

		virtual bool RunSharedCallback(const cGameInteractionSharedCallback& aCallback)
		{
			return gpBase->mpSharedCallbackHandler && gpBase->mpSharedCallbackHandler->RunSharedCallback(aCallback);
		}

		virtual bool PlacePlayer(const cGameInteractionPlacement& aPlacement)
		{
			cLuxPlayer *pPlayer = gpBase->mpPlayer;
			if(GetLocalPoseAvailability() != eGameInteractionLocalPoseAvailability_Live || !pPlayer->IsActive() ||
				pPlayer->IsDead() || pPlayer->GetHelperFlashback()->IsActive())
			{
				Log("Game Interaction Protocol: cannot place the player: not in play\n");
				return false;
			}

			// Not a script call, so Shared Callback mode's hold on player control does not apply.
			const cGameInteractionPosition& feet = aPlacement.mFeetPosition;
			pPlayer->PlaceAt(cVector3f(feet.mfX, feet.mfY, feet.mfZ), cMath::ToRad(aPlacement.mfBodyYawDegrees),
				aPlacement.mbCrouching);
			return true;
		}

		virtual void SetPeerConnected(bool abConnected)
		{
			// A Peer keeps the game updating while it is out of focus, so Sessions do not stall.
			gpBase->mpEngine->SetWaitIfAppOutOfFocus(gpBase->mpConfigHandler->mbSleepWhenOutOfFocus && !abConnected);
		}

	private:
		static eGameInteractionCommandOutcome FindEnemy(int alEntityId, iLuxEnemy*& apEnemy)
		{
			cLuxMap* map = gpBase->mpMapHandler->GetCurrentMap();
			iLuxEntity* entity = map ? map->GetEntityByID(alEntityId) : NULL;
			if(!entity || entity->GetDestroyMe()) return eGameInteractionCommandOutcome_EntityNotFound;
			if(entity->GetEntityType() != eLuxEntityType_Enemy || entity->IsCreatedAtRuntime())
				return eGameInteractionCommandOutcome_EntityWrongType;
			apEnemy = static_cast<iLuxEnemy*>(entity);
			return apEnemy->GetEnemyType() == eLuxEnemyType_Grunt ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_EntityWrongType;
		}

		static bool IsInMainMenu()
		{
			return gpBase->mpInputHandler->GetState() == eLuxInputState_MainMenu &&
				!gpBase->mpMapHandler->MapIsLoaded() && !gpBase->mpMainMenu->IsTransitioning();
		}

		static eGameInteractionCustomStoryAvailability FindCustomStory(const tWString& asIdentifier,
			cLuxCustomStorySettings& aStory)
		{
			if(!IsInMainMenu())
			{
				Log("Game Interaction Protocol: cannot start custom story '%ls': not in main menu\n",
					asIdentifier.c_str());
				return eGameInteractionCustomStoryAvailability_NotInMainMenu;
			}

			tWStringList lstStoryFolders;
			cLuxCustomStorySettings::FindInstalledStoryFolders(lstStoryFolders);
			for(tWStringListIt it = lstStoryFolders.begin(); it != lstStoryFolders.end(); ++it)
			{
				if(cString::GetFileNameW(*it) != asIdentifier) continue;
				// CreateFromPath logs why an installed story is invalid.
				return aStory.CreateFromPath(*it) ? eGameInteractionCustomStoryAvailability_Available :
					eGameInteractionCustomStoryAvailability_Invalid;
			}

			Log("Game Interaction Protocol: cannot start custom story '%ls': not found\n", asIdentifier.c_str());
			return eGameInteractionCustomStoryAvailability_NotFound;
		}
	};
}

cLuxSocketServer::cLuxSocketServer()
    : iLuxUpdateable("LuxSocketServer")
{
	mHost = "127.0.0.1";
	mPort = 5150;
	if (gpBase->mpChatHandler) gpBase->mpChatHandler->SetSubmissionSink(this);

	InitSocket();
    Log("cLuxSocketServer created!\n");
}

bool cLuxSocketServer::InitSocket()
{
	if (!mGateway.Listen(mHost, mPort))
	{
		Log("Game Interaction Protocol listener failed: %s\n", mGateway.GetDiagnostic().c_str());
		return false;
	}
	Log("Socket listening on %s:%d\n", mHost.c_str(), mPort);
	return true;
}

void cLuxSocketServer::Update(float afTimeStep)
{
	cLuxGameInteractionGameAdapter gameAdapter;
	mGateway.Update(gameAdapter);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::PublishEvent(const cGameInteractionEvent& aEvent)
{
	mGateway.Report(aEvent);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::BeginPlayerDeath()
{
	if(gpBase->mpMapHandler->GetCurrentMap() == NULL) return;
	const eSharedDeathPolicy policy = mGateway.BeginPlayerDeath(CurrentCheckPoint(),
		gpBase->mpAvatarHandler && gpBase->mpAvatarHandler->HasAwakeAvatar());
	if(policy == eSharedDeathPolicy_PreserveEnemies)
		Log("Shared Death: no death callback, so Shared Enemies survive this death\n");
	LogNewGatewayDiagnostic();
}

bool cLuxSocketServer::HoldsCheckPoint()
{
	return mGateway.HoldsCheckPoint(gpBase->mpEngine->GetGameTime() * 1000.0);
}

cSharedDeathCheckPoint cLuxSocketServer::CompleteCheckPoint()
{
	const cGameInteractionCheckPoint checkPoint = CurrentCheckPoint();
	const cSharedDeathCheckPoint death = mGateway.CompleteCheckPoint(checkPoint);
	if(death.mReset == eSharedDeathPolicy_Coordinated && !death.mbAgreed)
		Log("Shared Death: checkpoint '%s' keeps its own count %d: no matching agreed count arrived\n",
			checkPoint.msName.c_str(), death.mlCount);
	return death;
}

void cLuxSocketServer::ReportLocalChatEntry(const cChatEntry& aEntry)
{
	PublishEvent(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted,
		aEntry.GetAuthor(), aEntry.GetMessage()));
}

void cLuxSocketServer::LogNewGatewayDiagnostic()
{
	const tString& diagnostic = mGateway.GetDiagnostic();
	if (!diagnostic.empty() && diagnostic != mLastGatewayDiagnostic)
		Log("Game Interaction Protocol: %s\n", diagnostic.c_str());
	mLastGatewayDiagnostic = diagnostic;
}

void cLuxSocketServer::SetConnectionSettings(const tString& host, int port)
{
	// Confirming Options applies every option, so keep Sessions alive unless the endpoint changed.
	// A listener that failed to start is retried even on the same endpoint.
	if (host == mHost && port == mPort && mGateway.IsListening()) return;

    mHost = host;
    mPort = port;

	Log("LuxSocketServer config changed: re-init\n");
	mGateway.Shutdown();
	mLastGatewayDiagnostic.clear();
    InitSocket();
}

cLuxSocketServer::~cLuxSocketServer()
{
	mGateway.Shutdown();
    Log("cLuxSocketServer destroyed!\n");
}

void cLuxSocketServer::DestroyWorldEntities(cLuxMap* apMap)
{
	cLuxEnemyIterator it = apMap->GetEnemyIterator();
	while(it.HasNext()) it.Next()->SetEnemyDriven(false);
}
