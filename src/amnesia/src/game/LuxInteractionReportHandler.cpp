#include "LuxInteractionReportHandler.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxProp.h"
#include "LuxProp_Object.h"
#include "LuxSocketServer.h"

namespace
{
	size_t CountReportedBodies(iLuxProp *apProp)
	{
		size_t lCount = 0;
		for(int i=0; i<apProp->GetBodyNum(); ++i)
		{
			if(cLuxInteractionReportHandler::IsReportedBody(apProp->GetBody(i))) ++lCount;
		}
		return lCount;
	}

	// The prop a body belongs to, as a contact touched it. False when the body is not a prop's, as for
	// a character's or the map's static geometry. Every body the game gives user data has an entity.
	bool GetContactTarget(iPhysicsBody *apBody, cLocalInteractionContactTarget& aTarget)
	{
		iLuxEntity *pEntity = static_cast<iLuxEntity*>(apBody->GetUserData());
		if(pEntity==NULL || pEntity->GetEntityType()!=eLuxEntityType_Prop) return false;

		iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
		aTarget.mlEntityId = pProp->GetID();
		aTarget.mbBodyMoving = cLuxInteractionReportHandler::IsReportedBody(apBody);
		aTarget.mbHoldable = cLuxInteractionReportHandler::IsHoldable(pProp);
		aTarget.mbCreatedAtRuntime = pProp->IsCreatedAtRuntime();
		aTarget.mbPeerDriven = pProp->IsPeerDriven();
		aTarget.mlBodyCount = CountReportedBodies(pProp);
		return true;
	}

	cGameInteractionBodyState GetBodyState(iPhysicsBody *apBody)
	{
		const cMatrixf& mtxWorld = apBody->GetWorldMatrix();
		const cVector3f vPosition = mtxWorld.GetTranslation();
		const cQuaternion qOrientation(mtxWorld.GetRotation());
		const cVector3f vLinear = apBody->GetLinearVelocity();
		// The engine's angular velocity is in radians per second.
		const cVector3f vAngular = apBody->GetAngularVelocity();

		cGameInteractionBodyState state;
		state.mPosition = cGameInteractionPosition(vPosition.x, vPosition.y, vPosition.z);
		state.mOrientation = cGameInteractionQuaternion(qOrientation.v.x, qOrientation.v.y,
			qOrientation.v.z, qOrientation.w);
		state.mLinearVelocity = cGameInteractionVector(vLinear.x, vLinear.y, vLinear.z);
		state.mAngularVelocity = cGameInteractionVector(cMath::ToDeg(vAngular.x), cMath::ToDeg(vAngular.y),
			cMath::ToDeg(vAngular.z));
		return state;
	}

	// The current map's props, as the report model reads them.
	class cLuxLocalInteractionWorld : public iLocalInteractionWorld
	{
	public:
		explicit cLuxLocalInteractionWorld(cLuxMap *apMap) : mpMap(apMap) {}

		virtual bool GetBodies(int alEntityId, std::vector<cLocalInteractionBody>& avBodies) const
		{
			iLuxEntity *pEntity = mpMap->GetEntityByID(alEntityId, eLuxEntityType_Prop);
			if(pEntity==NULL || pEntity->GetDestroyMe()) return false;

			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			avBodies.clear();
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pBody = pProp->GetBody(i);
				if(!cLuxInteractionReportHandler::IsReportedBody(pBody)) continue;
				avBodies.push_back(cLocalInteractionBody(pBody->GetUniqueID(), GetBodyState(pBody),
					!pBody->GetEnabled()));
			}
			return true;
		}

	private:
		cLuxMap *mpMap;
	};
}

void cLuxInteractionContactCallback::OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody,
	cPhysicsContactData* apContactData)
{
	mpHandler->OnPropBodyContact(apBody, apCollideBody);
}

cLuxInteractionReportHandler::cLuxInteractionReportHandler()
	: iLuxUpdateable("LuxInteractionReportHandler"), mfReportTimeMs(0.0), mContactCallback(this)
{
}

void cLuxInteractionReportHandler::OnLocalInteractionStarted(iLuxProp *apProp, iPhysicsBody *apBody)
{
	mModel.StartInteraction(apProp->GetID(), apBody->GetUniqueID(), apProp->IsCreatedAtRuntime(),
		CountReportedBodies(apProp));
	PublishEvents();
}

