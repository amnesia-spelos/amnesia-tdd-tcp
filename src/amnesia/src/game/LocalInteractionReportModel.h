#ifndef LOCAL_INTERACTION_REPORT_MODEL_H
#define LOCAL_INTERACTION_REPORT_MODEL_H

#include "GameInteractionGateway.h"

#include <cstddef>
#include <vector>

// A body of an entity as the local game has it now.
struct cLocalInteractionBody
{
	cLocalInteractionBody() : mlBodyId(0), mbAsleep(false) {}
	cLocalInteractionBody(int alBodyId, const cGameInteractionBodyState& aState, bool abAsleep)
		: mlBodyId(alBodyId), mState(aState), mbAsleep(abAsleep) {}
	int mlBodyId;
	cGameInteractionBodyState mState;
	// Whether the physics engine has put the body to sleep.
	bool mbAsleep;
};

// The local game's entities on the current map, as the report reads them every update.
class iLocalInteractionWorld
{
public:
	virtual ~iLocalInteractionWorld() {}
	// False when the current map no longer has the entity.
	virtual bool GetBodies(int alEntityId, std::vector<cLocalInteractionBody>& avBodies) const = 0;
};

// An interactions Event the report raises. The map is left for the caller to fill.
struct cLocalInteractionReportEvent
{
	cLocalInteractionReportEvent(eGameInteractionEventType aType = eGameInteractionEvent_InteractionStarted)
		: mType(aType) {}
	eGameInteractionEventType mType;
	cGameInteractionEntityEvent mEntityEvent;
};

// The report of the holdable entities whose motion the local player decides (ADR 0005): the entity
// the local player is interacting with, and the ones released until they settle. Only map-placed
// entities are reported, and the report holds at most kMaxBodies bodies.
class cLocalInteractionReportModel
{
public:
	// A released entity whose bodies never all sleep settles this long after its interaction ended.
	static const double kSettlingCapMs;
	static const size_t kMaxBodies = 32;

	cLocalInteractionReportModel();

	// The local player started interacting with body alBodyId of an entity with alBodyCount bodies.
	// An entity created at runtime is ignored, with its interaction's end. An entity whose bodies do
	// not fit is not reported, but its interaction's start and end are.
	void StartInteraction(int alEntityId, int alBodyId, bool abCreatedAtRuntime, size_t alBodyCount);
	// A released, thrown, or dropped entity stays in the report until it settles. A destroyed one
	// leaves it.
	void EndInteraction(eGameInteractionEnding aEnding, double afTimeMs);

	// Reads every reported entity's bodies, and settles the released ones whose bodies all sleep or
	// that reached the Settling cap. An entity the world no longer has leaves the report.
	void Update(const iLocalInteractionWorld& aWorld, double afTimeMs);

	// The bodies of every reported entity as the latest Update read them.
	const std::vector<cGameInteractionBodySample>& GetReportedBodies() const { return mvReportedBodies; }

	// The Events raised since the last call, in order.
	std::vector<cLocalInteractionReportEvent> TakeEvents();

	// Empties the report and drops untaken Events, as a map change or save load does. An interaction
	// in progress is forgotten, so its end raises nothing.
	void Clear();

private:
	struct cReportedEntity
	{
		cReportedEntity() : mlEntityId(0), mlBodyCount(0), mbHeld(false), mfEndTimeMs(0.0) {}
		int mlEntityId;
		size_t mlBodyCount;
		bool mbHeld;
		// When its interaction ended, while it is not held.
		double mfEndTimeMs;
	};

	cReportedEntity* FindReported(int alEntityId);
	size_t CountReportedBodies() const;
	void Raise(eGameInteractionEventType aType, int alEntityId, int alBodyId = 0,
		eGameInteractionEnding aEnding = eGameInteractionEnding_Released);

	std::vector<cReportedEntity> mvReported;
	std::vector<cGameInteractionBodySample> mvReportedBodies;
	std::vector<cLocalInteractionReportEvent> mvEvents;
	// The holdable entity the local player is interacting with, reported or not.
	bool mbInteracting;
	int mlInteractionEntityId;
	int mlInteractionBodyId;
};

#endif // LOCAL_INTERACTION_REPORT_MODEL_H
