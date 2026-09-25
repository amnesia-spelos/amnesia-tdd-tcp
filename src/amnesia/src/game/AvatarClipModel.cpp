#include "AvatarClipModel.h"

const float cAvatarClipModel::kWalkThresholdMps = 0.25f;
const float cAvatarClipModel::kIdleThresholdMps = 0.15f;
const float cAvatarClipModel::kWalkReferenceSpeedMps = 2.5f;
const float cAvatarClipModel::kCrouchWalkReferenceSpeedMps = 1.625f;

cAvatarClipModel::cAvatarClipModel() : mbWalking(false), mbCrouching(false)
{
}

cAvatarClipChoice cAvatarClipModel::Update(float afHorizontalSpeedMps, float afForwardSpeedMps, bool abCrouching)
{
	const bool bWasWalking = mbWalking;
	const bool bWasCrouching = mbCrouching;
	if (mbWalking)
	{
		if (afHorizontalSpeedMps < kIdleThresholdMps) mbWalking = false;
	}
	else
	{
		if (afHorizontalSpeedMps > kWalkThresholdMps) mbWalking = true;
	}
	mbCrouching = abCrouching;

	cAvatarClipChoice choice;
	if (mbCrouching) choice.mClip = mbWalking ? eAvatarClip_CrouchWalk : eAvatarClip_CrouchIdle;
	else choice.mClip = mbWalking ? eAvatarClip_Walk : eAvatarClip_Idle;
	choice.mbChanged = (mbWalking != bWasWalking) || (mbCrouching != bWasCrouching);

	// Cadence scales with total horizontal speed so sideways travel moves the legs at the right rate
	// too; only the forward component's sign decides play direction (#48).
	const float fSign = afForwardSpeedMps < 0.0f ? -1.0f : 1.0f;
	const float fReferenceSpeedMps = mbCrouching ? kCrouchWalkReferenceSpeedMps : kWalkReferenceSpeedMps;
	choice.mfPlaybackSpeed = mbWalking ? fSign * afHorizontalSpeedMps / fReferenceSpeedMps : 1.0f;
	return choice;
}
