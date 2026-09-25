#include "AvatarClipModel.h"

#include <cmath>
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

	bool Near(float afActual, float afExpected)
	{
		return std::fabs(afActual - afExpected) < 0.001f;
	}
}

int main()
{
	{
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.0f, 0.0f);
		Expect(!choice.mbWalking, "stays idle at rest");
		Expect(!choice.mbChanged, "does not report a change while already idle");
	}

	{
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.3f, 0.3f);
		Expect(choice.mbWalking, "crosses to walk above the leave-idle threshold");
		Expect(choice.mbChanged, "reports the idle-to-walk crossing");

		choice = model.Update(0.3f, 0.3f);
		Expect(choice.mbWalking, "stays walking at the same speed");
		Expect(!choice.mbChanged, "does not report a change on a later update with the same choice");
	}

	{
		// Hysteresis: once walking, a speed between the two thresholds must not flicker back to idle.
		cAvatarClipModel model;
		model.Update(0.3f, 0.3f);
		cAvatarClipChoice choice = model.Update(0.2f, 0.2f);
		Expect(choice.mbWalking, "does not flicker to idle between the walk and idle thresholds");
		Expect(!choice.mbChanged, "no change is reported while hysteresis holds the walking choice");

		choice = model.Update(0.1f, 0.1f);
		Expect(!choice.mbWalking, "drops to idle once below the return-to-idle threshold");
		Expect(choice.mbChanged, "reports the walk-to-idle crossing");
	}

	{
		// Pure sideways travel (no forward component) still counts as walking from its total speed.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.5f, 0.0f);
		Expect(choice.mbWalking, "pure sideways speed above the threshold selects walk");
		Expect(choice.mfPlaybackSpeed > 0.0f, "a non-negative forward component plays walk forward");
	}

	{
		// A negative forward component plays walk backward even while sideways travel dominates the total.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(1.0f, -0.1f);
		Expect(choice.mbWalking, "fast sideways-plus-backward travel selects walk");
		Expect(choice.mfPlaybackSpeed < 0.0f,
			"any negative forward component plays walk backward, even under mostly sideways travel");
	}

	{
		// At normal playback speed walk represents kWalkReferenceSpeedMps (2.5 m/s); playback scales
		// by total horizontal speed divided by that reference.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(2.5f, 2.5f);
		Expect(Near(choice.mfPlaybackSpeed, 1.0f), "walk plays at 1x for the reference forward speed");

		choice = model.Update(5.0f, 5.0f);
		Expect(Near(choice.mfPlaybackSpeed, 2.0f), "playback scales linearly with total horizontal speed");

		choice = model.Update(5.0f, -5.0f);
		Expect(Near(choice.mfPlaybackSpeed, -2.0f), "playback scales the same way in reverse for backward travel");
	}

	std::cout << "Avatar clip model cases passed\n";
	return 0;
}
