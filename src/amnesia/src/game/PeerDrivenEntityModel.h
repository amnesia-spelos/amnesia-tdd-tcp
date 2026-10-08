#ifndef PEER_DRIVEN_ENTITY_MODEL_H
#define PEER_DRIVEN_ENTITY_MODEL_H

#include "GameInteractionGateway.h"
#include "SenderClock.h"

#include <deque>
#include <vector>

// The local game's entities on the current map, as the Peer-Driven Entity model finds and moves them.
class iPeerDrivenEntityWorld
{
public:
	virtual ~iPeerDrivenEntityWorld() {}
	// Success when the current map has the entity and a Peer may drive it; otherwise why not.
	virtual eGameInteractionEntityOutcome FindEntity(int alEntityId) const = 0;
	virtual bool HasBody(int alEntityId, int alBodyId) const = 0;
	virtual bool IsLocallyInteracting(int alEntityId) const = 0;
	// Ends the local player's interaction with the entity as a release.
	virtual void EndLocalInteraction(int alEntityId) = 0;
	// Takes the entity from local physics and the local player, and gives it back from its current
	// state.
	virtual void BeginDriving(int alEntityId) = 0;
	virtual void EndDriving(int alEntityId) = 0;
	// Marks the entity as being interacted with, starting an interaction with it as the local player's
	// would, or clears the mark. Told only when the mark changes.
	virtual void SetInteracting(int alEntityId, bool abInteracting) = 0;
	virtual void SetBodyState(int alEntityId, int alBodyId, const cGameInteractionBodyState& aState) = 0;
};

// The Peer-Driven Entities of the Session (ADR 0005). Their bodies follow a Peer's sender-timestamped
// samples, buffered and played back with the same delay and snapping as Avatar Poses (ADR 0003).
// Samples are for the current map only; the gateway turns away samples for any other.
class cPeerDrivenEntityModel
{
public:
	// How far behind the sender's newest sample the bodies are played back, to hide network jitter.
	static const double kRenderDelayMs;
	// Consecutive samples of a body further apart than this, in meters, are snapped between.
	static const float kSnapDistanceMeters;

	cPeerDrivenEntityModel();

	// Driving an entity the Session already drives succeeds and changes nothing. If the local player
	// is interacting with the entity, that interaction ends first.
	eGameInteractionEntityOutcome Drive(int alEntityId, iPeerDrivenEntityWorld& aWorld);

	// Buffers every sample of a driven entity's existing body. On failure, names the entity of the
	// first sample it could not buffer.
	eGameInteractionEntityOutcome AddBodies(const cGameInteractionBodySamples& aSamples, double afLocalTimeMs,
		const iPeerDrivenEntityWorld& aWorld, int& alFailedEntityId);

	eGameInteractionEntityOutcome SetInteracting(int alEntityId, bool abInteracting, iPeerDrivenEntityWorld& aWorld);

	// Gives the entity back to local physics from its current state, clearing any interacting mark.
	eGameInteractionEntityOutcome Release(int alEntityId, iPeerDrivenEntityWorld& aWorld);
	// As the Session ends, the map changes, or a save loads.
	void ReleaseAll(iPeerDrivenEntityWorld& aWorld);
	// Forgets every entity without touching the world, when the maps holding them are already gone.
	void Clear();

	// Sets each buffered body to its state at the playback time. An entity the world no longer has is
	// forgotten.
	void Update(double afLocalTimeMs, iPeerDrivenEntityWorld& aWorld);

	bool IsDriving(int alEntityId) const;

private:
	struct cBodySample
	{
		double mfSenderTimeMs;
		cGameInteractionBodyState mState;
	};

	struct cDrivenBody
	{
		cDrivenBody() : mlBodyId(0) {}
		int mlBodyId;
		std::deque<cBodySample> mvSamples;
	};

	struct cDrivenEntity
	{
		cDrivenEntity() : mlEntityId(0), mbInteracting(false) {}
		int mlEntityId;
		bool mbInteracting;
		std::vector<cDrivenBody> mvBodies;
	};

	cDrivenEntity* FindDriven(int alEntityId);
	void EndDriving(size_t alIndex, iPeerDrivenEntityWorld& aWorld);
	void ClearSamples();

	std::vector<cDrivenEntity> mvDriven;
	cSenderClock mClock;
	// Whether any sample was buffered since the clock last started over.
	bool mbHasSamples;
	double mfNewestSenderTimeMs;
};

#endif // PEER_DRIVEN_ENTITY_MODEL_H
