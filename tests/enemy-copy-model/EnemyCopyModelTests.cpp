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
	std::cout << "Enemy copy model cases passed\n";
}
