#include "AvatarMeshOffset.h"

#include "scene/Camera.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace hpl;

// The engine library's WinMain refers to the game's entry point, which these tests never reach.
int hplMain(const tString&) { return 0; }

namespace
{
	void Expect(bool abCondition, const std::string& asDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << asDescription << "\n";
		exit(1);
	}

	// The mesh matrix iCharacterBody gives the entity it carries, for a body at the origin.
	cMatrixf CarriedMeshMatrix(float afYaw, const cMatrixf& amtxOffset)
	{
		return cMath::MatrixMul(cMath::MatrixRotateY(afYaw), amtxOffset);
	}
}

int main()
{
	const float fBodyHeight = 1.9f;
	const cMatrixf mtxOffset = GetAvatarMeshOffset(fBodyHeight);

	const float vYawDegrees[] = { 0.0f, 45.0f, 90.0f, 180.0f, -135.0f };
	for (size_t index = 0; index < sizeof(vYawDegrees) / sizeof(vYawDegrees[0]); ++index)
	{
		const float fYaw = cMath::ToRad(vYawDegrees[index]);
		const std::string sYaw = " at yaw " + std::to_string(vYawDegrees[index]);

		// A Pose's body yaw is the posing player's body yaw, which follows their camera's yaw.
		cCamera camera;
		camera.SetYaw(fYaw);
		const cMatrixf mtxMesh = CarriedMeshMatrix(fYaw, mtxOffset);

		// A humanoid model faces +Z in its own space.
		const cVector3f vFront = cMath::MatrixMul3x3(mtxMesh, cVector3f(0, 0, 1));
		Expect(cMath::Vector3Dot(vFront, camera.GetForward()) > 0.99f,
			"Avatar faces where the posing player looks" + sYaw);
		Expect(cMath::Vector3Dot(cMath::MatrixMul3x3(mtxMesh, cVector3f(0, 1, 0)), cVector3f(0, 1, 0)) > 0.99f,
			"Avatar stays upright" + sYaw);
		Expect(cMath::Vector3Dist(mtxMesh.GetTranslation(), cVector3f(0, -fBodyHeight / 2, 0)) < 0.001f,
			"Avatar's feet are at the body's feet" + sYaw);
	}

	std::cout << "Avatar mesh offset cases passed\n";
	return 0;
}
