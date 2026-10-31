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

enum eEnemyCopySplash
{
	eEnemyCopySplash_None,
	eEnemyCopySplash_Walk,
	eEnemyCopySplash_Run
};

namespace EnemyCopySplash
{
	const float kEpsilon = 0.0001f;
	// Slower than this, the copy is still; the slowest Kaernk walk is 1.6 m/s.
	const float kStoppedSpeed = 0.25f;
	const float kStartDelay = 0.1f;
	const float kWalkInterval = 1.0f;
	const float kRunInterval = 0.6f;
	const float kEatPause = 1.3f;
	// Faster than this, the copy snapped to a teleport or its first pose; the fastest Kaernk run is 4 m/s.
	const float kTeleportSpeed = 20.0f;
}

// When a driven Kaernk copy splashes as it moves. The Kaernk has no visible body and no animations, so its
// poses carry no clip: the copy splashes as its horizontal speed implies, on the simulating Kaernk's
// cadence. That Kaernk splashes 0.1 s after it starts moving, then every second walking or every 0.6 s
// running, and not as it moves while eating.
class cEnemyCopySplashModel
{
public:
	// afRunSpeed is the slowest horizontal speed that splashes as running.
	explicit cEnemyCopySplashModel(float afRunSpeed)
		: mfRunSpeed(afRunSpeed), mfCountdown(0), mfEatPause(0), mbMoving(false) {}
	eEnemyCopySplash Update(float afTimeStep, float afSpeed)
	{
		using namespace EnemyCopySplash;
		if (mfEatPause > kEpsilon)
		{
			mfEatPause -= afTimeStep;
			mbMoving = false;
			return eEnemyCopySplash_None;
		}
		if (afSpeed > kTeleportSpeed) return eEnemyCopySplash_None;
		if (afSpeed < kStoppedSpeed)
		{
			mbMoving = false;
			return eEnemyCopySplash_None;
		}
		const bool bRun = afSpeed >= mfRunSpeed;
		const float fInterval = bRun ? kRunInterval : kWalkInterval;
		if (!mbMoving) mfCountdown = kStartDelay;
		else if (mfCountdown > fInterval) mfCountdown = fInterval;
		mbMoving = true;
		mfCountdown -= afTimeStep;
		if (mfCountdown > kEpsilon) return eEnemyCopySplash_None;
		mfCountdown = fInterval;
		return bRun ? eEnemyCopySplash_Run : eEnemyCopySplash_Walk;
	}
	// The simulating Kaernk took a bite. It bites every 1.2 s while eating, so the pause outlasts the next.
	void Ate() { mfEatPause = EnemyCopySplash::kEatPause; }
private:
	float mfRunSpeed;
	float mfCountdown;
	float mfEatPause;
	bool mbMoving;
};
#endif
