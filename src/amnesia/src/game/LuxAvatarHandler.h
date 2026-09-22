#ifndef LUX_AVATAR_HANDLER_H
#define LUX_AVATAR_HANDLER_H

#include "LuxBase.h"
#include "AvatarCollisionModel.h"
#include "AvatarLanternModel.h"
#include "AvatarPoseModel.h"
#include "GameInteractionGateway.h"
#include <vector>

//----------------------------------------------

// Owns the Avatars of the Game Interaction Protocol Session (ADR 0002). An Avatar is a mesh loaded
// from its model plus a character body sized like the player's, and neither is a map entity: they
// are never saved, scripts cannot reach them, and enemy sight and the focus ray skip character bodies.
// The local player collides with an awake Avatar unless its Peer turns that off; enemies pass
// through Avatars, so an Avatar standing in their way never leaves them stuck.
// While its Peer's lantern is raised, an awake Avatar also gives off a shadowless light like the
// local lantern's, at standing eye height on its facing side.
// World objects, the light included, live only in the current map. They are destroyed when that map
// is left or reloaded and recreated once the Avatar is awake in the current map again.
class cLuxAvatarHandler : public iLuxUpdateable
{
public:
	cLuxAvatarHandler();
	~cLuxAvatarHandler();

	// Fails when the model or the mesh it names cannot be found.
	bool CreateAvatar(const tString& asIdentifier, const tString& asEntityFile);
	void RemoveAvatar(const tString& asIdentifier);
	void PoseAvatar(const tString& asIdentifier, const cGameInteractionPose& aPose);
	void SetAvatarCollision(const tString& asIdentifier, bool abCollides);

	void Update(float afTimeStep);
	void Reset();
	void DestroyWorldEntities(cLuxMap *apMap);

	// The collide flag of Avatar bodies. Enemy bodies leave it out, so they never collide with Avatars.
	static const tFlag kCollideFlag = eFlagBit_15;

private:
	struct cAvatarAnimation
	{
		tString msFile;
		tString msName;
		float mfSpeed;
	};

	struct cAvatar
	{
		cAvatar() : mbMeshBroken(false), mpMap(NULL), mpMeshEntity(NULL), mpBody(NULL), mpLantern(NULL) {}
		tString msMeshFile;
		std::vector<cAvatarAnimation> mvAnimations;
		cAvatarPoseModel mPoseModel;
		// Kept with the Avatar rather than its body, so it lasts across map changes and dormancy.
		cAvatarCollisionModel mCollision;
		cAvatarLanternModel mLanternModel;
		// Set once the mesh fails to load, so it is not reloaded every update.
		bool mbMeshBroken;
		cLuxMap *mpMap;
		cMeshEntity *mpMeshEntity;
		// A Pose drives only this body, which carries the mesh with it. A future mode could drive an
		// existing enemy's character body the same way while its AI is disabled.
		iCharacterBody *mpBody;
		cLightPoint *mpLantern;
	};
	// The glow of the local player's lantern, which every Avatar's lantern copies.
	struct cLanternLight
	{
		cLanternLight() : mColor(0, 0), mfRadius(0) {}
		cColor mColor;
		float mfRadius;
		tString msGobo;
	};

	typedef std::map<tString, cAvatar> tAvatarMap;
	typedef tAvatarMap::iterator tAvatarMapIt;

	bool FindModelFiles(const tString& asEntityFile, tString& asMeshFile,
		std::vector<cAvatarAnimation>& avAnimations);
	void LoadLanternLight();
	static double GetLocalTimeMs();
	void CreateWorldObjects(const tString& asIdentifier, cAvatar& aAvatar, cLuxMap *apMap);
	void DestroyWorldObjects(cAvatar& aAvatar);
	void SetAwake(cAvatar& aAvatar, bool abAwake);
	void UpdateCollision(cAvatar& aAvatar, bool abAwake);
	void UpdateLantern(cAvatar& aAvatar, const cAvatarRenderedPose *apPose, float afTimeStep);

	tAvatarMap m_mapAvatars;
	cLanternLight mLanternLight;
};

//----------------------------------------------

#endif // LUX_AVATAR_HANDLER_H
