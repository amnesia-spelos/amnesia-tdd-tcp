#include "LuxPeerDrivenEntityHandler.h"

#include "LuxInteractionReportHandler.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerState_Interact.h"
#include "LuxProp.h"
#include "LuxProp_Object.h"

namespace
{
	// The props a player interacts with by moving them: the ones the local interaction report covers.
	bool IsHoldable(iLuxProp *apProp)
	{
		switch(apProp->GetPropType())
		{
		case eLuxPropType_SwingDoor:
		case eLuxPropType_Wheel:
		case eLuxPropType_Lever:
		case eLuxPropType_MultiSlider:
			return true;
		case eLuxPropType_Object:
			return static_cast<cLuxProp_Object*>(apProp)->GetObjectType()!=eLuxObjectType_Static;
		default:
			return false;
		}
	}

	// A map's props, as the Peer-Driven Entity model finds and moves them.
	class cLuxPeerDrivenEntityWorld : public iPeerDrivenEntityWorld
	{
	public:
		cLuxPeerDrivenEntityWorld(cLuxMap *apMap, cLuxPeerDrivenEntityHandler::tBodyGravityMap& a_mapBodyGravity)
			: mpMap(apMap), m_mapBodyGravity(a_mapBodyGravity) {}

		virtual eGameInteractionEntityOutcome FindEntity(int alEntityId) const
		{
			iLuxEntity *pEntity = mpMap->GetEntityByID(alEntityId);
			if(pEntity==NULL || pEntity->GetDestroyMe()) return eGameInteractionEntityOutcome_NotFound;
			if(pEntity->GetEntityType()!=eLuxEntityType_Prop || pEntity->IsCreatedAtRuntime())
				return eGameInteractionEntityOutcome_NotHoldable;

			if(!IsHoldable(static_cast<iLuxProp*>(pEntity))) return eGameInteractionEntityOutcome_NotHoldable;
			return eGameInteractionEntityOutcome_Success;
		}

		virtual bool HasBody(int alEntityId, int alBodyId) const
		{
			return FindBody(alEntityId, alBodyId) != NULL;
		}

		virtual bool IsLocallyInteracting(int alEntityId) const
		{
			iLuxPlayerState_Interact *pState = GetInteractState();
			return pState && pState->GetCurrentProp() && pState->GetCurrentProp()->GetID()==alEntityId;
		}

		virtual void EndLocalInteraction(int alEntityId)
		{
			iLuxPlayerState_Interact *pState = GetInteractState();
			if(pState) pState->EndInteraction();
		}

		// A driven body floats where the stream puts it, and never sleeps while it is driven.
		virtual void BeginDriving(int alEntityId)
		{
			iLuxProp *pProp = GetProp(alEntityId);
			if(pProp==NULL) return;
			pProp->SetPeerDriven(true);
			std::vector<bool>& vGravity = m_mapBodyGravity[alEntityId];
			vGravity.clear();
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pBody = pProp->GetBody(i);
				vGravity.push_back(pBody->GetGravity());
				if(!cLuxInteractionReportHandler::IsReportedBody(pBody)) continue;
				pBody->SetGravity(false);
				// Until its first sample, the body stays where it was.
				pBody->SetLinearVelocity(0);
				pBody->SetAngularVelocity(0);
				pBody->Enable();
			}
		}

		// Local physics resumes from the bodies' current transforms and velocities.
		virtual void EndDriving(int alEntityId)
		{
			iLuxProp *pProp = GetProp(alEntityId);
			if(pProp==NULL)
			{
				m_mapBodyGravity.erase(alEntityId);
				return;
			}
			const std::vector<bool>& vGravity = m_mapBodyGravity[alEntityId];
			pProp->SetPeerDriven(false);
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pBody = pProp->GetBody(i);
				if(!cLuxInteractionReportHandler::IsReportedBody(pBody)) continue;
				pBody->SetGravity(static_cast<size_t>(i) < vGravity.size() ? vGravity[i] : true);
				pBody->Enable();
			}
			m_mapBodyGravity.erase(alEntityId);
		}

		// The prop's own logic sees the Peer's interaction as the local player's: a door unlatches, and a
		// door, lever, or wheel stops auto-closing, auto-moving, or slowing while it is marked.
		virtual void SetInteracting(int alEntityId, bool abInteracting)
		{
			iLuxProp *pProp = GetProp(alEntityId);
			if(pProp==NULL) return;
			pProp->SetIsInteractedWith(abInteracting);
			if(abInteracting) pProp->OnInteractionStart();
		}

		virtual void SetBodyState(int alEntityId, int alBodyId, const cGameInteractionBodyState& aState)
		{
			iPhysicsBody *pBody = FindBody(alEntityId, alBodyId);
			const cQuaternion qOrientation(aState.mOrientation.mfW, aState.mOrientation.mfX,
				aState.mOrientation.mfY, aState.mOrientation.mfZ);
			cMatrixf mtxWorld = cMath::MatrixQuaternion(qOrientation);
			mtxWorld.SetTranslation(cVector3f(aState.mPosition.mfX, aState.mPosition.mfY, aState.mPosition.mfZ));
			pBody->SetMatrix(mtxWorld);
			pBody->SetLinearVelocity(cVector3f(aState.mLinearVelocity.mfX, aState.mLinearVelocity.mfY,
				aState.mLinearVelocity.mfZ));
			// The engine's angular velocity is in radians per second.
			pBody->SetAngularVelocity(cVector3f(cMath::ToRad(aState.mAngularVelocity.mfX),
				cMath::ToRad(aState.mAngularVelocity.mfY), cMath::ToRad(aState.mAngularVelocity.mfZ)));
			pBody->Enable();
		}

	private:
		// NULL once the entity is destroyed, which the model notices only on its next update.
		iLuxProp* GetProp(int alEntityId) const
		{
			if(FindEntity(alEntityId)!=eGameInteractionEntityOutcome_Success) return NULL;
			return static_cast<iLuxProp*>(mpMap->GetEntityByID(alEntityId));
		}

		iPhysicsBody* FindBody(int alEntityId, int alBodyId) const
		{
			iLuxProp *pProp = GetProp(alEntityId);
			if(pProp==NULL) return NULL;
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pBody = pProp->GetBody(i);
				if(pBody->GetUniqueID()==alBodyId && cLuxInteractionReportHandler::IsReportedBody(pBody)) return pBody;
			}
			return NULL;
		}

		static iLuxPlayerState_Interact* GetInteractState()
		{
			switch(gpBase->mpPlayer->GetCurrentState())
			{
			case eLuxPlayerState_InteractGrab:
			case eLuxPlayerState_InteractPush:
			case eLuxPlayerState_InteractSwingDoor:
			case eLuxPlayerState_InteractLever:
			case eLuxPlayerState_InteractWheel:
			case eLuxPlayerState_InteractSlide:
				return static_cast<iLuxPlayerState_Interact*>(gpBase->mpPlayer->GetCurrentStateData());
			default:
				return NULL;
			}
		}

		cLuxMap *mpMap;
		cLuxPeerDrivenEntityHandler::tBodyGravityMap& m_mapBodyGravity;
	};
}

