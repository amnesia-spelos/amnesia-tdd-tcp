#include "AvatarCollisionModel.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	const float kClear = 1.0f;
	const float kOverlapping = -0.3f;

	void Expect(bool abCondition, const std::string& asDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << asDescription << "\n";
		exit(1);
	}

	bool Near(float afActual, float afExpected)
	{
		return std::fabs(afActual - afExpected) < 0.001f;
	}

	cAvatarCollisionCylinder Cylinder(float afFeetX, float afFeetY, float afFeetZ, float afRadius = 0.25f,
		float afHeight = 1.5f)
	{
		cAvatarCollisionCylinder cylinder;
		cylinder.mfFeetX = afFeetX;
		cylinder.mfFeetY = afFeetY;
		cylinder.mfFeetZ = afFeetZ;
		cylinder.mfRadius = afRadius;
		cylinder.mfHeight = afHeight;
		return cylinder;
	}
}

int main()
{
	Expect(Near(GetAvatarCollisionGap(Cylinder(0.0f, 0.0f, 0.0f), Cylinder(0.0f, 0.0f, 2.0f)), 1.5f),
		"side by side, the gap is the distance between the rims");
	Expect(Near(GetAvatarCollisionGap(Cylinder(0.0f, 0.0f, 0.0f), Cylinder(0.06f, 0.0f, 0.08f)), -0.4f),
		"standing inside each other, the gap is how far the rims overlap");
	Expect(Near(GetAvatarCollisionGap(Cylinder(0.0f, 0.0f, 0.0f), Cylinder(0.1f, 2.0f, 0.0f)), 0.5f),
		"one standing above the other, the gap is the height between them");
	Expect(Near(GetAvatarCollisionGap(Cylinder(0.1f, -1.6f, 0.0f), Cylinder(0.0f, 0.0f, 0.0f)), 0.1f),
		"one standing below the other, the gap is the height between them");
	Expect(GetAvatarCollisionGap(Cylinder(0.0f, 1.0f, 0.0f), Cylinder(0.2f, 0.0f, 0.0f)) < 0.0f,
		"bodies overlapping both across and in height have a negative gap");
	Expect(Near(GetAvatarCollisionGap(Cylinder(0.0f, 0.0f, 0.0f, 0.5f), Cylinder(1.0f, 0.0f, 0.0f, 0.25f)), 0.25f),
		"the gap accounts for each body's own radius");

	{
		cAvatarCollisionModel model;
		Expect(model.IsEnabled(), "collision is on by default");
		Expect(!model.Update(false, kClear), "a dormant Avatar does not collide");
		Expect(model.Update(true, kClear), "an awake Avatar clear of the player collides");
		Expect(model.Update(true, cAvatarCollisionModel::kClearanceMeters * 0.5f),
			"an Avatar the player walks up to keeps colliding");
		Expect(model.Update(true, -0.001f), "an Avatar the player presses against keeps colliding");
		Expect(model.Update(true, -cAvatarCollisionModel::kOverlapToleranceMeters * 0.5f),
			"an Avatar that moves slightly into the player keeps colliding");
		Expect(!model.Update(false, kClear), "an Avatar that falls dormant stops colliding");
	}

	{
		cAvatarCollisionModel model;
		model.SetEnabled(false);
		Expect(!model.IsEnabled(), "collision can be turned off");
		Expect(!model.Update(true, kClear), "an awake Avatar with collision off does not collide");
		model.SetEnabled(true);
		Expect(model.Update(true, kClear), "turning collision back on restores it");
	}

	{
		cAvatarCollisionModel model;
		Expect(!model.Update(true, kOverlapping), "an Avatar that wakes overlapping the player does not collide");
		Expect(!model.Update(true, cAvatarCollisionModel::kClearanceMeters * 0.5f),
			"an Avatar only just apart from the player still does not collide");
		Expect(model.Update(true, cAvatarCollisionModel::kClearanceMeters),
			"an Avatar clear of the player collides again");
	}

	{
		cAvatarCollisionModel model;
		Expect(model.Update(true, kClear), "an awake Avatar collides");
		Expect(!model.Update(true, kOverlapping), "an Avatar snapped onto the player stops colliding");
		Expect(!model.Update(true, 0.0f), "it stays passable while touching the player");
		Expect(model.Update(true, kClear), "it collides again once clear");
	}

	{
		cAvatarCollisionModel model;
		model.SetEnabled(false);
		Expect(!model.Update(true, kOverlapping), "the player walks into an Avatar with collision off");
		model.SetEnabled(true);
		Expect(!model.Update(true, kOverlapping), "turning collision on around the player does not trap them");
		Expect(model.Update(true, kClear), "collision resumes once the player is clear");
	}

	{
		cAvatarCollisionModel model;
		Expect(model.Update(true, kClear), "an awake Avatar collides");
		Expect(!model.Update(false, kOverlapping), "a dormant Avatar does not collide");
		Expect(!model.Update(true, kOverlapping), "an Avatar that wakes again on the player does not trap them");
	}

	std::cout << "Avatar collision model cases passed\n";
	return 0;
}