void cLuxInteractionReportHandler::OnLocalInteractionEnded(eGameInteractionEnding aEnding)
{
	mModel.EndInteraction(aEnding, GetGameTimeMs());
	PublishEvents();
}

void cLuxInteractionReportHandler::OnLocalPlayerPushed(iPhysicsBody *apBody)
{
	cLocalInteractionContact contact;
	contact.mbByLocalPlayer = true;
	if(GetContactTarget(apBody, contact.mTouched)) mModel.RecordContact(contact);
}

// Every awake holdable prop's moving bodies' contacts arrive here, so the ones that touched no moving
// body are dropped before the model sees them.
void cLuxInteractionReportHandler::OnPropBodyContact(iPhysicsBody *apBody, iPhysicsBody *apCollideBody)
{
	if(!IsReportedBody(apCollideBody)) return;

	cLocalInteractionContact contact;
	contact.mlSourceEntityId = static_cast<iLuxEntity*>(apBody->GetUserData())->GetID();
	if(GetContactTarget(apCollideBody, contact.mTouched)) mModel.RecordContact(contact);
}

void cLuxInteractionReportHandler::ListenForContacts(iLuxProp *apProp)
{
	if(!IsHoldable(apProp)) return;
	for(int i=0; i<apProp->GetBodyNum(); ++i)
	{
		iPhysicsBody *pBody = apProp->GetBody(i);
		if(IsReportedBody(pBody)) pBody->AddBodyCallback(&mContactCallback);
	}
}

// The prop is to make its debris from the state its break body has now. Called while the physics step
// or the map's update runs.
void cLuxInteractionReportHandler::OnPropBroke(iLuxProp *apProp, iPhysicsBody *apBreakBody)
{
	mModel.Break(apProp->GetID(), GetBodyState(apBreakBody));
	PublishEvents();
}

void cLuxInteractionReportHandler::StopReporting(int alEntityId)
{
	mModel.StopReporting(alEntityId);
}

cGameInteractionBodySamples cLuxInteractionReportHandler::GetReportedBodies() const
{
	cGameInteractionBodySamples samples;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return samples;

	samples.mlTimeMs = static_cast<unsigned long long>(mfReportTimeMs);
	samples.mvBodies = mModel.GetReportedBodies();
	samples.msMapFile = pMap->GetMapPath();
	return samples;
}

void cLuxInteractionReportHandler::Update(float afTimeStep)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	mfReportTimeMs = GetGameTimeMs();
	mModel.Update(cLuxLocalInteractionWorld(pMap), mfReportTimeMs);
	PublishEvents();
}

// A game reset deletes every map without sending DestroyWorldEntities.
void cLuxInteractionReportHandler::Reset()
{
	mModel.Clear();
}

// Sent when a map is left or a save game is loaded into it.
void cLuxInteractionReportHandler::DestroyWorldEntities(cLuxMap *apMap)
{
	mModel.Clear();
}

// Sent exactly when a Map Visit starts: on entering a map, and when a save is loaded into the map
// already loaded. The previous visit's report is already empty and its Peer-Driven Entities released.
void cLuxInteractionReportHandler::CreateWorldEntities(cLuxMap *apMap)
{
	if(gpBase->mpSocketServer==NULL) return;
	gpBase->mpSocketServer->PublishEvent(cGameInteractionEvent(eGameInteractionEvent_MapEntered,
		apMap->GetMapPath()));
}

bool cLuxInteractionReportHandler::IsReportedBody(iPhysicsBody *apBody)
{
	return apBody->GetMass() > 0;
}

bool cLuxInteractionReportHandler::IsHoldable(iLuxProp *apProp)
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

double cLuxInteractionReportHandler::GetGameTimeMs()
{
	return gpBase->mpEngine->GetGameTime() * 1000.0;
}

void cLuxInteractionReportHandler::PublishEvents()
{
	std::vector<cLocalInteractionReportEvent> vEvents = mModel.TakeEvents();
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(gpBase->mpSocketServer==NULL || pMap==NULL) return;

	for(size_t i=0; i<vEvents.size(); ++i)
	{
		cGameInteractionEntityEvent entityEvent = vEvents[i].mEntityEvent;
		entityEvent.msMapFile = pMap->GetMapPath();
		gpBase->mpSocketServer->PublishEvent(cGameInteractionEvent(vEvents[i].mType, entityEvent));
	}
}
