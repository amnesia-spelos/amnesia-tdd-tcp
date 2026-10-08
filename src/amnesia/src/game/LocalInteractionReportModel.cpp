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

void cLocalInteractionReportModel::Break(int alEntityId, const cGameInteractionBodyState& aFinalState)
{
	const cReportedEntity* pReported = FindReported(alEntityId);
	if (pReported == NULL || pReported->mbHeld) return;
	StopReporting(alEntityId);
	Raise(eGameInteractionEvent_ReportBroke, alEntityId);
	mvEvents.back().mEntityEvent.mState = aFinalState;
}

void cLocalInteractionReportModel::StopReporting(int alEntityId)
{
	for (size_t i = 0; i < mvReported.size(); ++i)
	{
		if (mvReported[i].mlEntityId != alEntityId) continue;
		mvReported.erase(mvReported.begin() + i);
		break;
	}
	for (size_t i = 0; i < mvReportedBodies.size();)
	{
		if (mvReportedBodies[i].mlEntityId == alEntityId) mvReportedBodies.erase(mvReportedBodies.begin() + i);
		else ++i;
	}
}

void cLocalInteractionReportModel::RecordContact(const cLocalInteractionContact& aContact)
{
	mvContacts.push_back(aContact);
}

void cLocalInteractionReportModel::Update(const iLocalInteractionWorld& aWorld, double afTimeMs)
{
	EnterByContacts(afTimeMs);

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
	mvContacts.clear();
	mbInteracting = false;
}

void cLocalInteractionReportModel::EnterByContacts(double afTimeMs)
{
	// A prop knocked in this step may itself have knocked another one, recorded before it, so the
	// contacts are gone over until none enters anything.
	bool bEntered = true;
	while (bEntered)
	{
		bEntered = false;
		for (size_t i = 0; i < mvContacts.size(); ++i)
		{
			if (EnterByContact(mvContacts[i], afTimeMs)) bEntered = true;
		}
	}
	mvContacts.clear();
}

bool cLocalInteractionReportModel::EnterByContact(const cLocalInteractionContact& aContact, double afTimeMs)
{
	if (!aContact.mbByLocalPlayer && FindReported(aContact.mlSourceEntityId) == NULL) return false;
	const cLocalInteractionContactTarget& touched = aContact.mTouched;
	if (!touched.mbBodyMoving || !touched.mbHoldable || touched.mbCreatedAtRuntime || touched.mbPeerDriven)
		return false;
	// A held entity that did not fit stays out of the report until its interaction ends.
	if (mbInteracting && touched.mlEntityId == mlInteractionEntityId) return false;
	if (FindReported(touched.mlEntityId) != NULL) return false;
	if (CountReportedBodies() + touched.mlBodyCount > kMaxBodies) return false;

	cReportedEntity entered;
	entered.mlEntityId = touched.mlEntityId;
	entered.mlBodyCount = touched.mlBodyCount;
	entered.mfEndTimeMs = afTimeMs;
	mvReported.push_back(entered);
	Raise(eGameInteractionEvent_ReportContact, touched.mlEntityId);
	return true;
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
