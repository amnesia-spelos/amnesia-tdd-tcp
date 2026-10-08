#ifndef LUX_PEER_DRIVEN_ENTITY_HANDLER_H
#define LUX_PEER_DRIVEN_ENTITY_HANDLER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "PeerDrivenEntityModel.h"

//----------------------------------------------

// Drives the Session's Peer-Driven Entities on the current map (ADR 0005). The model decides; this
// hands entities between the local player, local physics, and the Peer's stream. Only map-placed
// holdable props can be driven, and only their moving bodies. Every Peer-Driven Entity ends when the
// Session ends, the map is left, or a save is loaded.
class cLuxPeerDrivenEntityHandler : public iLuxUpdateable
{
public:
	cLuxPeerDrivenEntityHandler();

	eGameInteractionEntityOutcome DriveEntity(int alEntityId);
	eGameInteractionEntityOutcome DriveEntityBodies(const cGameInteractionBodySamples& aSamples,
		int& alFailedEntityId);
	eGameInteractionEntityOutcome SetEntityInteracting(int alEntityId, bool abInteracting);
	eGameInteractionEntityOutcome ReleaseEntity(int alEntityId);
	void ReleaseEntities();

	void Update(float afTimeStep);
	void Reset();
	void DestroyWorldEntities(cLuxMap *apMap);

	// Whether each body of a driven entity had gravity before it was driven, in body order.
	typedef std::map<int, std::vector<bool> > tBodyGravityMap;

private:
	static double GetLocalTimeMs();

	cPeerDrivenEntityModel mModel;
	tBodyGravityMap m_mapBodyGravity;
};

//----------------------------------------------

#endif // LUX_PEER_DRIVEN_ENTITY_HANDLER_H
