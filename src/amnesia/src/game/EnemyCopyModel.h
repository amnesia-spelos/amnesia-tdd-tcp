#ifndef ENEMY_COPY_MODEL_H
#define ENEMY_COPY_MODEL_H
#include "AvatarPoseModel.h"
#include "GameInteractionGateway.h"

// One driven enemy's animation and motion on the same playback clock as an Avatar.
// The owning map entity discards this model when its Session or Map Visit ends.
class cEnemyCopyModel
{
public:
	void AddPose(const cGameInteractionEnemyPose& aPose, double afLocalTimeMs);
	bool Sample(double afLocalTimeMs, const std::string& asMap, cGameInteractionEnemyPose& aPose);
	// The last pose received for this map, undelayed, which a released enemy resumes from.
	bool NewestPose(const std::string& asMap, cGameInteractionEnemyPose& aPose) const;
private:
	cAvatarPoseModel mMotion;
	std::deque<cGameInteractionEnemyPose> mvSamples;
};

enum eEnemyCopyPoseAdmission
{
	eEnemyCopyPoseAdmission_Show,
	// Sampled no later than the copy's last disappearance, so it cannot bring the copy back.
	eEnemyCopyPoseAdmission_Ignore,
	// Sampled after the disappearance: the simulating game activated the enemy again.
	eEnemyCopyPoseAdmission_Reactivate
};

// Whether an enemy another game simulates has disappeared, in that game's time, the clock its poses
// carry. The map entity keeps it across driving, release, and driving again, so none revives it.
class cEnemyCopyLifecycle
{
public:
	cEnemyCopyLifecycle() : mbDisappeared(false), mbHasDisappearance(false), mlDisappearedMs(0) {}
	// A delivered disappearance. True when the copy should play it now: it is active and this is not
	// the last disappearance again or an older one.
	bool Disappear(unsigned long long alTimeMs, bool abActive)
	{
		if (mbHasDisappearance && alTimeMs <= mlDisappearedMs) return false;
		mbHasDisappearance = mbDisappeared = true;
		mlDisappearedMs = alTimeMs;
		return abActive;
	}
	eEnemyCopyPoseAdmission AdmitPose(unsigned long long alTimeMs, bool abActive)
	{
		if (mbHasDisappearance && alTimeMs <= mlDisappearedMs) return eEnemyCopyPoseAdmission_Ignore;
		const bool bReactivates = mbDisappeared && !abActive;
		mbDisappeared = false;
		return bReactivates ? eEnemyCopyPoseAdmission_Reactivate : eEnemyCopyPoseAdmission_Show;
	}
	bool ActivatesOnDrive() const { return !mbDisappeared; }
	// Something other than its poses, such as a Map Script, activated the copy.
	void Activated() { mbDisappeared = false; }
private:
	bool mbDisappeared;
	bool mbHasDisappearance;
	unsigned long long mlDisappearedMs;
};
#endif
