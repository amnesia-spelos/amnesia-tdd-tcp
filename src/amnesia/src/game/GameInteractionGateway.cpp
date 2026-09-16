#include "GameInteractionGateway.h"

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::string& asData)
	: mType(aType), msData(asData)
{
}

cGameInteractionGateway::cGameInteractionGateway()
	: mbSessionActive(false)
{
}

void cGameInteractionGateway::BeginLegacySession()
{
	mbSessionActive = true;
}

void cGameInteractionGateway::EndSession()
{
	mbSessionActive = false;
	mPublishedEvents.clear();
}

void cGameInteractionGateway::Publish(const cGameInteractionEvent& aEvent)
{
	if (mbSessionActive) mPublishedEvents.push_back(aEvent);
}

bool cGameInteractionGateway::TryTakePublishedEvent(cGameInteractionEvent& aEvent)
{
	if (mPublishedEvents.empty()) return false;
	aEvent = mPublishedEvents.front();
	mPublishedEvents.pop_front();
	return true;
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::string& asData)
	: mType(aType), msData(asData)
{
}

eGameInteractionCommandClassification cGameInteractionCommand::GetClassification() const
{
	return mType == eGameInteractionCommand_ExecuteScript ?
		eGameInteractionCommandClassification_StateChanging :
		eGameInteractionCommandClassification_Observational;
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome)
{
}

cGameInteractionResponse cGameInteractionGateway::Handle(const cGameInteractionCommand& aCommand,
	iGameInteractionGameAdapter& aGameAdapter) const
{
	switch (aCommand.GetType())
	{
	case eGameInteractionCommand_GetPosition:
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Position,
			aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_MapNotLoaded);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
			response.SetPosition(aGameAdapter.GetPosition());
		return response;
	}
	case eGameInteractionCommand_GetRotation:
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Rotation,
			aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_MapNotLoaded);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
			response.SetRotation(aGameAdapter.GetRotation());
		return response;
	}
	case eGameInteractionCommand_GetPositionRotation:
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_PositionRotation,
			aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_MapNotLoaded);
		if (response.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
		response.SetPosition(aGameAdapter.GetPosition());
		response.SetRotation(aGameAdapter.GetRotation());
		return response;
	}
	case eGameInteractionCommand_GetMap:
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Map,
			aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_MapNotLoaded);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
			response.SetMapFile(aGameAdapter.GetMapFile());
		return response;
	}
	case eGameInteractionCommand_ExecuteScript:
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_ScriptExecuted,
			aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
				eGameInteractionCommandOutcome_MapNotLoaded);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
			aGameAdapter.RunScript(aCommand.GetData());
		return response;
	}
	default:
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Pong);
	}
}
