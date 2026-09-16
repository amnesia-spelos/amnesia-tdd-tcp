#include "LegacyGameInteractionProtocol.h"

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
		cLegacyGameInteractionProtocol protocol(gateway, adapter);
		const std::string kind = ReadString(line, "kind");
		std::string message;
		if (kind == "greeting") message = cLegacyGameInteractionProtocol::Greeting();
		else if (kind == "command") message = protocol.HandleCommand(
			cLegacyGameInteractionProtocol::FirstCommandFromReceive(ReadString(line, "request_wire")));
		else if (kind == "map_changed_event") message = cLegacyGameInteractionProtocol::SerializeEvent(
			cGameInteractionEvent(eGameInteractionEvent_MapChanged, adapter.msMapFile));
		else if (kind == "script_call_observation") message = cLegacyGameInteractionProtocol::SerializeEvent(
			cGameInteractionEvent(eGameInteractionEvent_ScriptCallObserved, ReadString(line, "script_call")));
		else
		{
			std::cerr << "unknown fixture kind in case " << cases << "\n";
			++failures;
			continue;
		}

		const std::string actual = cLegacyGameInteractionProtocol::ToWireLine(message);
		const std::string expected = ReadString(line, "expected_wire");
		if (actual != expected)
		{
			std::cerr << "FAIL: " << ReadString(line, "name") << "\nexpected: " << expected << "actual: " << actual;
			++failures;
		}
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
