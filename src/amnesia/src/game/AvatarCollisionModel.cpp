#include "AvatarCollisionModel.h"

#include <algorithm>
#include <cmath>

const float cAvatarCollisionModel::kOverlapToleranceMeters = 0.05f;
const float cAvatarCollisionModel::kClearanceMeters = 0.05f;

float GetAvatarCollisionGap(const cAvatarCollisionCylinder& aA, const cAvatarCollisionCylinder& aB)
{
	const float fDeltaX = aB.mfFeetX - aA.mfFeetX;
	const float fDeltaZ = aB.mfFeetZ - aA.mfFeetZ;
	const float fAcrossGap = std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ) - (aA.mfRadius + aB.mfRadius);
	const float fHeightGap = std::max(aB.mfFeetY - (aA.mfFeetY + aA.mfHeight), aA.mfFeetY - (aB.mfFeetY + aB.mfHeight));
	return std::max(fAcrossGap, fHeightGap);
}

cAvatarCollisionModel::cAvatarCollisionModel() : mbEnabled(true), mbPassable(true)
{
}

void cAvatarCollisionModel::SetEnabled(bool abEnabled)
{
	mbEnabled = abEnabled;
}

bool cAvatarCollisionModel::Update(bool abAwake, float afGapToPlayer)
{
	if (!abAwake || !mbEnabled || afGapToPlayer < -kOverlapToleranceMeters) mbPassable = true;
	else if (afGapToPlayer >= kClearanceMeters) mbPassable = false;
	return !mbPassable;
}
