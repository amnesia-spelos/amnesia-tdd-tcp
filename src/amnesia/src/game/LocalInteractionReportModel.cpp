#include "LocalInteractionReportModel.h"

#include <cmath>

const double cLocalInteractionReportModel::kSettlingCapMs = 3000.0;
const float cLocalInteractionReportModel::kContactMotionDistance = 0.01f;
const float cLocalInteractionReportModel::kContactMotionDegrees = 2.0f;

namespace
{
	const float kPi = 3.14159265f;
}

cLocalInteractionReportModel::cLocalInteractionReportModel()
	: mbInteracting(false), mlInteractionEntityId(0), mlInteractionBodyId(0), mbInteractionBroke(false) {}

void cLocalInteractionReportModel::StartInteraction(int alEntityId, int alBodyId, bool abCreatedAtRuntime,
	size_t alBodyCount)
{
	mbInteracting = !abCreatedAtRuntime;
	if (!mbInteracting) return;
	mlInteractionEntityId = alEntityId;
	mlInteractionBodyId = alBodyId;
	mbInteractionBroke = false;
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
	pReported->mbTentative = false;
	pReported->mvBaseline.clear();
}

void cLocalInteractionReportModel::EndInteraction(eGameInteractionEnding aEnding, double afTimeMs)
{
	if (!mbInteracting) return;
	mbInteracting = false;
	Raise(eGameInteractionEvent_InteractionEnded, mlInteractionEntityId, mlInteractionBodyId, aEnding);
	if (aEnding == eGameInteractionEnding_Destroyed && mbInteractionBroke)
	{
		RaiseReportBroke(mlInteractionEntityId, mInteractionBreakState);
	}

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
	if (mbInteracting && alEntityId == mlInteractionEntityId)
	{
		mbInteractionBroke = true;
		mInteractionBreakState = aFinalState;
		return;
	}
	const cReportedEntity* pReported = FindReported(alEntityId);
	if (pReported == NULL) return;
	const bool bTentative = pReported->mbTentative;
	StopReporting(alEntityId);
	if (!bTentative) RaiseReportBroke(alEntityId, aFinalState);
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
	mvReportedBodies.clear();
	std::vector<cLocalInteractionBody> vBodies;
	for (size_t i = 0; i < mvReported.size();)
	{
		cReportedEntity& reported = mvReported[i];
		if (!aWorld.GetBodies(reported.mlEntityId, vBodies))
		{
			mvReported.erase(mvReported.begin() + i);
			continue;
		}

		bool bAllAsleep = true;
		for (size_t j = 0; j < vBodies.size(); ++j) bAllAsleep = bAllAsleep && vBodies[j].mbAsleep;
		const bool bSettled = bAllAsleep || afTimeMs - reported.mfEndTimeMs >= kSettlingCapMs;
		bool bJustMoved = false;
		if (reported.mbTentative)
		{
			if (!HasMoved(reported.mvBaseline, vBodies))
			{
				if (bSettled) mvReported.erase(mvReported.begin() + i);
				else ++i;
				continue;
			}
			reported.mbTentative = false;
			reported.mvBaseline.clear();
			bJustMoved = true;
			Raise(eGameInteractionEvent_ReportContact, reported.mlEntityId);
		}
		// A prop the contact just moved is reported at least once before it settles.
		if (!bJustMoved && !reported.mbHeld && bSettled)
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

	// After the motion check, so that a prop moved in the last step already knocks others in.
	EnterByContacts(aWorld, afTimeMs);
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

void cLocalInteractionReportModel::EnterByContacts(const iLocalInteractionWorld& aWorld, double afTimeMs)
{
	for (size_t i = 0; i < mvContacts.size(); ++i) EnterByContact(mvContacts[i], aWorld, afTimeMs);
	mvContacts.clear();
}

void cLocalInteractionReportModel::EnterByContact(const cLocalInteractionContact& aContact,
	const iLocalInteractionWorld& aWorld, double afTimeMs)
{
	if (!aContact.mbByLocalPlayer)
	{
		const cReportedEntity* pSource = FindReported(aContact.mlSourceEntityId);
		if (pSource == NULL || pSource->mbTentative) return;
	}
	const cLocalInteractionContactTarget& touched = aContact.mTouched;
	if (!touched.mbBodyMoving || !touched.mbHoldable || touched.mbCreatedAtRuntime || touched.mbPeerDriven)
		return;
	// A held entity that did not fit stays out of the report until its interaction ends.
	if (mbInteracting && touched.mlEntityId == mlInteractionEntityId) return;
	if (FindReported(touched.mlEntityId) != NULL) return;
	if (CountReportedBodies() + touched.mlBodyCount > kMaxBodies) return;

	cReportedEntity entered;
	entered.mlEntityId = touched.mlEntityId;
	entered.mlBodyCount = touched.mlBodyCount;
	entered.mbTentative = true;
	entered.mfEndTimeMs = afTimeMs;
	if (!aWorld.GetBodies(touched.mlEntityId, entered.mvBaseline)) return;
	mvReported.push_back(entered);
}

bool cLocalInteractionReportModel::HasMoved(const std::vector<cLocalInteractionBody>& avBaseline,
	const std::vector<cLocalInteractionBody>& avBodies)
{
	const float fMinTurnCos = cosf(kContactMotionDegrees * kPi / 360.0f);
	for (size_t i = 0; i < avBodies.size(); ++i)
	{
		const cGameInteractionBodyState& now = avBodies[i].mState;
		for (size_t j = 0; j < avBaseline.size(); ++j)
		{
			if (avBaseline[j].mlBodyId != avBodies[i].mlBodyId) continue;
			const cGameInteractionBodyState& then = avBaseline[j].mState;
			const float fDx = now.mPosition.mfX - then.mPosition.mfX;
			const float fDy = now.mPosition.mfY - then.mPosition.mfY;
			const float fDz = now.mPosition.mfZ - then.mPosition.mfZ;
			if (fDx * fDx + fDy * fDy + fDz * fDz > kContactMotionDistance * kContactMotionDistance) return true;
			// Two orientations q and -q are the same, and |q . p| is the cosine of half the turn between them.
			const cGameInteractionQuaternion& q = now.mOrientation;
			const cGameInteractionQuaternion& p = then.mOrientation;
			const float fDot = q.mfX * p.mfX + q.mfY * p.mfY + q.mfZ * p.mfZ + q.mfW * p.mfW;
			if (fabsf(fDot) < fMinTurnCos) return true;
		}
	}
	return false;
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

void cLocalInteractionReportModel::RaiseReportBroke(int alEntityId, const cGameInteractionBodyState& aFinalState)
{
	Raise(eGameInteractionEvent_ReportBroke, alEntityId);
	mvEvents.back().mEntityEvent.mState = aFinalState;
}
