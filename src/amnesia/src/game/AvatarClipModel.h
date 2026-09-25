#ifndef AVATAR_CLIP_MODEL_H
#define AVATAR_CLIP_MODEL_H

// The clip a standing or crouched Avatar should be playing, chosen from its rendered horizontal
// motion and crouch (issues #50, #51).
enum eAvatarClip
{
	eAvatarClip_Idle,
	eAvatarClip_Walk,
	eAvatarClip_CrouchIdle,
	eAvatarClip_CrouchWalk,
	eAvatarClip_LastEnum
};

struct cAvatarClipChoice
{
	cAvatarClipChoice() : mClip(eAvatarClip_Idle), mbChanged(false), mfPlaybackSpeed(1.0f) {}
	eAvatarClip mClip;
	// True only on the update where the choice crosses to a different clip, whether from idle/walk
	// hysteresis or a crouch change, so the handler starts a cross-fade instead of doing so every frame.
	bool mbChanged;
	// The multiplier for a walking clip's authored (1x = its reference speed) playback speed. Negative
	// plays it backward for backward travel (#48, ADR-independent engine behavior). Not meaningful
	// while idle or crouch idle.
	float mfPlaybackSpeed;
};

// Chooses idle, walk, crouch_idle, or crouch_walk from total horizontal speed and crouch, with
// hysteresis on the idle/walk choice so it does not flicker near the threshold, and reports the
// walking clip's playback speed. Engine-independent (ADR 0003); cLuxAvatarHandler applies the choice
// to the mesh's animation states.
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
	// The total horizontal speed the authored crouch_walk clip represents at 1x playback: the
	// installed game's 2.5 m/s normal forward cap times its 0.65 crouch multiplier.
	static const float kCrouchWalkReferenceSpeedMps;

	cAvatarClipModel();

	// afHorizontalSpeedMps: total horizontal speed from the two rendered Poses being interpolated,
	// including pure sideways travel. afForwardSpeedMps: the signed forward component of that same
	// motion; only its sign decides playback direction, even when sideways travel dominates the total.
	// abCrouching: the Pose's rendered crouch flag.
	cAvatarClipChoice Update(float afHorizontalSpeedMps, float afForwardSpeedMps, bool abCrouching);

private:
	bool mbWalking;
	bool mbCrouching;
};

#endif // AVATAR_CLIP_MODEL_H
