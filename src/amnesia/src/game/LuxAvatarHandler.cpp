#include "LuxAvatarHandler.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"

cLuxAvatarHandler::cLuxAvatarHandler()
	: iLuxUpdateable("LuxAvatarHandler")
{
}

// World objects belong to the map's world, which destroys them if it outlives this module.
cLuxAvatarHandler::~cLuxAvatarHandler()
{
}

bool cLuxAvatarHandler::CreateAvatar(const tString& asIdentifier, const tString& asEntityFile)
{
	tString sMeshFile;
	if(!FindMeshFile(asEntityFile, sMeshFile))
	{
		Log("Game Interaction Protocol: cannot create Avatar '%s': model '%s' not found\n",
			asIdentifier.c_str(), asEntityFile.c_str());
		return false;
	}

	// A new Avatar is dormant until its first Pose for the current map.
	cAvatar avatar;
	avatar.msMeshFile = sMeshFile;
	m_mapAvatars[asIdentifier] = avatar;
	return true;
}

void cLuxAvatarHandler::RemoveAvatar(const tString& asIdentifier)
{
	tAvatarMapIt it = m_mapAvatars.find(asIdentifier);
	if(it == m_mapAvatars.end()) return;
	DestroyWorldObjects(it->second);
	m_mapAvatars.erase(it);
}

void cLuxAvatarHandler::PoseAvatar(const tString& asIdentifier, const cGameInteractionPose& aPose)
{
	tAvatarMapIt it = m_mapAvatars.find(asIdentifier);
	if(it == m_mapAvatars.end()) return;
	it->second.mbPosed = true;
	it->second.mPose = aPose;
}

// The Pose is applied every update, so nothing that pushes the body moves the Avatar away from it.
void cLuxAvatarHandler::Update(float afTimeStep)
{
	cLuxMap *pCurrentMap = gpBase->mpMapHandler->GetCurrentMap();
	for(tAvatarMapIt it = m_mapAvatars.begin(); it != m_mapAvatars.end(); ++it)
	{
		cAvatar& avatar = it->second;
		if(!IsAwake(avatar, pCurrentMap))
		{
			SetAwake(avatar, false);
			continue;
		}

		if(avatar.mpBody == NULL) CreateWorldObjects(it->first, avatar, pCurrentMap);
		if(avatar.mpBody == NULL) continue;

		const cGameInteractionPose& pose = avatar.mPose;
		avatar.mpBody->SetFeetPosition(cVector3f(pose.mFeetPosition.mfX, pose.mFeetPosition.mfY,
			pose.mFeetPosition.mfZ));
		avatar.mpBody->SetYaw(cMath::ToRad(pose.mfBodyYawDegrees));
		if(!avatar.mpBody->IsActive())
		{
			// A dormant body is not updated, so its mesh is moved to the new Pose before it shows.
			avatar.mpBody->SetActive(true);
			avatar.mpBody->Update(0.001f);
		}
		SetAwake(avatar, true);
	}
}

// Sent when a map is left or a save game is loaded into it, before its world goes away.
void cLuxAvatarHandler::DestroyWorldEntities(cLuxMap *apMap)
{
	for(tAvatarMapIt it = m_mapAvatars.begin(); it != m_mapAvatars.end(); ++it)
	{
		if(it->second.mpMap == apMap) DestroyWorldObjects(it->second);
	}
}

// A game reset deletes every map without sending DestroyWorldEntities, and the worlds take the
// Avatars' objects with them. The Avatars themselves belong to the Session and stay.
void cLuxAvatarHandler::Reset()
{
	for(tAvatarMapIt it = m_mapAvatars.begin(); it != m_mapAvatars.end(); ++it)
	{
		it->second.mpMap = NULL;
		it->second.mpMeshEntity = NULL;
		it->second.mpBody = NULL;
	}
}

