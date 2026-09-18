#ifndef AVATAR_MESH_OFFSET_H
#define AVATAR_MESH_OFFSET_H

#include "math/Math.h"

//----------------------------------------------

// Where an Avatar's character body carries its mesh: feet at the body's feet, turned by yaw only.
// A character body faces -Z, but a humanoid model faces +Z, so the mesh is turned half a turn,
// like the servant enemies' Body_OffsetRot of "0 180 0".
inline hpl::cMatrixf GetAvatarMeshOffset(float afBodyHeight)
{
	hpl::cMatrixf mtxOffset = hpl::cMath::MatrixRotateY(kPif);
	mtxOffset.SetTranslation(hpl::cVector3f(0, -afBodyHeight / 2, 0));
	return mtxOffset;
}

//----------------------------------------------

#endif // AVATAR_MESH_OFFSET_H
