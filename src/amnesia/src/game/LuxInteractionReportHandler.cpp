#include "LuxInteractionReportHandler.h"

#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxProp.h"
#include "LuxSocketServer.h"

namespace
{
	// The current map's props, as the report model reads them.
	class cLuxLocalInteractionWorld : public iLocalInteractionWorld
	{
	public:
		explicit cLuxLocalInteractionWorld(cLuxMap *apMap) : mpMap(apMap) {}

		virtual bool GetBodies(int alEntityId, std::vector<cLocalInteractionBody>& avBodies) const
		{
			iLuxEntity *pEntity = mpMap->GetEntityByID(alEntityId, eLuxEntityType_Prop);
			if(pEntity==NULL || pEntity->GetDestroyMe()) return false;

			iLuxProp *pProp = static_cast<iLuxProp*>(pEntity);
			avBodies.clear();
			for(int i=0; i<pProp->GetBodyNum(); ++i)
			{
				iPhysicsBody *pBody = pProp->GetBody(i);
				const cMatrixf& mtxWorld = pBody->GetWorldMatrix();
				const cVector3f vPosition = mtxWorld.GetTranslation();
				const cQuaternion qOrientation(mtxWorld.GetRotation());
				const cVector3f vLinear = pBody->GetLinearVelocity();
				// The engine's angular velocity is in radians per second.
				const cVector3f vAngular = pBody->GetAngularVelocity();

				cGameInteractionBodyState state;
				state.mPosition = cGameInteractionPosition(vPosition.x, vPosition.y, vPosition.z);
				state.mOrientation = cGameInteractionQuaternion(qOrientation.v.x, qOrientation.v.y,
					qOrientation.v.z, qOrientation.w);
				state.mLinearVelocity = cGameInteractionVector(vLinear.x, vLinear.y, vLinear.z);
				state.mAngularVelocity = cGameInteractionVector(cMath::ToDeg(vAngular.x), cMath::ToDeg(vAngular.y),
					cMath::ToDeg(vAngular.z));
				avBodies.push_back(cLocalInteractionBody(pBody->GetUniqueID(), state, !pBody->GetEnabled()));
			}
			return true;
		}

	private:
		cLuxMap *mpMap;
	};
}

cLuxInteractionReportHandler::cLuxInteractionReportHandler()
	: iLuxUpdateable("LuxInteractionReportHandler"), mfReportTimeMs(0.0)
{
}

void cLuxInteractionReportHandler::OnLocalInteractionStarted(iLuxProp *apProp, iPhysicsBody *apBody)
{
	mModel.StartInteraction(apProp->GetID(), apBody->GetUniqueID(), apProp->IsCreatedAtRuntime(),
		static_cast<size_t>(apProp->GetBodyNum()));
	PublishEvents();
}

void cLuxInteractionReportHandler::OnLocalInteractionEnded(eGameInteractionEnding aEnding)
{
	mModel.EndInteraction(aEnding, GetGameTimeMs());
	PublishEvents();
}

cGameInteractionBodySamples cLuxInteractionReportHandler::GetReportedBodies() const
{
	cGameInteractionBodySamples samples;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return samples;

	samples.mlTimeMs = static_cast<unsigned long long>(mfReportTimeMs);
	samples.mvBodies = mModel.GetReportedBodies();
	samples.msMapFile = pMap->GetFileName();
	return samples;
}

void cLuxInteractionReportHandler::Update(float afTimeStep)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL) return;

	mfReportTimeMs = GetGameTimeMs();
	mModel.Update(cLuxLocalInteractionWorld(pMap), mfReportTimeMs);
	PublishEvents();
}

// A game reset deletes every map without sending DestroyWorldEntities.
void cLuxInteractionReportHandler::Reset()
{
	mModel.Clear();
}

// Sent when a map is left or a save game is loaded into it.
void cLuxInteractionReportHandler::DestroyWorldEntities(cLuxMap *apMap)
{
	mModel.Clear();
}

double cLuxInteractionReportHandler::GetGameTimeMs()
{
	return gpBase->mpEngine->GetGameTime() * 1000.0;
}

void cLuxInteractionReportHandler::PublishEvents()
{
	std::vector<cLocalInteractionReportEvent> vEvents = mModel.TakeEvents();
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(gpBase->mpSocketServer==NULL || pMap==NULL) return;

	for(size_t i=0; i<vEvents.size(); ++i)
	{
		cGameInteractionEntityEvent entityEvent = vEvents[i].mEntityEvent;
		entityEvent.msMapFile = pMap->GetFileName();
		gpBase->mpSocketServer->PublishEvent(cGameInteractionEvent(vEvents[i].mType, entityEvent));
	}
}
