#ifndef LUX_AVATAR_HANDLER_H
#define LUX_AVATAR_HANDLER_H

#include "LuxBase.h"
#include "AvatarPoseModel.h"
#include "GameInteractionGateway.h"

//----------------------------------------------

// Owns the Avatars of the Game Interaction Protocol Session (ADR 0002). An Avatar is a mesh loaded
// from its model plus a character body sized like the player's, and neither is a map entity: they
// are never saved, scripts cannot reach them, and enemy sight and the focus ray skip character bodies.
// World objects live only in the current map. They are destroyed when that map is left or reloaded
// and recreated once the Avatar is awake in the current map again.
class cLuxAvatarHandler : public iLuxUpdateable
{
public:
	cLuxAvatarHandler();
	~cLuxAvatarHandler();

	// Fails when the model or the mesh it names cannot be found.
	bool CreateAvatar(const tString& asIdentifier, const tString& asEntityFile);
	void RemoveAvatar(const tString& asIdentifier);
	void PoseAvatar(const tString& asIdentifier, const cGameInteractionPose& aPose);

	void Update(float afTimeStep);
	void Reset();
	void DestroyWorldEntities(cLuxMap *apMap);

private:
	struct cAvatar
	{
		cAvatar() : mbMeshBroken(false), mpMap(NULL), mpMeshEntity(NULL), mpBody(NULL) {}
		tString msMeshFile;
		cAvatarPoseModel mPoseModel;
		// Set once the mesh fails to load, so it is not reloaded every update.
		bool mbMeshBroken;
		cLuxMap *mpMap;
		cMeshEntity *mpMeshEntity;
		// A Pose drives only this body, which carries the mesh with it. A future mode could drive an
		// existing enemy's character body the same way while its AI is disabled.
		iCharacterBody *mpBody;
	};
	typedef std::map<tString, cAvatar> tAvatarMap;
	typedef tAvatarMap::iterator tAvatarMapIt;

	bool FindMeshFile(const tString& asEntityFile, tString& asMeshFile);
	static double GetLocalTimeMs();
	void CreateWorldObjects(const tString& asIdentifier, cAvatar& aAvatar, cLuxMap *apMap);
	void DestroyWorldObjects(cAvatar& aAvatar);
	void SetAwake(cAvatar& aAvatar, bool abAwake);

	tAvatarMap m_mapAvatars;
};

//----------------------------------------------

#endif // LUX_AVATAR_HANDLER_H
