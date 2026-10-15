#ifndef LUX_SHARED_CALLBACK_HANDLER_H
#define LUX_SHARED_CALLBACK_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "SharedCallbackModel.h"

class cLuxArea_Flashback;
class cLuxArea_Sticky;
class cLuxInventory_Item;
class cLuxProp_Item;

//----------------------------------------------

// Shares Map Script callbacks between games (amnesia-multiplayer's ADR 0006, not this repository's). It reports the collide,
// interact, use-item, and combine callbacks, the item pickups, the Flashback areas the local player triggers, and
// the sticky areas that attach an entity the local player Holds,
// naming each registration by identifiers only, and runs the registration a Peer's Command names as the
// local trigger would. While it runs one, nothing that runs is reported back and scripts cannot take the
// player's control or camera. Only map-placed entities are named, because an entity created at runtime
// has no identifier the other games share.
class cLuxSharedCallbackHandler : public iLuxUpdateable
{
public:
	cLuxSharedCallbackHandler();

	// A collide callback is about to run. apParentEntity is NULL when the local player is the parent.
	void OnCollideCallbackRunning(iLuxEntity *apParentEntity, iLuxEntity *apChild, int alState);
	void OnInteractCallbackRunning(iLuxEntity *apEntity);
	void OnUseItemCallbackRunning(const tString& asItem, iLuxEntity *apEntity);
	// The item was picked up and its OnPickup callback is about to run. Consumables are not reported.
	void OnItemPickedUp(cLuxProp_Item *apItem);
	// The Flashback area is about to start its flashback. Only the local player walking in is reported.
	void OnFlashbackStarting(cLuxArea_Flashback *apArea);
	// The local player combined the items and their combine callback is about to run. A combine of two
	// consumables is not reported.
	void OnCombineCallbackRunning(cLuxInventory_Item *apItem, cLuxInventory_Item *apOtherItem);
	// The sticky area's own check attached the body. Only an entity the local player Holds is reported.
	void OnStickyAreaAttached(cLuxArea_Sticky *apArea, iPhysicsBody *apBody);
	// A Peer is about to drive the prop, so a body a sticky area here holds is detached first, as grabbing
	// it detached it in the Peer's game. The DetachFunction runs in Shared Callback mode.
	void DetachForDrive(iLuxProp *apProp);

	void OnLocalEntityStateChanged(iLuxEntity *apEntity, eGameInteractionEntityStateKind aKind,
		bool abBefore, bool abAfter);
	eGameInteractionCommandOutcome ApplyEntityState(const cGameInteractionEntityState& aState);

	// Runs the current map's registration for the callback. False when none matches, so nothing ran.
	bool RunSharedCallback(const cGameInteractionSharedCallback& aCallback);

	// Whether a script may take the player's control or camera, which it may not during a Shared Callback.
	bool AllowsPlayerControl() const { return mModel.AllowsPlayerControl(); }

	void Reset();

private:
	bool RunRegistration(cLuxMap *apMap, const cGameInteractionSharedCallback& aCallback);
	bool PickUp(iLuxEntity *apEntity);
	bool StartFlashback(iLuxEntity *apEntity);
	bool Combine(const cGameInteractionSharedCallback& aCallback);
	bool AttachToStickyArea(iLuxEntity *apArea, iLuxEntity *apEntity, int alBodyId);
	void Publish(cGameInteractionSharedCallback& aCallback);

	cSharedCallbackModel mModel;
};

//----------------------------------------------

#endif // LUX_SHARED_CALLBACK_HANDLER_H
