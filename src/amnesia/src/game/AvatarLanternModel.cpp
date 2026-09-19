#include "AvatarLanternModel.h"

// As cLuxPlayerLantern::Update fades its light.
const float cAvatarLanternModel::kFadeInPerSecond = 1.0f;
const float cAvatarLanternModel::kFadeOutPerSecond = 2.0f;

cAvatarLanternModel::cAvatarLanternModel() : mfBrightness(0.0f)
{
}

float cAvatarLanternModel::Update(bool abAwake, bool abLanternRaised, float afTimeStep)
{
	if (!abAwake) mfBrightness = 0.0f;
	else if (abLanternRaised)
	{
		mfBrightness += kFadeInPerSecond * afTimeStep;
		if (mfBrightness > 1.0f) mfBrightness = 1.0f;
	}
	else
	{
		mfBrightness -= kFadeOutPerSecond * afTimeStep;
		if (mfBrightness < 0.0f) mfBrightness = 0.0f;
	}
	return mfBrightness;
}
