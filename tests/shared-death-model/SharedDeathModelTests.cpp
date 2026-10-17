#include "SharedDeathModel.h"
#include <cstdlib>
#include <iostream>
void Expect(bool value, const char* message) { if (!value) { std::cerr << message << "\n"; exit(1); } }

cSharedDeathEpisode Episode(unsigned int episode, int death, int count, const std::string& checkPoint)
{
	cSharedDeathEpisode result;
	result.mlEpisode = episode; result.mlDeath = death; result.mlCount = count; result.msCheckPoint = checkPoint;
	return result;
}

int main()
{
	cSharedDeathModel model;
	const int death = model.BeginDeath(eSharedDeathPolicy_Coordinated);
	Expect(death == 1, "a local death is numbered for its report");
	Expect(model.ApplyEpisode(Episode(1, death, 4, "cp")) == eSharedDeathOutcome_Joined,
		"an episode answering the local death joins it");
	cSharedDeathCheckPoint checkPoint = model.CompleteCheckPoint("cp", true, 2);
	Expect(checkPoint.mlCount == 4 && checkPoint.mbAgreed, "the callback receives the agreed count");
	Expect(checkPoint.mReset == eSharedDeathPolicy_Coordinated, "a shared death resets coordinated");

	cSharedDeathModel killed;
	Expect(killed.ApplyEpisode(Episode(7, 0, 3, "cp")) == eSharedDeathOutcome_Kill, "a living player is killed");
	Expect(killed.BeginDeath(eSharedDeathPolicy_PreserveEnemies) == 0, "a death an episode caused is not reported");
	checkPoint = killed.CompleteCheckPoint("cp", true, 0);
	Expect(checkPoint.mlCount == 3 && checkPoint.mReset == eSharedDeathPolicy_Coordinated,
		"the killed player's checkpoint follows the episode");

	// Delayed and duplicate deliveries never kill again.
	cSharedDeathModel late;
	const int first = late.BeginDeath(eSharedDeathPolicy_Coordinated);
	Expect(late.ApplyEpisode(Episode(2, 0, 1, "cp")) == eSharedDeathOutcome_Joined,
		"a simultaneous death's episode joins the death in progress");
	Expect(late.ApplyEpisode(Episode(2, first, 1, "cp")) == eSharedDeathOutcome_Duplicate,
		"the same episode twice is a duplicate");
	late.CompleteCheckPoint("cp", true, 1);
	Expect(late.ApplyEpisode(Episode(2, 0, 1, "cp")) == eSharedDeathOutcome_Duplicate,
		"a repeated episode after the checkpoint does not kill");
	Expect(late.ApplyEpisode(Episode(1, 0, 1, "cp")) == eSharedDeathOutcome_Duplicate,
		"an older episode does not kill");
	Expect(late.ApplyEpisode(Episode(3, first, 1, "cp")) == eSharedDeathOutcome_Stale,
		"an episode answering a finished death does not kill");
	Expect(late.ApplyEpisode(Episode(4, 9, 1, "cp")) == eSharedDeathOutcome_Stale,
		"an episode answering an unknown death does not kill");
	const int second = late.BeginDeath(eSharedDeathPolicy_Coordinated);
	Expect(late.ApplyEpisode(Episode(5, first, 1, "cp")) == eSharedDeathOutcome_Stale,
		"an episode answering an earlier death does not join the current one");
	Expect(late.ApplyEpisode(Episode(6, second, 2, "cp")) == eSharedDeathOutcome_Joined,
		"the current death is still answered");
	Expect(late.CompleteCheckPoint("cp", true, 2).mlCount == 2, "the answering episode's count applies");

	// A different registration still dies with everyone, with its own count.
	cSharedDeathModel mismatched;
	mismatched.ApplyEpisode(Episode(1, 0, 6, "cp"));
	mismatched.BeginDeath(eSharedDeathPolicy_Coordinated);
	checkPoint = mismatched.CompleteCheckPoint("other", true, 2);
	Expect(checkPoint.mlCount == 2 && !checkPoint.mbAgreed && checkPoint.mReset == eSharedDeathPolicy_Coordinated,
		"a different checkpoint name keeps the local count");
	mismatched.ApplyEpisode(Episode(2, 0, 6, "cp"));
	mismatched.BeginDeath(eSharedDeathPolicy_PreserveEnemies);
	checkPoint = mismatched.CompleteCheckPoint("cp", false, 0);
	Expect(checkPoint.mlCount == 0 && !checkPoint.mbAgreed && checkPoint.mReset == eSharedDeathPolicy_Coordinated,
		"a map without a callback keeps the local count");

	Expect(cSharedDeathModel::SelectPolicy(false, true) == eSharedDeathPolicy_Vanilla, "alone on the map is vanilla");
	Expect(cSharedDeathModel::SelectPolicy(true, false) == eSharedDeathPolicy_PreserveEnemies,
		"no callback preserves Shared Enemies");
	Expect(cSharedDeathModel::SelectPolicy(true, true) == eSharedDeathPolicy_Coordinated,
		"a callback coordinates the death");

	// A coordinated death waits briefly at its checkpoint for the agreed count.
	cSharedDeathModel waiting;
	waiting.BeginDeath(eSharedDeathPolicy_Coordinated);
	Expect(waiting.HoldsCheckPoint(1000) && waiting.HoldsCheckPoint(3999), "the checkpoint waits for its episode");
	Expect(!waiting.HoldsCheckPoint(4000), "the wait is bounded");
	checkPoint = waiting.CompleteCheckPoint("cp", true, 3);
	Expect(checkPoint.mlCount == 3 && !checkPoint.mbAgreed && checkPoint.mReset == eSharedDeathPolicy_Coordinated,
		"an unanswered death falls back to the local count");
	waiting.BeginDeath(eSharedDeathPolicy_Coordinated);
	Expect(waiting.HoldsCheckPoint(9000), "each death waits from its own checkpoint");
	waiting.ApplyEpisode(Episode(1, 2, 4, "cp"));
	Expect(!waiting.HoldsCheckPoint(9001), "an answered death does not wait");

	cSharedDeathModel preserving;
	Expect(!preserving.PreservesEnemies(), "no death preserves nothing");
	preserving.BeginDeath(eSharedDeathPolicy_PreserveEnemies);
	Expect(!preserving.HoldsCheckPoint(0) && preserving.PreservesEnemies(), "a death without a callback does not wait");
	checkPoint = preserving.CompleteCheckPoint("_auto", false, 5);
	Expect(checkPoint.mlCount == 5 && checkPoint.mReset == eSharedDeathPolicy_PreserveEnemies,
		"Shared Enemies survive a death without a callback");
	Expect(!preserving.PreservesEnemies(), "the respawned player's enemies resume");
	preserving.BeginDeath(eSharedDeathPolicy_Vanilla);
	Expect(!preserving.HoldsCheckPoint(0) && preserving.CompleteCheckPoint("cp", true, 1).mReset ==
		eSharedDeathPolicy_Vanilla, "a vanilla death neither waits nor changes the reset");
	Expect(preserving.CompleteCheckPoint("cp", true, 1).mReset == eSharedDeathPolicy_Vanilla,
		"a checkpoint without a death in progress is vanilla");

	// A new Map Visit forgets the death in progress, but never the episodes already applied.
	cSharedDeathModel visit;
	visit.ApplyEpisode(Episode(4, 0, 1, "cp"));
	visit.EndMapVisit();
	Expect(visit.BeginDeath(eSharedDeathPolicy_Coordinated) == 1, "a pending kill does not outlive its Map Visit");
	visit.EndMapVisit();
	Expect(visit.CompleteCheckPoint("cp", true, 1).mReset == eSharedDeathPolicy_Vanilla,
		"a death does not outlive its Map Visit");
	Expect(visit.ApplyEpisode(Episode(4, 0, 1, "cp")) == eSharedDeathOutcome_Duplicate,
		"an episode from the previous Map Visit stays applied");
	std::cout << "Shared death model cases passed\n";
}
