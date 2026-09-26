#include "PeerDrivenEntityModel.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
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

	bool Near(float afActual, float afExpected)
	{
		return std::fabs(afActual - afExpected) < 0.0001f;
	}

	// Records what the model does to the world, one line per call, and the latest state of each body.
	class cFakeWorld : public iPeerDrivenEntityWorld
	{
	public:
		cFakeWorld() : mlLocallyInteractingId(-1), mlBodyStatesSet(0) {}

		// Gives the entity bodies with IDs 0, 1, ..., alBodyCount - 1.
		void Place(int alEntityId, size_t alBodyCount = 1,
			eGameInteractionEntityOutcome aFound = eGameInteractionEntityOutcome_Success)
		{
			m_mapEntities[alEntityId] = aFound;
			m_mapBodyCounts[alEntityId] = alBodyCount;
		}

		void Remove(int alEntityId) { m_mapEntities.erase(alEntityId); }

		virtual eGameInteractionEntityOutcome FindEntity(int alEntityId) const
		{
			std::map<int, eGameInteractionEntityOutcome>::const_iterator it = m_mapEntities.find(alEntityId);
			return it == m_mapEntities.end() ? eGameInteractionEntityOutcome_NotFound : it->second;
		}

		virtual bool HasBody(int alEntityId, int alBodyId) const
		{
			if (FindEntity(alEntityId) != eGameInteractionEntityOutcome_Success) return false;
			return alBodyId >= 0 && static_cast<size_t>(alBodyId) < m_mapBodyCounts.find(alEntityId)->second;
		}

		virtual bool IsLocallyInteracting(int alEntityId) const { return alEntityId == mlLocallyInteractingId; }

		virtual void EndLocalInteraction(int alEntityId)
		{
			Log("end-local", alEntityId);
			mlLocallyInteractingId = -1;
		}

		virtual void BeginDriving(int alEntityId) { Log("begin", alEntityId); }
		virtual void EndDriving(int alEntityId) { Log("end", alEntityId); }

		virtual void SetInteracting(int alEntityId, bool abInteracting)
		{
			Log(abInteracting ? "interacting" : "not-interacting", alEntityId);
		}

		virtual void SetBodyState(int alEntityId, int alBodyId, const cGameInteractionBodyState& aState)
		{
			m_mapStates[std::make_pair(alEntityId, alBodyId)] = aState;
			++mlBodyStatesSet;
		}

		bool GetState(int alEntityId, int alBodyId, cGameInteractionBodyState& aState) const
		{
			std::map<std::pair<int, int>, cGameInteractionBodyState>::const_iterator it =
				m_mapStates.find(std::make_pair(alEntityId, alBodyId));
			if (it == m_mapStates.end()) return false;
			aState = it->second;
			return true;
		}

		float GetX(int alEntityId, int alBodyId = 0) const
		{
			cGameInteractionBodyState state;
			Expect(GetState(alEntityId, alBodyId, state), "the body was set");
			return state.mPosition.mfX;
		}

		void ForgetStates()
		{
			m_mapStates.clear();
			mlBodyStatesSet = 0;
		}

		int mlLocallyInteractingId;
		std::vector<std::string> mvCalls;
		int mlBodyStatesSet;

	private:
		void Log(const std::string& asCall, int alEntityId)
		{
			std::ostringstream stream;
			stream << asCall << " " << alEntityId;
			mvCalls.push_back(stream.str());
		}

		std::map<int, eGameInteractionEntityOutcome> m_mapEntities;
		std::map<int, size_t> m_mapBodyCounts;
		std::map<std::pair<int, int>, cGameInteractionBodyState> m_mapStates;
	};

	cGameInteractionBodySample Body(int alEntityId, int alBodyId, float afX)
	{
		cGameInteractionBodySample sample;
		sample.mlEntityId = alEntityId;
		sample.mlBodyId = alBodyId;
		sample.mState.mPosition.mfX = afX;
		return sample;
	}

	cGameInteractionBodySamples Samples(unsigned long long alTimeMs, const cGameInteractionBodySample& aBody)
	{
		cGameInteractionBodySamples samples;
		samples.mlTimeMs = alTimeMs;
		samples.mvBodies.push_back(aBody);
		samples.msMapFile = "maps/main/level01.map";
		return samples;
	}

	// Adds samples that must be accepted.
	void Add(cPeerDrivenEntityModel& aModel, const cFakeWorld& aWorld, unsigned long long alSenderTimeMs,
		double afLocalTimeMs, const cGameInteractionBodySample& aBody)
	{
		int lFailedEntityId = 0;
		Expect(aModel.AddBodies(Samples(alSenderTimeMs, aBody), afLocalTimeMs, aWorld, lFailedEntityId) ==
			eGameInteractionEntityOutcome_Success, "the samples are accepted");
	}

	void TestDrivingTakesTheEntityFromLocalPhysics()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;

		Expect(model.Drive(12, world) == eGameInteractionEntityOutcome_Success, "a holdable entity is driven");
		Expect(model.IsDriving(12), "the model drives it");
		Expect(world.mvCalls.size() == 1 && world.mvCalls[0] == "begin 12", "the world hands it over");

		Expect(model.Drive(12, world) == eGameInteractionEntityOutcome_Success, "driving it again succeeds");
		Expect(world.mvCalls.size() == 1, "driving it again changes nothing");
	}

	void TestOnlyAHoldableEntityOfTheMapIsDriven()
	{
		cFakeWorld world;
		world.Place(405, 1, eGameInteractionEntityOutcome_NotHoldable);
		cPeerDrivenEntityModel model;

		Expect(model.Drive(404, world) == eGameInteractionEntityOutcome_NotFound, "a missing entity is not found");
		Expect(model.Drive(405, world) == eGameInteractionEntityOutcome_NotHoldable,
			"an entity a player does not hold is not holdable");
		Expect(!model.IsDriving(404) && !model.IsDriving(405), "neither is driven");
		Expect(world.mvCalls.empty(), "the world is left alone");
	}

	void TestDrivingEndsTheLocalInteractionFirst()
	{
		cFakeWorld world;
		world.Place(12);
		world.mlLocallyInteractingId = 12;
		cPeerDrivenEntityModel model;

		model.Drive(12, world);
		Expect(world.mvCalls.size() == 2, "the interaction ends and the entity is handed over");
		Expect(world.mvCalls[0] == "end-local 12", "the local interaction ends first");
		Expect(world.mvCalls[1] == "begin 12", "then the entity is handed over, in the same update");
	}

	void TestDrivingLeavesOtherLocalInteractionsAlone()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(13);
		world.mlLocallyInteractingId = 13;
		cPeerDrivenEntityModel model;

		model.Drive(12, world);
		Expect(world.mvCalls.size() == 1 && world.mvCalls[0] == "begin 12",
			"the local player keeps holding another entity");
	}

	void TestSamplesAreRefusedForBodiesNotDriven()
	{
		cFakeWorld world;
		world.Place(12, 2);
		world.Place(13);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);

		cGameInteractionBodySamples samples = Samples(0, Body(13, 0, 1.0f));
		samples.mvBodies.push_back(Body(12, 99, 2.0f));
		samples.mvBodies.push_back(Body(12, 1, 3.0f));
		int lFailedEntityId = 0;
		Expect(model.AddBodies(samples, 1000.0, world, lFailedEntityId) == eGameInteractionEntityOutcome_NotFound,
			"samples for an entity not driven fail");
		Expect(lFailedEntityId == 13, "the failure names the first entity not driven");

		model.Update(1000.0, world);
		cGameInteractionBodyState state;
		Expect(!world.GetState(13, 0, state), "an entity not driven is not moved");
		Expect(!world.GetState(12, 99, state), "a missing body is not moved");
		Expect(world.GetX(12, 1) == 3.0f, "the other samples are still applied");

		samples = Samples(16, Body(12, 99, 2.0f));
		Expect(model.AddBodies(samples, 1016.0, world, lFailedEntityId) == eGameInteractionEntityOutcome_NotFound,
			"a sample for a missing body fails");
		Expect(lFailedEntityId == 12, "the failure names that body's entity");
	}

	void TestBodiesArePlayedBackAfterTheRenderDelay()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);

		model.Update(1000.0, world);
		Expect(world.mlBodyStatesSet == 0, "nothing moves the entity before its first sample");

		Add(model, world, 5000, 1000.0, Body(12, 0, 0.0f));
		Add(model, world, 5100, 1100.0, Body(12, 0, 2.0f));

		model.Update(1100.0, world);
		Expect(Near(world.GetX(12), 0.0f), "the render time trails the newest sample by the delay");
		model.Update(1150.0, world);
		Expect(Near(world.GetX(12), 1.0f), "between two samples the body is interpolated");
		model.Update(1300.0, world);
		Expect(Near(world.GetX(12), 2.0f), "after the newest sample the body holds it");
		model.Update(1316.0, world);
		Expect(Near(world.GetX(12), 2.0f), "and keeps being set to it every update");
	}

	void TestEveryPartOfTheStateIsInterpolated()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);

		cGameInteractionBodySample from = Body(12, 0, 0.0f);
		cGameInteractionBodySample to = Body(12, 0, 0.0f);
		to.mState.mPosition = cGameInteractionPosition(1.0f, 2.0f, -1.0f);
		// A quarter turn about Y, written with the opposite sign.
		const float fHalf = std::sqrt(0.5f);
		to.mState.mOrientation = cGameInteractionQuaternion(0.0f, -fHalf, 0.0f, -fHalf);
		to.mState.mLinearVelocity = cGameInteractionVector(4.0f, 0.0f, -8.0f);
		to.mState.mAngularVelocity = cGameInteractionVector(0.0f, 180.0f, 0.0f);
		Add(model, world, 0, 1000.0, from);
		Add(model, world, 100, 1100.0, to);

		model.Update(1150.0, world);
		cGameInteractionBodyState state;
		world.GetState(12, 0, state);
		Expect(Near(state.mPosition.mfY, 1.0f) && Near(state.mPosition.mfZ, -0.5f), "position is interpolated");
		Expect(Near(state.mLinearVelocity.mfX, 2.0f) && Near(state.mLinearVelocity.mfZ, -4.0f),
			"linear velocity is interpolated");
		Expect(Near(state.mAngularVelocity.mfY, 90.0f), "angular velocity is interpolated");
		Expect(Near(std::fabs(state.mOrientation.mfY), 0.3827f) && Near(std::fabs(state.mOrientation.mfW), 0.9239f),
			"orientation is a unit quaternion an eighth of a turn along, the short way");
		Expect(state.mOrientation.mfY * state.mOrientation.mfW > 0.0f,
			"whatever sign the quaternion is written with");
	}

	void TestAJumpTooFarIsSnappedTo()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);

		Add(model, world, 0, 1000.0, Body(12, 0, 0.0f));
		Add(model, world, 100, 1100.0, Body(12, 0, cPeerDrivenEntityModel::kSnapDistanceMeters + 1.0f));

		model.Update(1150.0, world);
		Expect(world.GetX(12) == 0.0f, "the body holds its state until the jump is due");
		model.Update(1200.0, world);
		Expect(world.GetX(12) == cPeerDrivenEntityModel::kSnapDistanceMeters + 1.0f, "then it snaps");
	}

	void TestStaleAndRestartedSamples()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);

		Add(model, world, 5000, 1000.0, Body(12, 0, 0.0f));
		Add(model, world, 5100, 1100.0, Body(12, 0, 2.0f));
		Add(model, world, 5050, 1110.0, Body(12, 0, 99.0f));
		model.Update(1150.0, world);
		Expect(Near(world.GetX(12), 1.0f), "an out-of-order sample is dropped");

		// The sender's clock started over, as when a recording is replayed.
		Add(model, world, 0, 1200.0, Body(12, 0, 50.0f));
		model.Update(1200.0, world);
		Expect(world.GetX(12) == 50.0f, "a clock far behind the newest sample starts over at once");
		Add(model, world, 100, 1300.0, Body(12, 0, 52.0f));
		model.Update(1350.0, world);
		Expect(Near(world.GetX(12), 51.0f), "and plays back on the new alignment");
	}

	void TestSeveralEntitiesAndBodiesShareTheStream()
	{
		cFakeWorld world;
		world.Place(12, 2);
		world.Place(13);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);
		model.Drive(13, world);

		cGameInteractionBodySamples samples = Samples(0, Body(12, 0, 0.0f));
		samples.mvBodies.push_back(Body(12, 1, 100.0f));
		samples.mvBodies.push_back(Body(13, 0, -10.0f));
		int lFailedEntityId = 0;
		model.AddBodies(samples, 1000.0, world, lFailedEntityId);
		samples = Samples(100, Body(12, 0, 2.0f));
		samples.mvBodies.push_back(Body(12, 1, 102.0f));
		samples.mvBodies.push_back(Body(13, 0, -12.0f));
		model.AddBodies(samples, 1100.0, world, lFailedEntityId);

		model.Update(1150.0, world);
		Expect(Near(world.GetX(12, 0), 1.0f), "the first body follows its samples");
		Expect(Near(world.GetX(12, 1), 101.0f), "the second body follows its own");
		Expect(Near(world.GetX(13, 0), -11.0f), "another entity follows its own");
	}

	void TestInteractingIsMirroredOnlyForADrivenEntity()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;

		Expect(model.SetInteracting(12, true, world) == eGameInteractionEntityOutcome_NotFound,
			"an entity not driven cannot be marked");
		Expect(world.mvCalls.empty(), "and is left alone");

		model.Drive(12, world);
		Expect(model.SetInteracting(12, true, world) == eGameInteractionEntityOutcome_Success, "a driven one can");
		Expect(model.SetInteracting(12, false, world) == eGameInteractionEntityOutcome_Success, "and cleared");
		Expect(world.mvCalls.size() == 3 && world.mvCalls[1] == "interacting 12" &&
			world.mvCalls[2] == "not-interacting 12", "the world mirrors the mark");
	}

	void TestReleasingGivesTheEntityBack()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		Expect(model.Release(12, world) == eGameInteractionEntityOutcome_NotFound,
			"an entity not driven cannot be released");

		model.Drive(12, world);
		model.SetInteracting(12, true, world);
		Add(model, world, 0, 1000.0, Body(12, 0, 1.0f));
		world.mvCalls.clear();

		Expect(model.Release(12, world) == eGameInteractionEntityOutcome_Success, "a driven entity is released");
		Expect(world.mvCalls.size() == 2 && world.mvCalls[0] == "not-interacting 12" && world.mvCalls[1] == "end 12",
			"its mark is cleared and it returns to local physics");
		Expect(!model.IsDriving(12), "it is no longer driven");

		world.ForgetStates();
		model.Update(1200.0, world);
		Expect(world.mlBodyStatesSet == 0, "its buffered samples no longer move it");

		model.Drive(12, world);
		model.Update(1300.0, world);
		Expect(world.mlBodyStatesSet == 0, "driving it again does not replay old samples");
	}

	void TestReleasingAllEndsEveryDrivenEntity()
	{
		cFakeWorld world;
		world.Place(12);
		world.Place(13);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);
		model.Drive(13, world);
		model.SetInteracting(13, true, world);
		world.mvCalls.clear();

		model.ReleaseAll(world);
		Expect(world.mvCalls.size() == 3, "every entity is released and every mark cleared");
		Expect(world.mvCalls[0] == "end 12" && world.mvCalls[1] == "not-interacting 13" && world.mvCalls[2] == "end 13",
			"in order");
		Expect(!model.IsDriving(12) && !model.IsDriving(13), "none is driven any more");
	}

	void TestAnEntityTheWorldLostIsForgotten()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);
		Add(model, world, 0, 1000.0, Body(12, 0, 1.0f));

		world.Remove(12);
		world.mvCalls.clear();
		model.Update(1100.0, world);
		Expect(!model.IsDriving(12), "a destroyed entity is no longer driven");
		Expect(world.mvCalls.empty(), "and is not handed back");
		model.ReleaseAll(world);
		Expect(world.mvCalls.empty(), "nor later");
	}

	void TestClearingForgetsWithoutTouchingTheWorld()
	{
		cFakeWorld world;
		world.Place(12);
		cPeerDrivenEntityModel model;
		model.Drive(12, world);
		Add(model, world, 0, 1000.0, Body(12, 0, 1.0f));
		world.mvCalls.clear();
		world.ForgetStates();

		model.Clear();
		model.Update(1100.0, world);
		Expect(!model.IsDriving(12), "nothing is driven");
		Expect(world.mvCalls.empty() && world.mlBodyStatesSet == 0, "the world is not touched");
	}
}

int main()
{
	TestDrivingTakesTheEntityFromLocalPhysics();
	TestOnlyAHoldableEntityOfTheMapIsDriven();
	TestDrivingEndsTheLocalInteractionFirst();
	TestDrivingLeavesOtherLocalInteractionsAlone();
	TestSamplesAreRefusedForBodiesNotDriven();
	TestBodiesArePlayedBackAfterTheRenderDelay();
	TestEveryPartOfTheStateIsInterpolated();
	TestAJumpTooFarIsSnappedTo();
	TestStaleAndRestartedSamples();
	TestSeveralEntitiesAndBodiesShareTheStream();
	TestInteractingIsMirroredOnlyForADrivenEntity();
	TestReleasingGivesTheEntityBack();
	TestReleasingAllEndsEveryDrivenEntity();
	TestAnEntityTheWorldLostIsForgotten();
	TestClearingForgetsWithoutTouchingTheWorld();
	std::cout << "Peer-Driven Entity model cases passed\n";
	return 0;
}
