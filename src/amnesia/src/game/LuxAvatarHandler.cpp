#include "LuxAvatarHandler.h"

#include "AvatarMeshOffset.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"

// Heavier than any character can push, yet finite so the engine's force arithmetic stays sound.
static const float kUnpushableMass = 1.0e6f;

cLuxAvatarHandler::cLuxAvatarHandler()
	: iLuxUpdateable("LuxAvatarHandler")
{
	LoadLanternLight();
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

	cAvatarPoseSample sample;
	sample.mfSenderTimeMs = static_cast<double>(aPose.mlTimeMs);
	sample.mlTeleportCounter = aPose.mlTeleportCounter;
	sample.mfX = aPose.mFeetPosition.mfX;
	sample.mfY = aPose.mFeetPosition.mfY;
	sample.mfZ = aPose.mFeetPosition.mfZ;
	sample.mfYawDegrees = aPose.mfBodyYawDegrees;
	sample.mbLanternRaised = aPose.mbLanternRaised;
	sample.msMapFile = aPose.msMapFile;
	it->second.mPoseModel.AddPose(sample, GetLocalTimeMs());
}

// Takes effect on the next update, whether or not the Avatar is awake.
void cLuxAvatarHandler::SetAvatarCollision(const tString& asIdentifier, bool abCollides)
{
	tAvatarMapIt it = m_mapAvatars.find(asIdentifier);
	if(it == m_mapAvatars.end()) return;
	it->second.mCollision.SetEnabled(abCollides);
}

