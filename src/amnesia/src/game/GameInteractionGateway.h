#ifndef GAME_INTERACTION_GATEWAY_H
#define GAME_INTERACTION_GATEWAY_H

enum eGameInteractionCommandType
{
	eGameInteractionCommand_Ping
};

class cGameInteractionCommand
{
public:
	explicit cGameInteractionCommand(eGameInteractionCommandType aType);
	eGameInteractionCommandType GetType() const { return mType; }

private:
	eGameInteractionCommandType mType;
};

enum eGameInteractionResponseType
{
	eGameInteractionResponse_Pong
};

class cGameInteractionResponse
{
public:
	cGameInteractionResponse(eGameInteractionCommandType aCommandType, eGameInteractionResponseType aType);
	eGameInteractionCommandType GetCommandType() const { return mCommandType; }
	eGameInteractionResponseType GetType() const { return mType; }

private:
	eGameInteractionCommandType mCommandType;
	eGameInteractionResponseType mType;
};

class cGameInteractionGateway
{
public:
	cGameInteractionResponse Handle(const cGameInteractionCommand& aCommand) const;
};

#endif
