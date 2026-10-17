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
private:
	cAvatarPoseModel mMotion;
	std::deque<cGameInteractionEnemyPose> mvSamples;
};
#endif
