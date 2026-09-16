#include "GameInteractionGateway.h"

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType)
	: mType(aType)
{
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome)
{
}

cGameInteractionResponse cGameInteractionGateway::Handle(const cGameInteractionCommand& aCommand,
	const iGameInteractionGameAdapter& aGameAdapter) const
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
	default:
		return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Pong);
	}
}
