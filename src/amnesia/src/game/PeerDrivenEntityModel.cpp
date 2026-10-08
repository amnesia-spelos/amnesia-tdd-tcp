#include "PeerDrivenEntityModel.h"

#include <cmath>

namespace
{
	const size_t kMaxBufferedSamples = 256;

	float Lerp(float afFrom, float afTo, float afT)
	{
		return afFrom + (afTo - afFrom) * afT;
	}

	cGameInteractionPosition Lerp(const cGameInteractionPosition& aFrom, const cGameInteractionPosition& aTo,
		float afT)
	{
		return cGameInteractionPosition(Lerp(aFrom.mfX, aTo.mfX, afT), Lerp(aFrom.mfY, aTo.mfY, afT),
			Lerp(aFrom.mfZ, aTo.mfZ, afT));
	}

	cGameInteractionVector Lerp(const cGameInteractionVector& aFrom, const cGameInteractionVector& aTo, float afT)
	{
		return cGameInteractionVector(Lerp(aFrom.mfX, aTo.mfX, afT), Lerp(aFrom.mfY, aTo.mfY, afT),
			Lerp(aFrom.mfZ, aTo.mfZ, afT));
	}

	// Blends two orientations the short way. The gateway only accepts quaternions close to unit length.
	cGameInteractionQuaternion Lerp(const cGameInteractionQuaternion& aFrom, const cGameInteractionQuaternion& aTo,
		float afT)
	{
		const float fDot = aFrom.mfX * aTo.mfX + aFrom.mfY * aTo.mfY + aFrom.mfZ * aTo.mfZ + aFrom.mfW * aTo.mfW;
		const float fSign = fDot < 0.0f ? -1.0f : 1.0f;
		cGameInteractionQuaternion blended(Lerp(aFrom.mfX, aTo.mfX * fSign, afT), Lerp(aFrom.mfY, aTo.mfY * fSign, afT),
			Lerp(aFrom.mfZ, aTo.mfZ * fSign, afT), Lerp(aFrom.mfW, aTo.mfW * fSign, afT));
		const float fLength = std::sqrt(blended.mfX * blended.mfX + blended.mfY * blended.mfY +
			blended.mfZ * blended.mfZ + blended.mfW * blended.mfW);
		if (fLength <= 0.0f) return aFrom;
		blended.mfX /= fLength;
		blended.mfY /= fLength;
		blended.mfZ /= fLength;
		blended.mfW /= fLength;
		return blended;
	}

	// A jump too far to be motion is snapped to rather than glided across.
	bool IsDiscontinuous(const cGameInteractionBodyState& aFrom, const cGameInteractionBodyState& aTo)
	{
		const float fX = aTo.mPosition.mfX - aFrom.mPosition.mfX;
		const float fY = aTo.mPosition.mfY - aFrom.mPosition.mfY;
		const float fZ = aTo.mPosition.mfZ - aFrom.mPosition.mfZ;
		const float fMaxDistance = cPeerDrivenEntityModel::kSnapDistanceMeters;
		return fX * fX + fY * fY + fZ * fZ > fMaxDistance * fMaxDistance;
	}
}

const double cPeerDrivenEntityModel::kRenderDelayMs = 100.0;
const float cPeerDrivenEntityModel::kSnapDistanceMeters = 3.0f;

cPeerDrivenEntityModel::cPeerDrivenEntityModel() : mbHasSamples(false), mfNewestSenderTimeMs(0.0)
{
}

eGameInteractionEntityOutcome cPeerDrivenEntityModel::Drive(int alEntityId, iPeerDrivenEntityWorld& aWorld)
{
	if (IsDriving(alEntityId)) return eGameInteractionEntityOutcome_Success;
	const eGameInteractionEntityOutcome found = aWorld.FindEntity(alEntityId);
	if (found != eGameInteractionEntityOutcome_Success) return found;

	if (aWorld.IsLocallyInteracting(alEntityId)) aWorld.EndLocalInteraction(alEntityId);
	aWorld.BeginDriving(alEntityId);
	mvDriven.push_back(cDrivenEntity());
	mvDriven.back().mlEntityId = alEntityId;
	return eGameInteractionEntityOutcome_Success;
}

