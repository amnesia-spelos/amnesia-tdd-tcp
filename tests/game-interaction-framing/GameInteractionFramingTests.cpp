#include "LegacyGameInteractionProtocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <vector>

namespace
{
	int gFailures = 0;

	void Expect(bool aCondition, const char* apName)
	{
		if (!aCondition)
		{
			std::cerr << "FAIL: " << apName << "\n";
			++gFailures;
		}
	}

	void CommandSplitAcrossReadsExecutesOnceAfterNewline()
	{
		cGameInteractionLineBuffer buffer;
		std::string command;

		buffer.Append("get", 3);
		Expect(!buffer.TryPopLine(command), "fragment is not a Command");

		buffer.Append("pos\n", 4);
		Expect(buffer.TryPopLine(command), "terminated Command becomes available");
		Expect(command == "getpos", "fragments form the original Command");
		Expect(!buffer.TryPopLine(command), "split Command is extracted exactly once");
	}

	void CompleteCommandsAreExtractedInOrderAndPartialCommandIsRetained()
	{
		cGameInteractionLineBuffer buffer;
		std::string command;
		const char received[] = "ping\ngetpos\r\nexe";
		buffer.Append(received, sizeof(received) - 1);

		Expect(buffer.TryPopLine(command) && command == "ping", "first combined Command is extracted first");
		Expect(buffer.TryPopLine(command) && command == "getpos", "CRLF Command is extracted without carriage return");
		Expect(!buffer.TryPopLine(command), "combined trailing fragment is retained");

		buffer.Append("c:foo\n", 6);
		Expect(buffer.TryPopLine(command) && command == "exec:foo", "retained fragment completes on a later read");
		Expect(!buffer.TryPopLine(command), "all combined Commands are extracted exactly once");
	}

	void DisconnectDiscardsUnterminatedCommand()
	{
		cGameInteractionLineBuffer buffer;
		std::string command;
		buffer.Append("exec:partial", 12);
		buffer.Clear();
		Expect(!buffer.TryPopLine(command), "disconnect does not expose an unterminated Command");
	}

	void LinesUpToTheLimitAreAccepted()
	{
		cGameInteractionLineBuffer buffer(6);
		std::string command;
		buffer.Append("getpos\r\n", 8);
		Expect(buffer.TryPopLine(command) && command == "getpos", "line at the length limit is extracted");
		buffer.Append("getp", 4);
		Expect(!buffer.TryPopLine(command) && !buffer.HasExceededLineLimit(),
			"unterminated fragment within the limit is retained");
	}

	void UnterminatedLineBeyondTheLimitIsReported()
	{
		cGameInteractionLineBuffer buffer(6);
		std::string command;
		buffer.Append("getposr", 7);
		Expect(!buffer.TryPopLine(command), "overlong fragment is not a Command");
		Expect(buffer.HasExceededLineLimit(), "overlong unterminated line exceeds the limit");
	}

	void CompleteLineBeyondTheLimitStopsExtraction()
	{
		cGameInteractionLineBuffer buffer(6);
		std::string command;
		const char received[] = "ping\ngetposrot\nping\n";
		buffer.Append(received, sizeof(received) - 1);
		Expect(buffer.TryPopLine(command) && command == "ping", "line before the overlong line is extracted");
		Expect(!buffer.TryPopLine(command), "overlong complete line is not extracted");
		Expect(buffer.HasExceededLineLimit(), "overlong complete line exceeds the limit");
		Expect(!buffer.TryPopLine(command), "no line is extracted after the limit is exceeded");
		buffer.Clear();
		Expect(!buffer.HasExceededLineLimit(), "clearing for a new Session resets the limit");
	}

	void LoopbackStreamUsesNewlineFramingAcrossReceiveBoundaries()
	{
		WSADATA data;
		Expect(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock starts for loopback test");

		SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		Expect(listener != INVALID_SOCKET, "loopback listener socket is created");
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		Expect(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "loopback listener binds");
		Expect(listen(listener, 1) == 0, "loopback listener listens");
		int addressLength = sizeof(address);
		Expect(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0, "loopback endpoint is discovered");

		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		Expect(connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Peer connects over loopback");
		SOCKET session = accept(listener, NULL, NULL);
		Expect(session != INVALID_SOCKET, "loopback Session is accepted");

		const char payload[] = "getpos\nping\r\nexec:partial";
		int sentBytes = 0;
		while (sentBytes < static_cast<int>(sizeof(payload) - 1))
		{
			const int sent = send(peer, payload + sentBytes, static_cast<int>(sizeof(payload) - 1) - sentBytes, 0);
			Expect(sent > 0, "Peer sends combined Commands and a fragment");
			if (sent <= 0) break;
			sentBytes += sent;
		}

		cGameInteractionLineBuffer buffer;
		char received[64];
		int count = recv(session, received, 3, 0);
		Expect(count > 0 && count <= 3, "first receive deliberately fragments a Command");
		buffer.Append(received, count);
		std::string command;
		Expect(!buffer.TryPopLine(command), "loopback fragment is not executed");

		std::vector<std::string> commands;
		int remainingBytes = static_cast<int>(sizeof(payload) - 1) - count;
		while (remainingBytes > 0)
		{
			count = recv(session, received, sizeof(received), 0);
			Expect(count > 0, "remaining loopback bytes are received");
			if (count <= 0) break;
			remainingBytes -= count;
			buffer.Append(received, count);
			while (buffer.TryPopLine(command)) commands.push_back(command);
		}
		Expect(commands.size() == 2, "loopback read exposes only complete Commands");
		Expect(commands.size() > 0 && commands[0] == "getpos", "fragmented loopback Command is first");
		Expect(commands.size() > 1 && commands[1] == "ping", "combined CRLF Command is second");

		closesocket(peer);
		Expect(recv(session, received, sizeof(received), 0) == 0, "loopback Peer disconnect is observed");
		buffer.Clear();
		Expect(!buffer.TryPopLine(command), "loopback disconnect discards its partial Command");
		closesocket(session);
		closesocket(listener);
		WSACleanup();
	}
}

int main()
{
	CommandSplitAcrossReadsExecutesOnceAfterNewline();
	CompleteCommandsAreExtractedInOrderAndPartialCommandIsRetained();
	DisconnectDiscardsUnterminatedCommand();
	LinesUpToTheLimitAreAccepted();
	UnterminatedLineBeyondTheLimitIsReported();
	CompleteLineBeyondTheLimitStopsExtraction();
	LoopbackStreamUsesNewlineFramingAcrossReceiveBoundaries();
	if (gFailures != 0) return 1;
	std::cout << "Game Interaction Protocol framing tests passed\n";
	return 0;
}
