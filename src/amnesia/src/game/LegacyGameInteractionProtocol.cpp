#include "LegacyGameInteractionProtocol.h"

#include <cstdio>
#include <climits>

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

	bool DecodeUtf8(const std::string& asText, std::wstring& asDecoded)
	{
		asDecoded.clear();
		for (std::string::size_type index = 0; index < asText.size();)
		{
			const unsigned char first = static_cast<unsigned char>(asText[index++]);
			unsigned int scalar = first;
			int continuationCount = 0;
			unsigned int minimum = 0;
			if (first >= 0xC2 && first <= 0xDF) { scalar = first & 0x1F; continuationCount = 1; minimum = 0x80; }
			else if (first >= 0xE0 && first <= 0xEF) { scalar = first & 0x0F; continuationCount = 2; minimum = 0x800; }
			else if (first >= 0xF0 && first <= 0xF4) { scalar = first & 0x07; continuationCount = 3; minimum = 0x10000; }
			else if (first >= 0x80) return false;
			if (index + continuationCount > asText.size()) return false;
			for (int continuation = 0; continuation < continuationCount; ++continuation)
			{
				const unsigned char next = static_cast<unsigned char>(asText[index++]);
				if ((next & 0xC0) != 0x80) return false;
				scalar = (scalar << 6) | (next & 0x3F);
			}
			if ((continuationCount && scalar < minimum) || scalar > 0x10FFFF ||
				(scalar >= 0xD800 && scalar <= 0xDFFF)) return false;
#if WCHAR_MAX <= 0xFFFF
			if (scalar > 0xFFFF)
			{
				scalar -= 0x10000;
				asDecoded += static_cast<wchar_t>(0xD800 + (scalar >> 10));
				asDecoded += static_cast<wchar_t>(0xDC00 + (scalar & 0x3FF));
			}
			else asDecoded += static_cast<wchar_t>(scalar);
#else
			asDecoded += static_cast<wchar_t>(scalar);
#endif
		}
		return true;
	}

	std::string EncodeUtf8(const std::wstring& asText)
	{
		std::string encoded;
		for (std::wstring::size_type index = 0; index < asText.size(); ++index)
		{
			unsigned int scalar = static_cast<unsigned int>(asText[index]);
#if WCHAR_MAX <= 0xFFFF
			if (scalar >= 0xD800 && scalar <= 0xDBFF && index + 1 < asText.size())
			{
				const unsigned int low = static_cast<unsigned int>(asText[++index]);
				scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
			}
#endif
			if (scalar <= 0x7F) encoded += static_cast<char>(scalar);
			else if (scalar <= 0x7FF)
			{
				encoded += static_cast<char>(0xC0 | (scalar >> 6));
				encoded += static_cast<char>(0x80 | (scalar & 0x3F));
			}
			else if (scalar <= 0xFFFF)
			{
				encoded += static_cast<char>(0xE0 | (scalar >> 12));
				encoded += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
				encoded += static_cast<char>(0x80 | (scalar & 0x3F));
			}
			else
			{
				encoded += static_cast<char>(0xF0 | (scalar >> 18));
				encoded += static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
				encoded += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
				encoded += static_cast<char>(0x80 | (scalar & 0x3F));
			}
		}
		return encoded;
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
	if (asCommand.compare(0, 5, "chat:") == 0)
	{
		const std::string::size_type authorDelimiter = asCommand.find(':', 5);
		const std::string authorBytes = asCommand.substr(5,
			authorDelimiter == std::string::npos ? std::string::npos : authorDelimiter - 5);
		const std::string messageBytes = authorDelimiter == std::string::npos ? std::string() :
			asCommand.substr(authorDelimiter + 1);
		std::wstring author;
		std::wstring message;
		if (!DecodeUtf8(authorBytes, author)) author.assign(1, static_cast<wchar_t>(1));
		if (!DecodeUtf8(messageBytes, message)) message.assign(1, static_cast<wchar_t>(1));
		return cGameInteractionCommand(eGameInteractionCommand_Chat, author, message);
	}
	if (asCommand.compare(0, 17, "startcustomstory:") == 0)
	{
		std::wstring identifier;
		// Malformed UTF-8 cannot name an installed folder, so it resolves to "not found".
		if (!DecodeUtf8(asCommand.substr(17), identifier)) identifier.assign(1, static_cast<wchar_t>(1));
		return cGameInteractionCommand(eGameInteractionCommand_StartCustomStory, identifier);
	}
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
	const char* command = "ping";
	if (aResponse.GetCommandType() == eGameInteractionCommand_GetPosition) command = "getpos";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetRotation) command = "getrot";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetPositionRotation) command = "getposrot";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_GetMap) command = "getmap";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_ExecuteScript) command = "exec";
	else if (aResponse.GetCommandType() == eGameInteractionCommand_Chat) command = "chat";
	if (aResponse.GetOutcome() == eGameInteractionCommandOutcome_InvalidAuthor)
		return "RESPONSE:chat:invalid author";
	if (aResponse.GetOutcome() == eGameInteractionCommandOutcome_InvalidMessage)
		return "RESPONSE:chat:invalid message";
	if (aResponse.GetOutcome() == eGameInteractionCommandOutcome_Unavailable)
		return "RESPONSE:chat:unavailable";
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
	if (aResponse.GetType() == eGameInteractionResponse_ChatDisplayed)
		return "RESPONSE:chat:message displayed";
	return "RESPONSE:exec:script executed";
}
