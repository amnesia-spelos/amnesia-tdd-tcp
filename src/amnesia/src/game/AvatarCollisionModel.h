#ifndef AVATAR_COLLISION_MODEL_H
#define AVATAR_COLLISION_MODEL_H

// A character body as an upright cylinder standing on its feet position.
struct cAvatarCollisionCylinder
{
	cAvatarCollisionCylinder() : mfFeetX(0.0f), mfFeetY(0.0f), mfFeetZ(0.0f), mfRadius(0.0f), mfHeight(0.0f) {}
	float mfFeetX;
	float mfFeetY;
	float mfFeetZ;
	float mfRadius;
	float mfHeight;
};

// How far apart two bodies are, across or in height, whichever is more. It is negative when they
// overlap, by the smaller of the two overlaps.
float GetAvatarCollisionGap(const cAvatarCollisionCylinder& aA, const cAvatarCollisionCylinder& aB);

// Decides whether one Avatar collides with the local player. Collision is on until the Peer turns
// it off. A dormant Avatar never collides. An Avatar that overlaps the player deeply, because it woke,
// snapped, or walked onto them or its collision was turned on around them, becomes passable until
// the two are clearly apart, so the player is neither trapped in it nor thrown out of it.
class cAvatarCollisionModel
{
public:
	// How far the Avatar and the player may overlap before it becomes passable. Pressing against a
	// body leaves the player a hair inside it, which must not let them walk through.
	static const float kOverlapToleranceMeters;
	// How far apart the Avatar and the player must be before an overlap stops making it passable.
	static const float kClearanceMeters;

	cAvatarCollisionModel();

	void SetEnabled(bool abEnabled);
	bool IsEnabled() const { return mbEnabled; }

	// Called every update with the Avatar's gap to the player; returns whether it collides now.
	bool Update(bool abAwake, float afGapToPlayer);

private:
	bool mbEnabled;
	// Passable until clear of the player. Set whenever the Avatar does not collide, so that it only
	// starts colliding again away from the player.
	bool mbPassable;
};

#endif // AVATAR_COLLISION_MODEL_H
