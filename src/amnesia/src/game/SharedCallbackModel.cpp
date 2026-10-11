#include "SharedCallbackModel.h"

bool cSharedCallbackModel::ReportsCollide(const cSharedCollide& aCollide) const
{
	if (!ReportsCallback()) return false;
	return aCollide.mbParentIsPlayer || aCollide.mbParentHeld || aCollide.mbChildHeld;
}

bool cSharedCallbackModel::RunCollide(const cSharedCollide& aCollide, int alStatesFilter, int alState,
	bool& abColliding)
{
	if (!aCollide.mbParentIsPlayer)
	{
		// The local check already fired this edge, so running it again would run the callback twice.
		if (abColliding == (alState > 0)) return false;
		abColliding = alState > 0;
	}
	return alStatesFilter == 0 || alStatesFilter == alState;
}
