#include "SharedCallbackModel.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void Expect(bool abCondition, const std::string& asDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << asDescription << "\n";
		exit(1);
	}

	cSharedCollide PlayerCollide()
	{
		cSharedCollide collide;
		collide.mbParentIsPlayer = true;
		return collide;
	}

	cSharedCollide EntityCollide(bool abParentHeld, bool abChildHeld)
	{
		cSharedCollide collide;
		collide.mbParentHeld = abParentHeld;
		collide.mbChildHeld = abChildHeld;
		return collide;
	}

	void LocalTriggersAreReportedOutsideASharedCallbackRun()
	{
		cSharedCallbackModel model;
		Expect(model.ReportsCallback(), "an interact or use-item callback the local player ran is reported");
		Expect(model.ReportsCollide(PlayerCollide()), "a Player collide is reported");
		Expect(model.ReportsCollide(EntityCollide(true, false)) && model.ReportsCollide(EntityCollide(false, true)),
			"an entity collide is reported when the local player Holds either side");
		Expect(!model.ReportsCollide(EntityCollide(false, false)), "an entity collide nobody here Holds is not reported");
		Expect(model.AllowsPlayerControl(), "scripts may take the player's control outside a run");
	}

	void ARunAndEverythingNestedInItIsNeverReported()
	{
		cSharedCallbackModel model;
		model.BeginRun();
		Expect(!model.ReportsCallback() && !model.ReportsCollide(PlayerCollide()),
			"a callback a Peer's Command runs is not reported");
		Expect(!model.AllowsPlayerControl(), "a run cannot take the player's control or camera");
		model.BeginRun();
		model.EndRun();
		Expect(!model.ReportsCallback() && !model.AllowsPlayerControl(), "a nested run's end leaves the outer run going");
		model.EndRun();
		Expect(model.ReportsCallback() && model.AllowsPlayerControl(),
			"a local trigger after the run is reported, even if the run set it up");
	}

	void ACollideRunHonoursTheStatesFilter()
	{
		bool colliding = false;
		Expect(cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, 1, colliding),
			"a registration for both states runs on entering");
		colliding = true;
		Expect(cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, -1, colliding),
			"a registration for both states runs on leaving");
		Expect(cSharedCallbackModel::RunCollide(PlayerCollide(), -1, -1, colliding),
			"a registration for leaving runs on leaving");
		Expect(!cSharedCallbackModel::RunCollide(PlayerCollide(), 1, -1, colliding),
			"a registration for entering does not match a leaving state");
	}

	void AnEntityCollideRunKeepsTheEdgeSoTheLocalCheckDoesNotFireAgain()
	{
		bool colliding = false;
		cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, 1, colliding);
		Expect(colliding, "after an entering run, the bodies count as colliding");
		cSharedCallbackModel::RunCollide(EntityCollide(false, false), 1, -1, colliding);
		Expect(!colliding, "a leaving state moves the edge even when the filter skips the callback");
	}

	void AnEntityCollideTheLocalCheckAlreadyFiredDoesNotRunAgain()
	{
		bool colliding = true;
		Expect(!cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, 1, colliding) && colliding,
			"an entering run for bodies already colliding here does not run the callback twice");
		colliding = false;
		Expect(!cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, -1, colliding) && !colliding,
			"a leaving run for bodies not colliding here does not run the callback twice");
		colliding = true;
		Expect(cSharedCallbackModel::RunCollide(EntityCollide(false, false), 0, -1, colliding),
			"a leaving run for bodies colliding here runs");
	}

	void APlayerCollideRunLeavesTheLocalPlayersEdgeAlone()
	{
		bool colliding = false;
		cSharedCallbackModel::RunCollide(PlayerCollide(), 0, 1, colliding);
		Expect(!colliding, "another player entering does not put the local player in the area");
		colliding = true;
		cSharedCallbackModel::RunCollide(PlayerCollide(), 0, -1, colliding);
		Expect(colliding, "another player leaving does not take the local player out of the area");
	}
}

int main()
{
	LocalTriggersAreReportedOutsideASharedCallbackRun();
	ARunAndEverythingNestedInItIsNeverReported();
	ACollideRunHonoursTheStatesFilter();
	AnEntityCollideRunKeepsTheEdgeSoTheLocalCheckDoesNotFireAgain();
	AnEntityCollideTheLocalCheckAlreadyFiredDoesNotRunAgain();
	APlayerCollideRunLeavesTheLocalPlayersEdgeAlone();
	std::cout << "Shared Callback model cases passed\n";
	return 0;
}
