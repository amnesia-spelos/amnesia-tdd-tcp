#ifndef LUX_AVATAR_HANDLER_H
#define LUX_AVATAR_HANDLER_H

#include "LuxBase.h"
#include "AvatarClipModel.h"
#include "AvatarCollisionModel.h"
#include "AvatarLanternModel.h"
#include "AvatarPoseModel.h"
#include "GameInteractionGateway.h"
#include <set>
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
	// The neck and head bones' total rendered pitch is clamped to this many degrees either way.
	static const float kMaxPitchDegrees;

private:
	struct cAvatarAnimation
	{
		tString msFile;
		tString msName;
		float mfSpeed;
	};

	// A named, weighted pitch bone as configured in the `.ent`'s `MoveHeadBones`/`MoveHeadBoneMuls`
	// NPC fields (unused by Avatars for their authored purpose, since Avatars bypass the entity
	// loader and NPC behavior entirely; see ADR 0002).
	struct cAvatarPitchBoneConfig
	{
		cAvatarPitchBoneConfig() : mfWeight(0.0f) {}
		tString msName;
		float mfWeight;
	};

	// A pitch bone resolved against a loaded mesh's skeleton: its index and the model's sideways
	// axis expressed in its bind-pose frame (cLuxProp_NPC's pre-animation bone-transform pattern).
	struct cAvatarPitchBone
	{
		cAvatarPitchBone() : mlBoneIndex(-1), mfWeight(0.0f) {}
		int mlBoneIndex;
		float mfWeight;
		cVector3f mvAxis;
	};

	struct cAvatar
	{
		cAvatar() : mbMeshBroken(false), mpMap(NULL), mpMeshEntity(NULL), mpIdleAnimation(NULL),
			mpWalkAnimation(NULL), mpBody(NULL), mpLantern(NULL) {}
		tString msMeshFile;
		std::vector<cAvatarAnimation> mvAnimations;
		std::vector<cAvatarPitchBoneConfig> mvPitchBoneConfig;
		std::vector<cAvatarPitchBone> mvPitchBones;
		cAvatarPoseModel mPoseModel;
		cAvatarClipModel mClipModel;
		// Kept with the Avatar rather than its body, so it lasts across map changes and dormancy.
		cAvatarCollisionModel mCollision;
		cAvatarLanternModel mLanternModel;
		// Set once the mesh fails to load, so it is not reloaded every update.
		bool mbMeshBroken;
		cLuxMap *mpMap;
		cMeshEntity *mpMeshEntity;
		// Resolved once when world objects are created (like the pitch bones), so choosing a clip
		// never needs a name lookup. NULL when the model is missing that required clip.
		cAnimationState *mpIdleAnimation;
		cAnimationState *mpWalkAnimation;
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
		std::vector<cAvatarAnimation>& avAnimations,
		std::vector<cAvatarPitchBoneConfig>& avPitchBoneConfig);
	void LoadLanternLight();
	static double GetLocalTimeMs();
	void CreateWorldObjects(const tString& asIdentifier, cAvatar& aAvatar, cLuxMap *apMap);
	void DestroyWorldObjects(cAvatar& aAvatar);
	void ResolvePitchBones(cAvatar& aAvatar, cMesh *apMesh);
	void ResolveClips(cAvatar& aAvatar);
	void ReportModelFault(const tString& asMeshFile, const tString& asFault);
	void SetAwake(cAvatar& aAvatar, bool abAwake);
	void UpdateCollision(cAvatar& aAvatar, bool abAwake);
	void UpdateLantern(cAvatar& aAvatar, const cAvatarRenderedPose *apPose, float afTimeStep);
	void UpdatePitch(cAvatar& aAvatar, const cAvatarRenderedPose *apPose);
	void UpdateAnimation(cAvatar& aAvatar, const cAvatarRenderedPose *apPose);

	tAvatarMap m_mapAvatars;
	cLanternLight mLanternLight;
	// Each distinct model/fault pair is reported to the Chat Log and hpl.log only once per Session.
	std::set<tString> m_setReportedModelFaults;
};

//----------------------------------------------

#endif // LUX_AVATAR_HANDLER_H
