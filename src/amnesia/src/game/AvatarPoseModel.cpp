#include "AvatarPoseModel.h"

#include <cmath>

namespace
{
	// A Pose whose clock disagrees with the alignment by more than this starts a new alignment.
	const double kClockResyncMs = 1000.0;
	// How much of the latency above the alignment each Pose adopts.
	const double kClockDriftRate = 0.05;
	const size_t kMaxBufferedPoses = 256;

	void Render(const cAvatarPoseSample& aPose, cAvatarRenderedPose& aRendered)
	{
		aRendered.mfX = aPose.mfX;
		aRendered.mfY = aPose.mfY;
		aRendered.mfZ = aPose.mfZ;
		aRendered.mfYawDegrees = aPose.mfYawDegrees;
	}

	float Lerp(float afFrom, float afTo, float afT)
	{
		return afFrom + (afTo - afFrom) * afT;
	}

	// Turns from one yaw towards another the short way, never more than half a turn.
	float LerpYawDegrees(float afFrom, float afTo, float afT)
	{
		float fDelta = std::fmod(afTo - afFrom, 360.0f);
		if (fDelta > 180.0f) fDelta -= 360.0f;
		else if (fDelta < -180.0f) fDelta += 360.0f;
		return afFrom + fDelta * afT;
	}

	// A teleport or a jump too far to be walking is snapped to rather than glided across.
	bool IsDiscontinuous(const cAvatarPoseSample& aFrom, const cAvatarPoseSample& aTo)
	{
		if (aFrom.mlTeleportCounter != aTo.mlTeleportCounter) return true;
		const float fX = aTo.mfX - aFrom.mfX;
		const float fY = aTo.mfY - aFrom.mfY;
		const float fZ = aTo.mfZ - aFrom.mfZ;
		const float fMaxDistance = cAvatarPoseModel::kSnapDistanceMeters;
		return fX * fX + fY * fY + fZ * fZ > fMaxDistance * fMaxDistance;
	}
}

const double cAvatarPoseModel::kRenderDelayMs = 100.0;
const float cAvatarPoseModel::kSnapDistanceMeters = 3.0f;

cAvatarPoseModel::cAvatarPoseModel() : mfClockOffsetMs(0.0)
{
}

void cAvatarPoseModel::AddPose(const cAvatarPoseSample& aPose, double afLocalTimeMs)
{
	if (!mvPoses.empty())
	{
		const cAvatarPoseSample& newest = mvPoses.back();
		// Poses from different maps are never interpolated between, and a clock far behind the
		// newest Pose has started over, as when the sender restarts or a recording is replayed.
		if (aPose.msMapFile != newest.msMapFile ||
			newest.mfSenderTimeMs - aPose.mfSenderTimeMs > kClockResyncMs)
			mvPoses.clear();
		// Out-of-order and duplicate Poses.
		else if (aPose.mfSenderTimeMs <= newest.mfSenderTimeMs) return;
	}

	// The least delayed Pose is the closest to the sender's clock; later ones only add latency, and
	// the alignment drifts towards them slowly, so that a lasting rise in latency is adopted.
	// A Pose far later than that means the sender's clock stood still, so it is aligned anew.
	const double fClockOffsetMs = afLocalTimeMs - aPose.mfSenderTimeMs;
	if (mvPoses.empty() || fClockOffsetMs < mfClockOffsetMs || fClockOffsetMs - mfClockOffsetMs > kClockResyncMs)
		mfClockOffsetMs = fClockOffsetMs;
	else
		mfClockOffsetMs += (fClockOffsetMs - mfClockOffsetMs) * kClockDriftRate;
	mvPoses.push_back(aPose);
	// Sampling trims the buffer, but nothing samples it while the game is not updating.
	if (mvPoses.size() > kMaxBufferedPoses) mvPoses.pop_front();
}

bool cAvatarPoseModel::Sample(double afLocalTimeMs, const std::string& asCurrentMapFile,
	cAvatarRenderedPose& aPose)
{
	if (mvPoses.empty()) return false;
	if (asCurrentMapFile.empty() || mvPoses.back().msMapFile != asCurrentMapFile)
	{
		mvPoses.clear();
		return false;
	}

	const double fRenderTimeMs = afLocalTimeMs - mfClockOffsetMs - kRenderDelayMs;
	// Only the latest Pose the render time has reached, and those after it, are still needed.
	while (mvPoses.size() > 1 && mvPoses[1].mfSenderTimeMs <= fRenderTimeMs) mvPoses.pop_front();

	// Holds the Pose before the oldest one, after the newest one, and until a snap is due.
	const cAvatarPoseSample& from = mvPoses.front();
	if (mvPoses.size() == 1 || fRenderTimeMs <= from.mfSenderTimeMs || IsDiscontinuous(from, mvPoses[1]))
	{
		Render(from, aPose);
		return true;
	}

	const cAvatarPoseSample& to = mvPoses[1];
	const float fFraction = static_cast<float>(
		(fRenderTimeMs - from.mfSenderTimeMs) / (to.mfSenderTimeMs - from.mfSenderTimeMs));
	aPose.mfX = Lerp(from.mfX, to.mfX, fFraction);
	aPose.mfY = Lerp(from.mfY, to.mfY, fFraction);
	aPose.mfZ = Lerp(from.mfZ, to.mfZ, fFraction);
	aPose.mfYawDegrees = LerpYawDegrees(from.mfYawDegrees, to.mfYawDegrees, fFraction);
	return true;
}
