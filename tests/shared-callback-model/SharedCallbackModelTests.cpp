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

	void LocalEntityStateChangesAreReportedOnlyOnceAndNeverDuringReplay()
	{
		cSharedCallbackModel model;
		Expect(model.ReportsEntityState(false, true), "a local player changing state produces a report");
		Expect(!model.ReportsEntityState(true, true), "a state already applied produces no report");
		model.BeginRun();
		Expect(!model.ReportsEntityState(false, true), "a state change nested in replay produces no report");
		model.EndRun();
		Expect(model.ReportsEntityState(true, false), "a local button switched off after replay is reported");
	}

	void OnlyALocalPickupOfANonConsumableIsReported()
	{
		cSharedCallbackModel model;
		Expect(model.ReportsPickup(false), "picking up a puzzle item, note, or diary is reported");
		Expect(!model.ReportsPickup(true), "picking up a tinderbox, oil, a sanity potion, or laudanum is not reported");
		model.BeginRun();
		Expect(!model.ReportsPickup(false), "a pickup a Peer's Command ran is not reported");
		model.EndRun();
	}

	void OnlyALocalCombineOfNotTwoConsumablesIsReported()
	{
		cSharedCallbackModel model;
		Expect(model.ReportsCombine(false, false), "combining two puzzle items is reported");
		Expect(model.ReportsCombine(true, false) && model.ReportsCombine(false, true),
			"combining a consumable with a puzzle item is reported");
		Expect(!model.ReportsCombine(true, true), "combining two consumables is not reported");
		model.BeginRun();
		Expect(!model.ReportsCombine(false, false), "a combine a Peer's Command ran is not reported");
		model.EndRun();
	}

	void OnlyTheHoldersGameDecidesAndReportsAStickyAttach()
	{
		cSharedCallbackModel model;
		Expect(model.ReportsStickyAttach(true), "a held or Settling entity attaching to a sticky area is reported");
		Expect(!model.ReportsStickyAttach(false), "an entity nobody here Holds attaching is not reported");
		Expect(cSharedCallbackModel::ChecksStickyAttach(false), "a sticky area checks a local entity's bodies");
		Expect(!cSharedCallbackModel::ChecksStickyAttach(true),
			"a sticky area neither attaches nor runs its AttachFunction for a Peer-Driven Entity");
		model.BeginRun();
		Expect(!model.ReportsStickyAttach(true), "an attach a Peer's Command ran is not reported");
		model.EndRun();
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

	void ASharedEnemysTriggersAreReportedByTheGameSimulatingIt()
	{
		cSharedCallbackModel model;
		cSharedCollide collide;
		collide.mbParentSharedEnemy = true;
		Expect(model.ReportsCollide(collide), "a collide whose parent is a Shared Enemy simulated here is reported");
		collide.mbParentSharedEnemy = false;
		collide.mbChildSharedEnemy = true;
		Expect(model.ReportsCollide(collide), "a collide whose child is a Shared Enemy simulated here is reported");
		Expect(model.ReportsAutoDisabled(true), "a Shared Enemy simulated here disabling itself is reported");
		Expect(!model.ReportsAutoDisabled(false), "an enemy that is not shared disabling itself is not reported");
		model.BeginRun();
		Expect(!model.ReportsCollide(collide) && !model.ReportsAutoDisabled(true),
			"an enemy trigger nested in a Peer's Command is not reported");
		model.EndRun();
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

	void PeerDrivenCollidesStaySilentAndKeepTheirCurrentEdge()
	{
		cSharedCollide collide;
		collide.mbChildPeerDriven = true;
		bool colliding = false;
		bool callbackColliding = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"a Peer-Driven child entering runs no callback");
		collide.mbChildPeerDriven = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"ending driving while still inside does not fire an entering callback");
		Expect(cSharedCallbackModel::CheckLocalCollide(collide, 0, false, colliding, callbackColliding),
			"a later local exit runs the callback");
		collide.mbParentPeerDriven = true;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"a Peer-Driven parent entering runs no callback");
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, false, colliding, callbackColliding),
			"a Peer-Driven parent leaving runs no callback");
		collide.mbParentPeerDriven = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, false, colliding, callbackColliding),
			"ending driving outside does not fire a leaving callback");
		collide.mbParentHeld = true;
		Expect(cSharedCallbackModel::CheckLocalCollide(collide, 1, true, colliding, callbackColliding),
			"the Holder's game still runs its entering callback");
	}

	void ASilentDrivenEdgeDoesNotConsumeTheSharedCallbackReplay()
	{
		cSharedCollide collide;
		collide.mbChildPeerDriven = true;
		bool colliding = false;
		bool callbackColliding = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"driven motion reaches the area silently before the Command arrives");
		Expect(cSharedCallbackModel::RunCollide(collide, 0, 1, callbackColliding),
			"the arriving Shared Callback still runs its registration");
		Expect(!cSharedCallbackModel::RunCollide(collide, 0, 1, callbackColliding),
			"a duplicate entering Command does not run it twice");
		cSharedCallbackModel::RunCollide(collide, 0, -1, callbackColliding);
		Expect(cSharedCallbackModel::RunCollide(collide, 0, 1, callbackColliding),
			"a later entry Command may arrive before the body's sample");
		cSharedCallbackModel::CheckLocalCollide(collide, 0, false, colliding, callbackColliding);
		cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding);
		Expect(callbackColliding, "silent samples do not undo the replayed callback state");
		collide.mbChildPeerDriven = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"release inside still does not run a spurious entry");
	}

	void AReplayAheadOfDrivenMotionDoesNotInventAnEntryOnRelease()
	{
		cSharedCollide collide;
		collide.mbChildPeerDriven = true;
		bool colliding = false;
		bool callbackColliding = false;
		cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding);
		cSharedCallbackModel::RunCollide(collide, 0, 1, callbackColliding, &colliding);
		Expect(cSharedCallbackModel::RunCollide(collide, 0, -1, callbackColliding, &colliding),
			"an exit replay may arrive while the driven body is still physically inside");
		collide.mbChildPeerDriven = false;
		Expect(!cSharedCallbackModel::CheckLocalCollide(collide, 0, true, colliding, callbackColliding),
			"releasing before another driven tick does not invent an entry");
	}
}

int main()
{
	LocalTriggersAreReportedOutsideASharedCallbackRun();
	LocalEntityStateChangesAreReportedOnlyOnceAndNeverDuringReplay();
	OnlyALocalPickupOfANonConsumableIsReported();
	OnlyALocalCombineOfNotTwoConsumablesIsReported();
	OnlyTheHoldersGameDecidesAndReportsAStickyAttach();
	ARunAndEverythingNestedInItIsNeverReported();
	ASharedEnemysTriggersAreReportedByTheGameSimulatingIt();
	ACollideRunHonoursTheStatesFilter();
	AnEntityCollideRunKeepsTheEdgeSoTheLocalCheckDoesNotFireAgain();
	AnEntityCollideTheLocalCheckAlreadyFiredDoesNotRunAgain();
	APlayerCollideRunLeavesTheLocalPlayersEdgeAlone();
	PeerDrivenCollidesStaySilentAndKeepTheirCurrentEdge();
	ASilentDrivenEdgeDoesNotConsumeTheSharedCallbackReplay();
	AReplayAheadOfDrivenMotionDoesNotInventAnEntryOnRelease();
	std::cout << "Shared Callback model cases passed\n";
	return 0;
}