// The Pose is applied every update and the body cannot be pushed, so nothing moves the Avatar away from it.
void cLuxAvatarHandler::Update(float afTimeStep)
{
	cLuxMap *pCurrentMap = gpBase->mpMapHandler->GetCurrentMap();
	const tString sCurrentMapFile = pCurrentMap ? pCurrentMap->GetFileName() : "";
	const double fLocalTimeMs = GetLocalTimeMs();
	for(tAvatarMapIt it = m_mapAvatars.begin(); it != m_mapAvatars.end(); ++it)
	{
		cAvatar& avatar = it->second;
		cAvatarRenderedPose pose;
		if(!avatar.mPoseModel.Sample(fLocalTimeMs, sCurrentMapFile, pose))
		{
			SetAwake(avatar, false);
			UpdateCollision(avatar, false);
			UpdateLantern(avatar, NULL, afTimeStep);
			continue;
		}

		if(avatar.mpBody == NULL) CreateWorldObjects(it->first, avatar, pCurrentMap);
		if(avatar.mpBody == NULL) continue;

		// The body's own update moves it, and the mesh with it, by any velocity it gathered, so only
		// the Pose may place it.
		avatar.mpBody->SetForce(0);
		avatar.mpBody->SetForceVelocity(0);
		avatar.mpBody->SetFeetPosition(cVector3f(pose.mfX, pose.mfY, pose.mfZ));
		avatar.mpBody->SetYaw(cMath::ToRad(pose.mfYawDegrees));
		UpdateCollision(avatar, true);
		if(!avatar.mpBody->IsActive())
		{
			// A dormant body is not updated, so its mesh is moved to the new Pose before it shows.
			avatar.mpBody->SetActive(true);
			avatar.mpBody->Update(0.001f);
		}
		SetAwake(avatar, true);
		UpdateLantern(avatar, &pose, afTimeStep);
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
		it->second.mpLantern = NULL;
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

// The local lantern glows through the lights of the hand-held lantern model, since the player
// lantern's own light in game.cfg is a placeholder without range. The widest of them is the glow;
// the others only light the lantern's glass.
void cLuxAvatarHandler::LoadLanternLight()
{
	const tString sHandObjectFile = "/models/hand_objects/lantern/lantern.ho";
	cResources *pResources = gpBase->mpEngine->GetResources();
	iXmlDocument *pHandObjectDoc = pResources->LoadXmlDocument(sHandObjectFile);
	cXmlElement *pMainElem = pHandObjectDoc ? pHandObjectDoc->GetFirstElement("Main") : NULL;
	const tString sModelFile = pMainElem ? pMainElem->GetAttributeString("Model", "") : "";
	if(pHandObjectDoc) pResources->DestroyXmlDocument(pHandObjectDoc);

	iXmlDocument *pModelDoc = sModelFile != "" ? pResources->LoadXmlDocument(sModelFile) : NULL;
	cXmlElement *pModelDataElem = pModelDoc ? pModelDoc->GetFirstElement("ModelData") : NULL;
	cXmlElement *pEntitiesElem = pModelDataElem ? pModelDataElem->GetFirstElement("Entities") : NULL;
	if(pEntitiesElem)
	{
		cXmlNodeListIterator it = pEntitiesElem->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement *pLightElem = it.Next()->ToElement();
			if(pLightElem->GetValue() != "PointLight") continue;
			const float fRadius = pLightElem->GetAttributeFloat("Radius", 0);
			if(fRadius <= mLanternLight.mfRadius) continue;
			mLanternLight.mColor = pLightElem->GetAttributeColor("DiffuseColor", cColor(0, 0));
			mLanternLight.mfRadius = fRadius;
			mLanternLight.msGobo = pLightElem->GetAttributeString("Gobo", "");
		}
	}
	if(pModelDoc) pResources->DestroyXmlDocument(pModelDoc);

	if(mLanternLight.mfRadius <= 0)
		Error("No lantern light found through '%s', so Avatars give off no light\n", sHandObjectFile.c_str());
}

// The clock Poses arrive and are rendered by; the local Pose is stamped with the same one.
double cLuxAvatarHandler::GetLocalTimeMs()
{
	return gpBase->mpEngine->GetGameTime() * 1000.0;
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
	pBody->SetCollideFlags(kCollideFlag);
	// A character pushes only bodies no heavier than its CharacterMaxPushMass, so the player bumps
	// into an Avatar instead of shoving its mesh away from its Pose.
	pBody->SetMass(kUnpushableMass);
	pBody->SetEntity(aAvatar.mpMeshEntity);
	pBody->SetEntityOffset(GetAvatarMeshOffset(pBody->GetSize().y));
	aAvatar.mpBody = pBody;

	// Like the local lantern's glow, except that it never casts shadows.
	if(mLanternLight.mfRadius > 0)
	{
		cLightPoint *pLantern = apMap->GetWorld()->CreateLightPoint(sName + "_Lantern", mLanternLight.msGobo, false);
		pLantern->SetDiffuseColor(cColor(0, 0));
		pLantern->SetRadius(mLanternLight.mfRadius);
		pLantern->SetCastShadows(false);
		pLantern->SetIsSaved(false);
		aAvatar.mpLantern = pLantern;
	}

	// Waking places the new objects before they show.
	SetAwake(aAvatar, false);
}

void cLuxAvatarHandler::DestroyWorldObjects(cAvatar& aAvatar)
{
	if(aAvatar.mpLantern) aAvatar.mpMap->GetWorld()->DestroyLight(aAvatar.mpLantern);
	if(aAvatar.mpBody) aAvatar.mpMap->GetPhysicsWorld()->DestroyCharacterBody(aAvatar.mpBody);
	if(aAvatar.mpMeshEntity) aAvatar.mpMap->GetWorld()->DestroyMeshEntity(aAvatar.mpMeshEntity);
	aAvatar.mpLantern = NULL;
	aAvatar.mpBody = NULL;
	aAvatar.mpMeshEntity = NULL;
	aAvatar.mpMap = NULL;
}

// A dormant Avatar is neither visible nor collidable, and gives off no light.
void cLuxAvatarHandler::SetAwake(cAvatar& aAvatar, bool abAwake)
{
	if(aAvatar.mpBody) aAvatar.mpBody->SetActive(abAwake);
	if(aAvatar.mpMeshEntity) aAvatar.mpMeshEntity->SetVisible(abAwake);
	if(aAvatar.mpLantern && !abAwake) aAvatar.mpLantern->SetVisible(false);
}

static cAvatarCollisionCylinder GetCollisionCylinder(iCharacterBody *apBody)
{
	cAvatarCollisionCylinder cylinder;
	const cVector3f vFeet = apBody->GetFeetPosition();
	cylinder.mfFeetX = vFeet.x;
	cylinder.mfFeetY = vFeet.y;
	cylinder.mfFeetZ = vFeet.z;
	cylinder.mfRadius = cMath::Max(apBody->GetSize().x, apBody->GetSize().z) * 0.5f;
	cylinder.mfHeight = apBody->GetSize().y;
	return cylinder;
}

// Turning off the body's collision test, rather than the body, keeps it carrying the mesh.
void cLuxAvatarHandler::UpdateCollision(cAvatar& aAvatar, bool abAwake)
{
	iCharacterBody *pPlayerBody = gpBase->mpPlayer->GetCharacterBody();
	float fGapToPlayer = cAvatarCollisionModel::kClearanceMeters;
	if(abAwake && aAvatar.mpBody && pPlayerBody)
		fGapToPlayer = GetAvatarCollisionGap(GetCollisionCylinder(aAvatar.mpBody), GetCollisionCylinder(pPlayerBody));
	const bool bCollides = aAvatar.mCollision.Update(abAwake, fGapToPlayer);
	if(aAvatar.mpBody) aAvatar.mpBody->SetTestCollision(bCollides);
}

// Where a standing player's lantern would be, with the camera level: iCharacterBody::UpdateCamera
// places the camera at the standing body's height plus its offset, and the lantern sits at its local
// offset from the camera. The Pose's crouch and pitch are ignored.
static cMatrixf GetLanternMatrix(const cAvatarRenderedPose& aPose)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	const cVector3f& vCameraPosAdd = pPlayer->GetCameraPosAdd();
	// A character body's forward is -Z.
	const cVector3f vCameraOffset(vCameraPosAdd.x, pPlayer->GetBodySize().y + vCameraPosAdd.y, -vCameraPosAdd.z);
	cMatrixf mtxCamera = cMath::MatrixRotateY(cMath::ToRad(aPose.mfYawDegrees));
	mtxCamera.SetTranslation(cVector3f(aPose.mfX, aPose.mfY, aPose.mfZ));
	return cMath::MatrixMul(mtxCamera,
		cMath::MatrixTranslate(vCameraOffset + pPlayer->GetHelperLantern()->GetLocalOffset()));
}

// The Peer's flag decides the light, even where the local map disables the local lantern. The
// pose is NULL while the Avatar is dormant.
void cLuxAvatarHandler::UpdateLantern(cAvatar& aAvatar, const cAvatarRenderedPose *apPose, float afTimeStep)
{
	const bool bAwake = apPose != NULL;
	const float fBrightness = aAvatar.mLanternModel.Update(bAwake, bAwake && apPose->mbLanternRaised, afTimeStep);
	if(aAvatar.mpLantern == NULL) return;

	aAvatar.mpLantern->SetDiffuseColor(mLanternLight.mColor * fBrightness);
	aAvatar.mpLantern->SetVisible(fBrightness > 0);
	if(bAwake) aAvatar.mpLantern->SetMatrix(GetLanternMatrix(*apPose));
}
