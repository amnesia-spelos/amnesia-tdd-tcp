#include "LuxAvatarHandler.h"

#include "AvatarMeshOffset.h"
#include "LuxChatHandler.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"

// Heavier than any character can push, yet finite so the engine's force arithmetic stays sound.
static const float kUnpushableMass = 1.0e6f;
// How long crossing between idle and walk takes, matching the enemy movement selector's cross-fade.
static const float kClipFadeSeconds = 0.3f;

const float cLuxAvatarHandler::kMaxPitchDegrees = 45.0f;

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
	std::vector<cAvatarAnimation> vAnimations;
	std::vector<cAvatarPitchBoneConfig> vPitchBoneConfig;
	if(!FindModelFiles(asEntityFile, sMeshFile, vAnimations, vPitchBoneConfig))
	{
		Log("Game Interaction Protocol: cannot create Avatar '%s': model '%s' not found\n",
			asIdentifier.c_str(), asEntityFile.c_str());
		return false;
	}

	// A new Avatar is dormant until its first Pose for the current map.
	cAvatar avatar;
	avatar.msMeshFile = sMeshFile;
	avatar.mvAnimations = vAnimations;
	avatar.mvPitchBoneConfig = vPitchBoneConfig;
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
	sample.mfCameraPitchDegrees = aPose.mfCameraPitchDegrees;
	sample.mbCrouching = aPose.mbCrouching;
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
			UpdatePitch(avatar, NULL);
			UpdateAnimation(avatar, NULL);
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
		UpdateStance(avatar, pose.mbCrouching);
		UpdateCollision(avatar, true);
		if(!avatar.mpBody->IsActive())
		{
			// A dormant body is not updated, so its mesh is moved to the new Pose before it shows.
			avatar.mpBody->SetActive(true);
			avatar.mpBody->Update(0.001f);
		}
		SetAwake(avatar, true);
		UpdateLantern(avatar, &pose, afTimeStep);
		UpdatePitch(avatar, &pose);
		UpdateAnimation(avatar, &pose);
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
		for(int i = 0; i < eAvatarClip_LastEnum; ++i) it->second.mvClipAnimations[i] = NULL;
		it->second.mpBody = NULL;
		it->second.mbCrouching = false;
		it->second.mlCrouchSizeIndex = -1;
		it->second.mpLantern = NULL;
	}
}

