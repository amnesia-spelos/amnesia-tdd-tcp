#include "SharedCallbackModel.h"

bool cSharedCallbackModel::CheckLocalCollide(const cSharedCollide& aCollide, int alStatesFilter,
	bool abNowColliding, bool& abColliding, bool& abCallbackColliding)
{
	const bool bChanged = abColliding != abNowColliding;
	abColliding = abNowColliding;
	if (aCollide.mbParentPeerDriven || aCollide.mbChildPeerDriven || !bChanged) return false;
	abCallbackColliding = abNowColliding;
	return alStatesFilter == 0 || alStatesFilter == (abNowColliding ? 1 : -1);
}

bool cSharedCallbackModel::ReportsCollide(const cSharedCollide& aCollide) const
{
	if (!ReportsCallback()) return false;
	return aCollide.mbParentIsPlayer || aCollide.mbParentHeld || aCollide.mbChildHeld ||
		aCollide.mbParentSharedEnemy || aCollide.mbChildSharedEnemy;
}

bool cSharedCallbackModel::RunCollide(const cSharedCollide& aCollide, int alStatesFilter, int alState,
	bool& abCallbackColliding, bool* apSampledColliding)
{
	if (!aCollide.mbParentIsPlayer)
	{
		// The local check already fired this edge, so running it again would run the callback twice.
		if (abCallbackColliding == (alState > 0)) return false;
		abCallbackColliding = alState > 0;
		if (apSampledColliding && !aCollide.mbParentPeerDriven && !aCollide.mbChildPeerDriven)
			*apSampledColliding = abCallbackColliding;
	}
	return alStatesFilter == 0 || alStatesFilter == alState;
}
