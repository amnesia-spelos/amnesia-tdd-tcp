#ifndef LUX_INTERACTION_REPORT_HANDLER_H
#define LUX_INTERACTION_REPORT_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "LocalInteractionReportModel.h"

//----------------------------------------------

// Reports the local player's interactions to Peers (ADR 0005): it tells the report model when the
// local player starts and ends an interaction, reads the reported entities' bodies from the current
// map every update, and publishes the model's Events. The report lives only in the current map, and
// is emptied when that map is left or a save is loaded into it.
class cLuxInteractionReportHandler : public iLuxUpdateable
{
public:
	cLuxInteractionReportHandler();

	void OnLocalInteractionStarted(iLuxProp *apProp, iPhysicsBody *apBody);
	void OnLocalInteractionEnded(eGameInteractionEnding aEnding);

	// The bodies the latest update read, stamped with its game time and the current map.
	cGameInteractionBodySamples GetReportedBodies() const;

	void Update(float afTimeStep);
	void Reset();
	void DestroyWorldEntities(cLuxMap *apMap);

private:
	static double GetGameTimeMs();
	void PublishEvents();

	cLocalInteractionReportModel mModel;
	double mfReportTimeMs;
};

//----------------------------------------------

#endif // LUX_INTERACTION_REPORT_HANDLER_H
