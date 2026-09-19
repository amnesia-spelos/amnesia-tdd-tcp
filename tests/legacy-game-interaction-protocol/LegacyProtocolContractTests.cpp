#include "GameInteractionGateway.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef GetMessage
#undef GetMessage
#endif

namespace
{
	std::string ReadQuoted(const std::string& line, std::string::size_type& position)
	{
		std::string value;
		bool escaped = false;
		for (; position < line.length(); ++position)
		{
			const char character = line[position];
			if (escaped)
			{
				if (character == 'n') value += '\n';
				else if (character == 'r') value += '\r';
				else if (character == 't') value += '\t';
				else value += character;
				escaped = false;
			}
			else if (character == '\\') escaped = true;
			else if (character == '"') break;
			else value += character;
		}
		return value;
	}

	std::string ReadString(const std::string& line, const std::string& key)
	{
		const std::string marker = "\"" + key + "\":\"";
		std::string::size_type position = line.find(marker);
		if (position == std::string::npos) return "";
		position += marker.length();
		return ReadQuoted(line, position);
	}

	std::vector<std::string> ReadStrings(const std::string& line, const std::string& key)
	{
		std::vector<std::string> values;
		const std::string marker = "\"" + key + "\":[";
		std::string::size_type position = line.find(marker);
		if (position == std::string::npos) return values;
		position += marker.length();
		while (position < line.length() && line[position] == '"')
		{
			++position;
			values.push_back(ReadQuoted(line, position));
			++position;
			if (position < line.length() && line[position] == ',') ++position;
		}
		return values;
	}

	bool ReadBool(const std::string& line, const std::string& key)
	{
		return line.find("\"" + key + "\":true") != std::string::npos;
	}

