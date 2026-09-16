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

	std::string FormatPosition(const char* apCommand, const cLegacyPeerState& aState)
	{
		char response[128];
		sprintf(response, "RESPONSE:%s:%.2f, %.2f, %.2f", apCommand,
			aState.mfPositionX, aState.mfPositionY, aState.mfPositionZ);
		return response;
	}

	std::string FormatRotation(const char* apCommand, const cLegacyPeerState& aState)
	{
		char response[128];
		sprintf(response, "RESPONSE:%s:%.2f, %.2f, %.2f", apCommand,
			aState.mfYawRadians * kRadiansToDegrees,
			aState.mfPitchRadians * kRadiansToDegrees,
			0.0f);
		return response;
	}
}

cLegacyGameInteractionProtocol::cLegacyGameInteractionProtocol(cGameInteractionGateway& aGateway,
	iLegacyGameAdapter& aGameAdapter)
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
	if (asCommand == "ping")
	{
		const cGameInteractionCommand command(eGameInteractionCommand_Ping);
		const cGameInteractionResponse response = mGateway.Handle(command);
		if (response.GetCommandType() == eGameInteractionCommand_Ping &&
			response.GetType() == eGameInteractionResponse_Pong)
			return "RESPONSE:ping:pong";
	}

	if (asCommand == "getpos")
		return mGameAdapter.IsMapLoaded() ? FormatPosition("getpos", mGameAdapter.GetPeerState()) : "RESPONSE:getpos:no map loaded";
	if (asCommand == "getrot")
		return mGameAdapter.IsMapLoaded() ? FormatRotation("getrot", mGameAdapter.GetPeerState()) : "RESPONSE:getrot:no map loaded";
	if (asCommand == "getposrot")
	{
		if (!mGameAdapter.IsMapLoaded())
			return "RESPONSE:getposrot:no map loaded";
		const cLegacyPeerState state = mGameAdapter.GetPeerState();
		char response[160];
		sprintf(response, "RESPONSE:getposrot:%.2f, %.2f, %.2f:%.2f, %.2f, %.2f",
			state.mfPositionX, state.mfPositionY, state.mfPositionZ,
			state.mfYawRadians * kRadiansToDegrees,
			state.mfPitchRadians * kRadiansToDegrees,
			0.0f);
		return response;
	}
	if (asCommand == "getmap")
		return mGameAdapter.IsMapLoaded() ? "RESPONSE:getmap:" + mGameAdapter.GetMapFile() : "RESPONSE:getmap:no map loaded";
	if (asCommand.compare(0, 5, "exec:") == 0)
	{
		if (!mGameAdapter.IsMapLoaded())
			return "RESPONSE:exec:no map loaded";
		mGameAdapter.RunScript(asCommand.substr(5));
		return "RESPONSE:exec:script executed";
	}
	return "WARNING:Unknown command";
}
