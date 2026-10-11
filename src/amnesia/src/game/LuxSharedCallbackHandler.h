#ifndef LUX_SHARED_CALLBACK_HANDLER_H
#define LUX_SHARED_CALLBACK_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "SharedCallbackModel.h"

//----------------------------------------------

// Shares Map Script callbacks between games (amnesia-multiplayer's ADR 0006, not this repository's). It reports the collide,
// interact, and use-item callbacks the local player triggers, naming each registration by identifiers
// only, and runs the registration a Peer's Command names as the local trigger would. While it runs one,
// nothing that runs is reported back and scripts cannot take the player's control or camera. Only
// map-placed entities are named, because an entity created at runtime has no identifier the other
// games share.
class cLuxSharedCallbackHandler : public iLuxUpdateable
{
public:
	cLuxSharedCallbackHandler();

	// A collide callback is about to run. apParentEntity is NULL when the local player is the parent.
	void OnCollideCallbackRunning(iLuxEntity *apParentEntity, iLuxEntity *apChild, int alState);
	void OnInteractCallbackRunning(iLuxEntity *apEntity);
	void OnUseItemCallbackRunning(const tString& asItem, iLuxEntity *apEntity);

	// Runs the current map's registration for the callback. False when none matches, so nothing ran.
	bool RunSharedCallback(const cGameInteractionSharedCallback& aCallback);

	// Whether a script may take the player's control or camera, which it may not during a Shared Callback.
	bool AllowsPlayerControl() const { return mModel.AllowsPlayerControl(); }

	void Reset();

private:
	bool RunRegistration(cLuxMap *apMap, const cGameInteractionSharedCallback& aCallback);
	void Publish(cGameInteractionSharedCallback& aCallback);

	cSharedCallbackModel mModel;
};

//----------------------------------------------

#endif // LUX_SHARED_CALLBACK_HANDLER_H