// Reads the model's mesh and animation clips, and its NPC MoveHeadBones/MoveHeadBoneMuls fields as
// the pitch bone configuration (the Grasp rig contract; unused by Avatars for their authored
// purpose, since Avatars bypass the entity loader and NPC behavior entirely, see ADR 0002). Its
// bodies and joints are ignored.
bool cLuxAvatarHandler::FindModelFiles(const tString& asEntityFile, tString& asMeshFile,
	std::vector<cAvatarAnimation>& avAnimations, std::vector<cAvatarPitchBoneConfig>& avPitchBoneConfig)
{
	cResources *pResources = gpBase->mpEngine->GetResources();
	const tWString sEntityPath = pResources->GetFileSearcher()->GetFilePath(asEntityFile);
	if(sEntityPath == _W("")) return false;

	iXmlDocument *pEntityDoc = pResources->LoadXmlDocument(asEntityFile);
	if(pEntityDoc == NULL) return false;
	cXmlElement *pModelDataElem = pEntityDoc->GetFirstElement("ModelData");
	cXmlElement *pMeshElem = pModelDataElem ? pModelDataElem->GetFirstElement("Mesh") : NULL;
	tString sMeshFile = pMeshElem ? pMeshElem->GetAttributeString("Filename", "") : "";
	cXmlElement *pAnimationsElem = pModelDataElem ? pModelDataElem->GetFirstElement("Animations") : NULL;
	if(pAnimationsElem)
	{
		cXmlNodeListIterator it = pAnimationsElem->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement *pAnimElem = it.Next()->ToElement();
			cAvatarAnimation anim;
			anim.msFile = pAnimElem->GetAttributeString("File", "");
			anim.msName = pAnimElem->GetAttributeString("Name", "");
			anim.mfSpeed = pAnimElem->GetAttributeFloat("Speed", 1.0f);
			if(cString::GetFilePath(anim.msFile).length() <= 1)
				anim.msFile = cString::SetFilePath(anim.msFile,
					cString::To8Char(cString::GetFilePathW(sEntityPath)));
			avAnimations.push_back(anim);
		}
	}

	cXmlElement *pVarsElem = pEntityDoc->GetFirstElement("UserDefinedVariables");
	tString sPitchBoneNames = "";
	tString sPitchBoneWeights = "";
	if(pVarsElem)
	{
		cXmlNodeListIterator it = pVarsElem->GetChildIterator();
		while(it.HasNext())
		{
			cXmlElement *pVarElem = it.Next()->ToElement();
			const tString sName = pVarElem->GetAttributeString("Name", "");
			if(sName == "MoveHeadBones") sPitchBoneNames = pVarElem->GetAttributeString("Value", "");
			else if(sName == "MoveHeadBoneMuls") sPitchBoneWeights = pVarElem->GetAttributeString("Value", "");
		}
	}
	pResources->DestroyXmlDocument(pEntityDoc);
	if(sMeshFile == "") return false;

	tStringVec vBoneNames;
	cString::GetStringVec(sPitchBoneNames, vBoneNames);
	tFloatVec vBoneWeights;
	cString::GetFloatVec(sPitchBoneWeights, vBoneWeights);
	if(vBoneNames.size() > 0 && vBoneNames.size() == vBoneWeights.size())
	{
		for(size_t i = 0; i < vBoneNames.size(); ++i)
		{
			cAvatarPitchBoneConfig config;
			config.msName = vBoneNames[i];
			config.mfWeight = vBoneWeights[i];
			avPitchBoneConfig.push_back(config);
		}
	}

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
	for(size_t i = 0; i < aAvatar.mvAnimations.size(); ++i)
	{
		const cAvatarAnimation& anim = aAvatar.mvAnimations[i];
		cAnimation *pAnimation = gpBase->mpEngine->GetResources()->GetAnimationManager()->CreateAnimation(anim.msFile);
		if(pAnimation)
			aAvatar.mpMeshEntity->AddAnimation(pAnimation, anim.msName, anim.mfSpeed);
		else
			Error("Could not load animation '%s' for Avatar '%s'\n", anim.msFile.c_str(), asIdentifier.c_str());
	}
	ResolveClips(aAvatar);
	ResolvePitchBones(aAvatar, pMesh);

	iCharacterBody *pBody = apMap->GetPhysicsWorld()->CreateCharacterBody(sName, gpBase->mpPlayer->GetBodySize());
	pBody->SetGravityActive(false);
	pBody->SetCollideFlags(kCollideFlag);
	// A character pushes only bodies no heavier than its CharacterMaxPushMass, so the player bumps
	// into an Avatar instead of shoving its mesh away from its Pose.
	pBody->SetMass(kUnpushableMass);
	pBody->SetEntity(aAvatar.mpMeshEntity);
	pBody->SetEntityOffset(GetAvatarMeshOffset(pBody->GetSize().y));
	aAvatar.mpBody = pBody;
	// Created once per body alongside the standing size; SetActiveSize switches to it on crouch (#51).
	aAvatar.mlCrouchSizeIndex = pBody->AddExtraSize(gpBase->mpPlayer->GetBodyCrouchSize());
	aAvatar.mbCrouching = false;

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
	for(int i = 0; i < eAvatarClip_LastEnum; ++i) aAvatar.mvClipAnimations[i] = NULL;
	aAvatar.mbCrouching = false;
	aAvatar.mlCrouchSizeIndex = -1;
	aAvatar.mpMap = NULL;
}

// Reports a distinct model/fault once per Session, in a SYSTEM Chat Entry and in hpl.log.
void cLuxAvatarHandler::ReportModelFault(const tString& asMeshFile, const tString& asFault)
{
	const tString sKey = asMeshFile + "|" + asFault;
	if(m_setReportedModelFaults.find(sKey) != m_setReportedModelFaults.end()) return;
	m_setReportedModelFaults.insert(sKey);

	const tString sMessage = "Avatar model '" + asMeshFile + "': " + asFault;
	Log("Game Interaction Protocol: %s\n", sMessage.c_str());
	if(gpBase->mpChatHandler)
		gpBase->mpChatHandler->DisplayChatEntry(cChatEntry(_W("SYSTEM"), cString::To16Char(sMessage)));
}

