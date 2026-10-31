#include "LegacyGameInteractionProtocol.h"
#include "Utf8Text.h"

#include <cstdio>

cGameInteractionLineBuffer::cGameInteractionLineBuffer(std::string::size_type aMaximumLineLength)
	: mSearchStart(0), mMaximumLineLength(aMaximumLineLength), mbExceededLineLimit(false)
{
}

void cGameInteractionLineBuffer::Append(const char* apBytes, std::string::size_type aLength)
{
	msPendingBytes.append(apBytes, aLength);
}

bool cGameInteractionLineBuffer::TryPopLine(std::string& asLine)
{
	if (mbExceededLineLimit) return false;
	const std::string::size_type delimiter = msPendingBytes.find('\n', mSearchStart);
	if (delimiter == std::string::npos)
	{
		mSearchStart = msPendingBytes.length();
		// One byte of slack keeps a CR whose LF has not arrived yet from counting against the line.
		const std::string::size_type length = msPendingBytes.length();
		mbExceededLineLimit = length > mMaximumLineLength + 1 ||
			(length == mMaximumLineLength + 1 && msPendingBytes[length - 1] != '\r');
		return false;
	}

	std::string::size_type lineLength = delimiter;
	if (lineLength > 0 && msPendingBytes[lineLength - 1] == '\r') --lineLength;
	if (lineLength > mMaximumLineLength)
	{
		mbExceededLineLimit = true;
		return false;
	}
	asLine = msPendingBytes.substr(0, lineLength);
	msPendingBytes.erase(0, delimiter + 1);
	mSearchStart = 0;
	return true;
}

void cGameInteractionLineBuffer::Clear()
{
	msPendingBytes.clear();
	mSearchStart = 0;
	mbExceededLineLimit = false;
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

	std::string FormatCustomStories(const std::vector<cGameInteractionCustomStory>& avCustomStories)
	{
		std::string formatted;
		for (std::vector<cGameInteractionCustomStory>::const_iterator story = avCustomStories.begin();
			story != avCustomStories.end(); ++story)
		{
			if (story != avCustomStories.begin()) formatted += '\t';
			std::wstring name = story->GetName();
			for (std::wstring::size_type index = 0; index < name.size(); ++index)
				if (name[index] == L'\t' || name[index] == L'\r' || name[index] == L'\n') name[index] = L' ';
			formatted += EncodeUtf8(story->GetIdentifier()) + "|" + EncodeUtf8(name);
		}
		return formatted;
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
	case eGameInteractionEvent_LocalChatSubmitted:
		return "EVENT:CHAT:" + EncodeUtf8(aEvent.GetChatAuthor()) + ":" +
			EncodeUtf8(aEvent.GetChatMessage());
	case eGameInteractionEvent_CustomStoryStarted:
		return "EVENT:CustomStoryStarted:" + EncodeUtf8(aEvent.GetCustomStoryIdentifier());
	case eGameInteractionEvent_MainStoryStarted:
		return "EVENT:MainStoryStarted";
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
	if (asCommand.compare(0, 17, "startcustomstory:") == 0)
	{
		std::wstring identifier;
		// Malformed UTF-8 cannot name an installed folder, so it resolves to "not found".
		if (!TryDecodeUtf8(asCommand.substr(17), identifier)) identifier.assign(1, static_cast<wchar_t>(1));
		return cGameInteractionCommand(eGameInteractionCommand_StartCustomStory, identifier);
	}
	if (asCommand == "startmainstory")
		return cGameInteractionCommand(eGameInteractionCommand_StartMainStory);
	if (asCommand == "getcustomstories")
		return cGameInteractionCommand(eGameInteractionCommand_GetCustomStories);
	if (asCommand == "ping" || asCommand == "getpos" || asCommand == "getrot" ||
		asCommand == "getposrot" || asCommand == "getmap")
		return cGameInteractionCommand(CommandTypeFor(asCommand));
	return cGameInteractionCommand(eGameInteractionCommand_Unknown, asCommand);
}

std::string cLegacyGameInteractionProtocol::SerializeResponse(const cGameInteractionResponse& aResponse)
{
	if (aResponse.GetCommandType() == eGameInteractionCommand_Unknown)
		return "WARNING:Unknown command";
	if (aResponse.GetCommandType() == eGameInteractionCommand_StartCustomStory)
	{
		switch (aResponse.GetOutcome())
		{
		case eGameInteractionCommandOutcome_Success: return "RESPONSE:startcustomstory:starting";
		case eGameInteractionCommandOutcome_CustomStoryNotFound: return "RESPONSE:startcustomstory:not found";
		case eGameInteractionCommandOutcome_CustomStoryInvalid: return "RESPONSE:startcustomstory:invalid";
		default: return "RESPONSE:startcustomstory:not in main menu";
		}
	}
	if (aResponse.GetCommandType() == eGameInteractionCommand_StartMainStory)
		return aResponse.GetOutcome() == eGameInteractionCommandOutcome_Success ? "RESPONSE:startmainstory:starting" :
			"RESPONSE:startmainstory:not in main menu";
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
	if (aResponse.GetType() == eGameInteractionResponse_CustomStories)
		return "RESPONSE:getcustomstories:" + FormatCustomStories(aResponse.GetCustomStories());
	return "RESPONSE:exec:script executed";
}