cLuxPeerDrivenEntityHandler::cLuxPeerDrivenEntityHandler() : iLuxUpdateable("LuxPeerDrivenEntityHandler")
{
}

// The gateway calls the Commands only while a map is loaded, and only for the current map.
eGameInteractionEntityOutcome cLuxPeerDrivenEntityHandler::DriveEntity(int alEntityId)
{
	cLuxPeerDrivenEntityWorld world(gpBase->mpMapHandler->GetCurrentMap(), m_mapBodyGravity);
	const eGameInteractionEntityOutcome outcome = mModel.Drive(alEntityId, world);
	// A Peer-Driven Entity's motion is no longer the local game's to report.
	if(outcome==eGameInteractionEntityOutcome_Success && gpBase->mpInteractionReportHandler)
		gpBase->mpInteractionReportHandler->StopReporting(alEntityId);
	return outcome;
}

eGameInteractionEntityOutcome cLuxPeerDrivenEntityHandler::DriveEntityBodies(
	const cGameInteractionBodySamples& aSamples, int& alFailedEntityId)
{
	cLuxPeerDrivenEntityWorld world(gpBase->mpMapHandler->GetCurrentMap(), m_mapBodyGravity);
	return mModel.AddBodies(aSamples, GetLocalTimeMs(), world, alFailedEntityId);
}

eGameInteractionEntityOutcome cLuxPeerDrivenEntityHandler::SetEntityInteracting(int alEntityId, bool abInteracting)
{
	cLuxPeerDrivenEntityWorld world(gpBase->mpMapHandler->GetCurrentMap(), m_mapBodyGravity);
	return mModel.SetInteracting(alEntityId, abInteracting, world);
}

eGameInteractionEntityOutcome cLuxPeerDrivenEntityHandler::ReleaseEntity(int alEntityId)
{
	cLuxPeerDrivenEntityWorld world(gpBase->mpMapHandler->GetCurrentMap(), m_mapBodyGravity);
	return mModel.Release(alEntityId, world);
}

// The Session ended. Without a map, leaving the last one already released every entity.
void cLuxPeerDrivenEntityHandler::ReleaseEntities()
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL)
	{
		Reset();
		return;
	}
	cLuxPeerDrivenEntityWorld world(pMap, m_mapBodyGravity);
	mModel.ReleaseAll(world);
}

void cLuxPeerDrivenEntityHandler::Update(float afTimeStep)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;
	cLuxPeerDrivenEntityWorld world(pMap, m_mapBodyGravity);
	mModel.Update(GetLocalTimeMs(), world);

	// An entity destroyed while driven is forgotten without being handed back.
	for(tBodyGravityMap::iterator it = m_mapBodyGravity.begin(); it != m_mapBodyGravity.end();)
	{
		if(mModel.IsDriving(it->first)) ++it;
		else m_mapBodyGravity.erase(it++);
	}
}

// A game reset deletes every map without sending DestroyWorldEntities.
void cLuxPeerDrivenEntityHandler::Reset()
{
	mModel.Clear();
	m_mapBodyGravity.clear();
}

// Sent when a map is left or a save game is loaded into it, while its entities still exist.
void cLuxPeerDrivenEntityHandler::DestroyWorldEntities(cLuxMap *apMap)
{
	cLuxPeerDrivenEntityWorld world(apMap, m_mapBodyGravity);
	mModel.ReleaseAll(world);
}

double cLuxPeerDrivenEntityHandler::GetLocalTimeMs()
{
	return gpBase->mpEngine->GetGameTime() * 1000.0;
}
