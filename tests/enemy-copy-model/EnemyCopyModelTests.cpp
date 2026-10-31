#include "EnemyCopyModel.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
void Expect(bool value, const char* message) { if (!value) { std::cerr << message << "\n"; exit(1); } }
int main()
{
	cEnemyCopyModel model;
	cGameInteractionEnemyPose sample, rendered;
	sample.mPose.msMapFile = "map";
	sample.mPose.mlTimeMs = 1000;
	sample.msClip = "Walk"; sample.mfAnimationTime = 0.2f;
	model.AddPose(sample, 5000);
	sample.mPose.mlTimeMs = 1100; sample.mPose.mFeetPosition.mfX = 2;
	sample.mfAnimationTime = 0.4f;
	model.AddPose(sample, 5100);
	Expect(model.Sample(5150, "map", rendered), "copy has a sample");
	Expect(std::fabs(rendered.mPose.mFeetPosition.mfX - 1) < 0.001f, "copy shares Avatar's 100 ms delay");
	Expect(std::fabs(rendered.mfAnimationTime - 0.3f) < 0.001f, "clip time follows rendered motion");
	Expect(model.Sample(6000, "map", rendered) && rendered.mPose.mFeetPosition.mfX == 2 && rendered.mfAnimationTime == 0.4f,
		"stalled stream holds pose and animation");
	// Clip changes occur at the delayed sample, never when the packet arrives.
	cEnemyCopyModel transition;
	sample.mPose.mlTimeMs = 1000; sample.mPose.mFeetPosition.mfX = 0; sample.msClip = "Walk";
	transition.AddPose(sample, 5000);
	sample.mPose.mlTimeMs = 1100; sample.mPose.mFeetPosition.mfX = 2; sample.msClip = "Attack"; sample.mfAnimationTime = 0;
	transition.AddPose(sample, 5100);
	Expect(transition.Sample(5150, "map", rendered) && rendered.msClip == "Walk", "future clip is not played early");
	Expect(transition.Sample(5200, "map", rendered) && rendered.msClip == "Attack", "clip changes on playback time");
	sample.mPose.mlTimeMs = 1200; sample.mPose.mlTeleportCounter = 1; sample.mPose.mFeetPosition.mfX = 3;
	transition.AddPose(sample, 5200);
	Expect(transition.Sample(5250, "map", rendered) && rendered.mPose.mFeetPosition.mfX == 2, "teleport holds until due");
	Expect(transition.Sample(5300, "map", rendered) && rendered.mPose.mFeetPosition.mfX == 3, "teleport snaps");
	Expect(!transition.Sample(5350, "other map", rendered), "another Map Visit cannot use the old samples");
	Expect(!transition.Sample(5400, "map", rendered), "returning does not resurrect old samples");
	// A release continues from the newest driven pose, not the delayed one on show.
	cEnemyCopyModel release;
	Expect(!release.NewestPose("map", rendered), "an undriven copy has no newest pose");
	sample.mPose.mlTimeMs = 1000; sample.mPose.mlTeleportCounter = 0; sample.mPose.mFeetPosition.mfX = 0;
	sample.mPose.mfBodyYawDegrees = 0; sample.msClip = "Walk";
	release.AddPose(sample, 5000);
	sample.mPose.mlTimeMs = 1100; sample.mPose.mFeetPosition.mfX = 2; sample.mPose.mfBodyYawDegrees = 90;
	sample.msClip = "Run";
	release.AddPose(sample, 5100);
	Expect(release.Sample(5150, "map", rendered) && rendered.mPose.mFeetPosition.mfX < 2, "shown pose lags");
	Expect(release.NewestPose("map", rendered) && rendered.mPose.mFeetPosition.mfX == 2 &&
		rendered.mPose.mfBodyYawDegrees == 90 && rendered.msClip == "Run", "newest pose is the last driven one");
	Expect(!release.NewestPose("other map", rendered), "another map's pose is not resumed");
	// A disappearance the simulating game reports plays once; only its later samples bring the copy back.
	cEnemyCopyLifecycle lifecycle;
	Expect(lifecycle.ActivatesOnDrive(), "a copy that never disappeared activates when driven");
	Expect(lifecycle.AdmitPose(900, true) == eEnemyCopyPoseAdmission_Show, "poses show before any disappearance");
	Expect(lifecycle.Disappear(1000, true), "an active copy plays the delivered disappearance");
	Expect(!lifecycle.Disappear(1000, false), "a duplicate disappearance plays nothing");
	Expect(!lifecycle.Disappear(800, false), "an older disappearance plays nothing");
	Expect(lifecycle.AdmitPose(1000, false) == eEnemyCopyPoseAdmission_Ignore, "a pose from the disappearing update is late");
	Expect(lifecycle.AdmitPose(950, false) == eEnemyCopyPoseAdmission_Ignore, "a late pose cannot resurrect the copy");
	Expect(!lifecycle.ActivatesOnDrive(), "driving again keeps a disappeared copy inactive");
	Expect(lifecycle.AdmitPose(1050, false) == eEnemyCopyPoseAdmission_Reactivate,
		"a pose sampled after the disappearance reactivates the copy");
	Expect(lifecycle.ActivatesOnDrive(), "a reactivated copy activates when driven again");
	Expect(lifecycle.AdmitPose(1100, true) == eEnemyCopyPoseAdmission_Show, "the reactivated copy shows its poses");
	Expect(!lifecycle.Disappear(1000, true), "a stale duplicate after reactivation plays nothing");
	Expect(lifecycle.AdmitPose(990, true) == eEnemyCopyPoseAdmission_Ignore, "a pose older than the last disappearance stays late");
	Expect(lifecycle.Disappear(2000, true), "a later disappearance plays again");
	cEnemyCopyLifecycle inactive;
	Expect(!inactive.Disappear(1000, false), "an inactive copy plays nothing");
	Expect(!inactive.ActivatesOnDrive(), "but still stays inactive when driven");
	inactive.Activated();
	Expect(inactive.ActivatesOnDrive(), "a Map Script activating the copy ends the disappearance");
	// A Kaernk copy splashes as its movement implies: walk splashes every second, run splashes every 0.6 s.
	cEnemyCopySplashModel splashes(2.8f);
	Expect(splashes.Update(1.0f, 0) == eEnemyCopySplash_None, "a still copy never splashes");
	Expect(splashes.Update(0.05f, 1.6f) == eEnemyCopySplash_None, "a copy starting to move splashes shortly after");
	Expect(splashes.Update(0.05f, 1.6f) == eEnemyCopySplash_Walk, "the first walk splash comes 0.1 s after it starts");
	Expect(splashes.Update(0.9f, 1.6f) == eEnemyCopySplash_None, "walk splashes wait a second");
	Expect(splashes.Update(0.1f, 1.6f) == eEnemyCopySplash_Walk, "the next walk splash comes a second later");
	Expect(splashes.Update(0.5f, 4.0f) == eEnemyCopySplash_None, "speeding up keeps no more than the run interval");
	Expect(splashes.Update(0.1f, 4.0f) == eEnemyCopySplash_Run, "a fast copy splashes as running");
	Expect(splashes.Update(0.5f, 4.0f) == eEnemyCopySplash_None, "run splashes wait 0.6 s");
	Expect(splashes.Update(0.1f, 4.0f) == eEnemyCopySplash_Run, "the next run splash comes 0.6 s later");
	Expect(splashes.Update(0.3f, 0.1f) == eEnemyCopySplash_None, "a copy that stops stops splashing");
	Expect(splashes.Update(0.1f, 4.0f) == eEnemyCopySplash_Run, "moving again splashes 0.1 s after it starts");
	Expect(splashes.Update(0.6f, 500.0f) == eEnemyCopySplash_None, "a copy snapping to a teleport does not splash");
	// The simulating Kaernk does not splash as it moves while it eats, and bites every 1.2 s.
	splashes.Ate();
	Expect(splashes.Update(1.2f, 1.6f) == eEnemyCopySplash_None, "a copy that just ate does not splash as it moves");
	Expect(splashes.Update(0.1f, 1.6f) == eEnemyCopySplash_None, "the pause outlasts the next bite");
	Expect(splashes.Update(0.1f, 1.6f) == eEnemyCopySplash_Walk, "after eating, moving splashes 0.1 s after the pause");
	std::cout << "Enemy copy model cases passed\n";
}
