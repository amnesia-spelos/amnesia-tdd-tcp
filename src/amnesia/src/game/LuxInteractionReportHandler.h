#ifndef LUX_INTERACTION_REPORT_HANDLER_H
#define LUX_INTERACTION_REPORT_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "LocalInteractionReportModel.h"

//----------------------------------------------

// Reports the local player's interactions to Peers (ADR 0005): it tells the report model when the
// local player starts and ends an interaction, reads the reported entities' bodies from the current
// map every update, and publishes the model's Events. The report lives only in one Map Visit: it is
// emptied when that visit ends, and the start of the next one is published as mapentered.
class cLuxInteractionReportHandler : public iLuxUpdateable
{
public:
	cLuxInteractionReportHandler();

	void OnLocalInteractionStarted(iLuxProp *apProp, iPhysicsBody *apBody);
	void OnLocalInteractionEnded(eGameInteractionEnding aEnding);
	// The entity is no longer the local game's to report, because a Peer drives it.
	void StopReporting(int alEntityId);

	// A door's frame or a lever's base is a static body that never moves, so it is neither reported nor
	// driven.
	static bool IsReportedBody(iPhysicsBody *apBody);

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
};

//----------------------------------------------

#endif // LUX_INTERACTION_REPORT_HANDLER_H