// Reads only the model's mesh; its bodies, joints, and prop variables are ignored.
bool cLuxAvatarHandler::FindMeshFile(const tString& asEntityFile, tString& asMeshFile)
{
	cResources *pResources = gpBase->mpEngine->GetResources();
	const tWString sEntityPath = pResources->GetFileSearcher()->GetFilePath(asEntityFile);
	if(sEntityPath == _W("")) return false;

	iXmlDocument *pEntityDoc = pResources->LoadXmlDocument(asEntityFile);
	if(pEntityDoc == NULL) return false;
	cXmlElement *pModelDataElem = pEntityDoc->GetFirstElement("ModelData");
	cXmlElement *pMeshElem = pModelDataElem ? pModelDataElem->GetFirstElement("Mesh") : NULL;
	tString sMeshFile = pMeshElem ? pMeshElem->GetAttributeString("Filename", "") : "";
	pResources->DestroyXmlDocument(pEntityDoc);
	if(sMeshFile == "") return false;

	// Like the entity loader, a mesh named without a folder is the one beside the model.
	if(cString::GetFilePath(sMeshFile).size() < 1)
		sMeshFile = cString::SetFilePath(sMeshFile, cString::To8Char(cString::GetFilePathW(sEntityPath)));
	if(pResources->GetFileSearcher()->GetFilePath(sMeshFile) == _W("")) return false;

	asMeshFile = sMeshFile;
	return true;
}

// Awake while its latest Pose names the current map; dormant otherwise, including with no map loaded.
bool cLuxAvatarHandler::IsAwake(const cAvatar& aAvatar, cLuxMap *apCurrentMap)
{
	return apCurrentMap && aAvatar.mbPosed && aAvatar.mPose.msMapFile == apCurrentMap->GetFileName();
}

void cLuxAvatarHandler::CreateWorldObjects(const tString& asIdentifier, cAvatar& aAvatar, cLuxMap *apMap)
{
	if(aAvatar.mbMeshBroken) return;

	cMesh *pMesh = gpBase->mpEngine->GetResources()->GetMeshManager()->CreateMesh(aAvatar.msMeshFile);
	if(pMesh == NULL)
	{
		Error("Could not load mesh '%s' for Avatar '%s'\n", aAvatar.msMeshFile.c_str(), asIdentifier.c_str());
		aAvatar.mbMeshBroken = true;
		return;
	}

	const tString sName = "Avatar_" + asIdentifier;
	aAvatar.mpMap = apMap;
	aAvatar.mpMeshEntity = apMap->GetWorld()->CreateMeshEntity(sName, pMesh);

	iCharacterBody *pBody = apMap->GetPhysicsWorld()->CreateCharacterBody(sName, gpBase->mpPlayer->GetBodySize());
	pBody->SetGravityActive(false);
	// The body carries the mesh with its feet at the body's feet, turned by yaw only.
	pBody->SetEntity(aAvatar.mpMeshEntity);
	pBody->SetEntityOffset(cMath::MatrixTranslate(cVector3f(0, -pBody->GetSize().y / 2, 0)));
	aAvatar.mpBody = pBody;
	// Waking places the new objects before they show.
	SetAwake(aAvatar, false);
}

void cLuxAvatarHandler::DestroyWorldObjects(cAvatar& aAvatar)
{
	if(aAvatar.mpBody) aAvatar.mpMap->GetPhysicsWorld()->DestroyCharacterBody(aAvatar.mpBody);
	if(aAvatar.mpMeshEntity) aAvatar.mpMap->GetWorld()->DestroyMeshEntity(aAvatar.mpMeshEntity);
	aAvatar.mpBody = NULL;
	aAvatar.mpMeshEntity = NULL;
	aAvatar.mpMap = NULL;
}

// A dormant Avatar is neither visible nor collidable.
void cLuxAvatarHandler::SetAwake(cAvatar& aAvatar, bool abAwake)
{
	if(aAvatar.mpBody) aAvatar.mpBody->SetActive(abAwake);
	if(aAvatar.mpMeshEntity) aAvatar.mpMeshEntity->SetVisible(abAwake);
}
