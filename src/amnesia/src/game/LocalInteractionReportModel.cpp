#include "LocalInteractionReportModel.h"

const double cLocalInteractionReportModel::kSettlingCapMs = 3000.0;

cLocalInteractionReportModel::cLocalInteractionReportModel()
	: mbInteracting(false), mlInteractionEntityId(0), mlInteractionBodyId(0) {}

void cLocalInteractionReportModel::StartInteraction(int alEntityId, int alBodyId, bool abCreatedAtRuntime,
	size_t alBodyCount)
{
	mbInteracting = !abCreatedAtRuntime;
	if (!mbInteracting) return;
	mlInteractionEntityId = alEntityId;
	mlInteractionBodyId = alBodyId;
	Raise(eGameInteractionEvent_InteractionStarted, alEntityId, alBodyId);

	cReportedEntity* pReported = FindReported(alEntityId);
	if (pReported == NULL)
	{
		if (CountReportedBodies() + alBodyCount > kMaxBodies) return;
		mvReported.push_back(cReportedEntity());
		pReported = &mvReported.back();
		pReported->mlEntityId = alEntityId;
		pReported->mlBodyCount = alBodyCount;
	}
	pReported->mbHeld = true;
}

void cLocalInteractionReportModel::EndInteraction(eGameInteractionEnding aEnding, double afTimeMs)
{
	if (!mbInteracting) return;
	mbInteracting = false;
	Raise(eGameInteractionEvent_InteractionEnded, mlInteractionEntityId, mlInteractionBodyId, aEnding);

	for (size_t i = 0; i < mvReported.size(); ++i)
	{
		if (mvReported[i].mlEntityId != mlInteractionEntityId) continue;
		if (aEnding == eGameInteractionEnding_Destroyed)
		{
			mvReported.erase(mvReported.begin() + i);
			return;
		}
		mvReported[i].mbHeld = false;
		mvReported[i].mfEndTimeMs = afTimeMs;
		return;
	}
}

void cLocalInteractionReportModel::Update(const iLocalInteractionWorld& aWorld, double afTimeMs)
{
	mvReportedBodies.clear();
	std::vector<cLocalInteractionBody> vBodies;
	for (size_t i = 0; i < mvReported.size();)
	{
		const cReportedEntity& reported = mvReported[i];
		if (!aWorld.GetBodies(reported.mlEntityId, vBodies))
		{
			mvReported.erase(mvReported.begin() + i);
			continue;
		}

		bool bAllAsleep = true;
		for (size_t j = 0; j < vBodies.size(); ++j) bAllAsleep = bAllAsleep && vBodies[j].mbAsleep;
		if (!reported.mbHeld && (bAllAsleep || afTimeMs - reported.mfEndTimeMs >= kSettlingCapMs))
		{
			Raise(eGameInteractionEvent_ReportSettled, reported.mlEntityId);
			mvReported.erase(mvReported.begin() + i);
			continue;
		}

		for (size_t j = 0; j < vBodies.size(); ++j)
		{
			cGameInteractionBodySample sample;
			sample.mlEntityId = reported.mlEntityId;
			sample.mlBodyId = vBodies[j].mlBodyId;
			sample.mState = vBodies[j].mState;
			mvReportedBodies.push_back(sample);
		}
		++i;
	}
}

std::vector<cLocalInteractionReportEvent> cLocalInteractionReportModel::TakeEvents()
{
	std::vector<cLocalInteractionReportEvent> vEvents;
	vEvents.swap(mvEvents);
	return vEvents;
}

void cLocalInteractionReportModel::Clear()
{
	mvReported.clear();
	mvReportedBodies.clear();
	mvEvents.clear();
	mbInteracting = false;
}

cLocalInteractionReportModel::cReportedEntity* cLocalInteractionReportModel::FindReported(int alEntityId)
{
	for (size_t i = 0; i < mvReported.size(); ++i)
	{
		if (mvReported[i].mlEntityId == alEntityId) return &mvReported[i];
	}
	return NULL;
}

size_t cLocalInteractionReportModel::CountReportedBodies() const
{
	size_t lCount = 0;
	for (size_t i = 0; i < mvReported.size(); ++i) lCount += mvReported[i].mlBodyCount;
	return lCount;
}

void cLocalInteractionReportModel::Raise(eGameInteractionEventType aType, int alEntityId, int alBodyId,
	eGameInteractionEnding aEnding)
{
	cLocalInteractionReportEvent event(aType);
	event.mEntityEvent.mlEntityId = alEntityId;
	event.mEntityEvent.mlBodyId = alBodyId;
	event.mEntityEvent.mEnding = aEnding;
	mvEvents.push_back(event);
}