// Resolves the configured pitch bones against the loaded mesh's skeleton and precomputes each bone's
// model sideways axis in its bind-pose frame (cLuxProp_NPC's pre-animation bone-transform pattern),
// so pitch can be applied every frame without a name lookup. A missing skeleton or bone is reported
// but leaves the Avatar visible; it simply renders no pitch through that bone.
void cLuxAvatarHandler::ResolvePitchBones(cAvatar& aAvatar, cMesh *apMesh)
{
	aAvatar.mvPitchBones.clear();

	if(aAvatar.mvPitchBoneConfig.empty())
	{
		ReportModelFault(aAvatar.msMeshFile,
			"no pitch bones configured (MoveHeadBones/MoveHeadBoneMuls missing or mismatched)");
		return;
	}

	cSkeleton *pSkeleton = apMesh->GetSkeleton();
	if(pSkeleton == NULL)
	{
		ReportModelFault(aAvatar.msMeshFile, "mesh has no skeleton, so camera pitch cannot be rendered");
		return;
	}

	for(size_t i = 0; i < aAvatar.mvPitchBoneConfig.size(); ++i)
	{
		const cAvatarPitchBoneConfig& config = aAvatar.mvPitchBoneConfig[i];
		const int lBoneIndex = pSkeleton->GetBoneIndexByName(config.msName);
		if(lBoneIndex < 0)
		{
			ReportModelFault(aAvatar.msMeshFile, "configured pitch bone '" + config.msName + "' not found");
			continue;
		}

		cBone *pBone = pSkeleton->GetBoneByIndex(lBoneIndex);
		cAvatarPitchBone bone;
		bone.mlBoneIndex = lBoneIndex;
		bone.mfWeight = config.mfWeight;
		// MatrixMul3x3, not MatrixMul: the axis is a direction, and MatrixMul would add the inverse
		// transform's translation, skewing it off true sideways and making pitch rotate around a
		// tilted axis instead of a clean lateral one.
		bone.mvAxis = cMath::Vector3Normalize(
			cMath::MatrixMul3x3(cMath::MatrixInverse(pBone->GetWorldTransform()), cVector3f(1, 0, 0)));
		aAvatar.mvPitchBones.push_back(bone);
	}
}

// Resolves the required "idle" and "walk" clips, and the "crouch_idle" and "crouch_walk" clips,
// against the loaded mesh's animations once, so choosing between them every update never needs a
// name lookup. A missing required clip is reported but leaves the Avatar visible, starting on
// whichever of "idle" and "walk" is present (#50). A missing crouch clip is likewise reported, and
// falls back to standing "idle" at render time where possible (#51).
void cLuxAvatarHandler::ResolveClips(cAvatar& aAvatar)
{
	static const char *kClipNames[eAvatarClip_LastEnum] = {"idle", "walk", "crouch_idle", "crouch_walk"};
	for(int i = 0; i < eAvatarClip_LastEnum; ++i)
		aAvatar.mvClipAnimations[i] = aAvatar.mpMeshEntity->GetAnimationStateFromName(kClipNames[i]);

	if(aAvatar.mvClipAnimations[eAvatarClip_Idle] == NULL)
		ReportModelFault(aAvatar.msMeshFile, "missing required 'idle' animation clip");
	if(aAvatar.mvClipAnimations[eAvatarClip_Walk] == NULL)
		ReportModelFault(aAvatar.msMeshFile, "missing required 'walk' animation clip");
	if(aAvatar.mvClipAnimations[eAvatarClip_CrouchIdle] == NULL)
		ReportModelFault(aAvatar.msMeshFile, "missing 'crouch_idle' animation clip, falling back to 'idle'");
	if(aAvatar.mvClipAnimations[eAvatarClip_CrouchWalk] == NULL)
		ReportModelFault(aAvatar.msMeshFile, "missing 'crouch_walk' animation clip, falling back to 'idle'");

	cAnimationState *pStart = NULL;
	for(int i = 0; i < eAvatarClip_LastEnum && pStart == NULL; ++i) pStart = aAvatar.mvClipAnimations[i];
	if(pStart) aAvatar.mpMeshEntity->PlayName(pStart->GetName(), true, true);
}

// A dormant Avatar is neither visible nor collidable, and gives off no light.
void cLuxAvatarHandler::SetAwake(cAvatar& aAvatar, bool abAwake)
{
	if(aAvatar.mpBody) aAvatar.mpBody->SetActive(abAwake);
	if(aAvatar.mpMeshEntity) aAvatar.mpMeshEntity->SetVisible(abAwake);
	if(aAvatar.mpLantern && !abAwake) aAvatar.mpLantern->SetVisible(false);
}

