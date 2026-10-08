#include "LocalInteractionReportModel.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
	void Expect(bool abCondition, const std::string& asDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << asDescription << "\n";
		exit(1);
	}

	class cFakeWorld : public iLocalInteractionWorld
	{
	public:
		// Gives the entity alBodyCount bodies with IDs 0, 1, ..., awake, each at x = its entity ID.
		void Place(int alEntityId, size_t alBodyCount = 1)
		{
			std::vector<cLocalInteractionBody>& vBodies = m_mapEntities[alEntityId];
			vBodies.clear();
			for (size_t i = 0; i < alBodyCount; ++i)
			{
				cGameInteractionBodyState state;
				state.mPosition.mfX = static_cast<float>(alEntityId);
				vBodies.push_back(cLocalInteractionBody(static_cast<int>(i), state, false));
			}
		}

		void Move(int alEntityId, float afX)
		{
			std::vector<cLocalInteractionBody>& vBodies = m_mapEntities[alEntityId];
			for (size_t i = 0; i < vBodies.size(); ++i) vBodies[i].mState.mPosition.mfX = afX;
		}

		void SetAsleep(int alEntityId, bool abAsleep, size_t alBodyIndex = static_cast<size_t>(-1))
		{
			std::vector<cLocalInteractionBody>& vBodies = m_mapEntities[alEntityId];
			for (size_t i = 0; i < vBodies.size(); ++i)
			{
				if (alBodyIndex == static_cast<size_t>(-1) || alBodyIndex == i) vBodies[i].mbAsleep = abAsleep;
			}
		}

		void Remove(int alEntityId) { m_mapEntities.erase(alEntityId); }

		virtual bool GetBodies(int alEntityId, std::vector<cLocalInteractionBody>& avBodies) const
		{
			std::map<int, std::vector<cLocalInteractionBody> >::const_iterator it = m_mapEntities.find(alEntityId);
			if (it == m_mapEntities.end()) return false;
			avBodies = it->second;
			return true;
		}

	private:
		std::map<int, std::vector<cLocalInteractionBody> > m_mapEntities;
	};

	std::vector<cLocalInteractionReportEvent> TakeEvents(cLocalInteractionReportModel& aModel)
	{
		return aModel.TakeEvents();
	}

	bool IsEvent(const cLocalInteractionReportEvent& aEvent, eGameInteractionEventType aType, int alEntityId)
	{
		return aEvent.mType == aType && aEvent.mEntityEvent.mlEntityId == alEntityId;
	}

	bool Reports(const cLocalInteractionReportModel& aModel, int alEntityId)
	{
		const std::vector<cGameInteractionBodySample>& vBodies = aModel.GetReportedBodies();
		for (size_t i = 0; i < vBodies.size(); ++i)
		{
			if (vBodies[i].mlEntityId == alEntityId) return true;
		}
		return false;
	}

	// The local player's character body touched a moving body of a free, map-placed holdable entity.
	cLocalInteractionContact PlayerTouches(int alEntityId, size_t alBodyCount = 1)
	{
		cLocalInteractionContact contact;
		contact.mbByLocalPlayer = true;
		contact.mTouched.mlEntityId = alEntityId;
		contact.mTouched.mlBodyCount = alBodyCount;
		return contact;
	}

	// A moving body of entity alSourceEntityId touched a moving body of a free, map-placed holdable entity.
	cLocalInteractionContact EntityTouches(int alSourceEntityId, int alEntityId, size_t alBodyCount = 1)
	{
		cLocalInteractionContact contact;
		contact.mlSourceEntityId = alSourceEntityId;
		contact.mTouched.mlEntityId = alEntityId;
		contact.mTouched.mlBodyCount = alBodyCount;
		return contact;
	}

	void TestThePlayerWalkingIntoAPropReportsItUntilItSettles()
	{
		cFakeWorld world;
		world.Place(20);
		cLocalInteractionReportModel model;

		model.RecordContact(PlayerTouches(20));
		Expect(TakeEvents(model).empty(), "a contact is acted on only by the next update");
		model.Update(world, 100.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportContact, 20),
			"a prop the local player touched raises reportcontact");
		Expect(Reports(model, 20), "a prop the local player touched is reported");

		model.Update(world, 116.0);
		Expect(TakeEvents(model).empty(), "a contact is acted on once");
		world.SetAsleep(20, true);
		model.Update(world, 132.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 20),
			"a prop that entered by contact settles");
		Expect(!Reports(model, 20), "a settled prop leaves the report");
	}

	void TestOnlyAReportedBodyKnocksAPropIntoTheReport()
	{
		cFakeWorld world;
		world.Place(10);
		world.Place(20);
		world.Place(21);
		cLocalInteractionReportModel model;
		model.StartInteraction(10, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.Update(world, 0.0);
		TakeEvents(model);

		// An Avatar, another character, or a prop no one moved is not a reported body.
		model.RecordContact(EntityTouches(99, 21));
		model.RecordContact(EntityTouches(10, 20));
		model.Update(world, 16.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportContact, 20),
			"only the thrown prop's contact raises reportcontact");
		Expect(Reports(model, 20), "a prop the thrown prop touched is reported");
		Expect(!Reports(model, 21), "a prop an unreported body touched is not reported");
	}

	void TestOnlyAFreeMapPlacedHoldablePropEntersByContact()
	{
		cFakeWorld world;
		for (int lEntityId = 20; lEntityId <= 23; ++lEntityId) world.Place(lEntityId);
		cLocalInteractionReportModel model;

		cLocalInteractionContact staticBody = PlayerTouches(20);
		staticBody.mTouched.mbBodyMoving = false;
		cLocalInteractionContact notHoldable = PlayerTouches(21);
		notHoldable.mTouched.mbHoldable = false;
		cLocalInteractionContact runtimeCreated = PlayerTouches(22);
		runtimeCreated.mTouched.mbCreatedAtRuntime = true;
		cLocalInteractionContact peerDriven = PlayerTouches(23);
		peerDriven.mTouched.mbPeerDriven = true;
		model.RecordContact(staticBody);
		model.RecordContact(notHoldable);
		model.RecordContact(runtimeCreated);
		model.RecordContact(peerDriven);
		model.Update(world, 0.0);

		Expect(TakeEvents(model).empty(), "touching a body that cannot enter raises nothing");
		Expect(model.GetReportedBodies().empty(),
			"a static body, an entity that is not holdable, a runtime-created entity, and a Peer-Driven Entity "
			"never enter by contact");
	}

	void TestAToppledStackIsReportedPropByProp()
	{
		cFakeWorld world;
		for (int lEntityId = 10; lEntityId <= 23; ++lEntityId) world.Place(lEntityId);
		cLocalInteractionReportModel model;
		model.StartInteraction(10, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.Update(world, 0.0);
		TakeEvents(model);

		// In one physics step the thrown prop hits the bottom crate, which knocks the next one.
		model.RecordContact(EntityTouches(20, 21));
		model.RecordContact(EntityTouches(10, 20));
		model.Update(world, 16.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 2 && IsEvent(vEvents[0], eGameInteractionEvent_ReportContact, 20) &&
			IsEvent(vEvents[1], eGameInteractionEvent_ReportContact, 21),
			"a prop knocked by a knocked prop in the same step is reported after it");

		model.RecordContact(EntityTouches(21, 22));
		model.Update(world, 32.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportContact, 22),
			"a prop knocked by a knocked prop in a later step is reported");
		Expect(Reports(model, 20) && Reports(model, 21) && Reports(model, 22), "the whole toppled stack is reported");
		Expect(!Reports(model, 23), "an untouched prop is not");

		for (int lEntityId = 10; lEntityId <= 22; ++lEntityId) world.SetAsleep(lEntityId, true);
		model.Update(world, 48.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 4, "every prop of the stack settles");
		Expect(model.GetReportedBodies().empty(), "a settled stack leaves the report");
	}

	void TestAContactThatWouldExceedTheBudgetIsNotReported()
	{
		cFakeWorld world;
		world.Place(10, 30);
		world.Place(20, 3);
		world.Place(21, 1);
		world.Place(22, 2);
		cLocalInteractionReportModel model;
		model.StartInteraction(10, 0, false, 30);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.Update(world, 0.0);
		TakeEvents(model);

		model.RecordContact(EntityTouches(10, 20, 3));
		model.Update(world, 16.0);
		Expect(TakeEvents(model).empty(), "a prop whose bodies do not fit raises no reportcontact");
		Expect(!Reports(model, 20), "a prop whose bodies do not fit stays under local physics");

		model.RecordContact(EntityTouches(20, 21));
		model.Update(world, 32.0);
		Expect(!Reports(model, 21), "a prop that did not fit knocks nothing into the report");

		model.RecordContact(EntityTouches(10, 22, 2));
		model.Update(world, 48.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportContact, 22),
			"a prop that fits exactly into 32 bodies enters by contact");
		Expect(model.GetReportedBodies().size() == cLocalInteractionReportModel::kMaxBodies,
			"the report holds at most 32 bodies");
	}

	void TestAHeldPropThatDidNotFitNeverEntersByContact()
	{
		cFakeWorld world;
		world.Place(10, 30);
		world.Place(20, 3);
		cLocalInteractionReportModel model;
		model.StartInteraction(10, 0, false, 30);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.StartInteraction(20, 0, false, 3);
		model.Update(world, 0.0);
		TakeEvents(model);
		model.StopReporting(10);

		model.RecordContact(PlayerTouches(20, 3));
		model.Update(world, 16.0);
		Expect(TakeEvents(model).empty(), "the prop the local player holds raises no reportcontact");
		Expect(!Reports(model, 20), "the prop the local player holds does not enter by contact");
	}

	void TestAKnockedPropThatNeverSleepsSettlesAtTheCapFromItsContact()
	{
		cFakeWorld world;
		world.Place(20);
		cLocalInteractionReportModel model;
		model.RecordContact(PlayerTouches(20));
		model.Update(world, 1000.0);
		TakeEvents(model);

		model.Update(world, 1000.0 + cLocalInteractionReportModel::kSettlingCapMs - 1.0);
		Expect(TakeEvents(model).empty(), "a knocked prop is reported until the cap");
		model.Update(world, 1000.0 + cLocalInteractionReportModel::kSettlingCapMs);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 20),
			"a knocked prop settles at the cap counted from its contact");
	}

	void TestClearingDropsContactsNotYetActedOn()
	{
		cFakeWorld world;
		world.Place(20);
		cLocalInteractionReportModel model;
		model.RecordContact(PlayerTouches(20));

		model.Clear();
		model.Update(world, 0.0);
		Expect(TakeEvents(model).empty(), "a contact from the previous Map Visit raises nothing");
		Expect(model.GetReportedBodies().empty(), "a contact from the previous Map Visit reports nothing");
	}

	void TestStartingAGrabRaisesStartedAndReportsTheHeldBody()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;

		model.StartInteraction(12, 3, false, 1);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1, "starting an interaction raises one Event");
		Expect(IsEvent(vEvents[0], eGameInteractionEvent_InteractionStarted, 12), "the Event is interactionstart");
		Expect(vEvents[0].mEntityEvent.mlBodyId == 3, "interactionstart names the held body");

		model.Update(world, 0.0);
		Expect(model.GetReportedBodies().size() == 1, "the held entity's body is reported");
		Expect(model.GetReportedBodies()[0].mlEntityId == 12, "the report names the entity");
		Expect(model.GetReportedBodies()[0].mlBodyId == 0, "the report names the body from the world");
		Expect(model.GetReportedBodies()[0].mState.mPosition.mfX == 12.0f, "the report carries the body state");
		Expect(TakeEvents(model).empty(), "an update while holding raises nothing");
	}

	void TestTheReportFollowsTheBodyEveryUpdate()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);

		model.Update(world, 0.0);
		world.Move(12, 5.0f);
		model.Update(world, 16.0);
		Expect(model.GetReportedBodies()[0].mState.mPosition.mfX == 5.0f, "the report carries the latest state");
	}

	void TestAHeldEntityNeverSettles()
	{
		cFakeWorld world;
		world.Place(12);
		world.SetAsleep(12, true);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);

		model.Update(world, 10000.0);
		Expect(TakeEvents(model).size() == 1, "a held entity raises only its start, even asleep and past the cap");
		Expect(Reports(model, 12), "a held entity stays reported");
	}

	void TestEachEndingIsRaised()
	{
		const eGameInteractionEnding vEndings[] = { eGameInteractionEnding_Released, eGameInteractionEnding_Thrown,
			eGameInteractionEnding_TooFar, eGameInteractionEnding_Destroyed };
		for (size_t i = 0; i < 4; ++i)
		{
			cLocalInteractionReportModel model;
			model.StartInteraction(12, 3, false, 1);
			TakeEvents(model);

			model.EndInteraction(vEndings[i], 100.0);
			std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
			Expect(vEvents.size() == 1, "ending an interaction raises one Event");
			Expect(IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 12), "the Event is interactionend");
			Expect(vEvents[0].mEntityEvent.mlBodyId == 3, "interactionend names the held body");
			Expect(vEvents[0].mEntityEvent.mEnding == vEndings[i], "interactionend carries how it ended");
		}
	}

	// The report knows nothing of prop types, so swinging, spinning, pushing, and sliding are reported as a
	// grab is. Each case is the shape of one type's interaction: the moving body the player holds, how many
	// moving bodies there are, and a way it ends. A released door, lever, wheel, or slider stays reported
	// while its own logic moves it, until it sleeps.
	void TestEachInteractionShapeIsReportedFromStartToSettling()
	{
		struct cCase
		{
			const char* msType;
			size_t mlBodyCount;
			int mlHeldBodyId;
			eGameInteractionEnding mEnding;
		};
		const cCase vCases[] = {
			{ "SwingDoor", 2, 1, eGameInteractionEnding_Thrown },
			{ "Wheel", 1, 0, eGameInteractionEnding_Released },
			{ "Lever", 1, 0, eGameInteractionEnding_TooFar },
			{ "MultiSlider", 1, 0, eGameInteractionEnding_Released },
			{ "Push Object", 1, 0, eGameInteractionEnding_Thrown },
			{ "Slide Object", 1, 0, eGameInteractionEnding_Released },
		};
		for (size_t i = 0; i < sizeof(vCases) / sizeof(vCases[0]); ++i)
		{
			const cCase& type = vCases[i];
			const std::string sType(type.msType);
			cFakeWorld world;
			world.Place(12, type.mlBodyCount);
			cLocalInteractionReportModel model;

			model.StartInteraction(12, type.mlHeldBodyId, false, type.mlBodyCount);
			model.Update(world, 0.0);
			std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
			Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionStarted, 12) &&
				vEvents[0].mEntityEvent.mlBodyId == type.mlHeldBodyId, sType + ": interactionstart names the held body");
			Expect(model.GetReportedBodies().size() == type.mlBodyCount, sType + ": every moving body is reported");

			model.EndInteraction(type.mEnding, 100.0);
			vEvents = TakeEvents(model);
			Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 12) &&
				vEvents[0].mEntityEvent.mlBodyId == type.mlHeldBodyId && vEvents[0].mEntityEvent.mEnding == type.mEnding,
				sType + ": interactionend names the held body and how it ended");

			world.Move(12, 2.0f);
			model.Update(world, 1000.0);
			Expect(Reports(model, 12) && TakeEvents(model).empty(), sType + ": it is reported while it still moves");

			world.SetAsleep(12, true);
			model.Update(world, 1100.0);
			vEvents = TakeEvents(model);
			Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 12),
				sType + ": it settles when it sleeps");
		}
	}

	void TestEndingWithoutAnInteractionRaisesNothing()
	{
		cLocalInteractionReportModel model;
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		Expect(TakeEvents(model).empty(), "an end without a start raises nothing");

		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		TakeEvents(model);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		Expect(TakeEvents(model).empty(), "a second end raises nothing");
	}

	void TestAThrownEntityIsReportedInFlightUntilItSleeps()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.Update(world, 0.0);
		model.EndInteraction(eGameInteractionEnding_Thrown, 100.0);
		TakeEvents(model);

		world.Move(12, 7.0f);
		model.Update(world, 1000.0);
		Expect(Reports(model, 12), "a thrown entity stays reported while it flies");
		Expect(model.GetReportedBodies()[0].mState.mPosition.mfX == 7.0f, "the flight is reported");
		Expect(TakeEvents(model).empty(), "a flying entity raises nothing");

		world.SetAsleep(12, true);
		model.Update(world, 1100.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 12),
			"a released entity whose bodies sleep raises reportsettled");
		Expect(!Reports(model, 12), "a settled entity leaves the report");

		model.Update(world, 1200.0);
		Expect(TakeEvents(model).empty(), "a settled entity settles once");
	}

	void TestAnEntitySettlesOnlyWhenEveryBodySleeps()
	{
		cFakeWorld world;
		world.Place(12, 2);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 2);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		TakeEvents(model);

		world.SetAsleep(12, true, 0);
		model.Update(world, 100.0);
		Expect(TakeEvents(model).empty(), "an entity with an awake body does not settle");
		Expect(model.GetReportedBodies().size() == 2, "every body of a reported entity is reported");

		world.SetAsleep(12, true);
		model.Update(world, 200.0);
		Expect(TakeEvents(model).size() == 1, "an entity whose bodies all sleep settles");
	}

	void TestAnEntityThatNeverSleepsSettlesAtTheCap()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Released, 1000.0);
		TakeEvents(model);

		model.Update(world, 1000.0 + cLocalInteractionReportModel::kSettlingCapMs - 1.0);
		Expect(TakeEvents(model).empty(), "an awake entity does not settle before the cap");
		Expect(Reports(model, 12), "an awake entity is reported before the cap");

		model.Update(world, 1000.0 + cLocalInteractionReportModel::kSettlingCapMs);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 12),
			"an awake entity settles at the cap");
		Expect(!Reports(model, 12), "an entity settled at the cap leaves the report");
		Expect(cLocalInteractionReportModel::kSettlingCapMs == 3000.0, "the Settling cap is about 3 s");
	}

	void TestAThrownEntityThatBreaksRaisesReportBrokeWithItsFinalState()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Thrown, 100.0);
		model.Update(world, 200.0);
		TakeEvents(model);

		cGameInteractionBodyState finalState;
		finalState.mPosition = cGameInteractionPosition(1.25f, -2.5f, 3.75f);
		finalState.mLinearVelocity = cGameInteractionVector(0.5f, 0.0f, -1.0f);
		model.Break(12, finalState);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportBroke, 12),
			"a reported entity that breaks raises reportbroke");
		Expect(vEvents[0].mEntityEvent.mState.mPosition.mfY == -2.5f &&
			vEvents[0].mEntityEvent.mState.mLinearVelocity.mfZ == -1.0f, "reportbroke carries the final state");
		Expect(!Reports(model, 12), "a broken entity leaves the report at once");

		model.Update(world, 300.0);
		Expect(!Reports(model, 12) && TakeEvents(model).empty(), "a broken entity does not come back to settle");
	}

	void TestAHeldEntityThatBreaksEndsItsInteractionAsDestroyedOnly()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(13);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.Update(world, 0.0);
		TakeEvents(model);

		model.Break(12, cGameInteractionBodyState());
		Expect(TakeEvents(model).empty(), "a held entity that breaks raises no reportbroke");
		model.EndInteraction(eGameInteractionEnding_Destroyed, 100.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 12) &&
			vEvents[0].mEntityEvent.mEnding == eGameInteractionEnding_Destroyed,
			"its interaction ends as destroyed");
		model.Update(world, 150.0);
		Expect(!Reports(model, 12) && TakeEvents(model).empty(), "and it leaves the report without settling");

		model.StartInteraction(13, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Destroyed, 200.0);
		TakeEvents(model);
		model.Break(13, cGameInteractionBodyState());
		Expect(TakeEvents(model).empty(), "a break after the destroyed ending raises nothing");
	}

	void TestADestroyedEntityLeavesTheReportWithoutSettling()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.Update(world, 0.0);
		model.EndInteraction(eGameInteractionEnding_Destroyed, 100.0);
		TakeEvents(model);

		model.Update(world, 200.0);
		Expect(!Reports(model, 12), "a destroyed entity leaves the report");
		Expect(TakeEvents(model).empty(), "a destroyed entity does not settle");
	}

	void TestAnEntityTheWorldLostLeavesTheReport()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		TakeEvents(model);

		world.Remove(12);
		model.Update(world, 100.0);
		Expect(!Reports(model, 12), "an entity the world no longer has leaves the report");
		Expect(TakeEvents(model).empty(), "an entity the world lost raises nothing");
	}

	void TestSeveralEntitiesAreReportedAtOnce()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(-7);
		cLocalInteractionReportModel model;

		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.StartInteraction(-7, 0, false, 1);
		TakeEvents(model);

		model.Update(world, 100.0);
		Expect(Reports(model, 12) && Reports(model, -7), "a released entity and a held one are reported together");

		world.SetAsleep(12, true);
		world.SetAsleep(-7, true);
		model.Update(world, 200.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_ReportSettled, 12),
			"only the released entity settles");
		Expect(!Reports(model, 12) && Reports(model, -7), "the held entity stays reported");

		model.EndInteraction(eGameInteractionEnding_Released, 300.0);
		model.Update(world, 300.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 2 && IsEvent(vEvents[1], eGameInteractionEvent_ReportSettled, -7),
			"the second entity settles after its own release");
	}

	void TestGrabbingASettlingEntityAgainHoldsIt()
	{
		cFakeWorld world;
		world.Place(12);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		model.StartInteraction(12, 0, false, 1);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 3 && IsEvent(vEvents[2], eGameInteractionEvent_InteractionStarted, 12),
			"grabbing a settling entity raises interactionstart again");

		world.SetAsleep(12, true);
		model.Update(world, cLocalInteractionReportModel::kSettlingCapMs * 2);
		Expect(TakeEvents(model).empty(), "a settling entity grabbed again does not settle");
		Expect(model.GetReportedBodies().size() == 1, "an entity grabbed again is reported once");

		model.EndInteraction(eGameInteractionEnding_Released, 10000.0);
		world.SetAsleep(12, false);
		model.Update(world, 10000.0 + cLocalInteractionReportModel::kSettlingCapMs - 1.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 12),
			"the cap counts from the latest release");
	}

	void TestRuntimeCreatedEntitiesNeverEnterTheReport()
	{
		cFakeWorld world;
		world.Place(40);
		cLocalInteractionReportModel model;

		model.StartInteraction(40, 0, true, 1);
		model.Update(world, 0.0);
		Expect(TakeEvents(model).empty(), "grabbing a runtime-created entity raises nothing");
		Expect(!Reports(model, 40), "a runtime-created entity is not reported");

		model.EndInteraction(eGameInteractionEnding_Thrown, 100.0);
		model.Update(world, 200.0);
		Expect(TakeEvents(model).empty(), "releasing a runtime-created entity raises nothing");
		Expect(model.GetReportedBodies().empty(), "a released runtime-created entity is not reported");
	}

	void TestAnEntityWhoseBodiesDoNotFitIsNotReported()
	{
		cFakeWorld world;
		world.Place(1, 30);
		world.Place(2, 3);
		world.Place(3, 2);
		cLocalInteractionReportModel model;
		model.StartInteraction(1, 0, false, 30);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		TakeEvents(model);

		model.StartInteraction(2, 0, false, 3);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionStarted, 2),
			"an entity that does not fit still raises interactionstart");
		model.Update(world, 100.0);
		Expect(!Reports(model, 2), "an entity whose bodies do not fit is not reported");
		Expect(model.GetReportedBodies().size() == 30, "the report keeps the bodies that fit");

		model.EndInteraction(eGameInteractionEnding_Released, 200.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 2),
			"an entity that does not fit still raises interactionend");
		world.SetAsleep(2, true);
		model.Update(world, 300.0);
		Expect(TakeEvents(model).empty(), "an unreported entity never settles");

		model.StartInteraction(3, 0, false, 2);
		model.Update(world, 400.0);
		Expect(Reports(model, 3), "an entity that fits exactly into 32 bodies is reported");
		Expect(model.GetReportedBodies().size() == cLocalInteractionReportModel::kMaxBodies,
			"the report holds at most 32 bodies");
	}

	void TestClearingEmptiesTheReportWithoutEvents()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(13);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Thrown, 0.0);
		model.StartInteraction(13, 0, false, 1);
		model.Update(world, 0.0);

		model.Clear();
		Expect(TakeEvents(model).empty(), "clearing drops untaken Events");
		Expect(model.GetReportedBodies().empty(), "clearing empties the report");
		model.Update(world, 10000.0);
		Expect(TakeEvents(model).empty(), "cleared entities raise nothing");
		model.EndInteraction(eGameInteractionEnding_Released, 10000.0);
		Expect(TakeEvents(model).empty(), "clearing forgets the interaction");
	}

	void TestADrivenEntityLeavesTheReportWithoutEvents()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(13);
		cLocalInteractionReportModel model;
		model.StartInteraction(12, 0, false, 1);
		model.EndInteraction(eGameInteractionEnding_Released, 0.0);
		model.StartInteraction(13, 0, false, 1);
		model.Update(world, 0.0);
		TakeEvents(model);

		model.StopReporting(12);
		model.Update(world, 16.0);
		Expect(!Reports(model, 12), "a settling entity leaves the report when it is no longer the local game's");
		Expect(Reports(model, 13), "other reported entities stay");
		Expect(TakeEvents(model).empty(), "leaving the report this way raises nothing");
		model.Update(world, 10000.0);
		std::vector<cLocalInteractionReportEvent> vEvents = TakeEvents(model);
		Expect(vEvents.empty(), "the held entity never settles and the stopped one raises nothing later");

		model.StopReporting(13);
		model.Update(world, 10016.0);
		Expect(model.GetReportedBodies().empty(), "a held entity leaves the report too");
		model.EndInteraction(eGameInteractionEnding_Released, 10016.0);
		vEvents = TakeEvents(model);
		Expect(vEvents.size() == 1 && IsEvent(vEvents[0], eGameInteractionEvent_InteractionEnded, 13),
			"its interaction still ends");
		model.Update(world, 20000.0);
		Expect(TakeEvents(model).empty(), "and it does not come back to settle");
	}
}

