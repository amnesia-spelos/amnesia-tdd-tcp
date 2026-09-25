#include "AvatarClipModel.h"

const float cAvatarClipModel::kWalkThresholdMps = 0.25f;
const float cAvatarClipModel::kIdleThresholdMps = 0.15f;
const float cAvatarClipModel::kWalkReferenceSpeedMps = 2.5f;

cAvatarClipModel::cAvatarClipModel() : mbWalking(false)
{
}

cAvatarClipChoice cAvatarClipModel::Update(float afHorizontalSpeedMps, float afForwardSpeedMps)
{
	const bool bWasWalking = mbWalking;
	if (mbWalking)
	{
		if (afHorizontalSpeedMps < kIdleThresholdMps) mbWalking = false;
	}
	else
	{
		if (afHorizontalSpeedMps > kWalkThresholdMps) mbWalking = true;
	}

	cAvatarClipChoice choice;
	choice.mbWalking = mbWalking;
	choice.mbChanged = mbWalking != bWasWalking;
	// Cadence scales with total horizontal speed so sideways travel moves the legs at the right rate
	// too; only the forward component's sign decides play direction (#48).
	const float fSign = afForwardSpeedMps < 0.0f ? -1.0f : 1.0f;
	choice.mfPlaybackSpeed = mbWalking ? fSign * afHorizontalSpeedMps / kWalkReferenceSpeedMps : 1.0f;
	return choice;
}
