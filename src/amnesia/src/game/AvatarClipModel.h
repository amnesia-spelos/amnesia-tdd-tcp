#ifndef AVATAR_CLIP_MODEL_H
#define AVATAR_CLIP_MODEL_H

// What a standing Avatar should be playing, chosen from its rendered horizontal motion (issue #50).
struct cAvatarClipChoice
{
	cAvatarClipChoice() : mbWalking(false), mbChanged(false), mfPlaybackSpeed(1.0f) {}
	// True selects the walk clip; false selects idle.
	bool mbWalking;
	// True only on the update where the choice crosses from idle to walk or back, so the handler
	// starts a cross-fade instead of doing so every frame.
	bool mbChanged;
	// The multiplier for the walk clip's authored (1x = kWalkReferenceSpeedMps) playback speed.
	// Negative plays it backward for backward travel (#48, ADR-independent engine behavior). Not
	// meaningful while idle.
	float mfPlaybackSpeed;
};

// Chooses idle or walk from total horizontal speed, with hysteresis so the choice does not flicker
// near the threshold, and reports the walk clip's playback speed. Engine-independent (ADR 0003);
// cLuxAvatarHandler applies the choice to the mesh's animation states.
class cAvatarClipModel
{
public:
	// Leaves idle for walk above this total horizontal speed.
	static const float kWalkThresholdMps;
	// Returns to idle below this total horizontal speed. Lower than kWalkThresholdMps so the choice
	// does not flicker right at the boundary.
	static const float kIdleThresholdMps;
	// The total horizontal speed the authored walk clip represents at 1x playback.
	static const float kWalkReferenceSpeedMps;

	cAvatarClipModel();

	// afHorizontalSpeedMps: total horizontal speed from the two rendered Poses being interpolated,
	// including pure sideways travel. afForwardSpeedMps: the signed forward component of that same
	// motion; only its sign decides playback direction, even when sideways travel dominates the total.
	cAvatarClipChoice Update(float afHorizontalSpeedMps, float afForwardSpeedMps);

private:
	bool mbWalking;
};

#endif // AVATAR_CLIP_MODEL_H
