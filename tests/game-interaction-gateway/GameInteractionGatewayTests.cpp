#include "GameInteractionGateway.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	class cFakeGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		cFakeGameAdapter()
			: mbMapLoaded(true), mPosition(1.25f, -2.5f, 3.75f),
			mRotation(1.570796325f, -0.7853981625f), msMapFile("maps/main/level01.map") {}

		virtual bool IsMapLoaded() const { return mbMapLoaded; }
		virtual cGameInteractionPosition GetPosition() const { return mPosition; }
		virtual cGameInteractionRotation GetRotation() const { return mRotation; }
		virtual std::string GetMapFile() const { return msMapFile; }
		virtual void RunScript(const std::string& asScript) { msExecutedScript = asScript; }

		bool mbMapLoaded;
		cGameInteractionPosition mPosition;
		cGameInteractionRotation mRotation;
		std::string msMapFile;
		std::string msExecutedScript;
	};

	SOCKET Connect(cGameInteractionGateway& aGateway, cFakeGameAdapter& aAdapter)
	{
		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<u_short>(aGateway.GetPort()));
		Expect(connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
			"Peer connects to gateway");
		aGateway.Update(aAdapter);
		aGateway.Update(aAdapter);
		return peer;
	}

	std::string Receive(SOCKET aPeer)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(aPeer, &readable);
		timeval timeout = { 2, 0 };
		Expect(select(0, &readable, NULL, NULL, &timeout) == 1, "Peer receives gateway output");
		char buffer[2048];
		const int count = recv(aPeer, buffer, sizeof(buffer), 0);
		Expect(count > 0, "gateway output is readable");
		return std::string(buffer, count);
	}

	void SendCommands(SOCKET aPeer, cGameInteractionGateway& aGateway,
		cFakeGameAdapter& aAdapter, const std::string& asCommands)
	{
		Expect(send(aPeer, asCommands.data(), static_cast<int>(asCommands.size()), 0) ==
			static_cast<int>(asCommands.size()), "Peer sends Commands");
		aGateway.Update(aAdapter);
		aGateway.Update(aAdapter);
	}
}

int main()
{
	cFakeGameAdapter adapter;
	cGameInteractionGateway gateway;
	Expect(gateway.Listen("127.0.0.1", 0), "gateway listens without exposing transport details");
	SOCKET peer = Connect(gateway, adapter);
	Expect(Receive(peer) == "Hello, from Amnesia: The Dark Descent!\n", "gateway sends legacy greeting");

	SendCommands(peer, gateway, adapter,
		"ping\ngetpos\ngetrot\ngetposrot\ngetmap\nexec:SetLocalVarInt(\"lever\", 1):with:colons\n");
	Expect(Receive(peer) ==
		"RESPONSE:ping:pong\n"
		"RESPONSE:getpos:1.25, -2.50, 3.75\n"
		"RESPONSE:getrot:90.00, -45.00, 0.00\n"
		"RESPONSE:getposrot:1.25, -2.50, 3.75:90.00, -45.00, 0.00\n"
		"RESPONSE:getmap:maps/main/level01.map\n"
		"RESPONSE:exec:script executed\n", "all current Commands pass through the gateway in order");
	Expect(adapter.msExecutedScript == "SetLocalVarInt(\"lever\", 1):with:colons",
		"state-changing Command crosses the game adapter");

	Expect(send(peer, "get", 3, 0) == 3, "Peer sends a fragmented Command prefix");
	gateway.Update(adapter);
	SendCommands(peer, gateway, adapter, "map\nunknown\n");
	Expect(Receive(peer) == "RESPONSE:getmap:maps/main/level01.map\nWARNING:Unknown command\n",
		"gateway buffers fragmented input and separates coalesced Commands");

	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_MapChanged, "maps/main/level02.map"));
	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_ScriptCallObserved, "OnEnter()"));
	gateway.Update(adapter);
	Expect(Receive(peer) == "EVENT:MapChanged:maps/main/level02.map\nSCRIPT_CALL:OnEnter()\n",
		"typed Events pass through the gateway");

	closesocket(peer);
	for (int index = 0; index < 50 && gateway.GetDiagnostic().empty(); ++index)
		gateway.Update(adapter);
	Expect(!gateway.GetDiagnostic().empty(), "disconnect leaves a useful diagnostic");
	SOCKET laterPeer = Connect(gateway, adapter);
	Expect(Receive(laterPeer) == "Hello, from Amnesia: The Dark Descent!\n",
		"gateway relistens after a Peer disconnects");
	closesocket(laterPeer);

	cGameInteractionGateway blockedGateway;
	Expect(!blockedGateway.Listen("127.0.0.1", gateway.GetPort()),
		"a second gateway reports endpoint binding failure");
	Expect(!blockedGateway.GetDiagnostic().empty(), "binding failure exposes a useful diagnostic");
	blockedGateway.Update(adapter);
	gateway.Shutdown();
	std::cout << "Game Interaction Protocol gateway loopback cases passed\n";
	return 0;
}