int main()
{
	TestStartingAGrabRaisesStartedAndReportsTheHeldBody();
	TestTheReportFollowsTheBodyEveryUpdate();
	TestAHeldEntityNeverSettles();
	TestEachEndingIsRaised();
	TestEachInteractionShapeIsReportedFromStartToSettling();
	TestEndingWithoutAnInteractionRaisesNothing();
	TestAThrownEntityIsReportedInFlightUntilItSleeps();
	TestAnEntitySettlesOnlyWhenEveryBodySleeps();
	TestAnEntityThatNeverSleepsSettlesAtTheCap();
	TestADestroyedEntityLeavesTheReportWithoutSettling();
	TestAThrownEntityThatBreaksRaisesReportBrokeWithItsFinalState();
	TestAHeldEntityThatBreaksEndsItsInteractionAsDestroyedOnly();
	TestAnEntityTheWorldLostLeavesTheReport();
	TestSeveralEntitiesAreReportedAtOnce();
	TestGrabbingASettlingEntityAgainHoldsIt();
	TestRuntimeCreatedEntitiesNeverEnterTheReport();
	TestAnEntityWhoseBodiesDoNotFitIsNotReported();
	TestClearingEmptiesTheReportWithoutEvents();
	TestADrivenEntityLeavesTheReportWithoutEvents();
	TestThePlayerWalkingIntoAPropReportsItUntilItSettles();
	TestOnlyAReportedBodyKnocksAPropIntoTheReport();
	TestOnlyAFreeMapPlacedHoldablePropEntersByContact();
	TestAToppledStackIsReportedPropByProp();
	TestAContactThatWouldExceedTheBudgetIsNotReported();
	TestAHeldPropThatDidNotFitNeverEntersByContact();
	TestAKnockedPropThatNeverSleepsSettlesAtTheCapFromItsContact();
	TestClearingDropsContactsNotYetActedOn();
	std::cout << "Local interaction report model cases passed\n";
	return 0;
}