// Switches the body's active size on a change in the rendered Pose's crouch flag, and moves the
// mesh offset to match the new active height. UpdateCollision, called right after, recomputes the
// player's collision clearance from the body's now-current size. CheckCharacterFits is not called:
// the Avatar follows its Peer rather than being pushed by the world, and the player-overlap guard in
// cAvatarCollisionModel already covers the local player standing into it (#51).
void cLuxAvatarHandler::UpdateStance(cAvatar& aAvatar, bool abCrouching)
{
	if(aAvatar.mpBody == NULL || aAvatar.mbCrouching == abCrouching) return;
	aAvatar.mbCrouching = abCrouching;
	aAvatar.mpBody->SetActiveSize(abCrouching ? aAvatar.mlCrouchSizeIndex : 0);
	aAvatar.mpBody->SetEntityOffset(GetAvatarMeshOffset(aAvatar.mpBody->GetSize().y));
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

// Where a standing or crouched player's lantern would be, with the camera level: iCharacterBody::
// UpdateCamera places the camera at the active body's height plus its offset, and the lantern sits
// at its local offset from the camera. Crouching switches straight to the crouched body height,
// reaching the same final height cLuxMoveState_Normal::SetCrouch eases the local player's own camera
// to, but without the eased transition: an Avatar's rendered crouch flag is already a discrete,
// held Pose (#51), not a smoothly animated local stance. The Pose's pitch is ignored, since a point
// light's rotation has no useful pitch effect.
static cMatrixf GetLanternMatrix(const cAvatarRenderedPose& aPose)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	const cVector3f& vCameraPosAdd = pPlayer->GetCameraPosAdd();
	const float fBodyHeight = aPose.mbCrouching ? pPlayer->GetBodyCrouchSize().y : pPlayer->GetBodySize().y;
	// A character body's forward is -Z.
	const cVector3f vCameraOffset(vCameraPosAdd.x, fBodyHeight + vCameraPosAdd.y, -vCameraPosAdd.z);
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

// Renders the Pose's camera pitch through the resolved neck and head bones, clamped in total to
// kMaxPitchDegrees and distributed by their configured weights (cLuxProp_NPC's pre-animation
// bone-transform pattern). Pre/post bone transforms only apply while an animation is active, so this
// is inert on a mesh whose only animation failed to load. The pose is NULL while the Avatar is dormant.
void cLuxAvatarHandler::UpdatePitch(cAvatar& aAvatar, const cAvatarRenderedPose *apPose)
{
	if(aAvatar.mpMeshEntity == NULL || apPose == NULL) return;

	const float fPitchRadians = cMath::ToRad(
		cMath::Clamp(apPose->mfCameraPitchDegrees, -kMaxPitchDegrees, kMaxPitchDegrees));
	for(size_t i = 0; i < aAvatar.mvPitchBones.size(); ++i)
	{
		const cAvatarPitchBone& bone = aAvatar.mvPitchBones[i];
		cBoneState *pBoneState = aAvatar.mpMeshEntity->GetBoneState(bone.mlBoneIndex);
		if(pBoneState == NULL) continue;
		pBoneState->SetUsePreTransform(true);
		pBoneState->SetPreTransform(
			cMath::MatrixQuaternion(cQuaternion(fPitchRadians * bone.mfWeight, bone.mvAxis)));
	}
}

// Chooses idle, walk, crouch_idle, or crouch_walk from the Pose's rendered horizontal motion and
// crouch (cAvatarClipModel) and applies it to the mesh, cross-fading only on the update the choice
// actually changes and scaling a walking clip's playback by the measured speed. If the chosen clip
// failed to load, falls back to standing "idle" where possible, then to whichever of "idle" and
// "walk" is present; ResolveClips already reported the fault. The pose is NULL while the Avatar is
// dormant, which leaves the last playing animation alone.
void cLuxAvatarHandler::UpdateAnimation(cAvatar& aAvatar, const cAvatarRenderedPose *apPose)
{
	if(aAvatar.mpMeshEntity == NULL || apPose == NULL) return;
	if(aAvatar.mvClipAnimations[eAvatarClip_Idle] == NULL && aAvatar.mvClipAnimations[eAvatarClip_Walk] == NULL)
		return;

	const cAvatarClipChoice choice =
		aAvatar.mClipModel.Update(apPose->mfHorizontalSpeedMps, apPose->mfForwardSpeedMps, apPose->mbCrouching);

	cAnimationState *pTarget = aAvatar.mvClipAnimations[choice.mClip];
	if(pTarget == NULL) pTarget = aAvatar.mvClipAnimations[eAvatarClip_Idle];
	if(pTarget == NULL) pTarget = aAvatar.mvClipAnimations[eAvatarClip_Walk];

	if(choice.mbChanged) aAvatar.mpMeshEntity->PlayFadeToName(pTarget->GetName(), true, kClipFadeSeconds);
	if(pTarget == aAvatar.mvClipAnimations[eAvatarClip_Walk] ||
		pTarget == aAvatar.mvClipAnimations[eAvatarClip_CrouchWalk])
		pTarget->SetSpeed(choice.mfPlaybackSpeed);
}
