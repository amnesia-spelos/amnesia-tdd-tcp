#ifndef AVATAR_LANTERN_MODEL_H
#define AVATAR_LANTERN_MODEL_H

// How bright an Avatar's lantern light is, from 0 (dark) to 1 (the local lantern's full colour).
// It fades at the local lantern's rates, but never dims with oil, since oil is not part of the Pose.
class cAvatarLanternModel
{
public:
	static const float kFadeInPerSecond;
	static const float kFadeOutPerSecond;

	cAvatarLanternModel();

	// Advances the fade by a time step in seconds and returns the brightness. A Dormant Avatar is
	// dark at once, so it fades in from dark when it wakes.
	float Update(bool abAwake, bool abLanternRaised, float afTimeStep);

private:
	float mfBrightness;
};

#endif // AVATAR_LANTERN_MODEL_H