eGameInteractionEntityOutcome cPeerDrivenEntityModel::AddBodies(const cGameInteractionBodySamples& aSamples,
	double afLocalTimeMs, const iPeerDrivenEntityWorld& aWorld, int& alFailedEntityId)
{
	const double fSenderTimeMs = static_cast<double>(aSamples.mlTimeMs);
	// A clock far behind the newest sample has started over, as when the sender restarts or a recording
	// is replayed.
	if (mbHasSamples && mfNewestSenderTimeMs - fSenderTimeMs > cSenderClock::kResyncMs) ClearSamples();

	eGameInteractionEntityOutcome outcome = eGameInteractionEntityOutcome_Success;
	bool bBuffered = false;
	for (size_t i = 0; i < aSamples.mvBodies.size(); ++i)
	{
		const cGameInteractionBodySample& sample = aSamples.mvBodies[i];
		cDrivenEntity* pDriven = FindDriven(sample.mlEntityId);
		if (pDriven == NULL || !aWorld.HasBody(sample.mlEntityId, sample.mlBodyId))
		{
			if (outcome == eGameInteractionEntityOutcome_Success) alFailedEntityId = sample.mlEntityId;
			outcome = eGameInteractionEntityOutcome_NotFound;
			continue;
		}

		cDrivenBody* pBody = NULL;
		for (size_t j = 0; j < pDriven->mvBodies.size() && pBody == NULL; ++j)
		{
			if (pDriven->mvBodies[j].mlBodyId == sample.mlBodyId) pBody = &pDriven->mvBodies[j];
		}
		if (pBody == NULL)
		{
			pDriven->mvBodies.push_back(cDrivenBody());
			pBody = &pDriven->mvBodies.back();
			pBody->mlBodyId = sample.mlBodyId;
		}
		// Out-of-order and duplicate samples.
		if (!pBody->mvSamples.empty() && fSenderTimeMs <= pBody->mvSamples.back().mfSenderTimeMs) continue;

		cBodySample buffered;
		buffered.mfSenderTimeMs = fSenderTimeMs;
		buffered.mState = sample.mState;
		pBody->mvSamples.push_back(buffered);
		// Updating trims the buffer, but nothing updates it while the game is not updating.
		if (pBody->mvSamples.size() > kMaxBufferedSamples) pBody->mvSamples.pop_front();
		bBuffered = true;
	}

	if (bBuffered)
	{
		mClock.Align(fSenderTimeMs, afLocalTimeMs, !mbHasSamples);
		mbHasSamples = true;
		if (fSenderTimeMs > mfNewestSenderTimeMs) mfNewestSenderTimeMs = fSenderTimeMs;
	}
	return outcome;
}

eGameInteractionEntityOutcome cPeerDrivenEntityModel::SetInteracting(int alEntityId, bool abInteracting,
	iPeerDrivenEntityWorld& aWorld)
{
	cDrivenEntity* pDriven = FindDriven(alEntityId);
	if (pDriven == NULL) return eGameInteractionEntityOutcome_NotFound;
	if (pDriven->mbInteracting == abInteracting) return eGameInteractionEntityOutcome_Success;
	pDriven->mbInteracting = abInteracting;
	aWorld.SetInteracting(alEntityId, abInteracting);
	return eGameInteractionEntityOutcome_Success;
}

eGameInteractionEntityOutcome cPeerDrivenEntityModel::Release(int alEntityId, iPeerDrivenEntityWorld& aWorld)
{
	for (size_t i = 0; i < mvDriven.size(); ++i)
	{
		if (mvDriven[i].mlEntityId != alEntityId) continue;
		EndDriving(i, aWorld);
		return eGameInteractionEntityOutcome_Success;
	}
	return eGameInteractionEntityOutcome_NotFound;
}

eGameInteractionEntityOutcome cPeerDrivenEntityModel::Break(int alEntityId,
	const cGameInteractionBodyState& aFinalState, iPeerDrivenEntityWorld& aWorld)
{
	for (size_t i = 0; i < mvDriven.size(); ++i)
	{
		if (mvDriven[i].mlEntityId != alEntityId) continue;
		const bool bInteracting = mvDriven[i].mbInteracting;
		mvDriven.erase(mvDriven.begin() + i);
		if (bInteracting) aWorld.SetInteracting(alEntityId, false);
		aWorld.BreakEntity(alEntityId, aFinalState);
		return eGameInteractionEntityOutcome_Success;
	}
	return eGameInteractionEntityOutcome_NotFound;
}

