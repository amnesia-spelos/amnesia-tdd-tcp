#include "EnemyCopyModel.h"

void cEnemyCopyModel::AddPose(const cGameInteractionEnemyPose& aPose, double afLocalTimeMs)
{
	const cGameInteractionPose& pose = aPose.mPose;
	if (!mvSamples.empty())
	{
		const cGameInteractionPose& newest = mvSamples.back().mPose;
		if (newest.msMapFile != pose.msMapFile ||
			static_cast<double>(newest.mlTimeMs) - static_cast<double>(pose.mlTimeMs) > cSenderClock::kResyncMs)
			mvSamples.clear();
		else if (pose.mlTimeMs <= newest.mlTimeMs) return;
	}
	cAvatarPoseSample motion;
	motion.mfSenderTimeMs = static_cast<double>(pose.mlTimeMs);
	motion.mlTeleportCounter = pose.mlTeleportCounter;
	motion.msMapFile = pose.msMapFile;
	motion.mfX = pose.mFeetPosition.mfX; motion.mfY = pose.mFeetPosition.mfY; motion.mfZ = pose.mFeetPosition.mfZ;
	motion.mfYawDegrees = pose.mfBodyYawDegrees;
	mMotion.AddPose(motion, afLocalTimeMs);
	mvSamples.push_back(aPose);
	if (mvSamples.size() > 256) mvSamples.pop_front();
}

bool cEnemyCopyModel::Sample(double afLocalTimeMs, const std::string& asMap, cGameInteractionEnemyPose& aPose)
{
	cAvatarRenderedPose motion;
	if (!mMotion.Sample(afLocalTimeMs, asMap, motion)) { mvSamples.clear(); return false; }
	while (mvSamples.size() > 1 && static_cast<double>(mvSamples[1].mPose.mlTimeMs) <= motion.mfSenderTimeMs)
		mvSamples.pop_front();
	aPose = mvSamples.front();
	aPose.mPose.mFeetPosition = cGameInteractionPosition(motion.mfX, motion.mfY, motion.mfZ);
	aPose.mPose.mfBodyYawDegrees = motion.mfYawDegrees;
	if (mvSamples.size() > 1)
	{
		const cGameInteractionEnemyPose& next = mvSamples[1];
		// A changed clip, restart or loop wrap is discrete; switch on its sample's time.
		if (aPose.msClip == next.msClip && aPose.mbLoop == next.mbLoop && next.mfAnimationTime >= aPose.mfAnimationTime)
		{
			const double from = static_cast<double>(aPose.mPose.mlTimeMs);
			const float fraction = static_cast<float>((motion.mfSenderTimeMs - from) /
				(static_cast<double>(next.mPose.mlTimeMs) - from));
			aPose.mfAnimationTime += (next.mfAnimationTime - aPose.mfAnimationTime) * fraction;
		}
	}
	return true;
}

bool cEnemyCopyModel::NewestPose(const std::string& asMap, cGameInteractionEnemyPose& aPose) const
{
	if (mvSamples.empty() || mvSamples.back().mPose.msMapFile != asMap) return false;
	aPose = mvSamples.back();
	return true;
}
