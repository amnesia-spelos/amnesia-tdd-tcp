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

const char* cLegacyGameInteractionProtocol::Greeting()
{
	return "Hello, from Amnesia: The Dark Descent!";
}

std::string cLegacyGameInteractionProtocol::SerializeEvent(const cGameInteractionEvent& aEvent)
{
	switch (aEvent.GetType())
	{
	case eGameInteractionEvent_MapChanged:
		return "EVENT:MapChanged:" + aEvent.GetData();
	case eGameInteractionEvent_ScriptCallObserved:
		return "SCRIPT_CALL:" + aEvent.GetData();
	}
	return std::string();
}

std::string cLegacyGameInteractionProtocol::ToWireLine(const std::string& asMessage)
{
	if (!asMessage.empty() && asMessage[asMessage.length() - 1] == '\n')
		return asMessage;
	return asMessage + "\n";
}

cGameInteractionCommand cLegacyGameInteractionProtocol::ParseCommand(const std::string& asCommand)
{
	if (asCommand.compare(0, 5, "exec:") == 0)
		return cGameInteractionCommand(eGameInteractionCommand_ExecuteScript, asCommand.substr(5));
	if (asCommand == "ping" || asCommand == "getpos" || asCommand == "getrot" ||
		asCommand == "getposrot" || asCommand == "getmap")
		return cGameInteractionCommand(CommandTypeFor(asCommand));
	return cGameInteractionCommand(eGameInteractionCommand_Unknown, asCommand);
}

std::string cLegacyGameInteractionProtocol::SerializeResponse(const cGameInteractionResponse& aResponse)
{
	if (aResponse.GetCommandType() == eGameInteractionCommand_Unknown)
		return "WARNING:Unknown command";
	const char* command = "ping";
	if (aResponse.GetCommandType() == eGameInteractionCommand_GetPosition) command = "getpos";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetRotation) command = "getrot";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetPositionRotation) command = "getposrot";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetMap) command = "getmap";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_ExecuteScript) command = "exec";
	if (aResponse.GetOutcome() == eGameInteractionCommandOutcome_MapNotLoaded)
		return std::string("RESPONSE:") + command + ":no map loaded";
	if (aResponse.GetType() == eGameInteractionResponse_Pong) return "RESPONSE:ping:pong";
	if (aResponse.GetType() == eGameInteractionResponse_Position)
		return FormatPosition("getpos", aResponse.GetPosition());
	if (aResponse.GetType() == eGameInteractionResponse_Rotation)
		return FormatRotation("getrot", aResponse.GetRotation());
	if (aResponse.GetType() == eGameInteractionResponse_PositionRotation)
		return FormatPosition("getposrot", aResponse.GetPosition()) + ":" +
			FormatRotationValues(aResponse.GetRotation());
	if (aResponse.GetType() == eGameInteractionResponse_Map)
		return "RESPONSE:getmap:" + aResponse.GetMapFile();
	return "RESPONSE:exec:script executed";
}
