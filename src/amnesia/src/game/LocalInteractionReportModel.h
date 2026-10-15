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

// The entity whose body was touched in a contact, as the game had it then.
struct cLocalInteractionContactTarget
{
	cLocalInteractionContactTarget()
		: mlEntityId(0), mbBodyMoving(true), mbHoldable(true), mbCreatedAtRuntime(false), mbPeerDriven(false),
		mlBodyCount(1) {}
	int mlEntityId;
	// False for a static body, such as a door's frame.
	bool mbBodyMoving;
	bool mbHoldable;
	bool mbCreatedAtRuntime;
	bool mbPeerDriven;
	// How many moving bodies the entity has.
	size_t mlBodyCount;
};

// A touch the physics step recorded: by the local player's character body, or by a moving body of an
// entity.
struct cLocalInteractionContact
{
	cLocalInteractionContact() : mbByLocalPlayer(false), mlSourceEntityId(0) {}
	bool mbByLocalPlayer;
	// The entity whose moving body touched, unless the local player did.
	int mlSourceEntityId;
	cLocalInteractionContactTarget mTouched;
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
// the local player is interacting with, the ones released until they settle, and the free ones a
// reported moving body or the local player's character body knocked, until they settle. Only
// map-placed entities are reported, and the report holds at most kMaxBodies bodies.
class cLocalInteractionReportModel
{
public:
	// A released entity whose bodies never all sleep settles this long after its interaction ended.
	static const double kSettlingCapMs;
	// A prop that entered by contact is reported only once a body moves this far, in world units, or turns
	// this many degrees, from where the Update that acted on the contact read it.
	static const float kContactMotionDistance;
	static const float kContactMotionDegrees;
	static const size_t kMaxBodies = 32;

	cLocalInteractionReportModel();

	// The local player started interacting with body alBodyId of an entity with alBodyCount bodies.
	// An entity created at runtime is ignored, with its interaction's end. An entity whose bodies do
	// not fit is not reported, but its interaction's start and end are.
	void StartInteraction(int alEntityId, int alBodyId, bool abCreatedAtRuntime, size_t alBodyCount);
	// A released, thrown, or dropped entity stays in the report until it settles. A destroyed one
	// leaves it, and one that broke meanwhile raises reportbroke right after its interaction's end.
	void EndInteraction(eGameInteractionEnding aEnding, double afTimeMs);

	// A reported entity that broke raises reportbroke with the final state of its break body, and leaves
	// the report. One that entered by contact and has not moved leaves it without an Event, since no Peer
	// heard of it. One the local player is holding raises reportbroke right after its interaction ends as
	// destroyed instead, whether it is reported or not.
	void Break(int alEntityId, const cGameInteractionBodyState& aFinalState);

	// The entity leaves the report without an Event, as when a Peer drives it. An interaction with it
	// still ends as usual, but the entity does not return to the report to settle.
	void StopReporting(int alEntityId);

	// A contact from the physics step, acted on by the next Update. A free, map-placed holdable entity
	// whose moving body the local player or a reported entity's moving body touched enters the report
	// tentatively, unless a Peer drives it or its bodies do not fit. It raises reportcontact and is
	// reported only once the contact moves it; until then it raises nothing, knocks nothing into the
	// report, and leaves silently when it settles. Once it has moved, one it touches enters as well.
	void RecordContact(const cLocalInteractionContact& aContact);

	// Reads every reported entity's bodies, and settles the released ones whose bodies all sleep or
	// that reached the Settling cap. An entity the world no longer has leaves the report.
	void Update(const iLocalInteractionWorld& aWorld, double afTimeMs);

	// The bodies of every reported entity as the latest Update read them.
	const std::vector<cGameInteractionBodySample>& GetReportedBodies() const { return mvReportedBodies; }

	// The Events raised since the last call, in order.
	std::vector<cLocalInteractionReportEvent> TakeEvents();

	// Empties the report and drops untaken Events and contacts not yet acted on, as a map change or
	// save load does. An interaction in progress is forgotten, so its end raises nothing.
	void Clear();

private:
	struct cReportedEntity
	{
		cReportedEntity() : mlEntityId(0), mlBodyCount(0), mbHeld(false), mbTentative(false), mfEndTimeMs(0.0) {}
		int mlEntityId;
		size_t mlBodyCount;
		bool mbHeld;
		// Entered by a contact that has not yet moved it: it raises nothing and none of its bodies are
		// reported until it moves.
		bool mbTentative;
		// When its interaction ended, while it is not held.
		double mfEndTimeMs;
		// Its bodies as the Update that acted on its contact read them, while it is tentative.
		std::vector<cLocalInteractionBody> mvBaseline;
	};

	// Enters the free entities the recorded contacts touched, and forgets the contacts.
	void EnterByContacts(const iLocalInteractionWorld& aWorld, double afTimeMs);
	void EnterByContact(const cLocalInteractionContact& aContact, const iLocalInteractionWorld& aWorld,
		double afTimeMs);
	static bool HasMoved(const std::vector<cLocalInteractionBody>& avBaseline,
		const std::vector<cLocalInteractionBody>& avBodies);
	cReportedEntity* FindReported(int alEntityId);
	size_t CountReportedBodies() const;
	void Raise(eGameInteractionEventType aType, int alEntityId, int alBodyId = 0,
		eGameInteractionEnding aEnding = eGameInteractionEnding_Released);
	void RaiseReportBroke(int alEntityId, const cGameInteractionBodyState& aFinalState);

	std::vector<cReportedEntity> mvReported;
	std::vector<cGameInteractionBodySample> mvReportedBodies;
	std::vector<cLocalInteractionReportEvent> mvEvents;
	std::vector<cLocalInteractionContact> mvContacts;
	// The holdable entity the local player is interacting with, reported or not.
	bool mbInteracting;
	int mlInteractionEntityId;
	int mlInteractionBodyId;
	// Whether the entity the local player is interacting with broke, and the state it broke from.
	bool mbInteractionBroke;
	cGameInteractionBodyState mInteractionBreakState;
};

#endif // LOCAL_INTERACTION_REPORT_MODEL_H
