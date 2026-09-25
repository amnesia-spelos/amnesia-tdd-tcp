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
		cAvatarClipChoice choice = model.Update(0.0f, 0.0f, false);
		Expect(choice.mClip == eAvatarClip_Idle, "stays idle at rest");
		Expect(!choice.mbChanged, "does not report a change while already idle");
	}

	{
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.3f, 0.3f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "crosses to walk above the leave-idle threshold");
		Expect(choice.mbChanged, "reports the idle-to-walk crossing");

		choice = model.Update(0.3f, 0.3f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "stays walking at the same speed");
		Expect(!choice.mbChanged, "does not report a change on a later update with the same choice");
	}

	{
		// Hysteresis: once walking, a speed between the two thresholds must not flicker back to idle.
		cAvatarClipModel model;
		model.Update(0.3f, 0.3f, false);
		cAvatarClipChoice choice = model.Update(0.2f, 0.2f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "does not flicker to idle between the walk and idle thresholds");
		Expect(!choice.mbChanged, "no change is reported while hysteresis holds the walking choice");

		choice = model.Update(0.1f, 0.1f, false);
		Expect(choice.mClip == eAvatarClip_Idle, "drops to idle once below the return-to-idle threshold");
		Expect(choice.mbChanged, "reports the walk-to-idle crossing");
	}

	{
		// Pure sideways travel (no forward component) still counts as walking from its total speed.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.5f, 0.0f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "pure sideways speed above the threshold selects walk");
		Expect(choice.mfPlaybackSpeed > 0.0f, "a non-negative forward component plays walk forward");
	}

	{
		// A negative forward component plays walk backward even while sideways travel dominates the total.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(1.0f, -0.1f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "fast sideways-plus-backward travel selects walk");
		Expect(choice.mfPlaybackSpeed < 0.0f,
			"any negative forward component plays walk backward, even under mostly sideways travel");
	}

	{
		// At normal playback speed walk represents kWalkReferenceSpeedMps (2.5 m/s); playback scales
		// by total horizontal speed divided by that reference.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(2.5f, 2.5f, false);
		Expect(Near(choice.mfPlaybackSpeed, 1.0f), "walk plays at 1x for the reference forward speed");

		choice = model.Update(5.0f, 5.0f, false);
		Expect(Near(choice.mfPlaybackSpeed, 2.0f), "playback scales linearly with total horizontal speed");

		choice = model.Update(5.0f, -5.0f, false);
		Expect(Near(choice.mfPlaybackSpeed, -2.0f), "playback scales the same way in reverse for backward travel");
	}

	{
		// Crouching at rest selects crouch_idle, and crouching while moving selects crouch_walk,
		// using the same idle/walk hysteresis thresholds as standing (issue #51).
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(0.0f, 0.0f, true);
		Expect(choice.mClip == eAvatarClip_CrouchIdle, "selects crouch idle at rest while crouching");

		choice = model.Update(0.0f, 0.0f, true);
		Expect(!choice.mbChanged, "no change is reported on a later update with the same crouch idle choice");

		choice = model.Update(0.3f, 0.3f, true);
		Expect(choice.mClip == eAvatarClip_CrouchWalk, "crosses to crouch walk above the leave-idle threshold");
		Expect(choice.mbChanged, "reports the crouch idle-to-walk crossing");

		choice = model.Update(0.1f, 0.1f, true);
		Expect(choice.mClip == eAvatarClip_CrouchIdle, "drops to crouch idle once below the return-to-idle threshold");
		Expect(choice.mbChanged, "reports the crouch walk-to-idle crossing");
	}

	{
		// Crouching down or standing up is its own change even while the walking choice is unaffected,
		// so the handler still cross-fades between the standing and crouched clip pair.
		cAvatarClipModel model;
		model.Update(0.0f, 0.0f, false);
		cAvatarClipChoice choice = model.Update(0.0f, 0.0f, true);
		Expect(choice.mClip == eAvatarClip_CrouchIdle, "crouching down while idle switches to crouch idle");
		Expect(choice.mbChanged, "reports the change from crouching alone");

		choice = model.Update(0.3f, 0.3f, true);
		Expect(choice.mClip == eAvatarClip_CrouchWalk, "moving while crouched selects crouch walk");
		Expect(choice.mbChanged, "reports the crouch idle-to-walk crossing");

		choice = model.Update(0.3f, 0.3f, false);
		Expect(choice.mClip == eAvatarClip_Walk, "standing up while walking switches to the standing walk clip");
		Expect(choice.mbChanged, "reports the change from standing up alone");
	}

	{
		// At normal playback speed crouch_walk represents kCrouchWalkReferenceSpeedMps (1.625 m/s),
		// the installed game's 2.5 m/s forward cap times its 0.65 crouch multiplier.
		cAvatarClipModel model;
		cAvatarClipChoice choice = model.Update(1.625f, 1.625f, true);
		Expect(Near(choice.mfPlaybackSpeed, 1.0f), "crouch walk plays at 1x for its reference forward speed");

		choice = model.Update(3.25f, 3.25f, true);
		Expect(Near(choice.mfPlaybackSpeed, 2.0f), "crouch walk playback scales linearly with total horizontal speed");

		choice = model.Update(3.25f, -3.25f, true);
		Expect(Near(choice.mfPlaybackSpeed, -2.0f), "crouch walk playback scales the same way in reverse");
	}

	std::cout << "Avatar clip model cases passed\n";
	return 0;
}
