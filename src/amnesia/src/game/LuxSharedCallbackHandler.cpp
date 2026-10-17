#include "LuxSharedCallbackHandler.h"

#include "GameInteractionProtocolVersion2.h"
#include "LuxArea_Flashback.h"
#include "LuxArea_Sticky.h"
#include "LuxEnemy.h"
#include "LuxEntity.h"
#include "LuxInteractionReportHandler.h"
#include "LuxInventory.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPeerDrivenEntityHandler.h"
#include "LuxPlayer.h"
#include "LuxProp_Button.h"
#include "LuxProp_Item.h"
#include "LuxProp_Lamp.h"
#include "LuxProp_SwingDoor.h"
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

	bool IsPropOfType(iLuxEntity *apEntity, eLuxPropType aType)
	{
		return apEntity->GetEntityType() == eLuxEntityType_Prop && static_cast<iLuxProp*>(apEntity)->GetPropType() == aType;
	}

	bool IsAreaOfType(iLuxEntity *apEntity, eLuxAreaType aType)
	{
		return apEntity->GetEntityType() == eLuxEntityType_Area && static_cast<iLuxArea*>(apEntity)->GetAreaType() == aType;
	}

	bool IsSharedEnemyEntity(iLuxEntity *apEntity)
	{
		return apEntity && apEntity->GetEntityType() == eLuxEntityType_Enemy && static_cast<iLuxEnemy*>(apEntity)->IsSharedEnemy();
	}

	// Tinderboxes, oil, sanity potions, and laudanum stay local.
	bool IsConsumable(eLuxItemType aType)
	{
		return aType == eLuxItemType_Tinderbox || aType == eLuxItemType_LampOil ||
			aType == eLuxItemType_Sanity || aType == eLuxItemType_Health;
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
	collide.mbParentSharedEnemy = IsSharedEnemyEntity(apParentEntity);
	collide.mbChildSharedEnemy = IsSharedEnemyEntity(apChild);
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

void cLuxSharedCallbackHandler::OnItemPickedUp(cLuxProp_Item *apItem)
{
	if(!IsMapPlaced(apItem) || !mModel.ReportsPickup(IsConsumable(apItem->GetItemType()))) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_Pickup;
	callback.mlEntityId = apItem->GetID();
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnFlashbackStarting(cLuxArea_Flashback *apArea)
{
	if(!mModel.ReportsCallback() || !IsMapPlaced(apArea)) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_Flashback;
	callback.mlEntityId = apArea->GetID();
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnCombineCallbackRunning(cLuxInventory_Item *apItem, cLuxInventory_Item *apOtherItem)
{
	if(!mModel.ReportsCombine(IsConsumable(apItem->GetType()), IsConsumable(apOtherItem->GetType()))) return;
	if(!cGameInteractionProtocolVersion2::IsValidItemName(apItem->GetName()) ||
		!cGameInteractionProtocolVersion2::IsValidItemName(apOtherItem->GetName()))
	{
		Log("Game Interaction Protocol: combine callback of items '%s' and '%s' is not shared: their names cannot be sent\n",
			apItem->GetName().c_str(), apOtherItem->GetName().c_str());
		return;
	}

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_Combine;
	callback.msItem = apItem->GetName();
	callback.msOtherItem = apOtherItem->GetName();
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnStickyAreaAttached(cLuxArea_Sticky *apArea, iPhysicsBody *apBody)
{
	iLuxEntity *pEntity = static_cast<iLuxEntity*>(apBody->GetUserData());
	if(!IsMapPlaced(apArea) || !IsMapPlaced(pEntity) || !mModel.ReportsStickyAttach(IsHeld(pEntity))) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_StickyAttach;
	callback.mlParentEntityId = apArea->GetID();
	callback.mlEntityId = pEntity->GetID();
	callback.mlBodyId = apBody->GetUniqueID();
	Publish(callback);
}

void cLuxSharedCallbackHandler::OnEnemyAutoDisabled(iLuxEnemy *apEnemy)
{
	if(!IsMapPlaced(apEnemy) || !mModel.ReportsAutoDisabled(apEnemy->IsSharedEnemy())) return;

	cGameInteractionSharedCallback callback;
	callback.mKind = eGameInteractionSharedCallbackKind_AutoDisabled;
	callback.mlEntityId = apEnemy->GetID();
	Publish(callback);
}

void cLuxSharedCallbackHandler::DetachForDrive(iLuxProp *apProp)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	mModel.BeginRun();
	for(int i=0; i<apProp->GetBodyNum(); ++i)
		pMap->DetachBodyFromStickyArea(apProp->GetBody(i));
	mModel.EndRun();
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
	if(!IsPropOfType(pEntity, type)) return eGameInteractionCommandOutcome_EntityWrongType;

	mModel.BeginRun();
	if(aState.mKind == eGameInteractionEntityStateKind_Button)
		static_cast<cLuxProp_Button*>(pEntity)->SetSwitchedOn(aState.mbState, true);
	else
		static_cast<cLuxProp_Lamp*>(pEntity)->SetLit(aState.mbState, true);
	mModel.EndRun();
	return eGameInteractionCommandOutcome_Success;
}

// The gateway calls this only for the current map. A door already broken stays broken.
eGameInteractionCommandOutcome cLuxSharedCallbackHandler::BreakEnemyDoor(int alEntityId)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	iLuxEntity *pEntity = pMap ? pMap->GetEntityByID(alEntityId) : NULL;
	if(!IsMapPlaced(pEntity)) return eGameInteractionCommandOutcome_EntityNotFound;
	if(!IsPropOfType(pEntity, eLuxPropType_SwingDoor) || !static_cast<cLuxProp_SwingDoor*>(pEntity)->IsBreakable())
		return eGameInteractionCommandOutcome_EntityWrongType;

	mModel.BeginRun();
	static_cast<cLuxProp_SwingDoor*>(pEntity)->Break();
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
	if(aCallback.mKind == eGameInteractionSharedCallbackKind_Combine) return Combine(aCallback);

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
	case eGameInteractionSharedCallbackKind_Pickup:
		return PickUp(pEntity);
	case eGameInteractionSharedCallbackKind_Flashback:
		return StartFlashback(pEntity);
	case eGameInteractionSharedCallbackKind_StickyAttach:
		return AttachToStickyArea(apMap->GetEntityByID(aCallback.mlParentEntityId), pEntity, aCallback.mlBodyId);
	case eGameInteractionSharedCallbackKind_AutoDisabled:
		return AutoDisable(pEntity);
	default:
		return false;
	}
}

// Only a shared item still in the world is picked up. A picked-up item stays in the map until the end
// of the update, so it is checked for being destroyed rather than for being in the map.
bool cLuxSharedCallbackHandler::PickUp(iLuxEntity *apEntity)
{
	if(apEntity->GetDestroyMe() || !apEntity->IsActive() || !IsPropOfType(apEntity, eLuxPropType_Item)) return false;
	cLuxProp_Item *pItem = static_cast<cLuxProp_Item*>(apEntity);
	return !IsConsumable(pItem->GetItemType()) && pItem->PickUp();
}

// Only an active Flashback area starts its flashback, so a player who already had it does not have it again.
bool cLuxSharedCallbackHandler::StartFlashback(iLuxEntity *apEntity)
{
	if(!apEntity->IsActive() || !IsAreaOfType(apEntity, eLuxAreaType_Flashback)) return false;
	static_cast<cLuxArea_Flashback*>(apEntity)->StartFlashback();
	return true;
}

// Only a combine of two different items the inventory holds runs, so a combine never gives the player
// items they did not hold. The callback's own script then changes the receiving player's inventory.
bool cLuxSharedCallbackHandler::Combine(const cGameInteractionSharedCallback& aCallback)
{
	cLuxInventory *pInventory = gpBase->mpInventory;
	if(aCallback.msItem == aCallback.msOtherItem || pInventory->GetItem(aCallback.msItem)==NULL ||
		pInventory->GetItem(aCallback.msOtherItem)==NULL) return false;
	cLuxCombineItemsCallback *pCallback = pInventory->GetCombineCallback(aCallback.msItem, aCallback.msOtherItem);
	if(pCallback==NULL) return false;
	pInventory->RunCombineCallback(pCallback);
	return true;
}

// The Holder's game already decided, so the body attaches whatever AllowAttachment says. An area that
// already holds a body, as when this game attached it first, does not take another.
bool cLuxSharedCallbackHandler::AttachToStickyArea(iLuxEntity *apArea, iLuxEntity *apEntity, int alBodyId)
{
	if(!IsMapPlaced(apArea) || apArea->GetDestroyMe() || !apArea->IsActive() ||
		!IsAreaOfType(apArea, eLuxAreaType_Sticky)) return false;
	cLuxArea_Sticky *pArea = static_cast<cLuxArea_Sticky*>(apArea);
	if(pArea->GetAttachedBody()) return false;

	if(apEntity->GetDestroyMe() || !apEntity->IsActive() || apEntity->GetEntityType()!=eLuxEntityType_Prop) return false;
	iPhysicsBody *pBody = static_cast<iLuxProp*>(apEntity)->GetBodyFromID(alBodyId);
	if(pBody==NULL) return false;

	// The entity stops following the Holder's samples, so local physics and the area take it from here.
	if(apEntity->IsPeerDriven() && gpBase->mpPeerDrivenEntityHandler)
		gpBase->mpPeerDrivenEntityHandler->ReleaseEntity(apEntity->GetID());
	pArea->AttachBodyFromSharedCallback(pBody);
	return true;
}

// Only an active enemy disables itself, so a game whose enemy is already gone runs nothing. A driven copy
// stays inactive when it is released, as the enemy it copies is.
bool cLuxSharedCallbackHandler::AutoDisable(iLuxEntity *apEntity)
{
	if(!apEntity->IsActive() || apEntity->GetEntityType() != eLuxEntityType_Enemy) return false;
	iLuxEnemy *pEnemy = static_cast<iLuxEnemy*>(apEntity);
	if(pEnemy->IsEnemyDriven()) pEnemy->ResetEnemyCopy();
	else pEnemy->SetActive(false);
	pEnemy->RunCallbackFunc("OnAutoDisabled");
	return true;
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
