#include "LuxSharedCallbackHandler.h"

#include "GameInteractionProtocolVersion2.h"
#include "LuxEntity.h"
#include "LuxInteractionReportHandler.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxProp_Button.h"
#include "LuxProp_Lamp.h"
#include "LuxSocketServer.h"

namespace
{
	bool IsMapPlaced(iLuxEntity *apEntity)
	{
		return apEntity && !apEntity->IsCreatedAtRuntime();
	}

	// The local player Holds the entities in the report; the other games follow them.
	bool IsHeld(iLuxEntity *apEntity)
	{
		if(apEntity==NULL || gpBase->mpInteractionReportHandler==NULL) return false;
		const cGameInteractionBodySamples report = gpBase->mpInteractionReportHandler->GetReportedBodies();
		for(size_t i=0; i<report.mvBodies.size(); ++i)
			if(report.mvBodies[i].mlEntityId == apEntity->GetID()) return true;
		return false;
	}
}

cLuxSharedCallbackHandler::cLuxSharedCallbackHandler() : iLuxUpdateable("LuxSharedCallbackHandler")
{
}

void cLuxSharedCallbackHandler::OnCollideCallbackRunning(iLuxEntity *apParentEntity, iLuxEntity *apChild, int alState)
{
	cSharedCollide collide;
	collide.mbParentIsPlayer = apParentEntity==NULL;
	collide.mbParentHeld = IsHeld(apParentEntity);
	collide.mbChildHeld = IsHeld(apChild);
	if(!mModel.ReportsCollide(collide)) return;
	if(!IsMapPlaced(apChild) || (apParentEntity && !IsMapPlaced(apParentEntity))) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = collide.mbParentIsPlayer ? eGameInteractionSharedCallbackKind_PlayerCollide :
		eGameInteractionSharedCallbackKind_EntityCollide;
	if(apParentEntity) callback.mlParentEntityId = apParentEntity->GetID();
	callback.mlEntityId = apChild->GetID();
	callback.mlState = alState;
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnInteractCallbackRunning(iLuxEntity *apEntity)
{
	if(!mModel.ReportsCallback() || !IsMapPlaced(apEntity)) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_Interact;
	callback.mlEntityId = apEntity->GetID();
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnUseItemCallbackRunning(const tString& asItem, iLuxEntity *apEntity)
{
	if(!mModel.ReportsCallback() || !IsMapPlaced(apEntity)) return;
	if(!cGameInteractionProtocolVersion2::IsValidItemName(asItem))
	{
		Log("Game Interaction Protocol: use-item callback of item '%s' is not shared: its name cannot be sent\n",
			asItem.c_str());
		return;
	}

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_UseItem;
	callback.mlEntityId = apEntity->GetID();
	callback.msItem = asItem;
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnLocalEntityStateChanged(iLuxEntity *apEntity,
	eGameInteractionEntityStateKind aKind, bool abBefore, bool abAfter)
{
	if(!mModel.ReportsEntityState(abBefore, abAfter) || !IsMapPlaced(apEntity)) return;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL || gpBase->mpSocketServer==NULL) return;
	cGameInteractionEntityState state;
	state.mKind = aKind;
	state.mlEntityId = apEntity->GetID();
	state.mbState = abAfter;
	state.msMapFile = pMap->GetMapPath();
	gpBase->mpSocketServer->PublishEvent(cGameInteractionEvent(aKind == eGameInteractionEntityStateKind_Button ?
		eGameInteractionEvent_ButtonState : eGameInteractionEvent_LampState, state));
}

eGameInteractionCommandOutcome cLuxSharedCallbackHandler::ApplyEntityState(const cGameInteractionEntityState& aState)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	iLuxEntity *pEntity = pMap ? pMap->GetEntityByID(aState.mlEntityId) : NULL;
	if(!IsMapPlaced(pEntity)) return eGameInteractionCommandOutcome_EntityNotFound;
	const eLuxPropType type = aState.mKind == eGameInteractionEntityStateKind_Button ? eLuxPropType_Button : eLuxPropType_Lamp;
	if(pEntity->GetEntityType() != eLuxEntityType_Prop || static_cast<iLuxProp*>(pEntity)->GetPropType() != type)
		return eGameInteractionCommandOutcome_EntityWrongType;

	mModel.BeginRun();
	if(aState.mKind == eGameInteractionEntityStateKind_Button)
		static_cast<cLuxProp_Button*>(pEntity)->SetSwitchedOn(aState.mbState, true);
	else
		static_cast<cLuxProp_Lamp*>(pEntity)->SetLit(aState.mbState, true);
	mModel.EndRun();
	return eGameInteractionCommandOutcome_Success;
}

// The gateway calls this only for the current map.
bool cLuxSharedCallbackHandler::RunSharedCallback(const cGameInteractionSharedCallback& aCallback)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return false;

	mModel.BeginRun();
	const bool bRan = RunRegistration(pMap, aCallback);
	mModel.EndRun();
	if(!bRan)
		Log("Game Interaction Protocol: no Map Script registration for sharedcallback %s\n",
			cGameInteractionProtocolVersion2::FormatSharedCallback(aCallback).c_str());
	return bRan;
}

bool cLuxSharedCallbackHandler::RunRegistration(cLuxMap *apMap, const cGameInteractionSharedCallback& aCallback)
{
	iLuxEntity *pEntity = apMap->GetEntityByID(aCallback.mlEntityId);
	if(!IsMapPlaced(pEntity)) return false;

	switch(aCallback.mKind)
	{
	case eGameInteractionSharedCallbackKind_PlayerCollide:
		return gpBase->mpPlayer->RunSharedCollideCallback(pEntity, "Player", aCallback.mlState, apMap);
	case eGameInteractionSharedCallbackKind_EntityCollide:
	{
		iLuxEntity *pParent = apMap->GetEntityByID(aCallback.mlParentEntityId);
		if(!IsMapPlaced(pParent)) return false;
		return pParent->RunSharedCollideCallback(pEntity, pParent->GetName(), aCallback.mlState, apMap);
	}
	case eGameInteractionSharedCallbackKind_Interact:
		return pEntity->RunInteractCallbackFunc();
	case eGameInteractionSharedCallbackKind_UseItem:
		return apMap->RunUseItemCallback(aCallback.msItem, pEntity);
	default:
		return false;
	}
}

void cLuxSharedCallbackHandler::Publish(cGameInteractionSharedCallback& aCallback)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(gpBase->mpSocketServer==NULL || pMap==NULL) return;
	aCallback.msMapFile = pMap->GetMapPath();
	gpBase->mpSocketServer->PublishEvent(cGameInteractionEvent(eGameInteractionEvent_SharedCallback, aCallback));
}

// A run never outlasts the Command, but a reset must not leave the game in Shared Callback mode.
void cLuxSharedCallbackHandler::Reset()
{
	mModel = cSharedCallbackModel();
}
