#ifndef AVATAR_POSE_MODEL_H
#define AVATAR_POSE_MODEL_H

#include <deque>
#include <string>

// A Pose as the Avatar's Peer sent it, stamped with the sending game's clock.
struct cAvatarPoseSample
{
	cAvatarPoseSample()
		: mfSenderTimeMs(0.0), mlTeleportCounter(0), mfX(0.0f), mfY(0.0f), mfZ(0.0f), mfYawDegrees(0.0f),
		  mfCameraPitchDegrees(0.0f), mbCrouching(false), mbLanternRaised(false) {}
	double mfSenderTimeMs;
	unsigned int mlTeleportCounter;
	float mfX;
	float mfY;
	float mfZ;
	float mfYawDegrees;
	float mfCameraPitchDegrees;
	bool mbCrouching;
	bool mbLanternRaised;
	std::string msMapFile;
};

// Where an awake Avatar is shown: feet position and body yaw, camera pitch, and whether its lantern
// is raised.
struct cAvatarRenderedPose
{
	cAvatarRenderedPose()
		: mfX(0.0f), mfY(0.0f), mfZ(0.0f), mfYawDegrees(0.0f), mfCameraPitchDegrees(0.0f),
		  mbCrouching(false), mbLanternRaised(false), mfHorizontalSpeedMps(0.0f), mfForwardSpeedMps(0.0f) {}
	float mfX;
	float mfY;
	float mfZ;
	float mfYawDegrees;
	float mfCameraPitchDegrees;
	// A flag cannot be blended, so it is the older sample's: it switches on the sender's time.
	bool mbCrouching;
	bool mbLanternRaised;
	// Total horizontal speed and the signed forward component (the horizontal velocity dotted with
	// this Pose's yaw's forward vector) of the two Poses actually being interpolated. Zero while
	// holding a Pose or at a snap, and never derived from the Avatar body: SetFeetPosition resets its
	// previous position every update, so the body itself always reports zero velocity.
	float mfHorizontalSpeedMps;
	float mfForwardSpeedMps;
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
