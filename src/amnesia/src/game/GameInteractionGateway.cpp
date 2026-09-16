#include "GameInteractionGateway.h"

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType)
	: mType(aType)
{
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType)
	: mCommandType(aCommandType), mType(aType)
{
}

cGameInteractionResponse cGameInteractionGateway::Handle(const cGameInteractionCommand& aCommand) const
{
	return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Pong);
}