	std::wstring Utf8ToWide(const std::string& text)
	{
		if (text.empty()) return std::wstring();
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
			static_cast<int>(text.size()), NULL, 0);
		std::wstring result(count, L'\0');
		if (count > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
			static_cast<int>(text.size()), &result[0], count);
		return result;
	}

	std::string ReadHex(const std::string& line, const std::string& key)
	{
		const std::string hex = ReadString(line, key);
		std::string bytes;
		for (std::string::size_type index = 0; index + 1 < hex.size(); index += 2)
		{
			unsigned int value = 0;
			std::stringstream pair(hex.substr(index, 2));
			pair >> std::hex >> value;
			bytes += static_cast<char>(value);
		}
		return bytes;
	}

	void ReadNumbers(const std::string& line, const std::string& key, float* values, int count)
	{
		const std::string marker = "\"" + key + "\":[";
		std::string::size_type position = line.find(marker);
		if (position == std::string::npos) return;
		std::stringstream numbers(line.substr(position + marker.length()));
		for (int index = 0; index < count; ++index)
		{
			numbers >> values[index];
			if (index + 1 < count) numbers.ignore(1, ',');
		}
	}

	class cFixtureGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		bool mbMapLoaded;
		cGameInteractionPosition mPosition;
		cGameInteractionRotation mRotation;
		std::string msMapFile;
		std::string mExecutedScript;
		bool mbChatAvailable;
		int mlDisplayedCount;
		std::wstring msDisplayedAuthor;
		std::wstring msDisplayedMessage;
		std::vector<cGameInteractionCustomStory> mvCustomStories;
		bool mbInMainMenu;
		std::wstring msInvalidCustomStory;
		std::vector<std::wstring> mvStartedCustomStories;

		virtual bool IsMapLoaded() const { return mbMapLoaded; }
		virtual cGameInteractionPosition GetPosition() const { return mPosition; }
		virtual cGameInteractionRotation GetRotation() const { return mRotation; }
		virtual std::string GetMapFile() const { return msMapFile; }
		virtual void RunScript(const std::string& asScript) { mExecutedScript = asScript; }
		virtual bool DisplayChatEntry(const cChatEntry& entry)
		{
			if (!mbChatAvailable) return false;
			++mlDisplayedCount;
			msDisplayedAuthor = entry.GetAuthor();
			msDisplayedMessage = entry.GetMessage();
			return true;
		}
		virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const { return mvCustomStories; }
		virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(const std::wstring& identifier) const
		{
			if (!mbInMainMenu) return eGameInteractionCustomStoryAvailability_NotInMainMenu;
			if (identifier == msInvalidCustomStory) return eGameInteractionCustomStoryAvailability_Invalid;
			for (std::vector<cGameInteractionCustomStory>::size_type index = 0; index < mvCustomStories.size(); ++index)
				if (mvCustomStories[index].GetIdentifier() == identifier)
					return eGameInteractionCustomStoryAvailability_Available;
			return eGameInteractionCustomStoryAvailability_NotFound;
		}
		virtual void StartCustomStory(const std::wstring& identifier) { mvStartedCustomStories.push_back(identifier); }
		virtual eGameInteractionLocalPoseAvailability GetLocalPoseAvailability() const
		{
			return eGameInteractionLocalPoseAvailability_Unavailable;
		}
		virtual cGameInteractionPose GetLocalPose() const { return cGameInteractionPose(); }
		virtual bool CreateAvatar(const std::string&, const std::string&) { return true; }
		virtual void RemoveAvatar(const std::string&) {}
		virtual void PoseAvatar(const std::string&, const cGameInteractionPose&) {}
		virtual void SetAvatarCollision(const std::string&, bool) {}
	};

	SOCKET Connect(cGameInteractionGateway& gateway, cFixtureGameAdapter& adapter)
	{
		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<u_short>(gateway.GetPort()));
		if (connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return INVALID_SOCKET;
		gateway.Update(adapter);
		gateway.Update(adapter);
		return peer;
	}

	std::string Receive(SOCKET peer)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(peer, &readable);
		timeval timeout = { 2, 0 };
		if (select(0, &readable, NULL, NULL, &timeout) != 1) return std::string();
		char buffer[2048];
		const int count = recv(peer, buffer, sizeof(buffer), 0);
		return count > 0 ? std::string(buffer, count) : std::string();
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: LegacyProtocolContractTests <contract.jsonl>\n";
		return 2;
	}

	std::ifstream fixtures(argv[1]);
	if (!fixtures)
	{
		std::cerr << "could not open fixtures: " << argv[1] << "\n";
		return 2;
	}

	int failures = 0;
	int cases = 0;
	std::string line;
	while (std::getline(fixtures, line))
	{
		++cases;
		cFixtureGameAdapter adapter;
		adapter.mbMapLoaded = ReadBool(line, "map_loaded");
		adapter.mbChatAvailable = !ReadBool(line, "chat_unavailable");
		adapter.mlDisplayedCount = 0;
		adapter.msMapFile = ReadString(line, "map_file");
		float position[3] = { 0.0f, 0.0f, 0.0f };
		float rotation[2] = { 0.0f, 0.0f };
		ReadNumbers(line, "position", position, 3);
		ReadNumbers(line, "rotation_radians", rotation, 2);
		adapter.mPosition = cGameInteractionPosition(position[0], position[1], position[2]);
		adapter.mRotation = cGameInteractionRotation(rotation[0], rotation[1]);
		adapter.mbInMainMenu = ReadBool(line, "in_main_menu");
		adapter.msInvalidCustomStory = Utf8ToWide(ReadString(line, "invalid_custom_story"));
		const std::vector<std::string> storyIds = ReadStrings(line, "custom_story_ids");
		const std::vector<std::string> storyNames = ReadStrings(line, "custom_story_names");
		for (std::vector<std::string>::size_type index = 0; index < storyIds.size() && index < storyNames.size(); ++index)
			adapter.mvCustomStories.push_back(cGameInteractionCustomStory(
				Utf8ToWide(storyIds[index]), Utf8ToWide(storyNames[index])));

		cGameInteractionGateway gateway;
		if (!gateway.Listen("127.0.0.1", 0))
		{
			std::cerr << "could not listen for fixture " << cases << ": " << gateway.GetDiagnostic() << "\n";
			return 2;
		}
		SOCKET peer = Connect(gateway, adapter);
		if (peer == INVALID_SOCKET)
		{
			std::cerr << "could not connect fixture Peer " << cases << "\n";
			return 2;
		}
		const std::string greeting = Receive(peer);
		const std::string kind = ReadString(line, "kind");
		std::string actual;
		if (kind == "greeting") actual = greeting;
		else if (kind == "command" || kind == "fragmented_command" || kind == "coalesced_commands")
		{
			std::string request = ReadString(line, "request_wire");
			if (request.empty()) request = ReadHex(line, "request_hex");
			const std::string first = ReadString(line, "request_wire_1");
			const std::string second = ReadString(line, "request_wire_2");
			if (!first.empty())
			{
				send(peer, first.data(), static_cast<int>(first.size()), 0);
				gateway.Update(adapter);
			}
			if (!second.empty()) request = second;
			send(peer, request.data(), static_cast<int>(request.size()), 0);
			gateway.Update(adapter);
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else if (kind == "local_chat_event")
		{
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted,
				Utf8ToWide(ReadString(line, "author")), Utf8ToWide(ReadString(line, "message"))));
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else if (kind == "local_chat_event_without_peer")
		{
			closesocket(peer);
			gateway.Shutdown();
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted,
				L"Daniel", L"discard me"));
			gateway.Listen("127.0.0.1", 0);
			peer = Connect(gateway, adapter);
			actual = Receive(peer);
		}
		else if (kind == "disconnect_regression")
		{
			const std::string partial = ReadString(line, "request_wire_1");
			send(peer, partial.data(), static_cast<int>(partial.size()), 0);
			gateway.Update(adapter);
			closesocket(peer);
			for (int update = 0; update < 50 && gateway.GetDiagnostic().empty(); ++update)
				gateway.Update(adapter);
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted,
				L"Daniel", L"discard me"));
			peer = Connect(gateway, adapter);
			actual = Receive(peer);
			const std::string suffix = ReadString(line, "request_wire_2");
			send(peer, suffix.data(), static_cast<int>(suffix.size()), 0);
			gateway.Update(adapter);
			gateway.Update(adapter);
			actual += Receive(peer);
		}
		else if (kind == "map_changed_event")
		{
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_MapChanged, adapter.msMapFile));
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else if (kind == "custom_story_started_event")
		{
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_CustomStoryStarted,
				Utf8ToWide(ReadString(line, "custom_story_id"))));
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else if (kind == "script_call_observation")
		{
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_ScriptCallObserved,
				ReadString(line, "script_call")));
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else
		{
			std::cerr << "unknown fixture kind in case " << cases << "\n";
			++failures;
			continue;
		}

		const std::string expected = ReadString(line, "expected_wire");
		if (actual != expected)
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << "\nexpected: " << expected << "actual: " << actual;
			++failures;
		}
		closesocket(peer);
		gateway.Shutdown();
		const std::string expectedScript = ReadString(line, "expected_script");
		if (!expectedScript.empty() && adapter.mExecutedScript != expectedScript)
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << " did not execute expected script\n";
			++failures;
		}
		const std::string expectedAuthor = ReadString(line, "expected_author");
		const std::string expectedMessage = ReadString(line, "expected_message");
		if ((!expectedAuthor.empty() || !expectedMessage.empty()) &&
			(adapter.mlDisplayedCount != 1 || adapter.msDisplayedAuthor != Utf8ToWide(expectedAuthor) ||
			 adapter.msDisplayedMessage != Utf8ToWide(expectedMessage)))
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << " did not display expected Chat Entry\n";
			++failures;
		}
		const std::string expectedStart = ReadString(line, "expected_started_custom_story");
		if (!expectedStart.empty() && (adapter.mvStartedCustomStories.size() != 1 ||
			adapter.mvStartedCustomStories[0] != Utf8ToWide(expectedStart)))
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << " did not start the expected Custom Story once\n";
			++failures;
		}
		if (ReadBool(line, "expected_no_start") && !adapter.mvStartedCustomStories.empty())
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << " started a Custom Story\n";
			++failures;
		}
		if (ReadBool(line, "expected_no_display") && adapter.mlDisplayedCount != 0)
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << " displayed a partial Chat Entry\n";
			++failures;
		}
	}

	if (failures != 0)
	{
		std::cerr << failures << " failure(s) across " << cases << " contract cases\n";
		return 1;
	}
	std::cout << cases << " legacy Game Interaction Protocol contract cases passed\n";
	return 0;
}
