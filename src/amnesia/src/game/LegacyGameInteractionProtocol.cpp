#include "LegacyGameInteractionProtocol.h"

#include <cstdio>

cGameInteractionLineBuffer::cGameInteractionLineBuffer()
	: mSearchStart(0)
{
}

void cGameInteractionLineBuffer::Append(const char* apBytes, std::string::size_type aLength)
{
	msPendingBytes.append(apBytes, aLength);
}

bool cGameInteractionLineBuffer::TryPopLine(std::string& asLine)
{
	const std::string::size_type delimiter = msPendingBytes.find('\n', mSearchStart);
	if (delimiter == std::string::npos)
	{
		mSearchStart = msPendingBytes.length();
		return false;
	}

	asLine = msPendingBytes.substr(0, delimiter);
	if (!asLine.empty() && asLine[asLine.length() - 1] == '\r')
		asLine.erase(asLine.length() - 1);
	msPendingBytes.erase(0, delimiter + 1);
	mSearchStart = 0;
	return true;
}

void cGameInteractionLineBuffer::Clear()
{
	msPendingBytes.clear();
	mSearchStart = 0;
}

namespace
{
	const float kRadiansToDegrees = 180.0f / 3.14159265f;

	std::string FormatPosition(const char* apCommand, const cGameInteractionPosition& aPosition)
	{
		char response[128];
		sprintf(response, "RESPONSE:%s:%.2f, %.2f, %.2f", apCommand,
			aPosition.mfX, aPosition.mfY, aPosition.mfZ);
		return response;
	}

	std::string FormatRotation(const char* apCommand, const cGameInteractionRotation& aRotation)
	{
		char response[128];
		sprintf(response, "RESPONSE:%s:%.2f, %.2f, %.2f", apCommand,
			aRotation.mfYawRadians * kRadiansToDegrees,
			aRotation.mfPitchRadians * kRadiansToDegrees,
			0.0f);
		return response;
	}

	std::string FormatRotationValues(const cGameInteractionRotation& aRotation)
	{
		char response[96];
		sprintf(response, "%.2f, %.2f, %.2f",
			aRotation.mfYawRadians * kRadiansToDegrees,
			aRotation.mfPitchRadians * kRadiansToDegrees,
			0.0f);
		return response;
	}

	eGameInteractionCommandType CommandTypeFor(const std::string& asCommand)
	{
		if (asCommand == "getpos") return eGameInteractionCommand_GetPosition;
		if (asCommand == "getrot") return eGameInteractionCommand_GetRotation;
		if (asCommand == "getposrot") return eGameInteractionCommand_GetPositionRotation;
		if (asCommand == "getmap") return eGameInteractionCommand_GetMap;
		return eGameInteractionCommand_Ping;
	}
}

cLegacyGameInteractionProtocol::cLegacyGameInteractionProtocol(cGameInteractionGateway& aGateway,
	iGameInteractionGameAdapter& aGameAdapter)
	: mGateway(aGateway), mGameAdapter(aGameAdapter)
{
}

const char* cLegacyGameInteractionProtocol::Greeting()
{
	return "Hello, from Amnesia: The Dark Descent!";
}

std::string cLegacyGameInteractionProtocol::MapChangedEvent(const std::string& asMapFile)
{
	return "EVENT:MapChanged:" + asMapFile;
}

std::string cLegacyGameInteractionProtocol::ScriptCallObservation(const std::string& asScriptCall)
{
	return "SCRIPT_CALL:" + asScriptCall;
}

std::string cLegacyGameInteractionProtocol::ToWireLine(const std::string& asMessage)
{
	if (!asMessage.empty() && asMessage[asMessage.length() - 1] == '\n')
		return asMessage;
	return asMessage + "\n";
}

std::string cLegacyGameInteractionProtocol::FirstCommandFromReceive(const std::string& asReceivedBytes)
{
	const std::string::size_type delimiter = asReceivedBytes.find_first_of("\r\n");
	return asReceivedBytes.substr(0, delimiter);
}

std::string cLegacyGameInteractionProtocol::HandleCommand(const std::string& asCommand)
{
	if (asCommand == "ping" || asCommand == "getpos" || asCommand == "getrot" ||
		asCommand == "getposrot" || asCommand == "getmap")
	{
		const cGameInteractionCommand command(CommandTypeFor(asCommand));
		const cGameInteractionResponse response = mGateway.Handle(command, mGameAdapter);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_MapNotLoaded)
			return "RESPONSE:" + asCommand + ":no map loaded";
		if (response.GetCommandType() == eGameInteractionCommand_Ping &&
			response.GetType() == eGameInteractionResponse_Pong)
			return "RESPONSE:ping:pong";
		if (response.GetType() == eGameInteractionResponse_Position)
			return FormatPosition("getpos", response.GetPosition());
		if (response.GetType() == eGameInteractionResponse_Rotation)
			return FormatRotation("getrot", response.GetRotation());
		if (response.GetType() == eGameInteractionResponse_PositionRotation)
			return FormatPosition("getposrot", response.GetPosition()) + ":" +
				FormatRotationValues(response.GetRotation());
		if (response.GetType() == eGameInteractionResponse_Map)
			return "RESPONSE:getmap:" + response.GetMapFile();
	}
	if (asCommand.compare(0, 5, "exec:") == 0)
	{
		const cGameInteractionCommand command(eGameInteractionCommand_ExecuteScript, asCommand.substr(5));
		const cGameInteractionResponse response = mGateway.Handle(command, mGameAdapter);
		if (response.GetOutcome() == eGameInteractionCommandOutcome_MapNotLoaded)
			return "RESPONSE:exec:no map loaded";
		return "RESPONSE:exec:script executed";
	}
	return "WARNING:Unknown command";
}
