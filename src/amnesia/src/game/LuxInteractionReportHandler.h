#ifndef LUX_INTERACTION_REPORT_HANDLER_H
#define LUX_INTERACTION_REPORT_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "LocalInteractionReportModel.h"

//----------------------------------------------

class cLuxInteractionReportHandler;

// Records the contacts of the bodies it is added to during the physics step.
class cLuxInteractionContactCallback : public iPhysicsBodyCallback
{
public:
	explicit cLuxInteractionContactCallback(cLuxInteractionReportHandler *apHandler) : mpHandler(apHandler) {}

	bool OnAABBCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody){ return true; }
	void OnBodyCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody, cPhysicsContactData* apContactData);

private:
	cLuxInteractionReportHandler *mpHandler;
};

//----------------------------------------------

// Reports the local player's interactions to Peers (ADR 0005): it tells the report model when the
// local player starts and ends an interaction, reads the reported entities' bodies from the current
// map every update, and publishes the model's Events. Contacts are recorded during the physics step,
// from the bodies of every holdable prop and from the local player's pushes, and the model acts on
// them in the next update. The report lives only in one Map Visit: it is emptied when that visit
// ends, and the start of the next one is published as mapentered.
class cLuxInteractionReportHandler : public iLuxUpdateable
{
public:
	cLuxInteractionReportHandler();

	void OnLocalInteractionStarted(iLuxProp *apProp, iPhysicsBody *apBody);
	void OnLocalInteractionEnded(eGameInteractionEnding aEnding);
	// The local player's character body walked into apBody and pushed it.
	void OnLocalPlayerPushed(iPhysicsBody *apBody);
	// Moving body apBody of a holdable prop touched apCollideBody.
	void OnPropBodyContact(iPhysicsBody *apBody, iPhysicsBody *apCollideBody);
	// Records the contacts of the prop's moving bodies from now on, if it is holdable.
	void ListenForContacts(iLuxProp *apProp);

	// The entity is no longer the local game's to report, because a Peer drives it.
	void StopReporting(int alEntityId);

	// A door's frame or a lever's base is a static body that never moves, so it is neither reported nor
	// driven.
	static bool IsReportedBody(iPhysicsBody *apBody);
	// The props a player interacts with by moving them: the ones the report and Peers cover.
	static bool IsHoldable(iLuxProp *apProp);

	// The bodies the latest update read, stamped with its game time and the current map.
	cGameInteractionBodySamples GetReportedBodies() const;

	void Update(float afTimeStep);
	void Reset();
	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

private:
	static double GetGameTimeMs();
	void PublishEvents();

	cLocalInteractionReportModel mModel;
	double mfReportTimeMs;
	cLuxInteractionContactCallback mContactCallback;
};

//----------------------------------------------

#endif // LUX_INTERACTION_REPORT_HANDLER_H
