#include "GameInteractionGateway.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
	std::string ReadString(const std::string& line, const std::string& key)
	{
		const std::string marker = "\"" + key + "\":\"";
		std::string::size_type position = line.find(marker);
		if (position == std::string::npos) return "";
		position += marker.length();
		std::string value;
		bool escaped = false;
		for (; position < line.length(); ++position)
		{
			const char character = line[position];
			if (escaped)
			{
				if (character == 'n') value += '\n';
				else if (character == 'r') value += '\r';
				else value += character;
				escaped = false;
			}
			else if (character == '\\') escaped = true;
			else if (character == '"') break;
			else value += character;
		}
		return value;
	}

	bool ReadBool(const std::string& line, const std::string& key)
	{
		return line.find("\"" + key + "\":true") != std::string::npos;
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

		virtual bool IsMapLoaded() const { return mbMapLoaded; }
		virtual cGameInteractionPosition GetPosition() const { return mPosition; }
		virtual cGameInteractionRotation GetRotation() const { return mRotation; }
		virtual std::string GetMapFile() const { return msMapFile; }
		virtual void RunScript(const std::string& asScript) { mExecutedScript = asScript; }
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
		adapter.msMapFile = ReadString(line, "map_file");
		float position[3] = { 0.0f, 0.0f, 0.0f };
		float rotation[2] = { 0.0f, 0.0f };
		ReadNumbers(line, "position", position, 3);
		ReadNumbers(line, "rotation_radians", rotation, 2);
		adapter.mPosition = cGameInteractionPosition(position[0], position[1], position[2]);
		adapter.mRotation = cGameInteractionRotation(rotation[0], rotation[1]);

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
		else if (kind == "command")
		{
			const std::string request = ReadString(line, "request_wire");
			send(peer, request.data(), static_cast<int>(request.size()), 0);
			gateway.Update(adapter);
			gateway.Update(adapter);
			actual = Receive(peer);
		}
		else if (kind == "map_changed_event")
		{
			gateway.Report(cGameInteractionEvent(eGameInteractionEvent_MapChanged, adapter.msMapFile));
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
	}

	if (failures != 0)
	{
		std::cerr << failures << " failure(s) across " << cases << " contract cases\n";
		return 1;
	}
	std::cout << cases << " legacy Game Interaction Protocol contract cases passed\n";
	return 0;
}