void cPeerDrivenEntityModel::ReleaseAll(iPeerDrivenEntityWorld& aWorld)
{
	while (!mvDriven.empty()) EndDriving(0, aWorld);
	ClearSamples();
}

void cPeerDrivenEntityModel::Clear()
{
	mvDriven.clear();
	ClearSamples();
}

void cPeerDrivenEntityModel::Update(double afLocalTimeMs, iPeerDrivenEntityWorld& aWorld)
{
	const double fRenderTimeMs = afLocalTimeMs - mClock.GetOffsetMs() - kRenderDelayMs;
	for (size_t i = 0; i < mvDriven.size();)
	{
		cDrivenEntity& driven = mvDriven[i];
		// Destroyed, as by a script, so there is nothing to give back.
		if (aWorld.FindEntity(driven.mlEntityId) != eGameInteractionEntityOutcome_Success)
		{
			mvDriven.erase(mvDriven.begin() + i);
			continue;
		}

		for (size_t j = 0; j < driven.mvBodies.size(); ++j)
		{
			std::deque<cBodySample>& vSamples = driven.mvBodies[j].mvSamples;
			if (vSamples.empty()) continue;
			// Only the latest sample the render time has reached, and those after it, are still needed.
			while (vSamples.size() > 1 && vSamples[1].mfSenderTimeMs <= fRenderTimeMs) vSamples.pop_front();

			// Holds the sample before the oldest one, after the newest one, and until a snap is due.
			const cBodySample& from = vSamples.front();
			if (vSamples.size() == 1 || fRenderTimeMs <= from.mfSenderTimeMs ||
				IsDiscontinuous(from.mState, vSamples[1].mState))
			{
				aWorld.SetBodyState(driven.mlEntityId, driven.mvBodies[j].mlBodyId, from.mState);
				continue;
			}

			const cBodySample& to = vSamples[1];
			const float fT = static_cast<float>(
				(fRenderTimeMs - from.mfSenderTimeMs) / (to.mfSenderTimeMs - from.mfSenderTimeMs));
			cGameInteractionBodyState state;
			state.mPosition = Lerp(from.mState.mPosition, to.mState.mPosition, fT);
			state.mOrientation = Lerp(from.mState.mOrientation, to.mState.mOrientation, fT);
			state.mLinearVelocity = Lerp(from.mState.mLinearVelocity, to.mState.mLinearVelocity, fT);
			state.mAngularVelocity = Lerp(from.mState.mAngularVelocity, to.mState.mAngularVelocity, fT);
			aWorld.SetBodyState(driven.mlEntityId, driven.mvBodies[j].mlBodyId, state);
		}
		++i;
	}
}

bool cPeerDrivenEntityModel::IsDriving(int alEntityId) const
{
	for (size_t i = 0; i < mvDriven.size(); ++i)
	{
		if (mvDriven[i].mlEntityId == alEntityId) return true;
	}
	return false;
}

cPeerDrivenEntityModel::cDrivenEntity* cPeerDrivenEntityModel::FindDriven(int alEntityId)
{
	for (size_t i = 0; i < mvDriven.size(); ++i)
	{
		if (mvDriven[i].mlEntityId == alEntityId) return &mvDriven[i];
	}
	return NULL;
}

void cPeerDrivenEntityModel::EndDriving(size_t alIndex, iPeerDrivenEntityWorld& aWorld)
{
	const cDrivenEntity driven = mvDriven[alIndex];
	mvDriven.erase(mvDriven.begin() + alIndex);
	if (driven.mbInteracting) aWorld.SetInteracting(driven.mlEntityId, false);
	aWorld.EndDriving(driven.mlEntityId);
}

void cPeerDrivenEntityModel::ClearSamples()
{
	for (size_t i = 0; i < mvDriven.size(); ++i) mvDriven[i].mvBodies.clear();
	mbHasSamples = false;
	mfNewestSenderTimeMs = 0.0;
}
