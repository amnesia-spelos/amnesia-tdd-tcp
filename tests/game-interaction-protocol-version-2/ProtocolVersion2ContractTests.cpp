#include "GameInteractionProtocolVersion2.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <clocale>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

	int ReadInteger(const std::string& line, const std::string& key)
	{
		const std::string marker = "\"" + key + "\":";
		const std::string::size_type position = line.find(marker);
		if (position == std::string::npos) return 0;
		std::istringstream stream(line.substr(position + marker.length()));
		int value = 0;
		stream >> value;
		return value;
	}

	std::string JoinFields(const std::vector<std::string>& fields)
	{
		std::string joined;
		for (std::vector<std::string>::size_type index = 0; index < fields.size(); ++index)
			joined += "[" + fields[index] + "]";
		return joined;
	}

	// Fixture numbers are read with the classic locale so the harness itself never depends on the
	// comma-decimal locale selected to exercise the protocol helpers.
	double ReadClassicNumber(const std::string& text)
	{
		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		double value = 0.0;
		stream >> value;
		return value;
	}

	class cFixtureGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		virtual bool IsMapLoaded() const { return true; }
		virtual cGameInteractionPosition GetPosition() const { return cGameInteractionPosition(); }
		virtual cGameInteractionRotation GetRotation() const { return cGameInteractionRotation(); }
		virtual std::string GetMapFile() const { return "maps/main/level01.map"; }
		virtual void RunScript(const std::string&) {}
		virtual bool DisplayChatEntry(const cChatEntry&) { return true; }
		virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const
		{
			return std::vector<cGameInteractionCustomStory>();
		}
		virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(const std::wstring&) const
		{
			return eGameInteractionCustomStoryAvailability_NotFound;
		}
		virtual void StartCustomStory(const std::wstring&) {}
		virtual eGameInteractionLocalPoseAvailability GetLocalPoseAvailability() const
		{
			return eGameInteractionLocalPoseAvailability_Live;
		}
		virtual cGameInteractionPose GetLocalPose() const
		{
			cGameInteractionPose pose;
			pose.mlTimeMs = 123456;
			pose.mlTeleportCounter = 3;
			pose.mFeetPosition = cGameInteractionPosition(1.25f, -2.5f, 3.75f);
			pose.mfBodyYawDegrees = 90.0f;
			pose.mfCameraPitchDegrees = -45.0f;
			pose.mbCrouching = true;
			pose.mbLanternRaised = true;
			pose.msMapFile = "custom_stories/My Story: Part 2/maps/cellar one.map";
			return pose;
		}
		virtual bool CreateAvatar(const std::string&, const std::string& asEntityFile)
		{
			return asEntityFile != "entities/missing.ent";
		}
		virtual void RemoveAvatar(const std::string&) {}
		virtual void PoseAvatar(const std::string&, const cGameInteractionPose&) {}
		virtual void SetAvatarCollision(const std::string&, bool) {}
		virtual cGameInteractionBodySamples GetReportedBodies() const
		{
			cGameInteractionBodySamples report;
			report.mlTimeMs = 123456;
			report.msMapFile = "maps/main/level01.map";
			cGameInteractionBodySample held;
			held.mlEntityId = 12;
			held.mlBodyId = 3;
			held.mState.mPosition = cGameInteractionPosition(1.25f, -2.5f, 3.75f);
			held.mState.mOrientation = cGameInteractionQuaternion(0.0f, 0.70710678f, 0.0f, 0.70710678f);
			held.mState.mLinearVelocity = cGameInteractionVector(0.5f, 0.0f, -1.0f);
			held.mState.mAngularVelocity = cGameInteractionVector(0.0f, 90.0f, 0.0f);
			report.mvBodies.push_back(held);
			cGameInteractionBodySample resting;
			resting.mlEntityId = -7;
			report.mvBodies.push_back(resting);
			return report;
		}
		virtual eGameInteractionEntityOutcome DriveEntity(int alEntityId)
		{
			if (alEntityId == 404) return eGameInteractionEntityOutcome_NotFound;
			if (alEntityId == 405) return eGameInteractionEntityOutcome_NotHoldable;
			msetDrivenEntities.insert(alEntityId);
			return eGameInteractionEntityOutcome_Success;
		}
		virtual eGameInteractionEntityOutcome DriveEntityBodies(const cGameInteractionBodySamples& aSamples,
			int& alFailedEntityId)
		{
			for (size_t index = 0; index < aSamples.mvBodies.size(); ++index)
			{
				const cGameInteractionBodySample& sample = aSamples.mvBodies[index];
				if (msetDrivenEntities.count(sample.mlEntityId) != 0 && sample.mlBodyId != 99) continue;
				alFailedEntityId = sample.mlEntityId;
				return eGameInteractionEntityOutcome_NotFound;
			}
			return eGameInteractionEntityOutcome_Success;
		}
		virtual eGameInteractionEntityOutcome SetDrivenEntityInteracting(int alEntityId, bool)
		{
			return msetDrivenEntities.count(alEntityId) != 0 ? eGameInteractionEntityOutcome_Success :
				eGameInteractionEntityOutcome_NotFound;
		}
		virtual eGameInteractionEntityOutcome BreakDrivenEntity(int alEntityId, const cGameInteractionBodyState&)
		{
			return ReleaseDrivenEntity(alEntityId);
		}
		virtual eGameInteractionEntityOutcome ReleaseDrivenEntity(int alEntityId)
		{
			return msetDrivenEntities.erase(alEntityId) != 0 ? eGameInteractionEntityOutcome_Success :
				eGameInteractionEntityOutcome_NotFound;
		}
		virtual void ReleaseDrivenEntities() { msetDrivenEntities.clear(); }

	private:
		std::set<int> msetDrivenEntities;
	};

	cGameInteractionBodyState ReadBodyState(std::istringstream& aFields)
	{
		double values[13] = {};
		for (int index = 0; index < 13; ++index) aFields >> values[index];
		cGameInteractionBodyState state;
		state.mPosition = cGameInteractionPosition(static_cast<float>(values[0]), static_cast<float>(values[1]),
			static_cast<float>(values[2]));
		state.mOrientation = cGameInteractionQuaternion(static_cast<float>(values[3]), static_cast<float>(values[4]),
			static_cast<float>(values[5]), static_cast<float>(values[6]));
		state.mLinearVelocity = cGameInteractionVector(static_cast<float>(values[7]),
			static_cast<float>(values[8]), static_cast<float>(values[9]));
		state.mAngularVelocity = cGameInteractionVector(static_cast<float>(values[10]),
			static_cast<float>(values[11]), static_cast<float>(values[12]));
		return state;
	}

	// Fixture numbers are read with the classic locale; see ReadClassicNumber.
	std::istringstream ClassicStream(const std::string& text)
	{
		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		return stream;
	}

	std::string FormatSamples(const cGameInteractionBodySamples& aSamples)
	{
		char clockFields[64];
		sprintf(clockFields, "%llu %u", aSamples.mlTimeMs, static_cast<unsigned int>(aSamples.mvBodies.size()));
		std::string text = clockFields;
		for (size_t index = 0; index < aSamples.mvBodies.size(); ++index)
		{
			const cGameInteractionBodySample& sample = aSamples.mvBodies[index];
			text += " " + cGameInteractionProtocolVersion2::FormatEntityIdentifier(sample.mlEntityId) + " " +
				cGameInteractionProtocolVersion2::FormatEntityIdentifier(sample.mlBodyId) + " " +
				cGameInteractionProtocolVersion2::FormatBodyState(sample.mState);
		}
		return text;
	}

	// Writes a parsed Peer-Driven Entity Command back in its own grammar, so a fixture shows every
	// field it was read into.
	std::string DescribeEntityCommand(const cGameInteractionCommand& aCommand)
	{
		const cGameInteractionEntityRequest& request = aCommand.GetEntityRequest();
		if (!request.mbValid) return "invalid";
		const std::string entity = cGameInteractionProtocolVersion2::FormatEntityIdentifier(request.mlEntityId);
		switch (aCommand.GetType())
		{
		case eGameInteractionCommand_EntityDrive: return "entitydrive " + entity + " " + request.msMapFile;
		case eGameInteractionCommand_EntityRelease: return "entityrelease " + entity + " " + request.msMapFile;
		case eGameInteractionCommand_EntityInteracting:
			return "entityinteracting " + entity + (request.mbInteracting ? " 1 " : " 0 ") + request.msMapFile;
		case eGameInteractionCommand_EntityBreak:
			return "entitybreak " + entity + " " + cGameInteractionProtocolVersion2::FormatBodyState(request.mState) +
				" " + request.msMapFile;
		case eGameInteractionCommand_EntityBodies:
			return "entitybodies " + FormatSamples(request.mBodies) + " " + request.msMapFile;
		default: return "not an entity Command";
		}
	}

	eGameInteractionEventType InteractionEventTypeNamed(const std::string& name)
	{
		if (name == "interactionstart") return eGameInteractionEvent_InteractionStarted;
		if (name == "interactionend") return eGameInteractionEvent_InteractionEnded;
		if (name == "reportcontact") return eGameInteractionEvent_ReportContact;
		if (name == "reportsettled") return eGameInteractionEvent_ReportSettled;
		return eGameInteractionEvent_ReportBroke;
	}

	eGameInteractionEnding EndingNamed(const std::string& name)
	{
		if (name == "thrown") return eGameInteractionEnding_Thrown;
		if (name == "too-far") return eGameInteractionEnding_TooFar;
		if (name == "destroyed") return eGameInteractionEnding_Destroyed;
		return eGameInteractionEnding_Released;
	}

	std::string Receive(SOCKET peer, long microseconds)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(peer, &readable);
		timeval timeout = { 0, microseconds };
		if (select(0, &readable, NULL, NULL, &timeout) != 1) return std::string();
		char buffer[4096];
		const int count = recv(peer, buffer, sizeof(buffer), 0);
		return count > 0 ? std::string(buffer, count) : std::string();
	}

	std::string ReceiveAll(SOCKET peer)
	{
		std::string received;
		for (std::string chunk = Receive(peer, 500000); !chunk.empty(); chunk = Receive(peer, 50000))
			received += chunk;
		return received;
	}

	// Sends the fixture's Commands in one write and returns everything the Session answered.
	bool TryRunSession(const std::string& request, std::string& output)
	{
		cFixtureGameAdapter adapter;
		cGameInteractionGateway gateway;
		if (!gateway.Listen("127.0.0.1", 0)) return false;
		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<u_short>(gateway.GetPort()));
		if (connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return false;
		gateway.Update(adapter);
		const std::string greeting = "Hello, from Amnesia: The Dark Descent!\n";
		std::string received = ReceiveAll(peer);
		send(peer, request.data(), static_cast<int>(request.size()), 0);
		for (int update = 0; update < 3; ++update) gateway.Update(adapter);
		received += ReceiveAll(peer);
		closesocket(peer);
		gateway.Shutdown();
		if (received.compare(0, greeting.size(), greeting) != 0) return false;
		output = received.substr(greeting.size());
		return true;
	}

	bool SelectCommaDecimalLocale()
	{
		if (!setlocale(LC_ALL, "de-DE")) return false;
		char formatted[16];
		sprintf(formatted, "%.1f", 1.5);
		return std::string(formatted) == "1,5";
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: ProtocolVersion2ContractTests <contract.jsonl>\n";
		return 2;
	}

	std::ifstream fixtures(argv[1]);
	if (!fixtures)
	{
		std::cerr << "could not open fixtures: " << argv[1] << "\n";
		return 2;
	}
	if (!SelectCommaDecimalLocale())
	{
		std::cerr << "could not select a comma-decimal locale to prove locale independence\n";
		return 2;
	}

	int failures = 0;
	int cases = 0;
	std::string line;
	while (std::getline(fixtures, line))
	{
		++cases;
		const std::string name = ReadString(line, "name");
		const std::string kind = ReadString(line, "kind");
		std::string actual;
		std::string expected;
		if (kind == "session")
		{
			if (!TryRunSession(ReadString(line, "request_wire"), actual))
			{
				std::cerr << "could not run the Session for fixture " << cases << "\n";
				return 2;
			}
			expected = ReadString(line, "expected_wire");
		}
		else if (kind == "format_number")
		{
			actual = cGameInteractionProtocolVersion2::FormatNumber(ReadClassicNumber(ReadString(line, "value")));
			expected = ReadString(line, "expected_text");
		}
		else if (kind == "parse_number")
		{
			double value = 0.0;
			const bool valid = cGameInteractionProtocolVersion2::TryParseNumber(ReadString(line, "text"), value);
			actual = valid ? "valid" : "invalid";
			expected = ReadBool(line, "valid") ? "valid" : "invalid";
			if (valid && value != ReadClassicNumber(ReadString(line, "expected_value")))
				actual = "valid but wrong value";
		}
		else if (kind == "fields")
		{
			cGameInteractionFieldReader reader(ReadString(line, "line"));
			std::vector<std::string> fields;
			bool valid = true;
			const int fieldCount = ReadInteger(line, "field_count");
			for (int index = 0; valid && index < fieldCount; ++index)
			{
				std::string field;
				valid = reader.TryReadField(field);
				fields.push_back(field);
			}
			if (valid && ReadBool(line, "rest"))
			{
				std::string rest;
				valid = reader.TryReadRest(rest);
				fields.push_back(rest);
			}
			valid = valid && reader.IsAtEnd();
			actual = valid ? JoinFields(fields) : "invalid";
			expected = ReadBool(line, "valid") ? JoinFields(ReadStrings(line, "expected_fields")) : "invalid";
		}
		else if (kind == "local_pose")
		{
			cGameInteractionPose pose;
			std::istringstream(ReadString(line, "time_ms")) >> pose.mlTimeMs;
			std::istringstream(ReadString(line, "teleport_counter")) >> pose.mlTeleportCounter;
			pose.mFeetPosition = cGameInteractionPosition(
				static_cast<float>(ReadClassicNumber(ReadString(line, "x"))),
				static_cast<float>(ReadClassicNumber(ReadString(line, "y"))),
				static_cast<float>(ReadClassicNumber(ReadString(line, "z"))));
			pose.mfBodyYawDegrees = static_cast<float>(ReadClassicNumber(ReadString(line, "yaw")));
			pose.mfCameraPitchDegrees = static_cast<float>(ReadClassicNumber(ReadString(line, "pitch")));
			pose.mbCrouching = ReadBool(line, "crouch");
			pose.mbLanternRaised = ReadBool(line, "lantern");
			pose.msMapFile = ReadString(line, "map");
			actual = cGameInteractionProtocolVersion2::SerializeLocalPose(pose);
			expected = ReadString(line, "expected_text");
		}
		else if (kind == "avatar_pose")
		{
			const cGameInteractionAvatarRequest request =
				cGameInteractionProtocolVersion2::ParseCommand(ReadString(line, "line")).GetAvatarRequest();
			const cGameInteractionPose& pose = request.mPose;
			actual = !request.mbValid ? "invalid" : std::string("crouch ") + (pose.mbCrouching ? "1" : "0") +
				" lantern " + (pose.mbLanternRaised ? "1" : "0") + " map " + pose.msMapFile;
			expected = !ReadBool(line, "valid") ? "invalid" : std::string("crouch ") +
				(ReadBool(line, "crouch") ? "1" : "0") + " lantern " + (ReadBool(line, "lantern") ? "1" : "0") +
				" map " + ReadString(line, "map");
		}
		else if (kind == "avatar_identifier")
		{
			actual = cGameInteractionProtocolVersion2::IsValidAvatarIdentifier(ReadString(line, "text")) ?
				"valid" : "invalid";
			expected = ReadBool(line, "valid") ? "valid" : "invalid";
		}
		else if (kind == "entity_identifier")
		{
			int value = 0;
			const bool valid = cGameInteractionProtocolVersion2::TryParseEntityIdentifier(ReadString(line, "text"), value);
			actual = valid ? cGameInteractionProtocolVersion2::FormatEntityIdentifier(value) : "invalid";
			expected = ReadBool(line, "valid") ? ReadString(line, "expected_value") : "invalid";
		}
		else if (kind == "entity_command")
		{
			actual = DescribeEntityCommand(cGameInteractionProtocolVersion2::ParseCommand(ReadString(line, "line")));
			expected = ReadBool(line, "valid") ? ReadString(line, "expected_text") : "invalid";
		}
		else if (kind == "reported_bodies")
		{
			cGameInteractionBodySamples report;
			std::istringstream(ReadString(line, "time_ms")) >> report.mlTimeMs;
			report.msMapFile = ReadString(line, "map");
			std::istringstream entries = ClassicStream(ReadString(line, "entries"));
			cGameInteractionBodySample sample;
			while (entries >> sample.mlEntityId >> sample.mlBodyId)
			{
				sample.mState = ReadBodyState(entries);
				report.mvBodies.push_back(sample);
			}
			actual = cGameInteractionProtocolVersion2::SerializeReportedBodies(report);
			expected = ReadString(line, "expected_text");
		}
		else if (kind == "interaction_event")
		{
			cGameInteractionEntityEvent entityEvent;
			std::istringstream(ReadString(line, "entity_id")) >> entityEvent.mlEntityId;
			std::istringstream(ReadString(line, "body_id")) >> entityEvent.mlBodyId;
			entityEvent.mEnding = EndingNamed(ReadString(line, "ending"));
			std::istringstream state = ClassicStream(ReadString(line, "state"));
			entityEvent.mState = ReadBodyState(state);
			entityEvent.msMapFile = ReadString(line, "map");
			actual = cGameInteractionProtocolVersion2::SerializeEvent(
				cGameInteractionEvent(InteractionEventTypeNamed(ReadString(line, "event")), entityEvent));
			expected = ReadString(line, "expected_text");
		}
		else
		{
			std::cerr << "unknown fixture kind in case " << cases << "\n";
			++failures;
			continue;
		}

		if (actual != expected)
		{
			std::cerr << "FAIL: " << name << "\nexpected: " << expected << "\nactual: " << actual << "\n";
			++failures;
		}
	}

	if (failures != 0)
	{
		std::cerr << failures << " failure(s) across " << cases << " contract cases\n";
		return 1;
	}
	std::cout << cases << " Protocol Version 2 contract cases passed\n";
	return 0;
}
