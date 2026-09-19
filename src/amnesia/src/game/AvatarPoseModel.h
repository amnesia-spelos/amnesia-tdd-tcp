#ifndef AVATAR_POSE_MODEL_H
#define AVATAR_POSE_MODEL_H

#include <deque>
#include <string>

// A Pose as the Avatar's Peer sent it, stamped with the sending game's clock.
struct cAvatarPoseSample
{
	cAvatarPoseSample()
		: mfSenderTimeMs(0.0), mlTeleportCounter(0), mfX(0.0f), mfY(0.0f), mfZ(0.0f), mfYawDegrees(0.0f) {}
	double mfSenderTimeMs;
	unsigned int mlTeleportCounter;
	float mfX;
	float mfY;
	float mfZ;
	float mfYawDegrees;
	std::string msMapFile;
};

// Where an awake Avatar is shown: feet position and body yaw.
struct cAvatarRenderedPose
{
	cAvatarRenderedPose() : mfX(0.0f), mfY(0.0f), mfZ(0.0f), mfYawDegrees(0.0f) {}
	float mfX;
	float mfY;
	float mfZ;
	float mfYawDegrees;
};

// Buffers one Avatar's Poses and renders them a fixed delay in the past (ADR 0003).
class cAvatarPoseModel
{
public:
	// How far behind the sender's newest Pose the Avatar is rendered, to hide network jitter.
	static const double kRenderDelayMs;
	// Consecutive Poses further apart than this, in meters, are snapped between, not interpolated.
	static const float kSnapDistanceMeters;

	cAvatarPoseModel();

	void AddPose(const cAvatarPoseSample& aPose, double afLocalTimeMs);

	// False while the Avatar is dormant: until a Pose arrives, and whenever the latest Pose names
	// another map than the current one, which is empty while no map is loaded. Being dormant clears
	// the buffer, so the Avatar wakes on the next Pose for the current map.
	bool Sample(double afLocalTimeMs, const std::string& asCurrentMapFile, cAvatarRenderedPose& aPose);

private:
	std::deque<cAvatarPoseSample> mvPoses;
	// Local time minus sender time.
	double mfClockOffsetMs;
};

#endif // AVATAR_POSE_MODEL_H
