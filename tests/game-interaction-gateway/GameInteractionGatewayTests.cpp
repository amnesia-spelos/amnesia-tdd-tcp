#include "GameInteractionGateway.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef GetMessage
#undef GetMessage
#endif

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
			mRotation(1.570796325f, -0.7853981625f), msMapFile("maps/main/level01.map"),
			mbChatAvailable(true), mlDisplayedChatEntries(0), mbInMainMenu(true),
			mPeer(INVALID_SOCKET), mbResponseDeliveredBeforeStart(false) {}

		virtual bool IsMapLoaded() const { return mbMapLoaded; }
		virtual cGameInteractionPosition GetPosition() const { return mPosition; }
		virtual cGameInteractionRotation GetRotation() const { return mRotation; }
		virtual std::string GetMapFile() const { return msMapFile; }
		virtual void RunScript(const std::string& asScript) { msExecutedScript = asScript; }
		virtual bool DisplayChatEntry(const cChatEntry& aEntry)
		{
			if (!mbChatAvailable) return false;
			++mlDisplayedChatEntries;
			msDisplayedAuthor = aEntry.GetAuthor();
			msDisplayedMessage = aEntry.GetMessage();
			return true;
		}
		virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const { return mvCustomStories; }
		virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(
			const std::wstring& asIdentifier) const
		{
			if (!mbInMainMenu) return eGameInteractionCustomStoryAvailability_NotInMainMenu;
			for (size_t index = 0; index < mvCustomStories.size(); ++index)
				if (mvCustomStories[index].GetIdentifier() == asIdentifier)
					return eGameInteractionCustomStoryAvailability_Available;
			return asIdentifier == msInvalidCustomStory ? eGameInteractionCustomStoryAvailability_Invalid :
				eGameInteractionCustomStoryAvailability_NotFound;
		}
		virtual void StartCustomStory(const std::wstring& asIdentifier)
		{
			fd_set readable;
			FD_ZERO(&readable);
			FD_SET(mPeer, &readable);
			timeval noWait = { 0, 0 };
			mbResponseDeliveredBeforeStart = select(0, &readable, NULL, NULL, &noWait) == 1;
			mvStartedCustomStories.push_back(asIdentifier);
		}

		bool mbMapLoaded;
		cGameInteractionPosition mPosition;
		cGameInteractionRotation mRotation;
		std::string msMapFile;
		std::string msExecutedScript;
		bool mbChatAvailable;
		int mlDisplayedChatEntries;
		std::wstring msDisplayedAuthor;
		std::wstring msDisplayedMessage;
		bool mbInMainMenu;
		std::vector<cGameInteractionCustomStory> mvCustomStories;
		std::wstring msInvalidCustomStory;
		std::vector<std::wstring> mvStartedCustomStories;
		SOCKET mPeer;
		bool mbResponseDeliveredBeforeStart;
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
	cGameInteractionCommand typedChat(eGameInteractionCommand_Chat, L"Alice", L"hello: world");
	Expect(typedChat.GetClassification() == eGameInteractionCommandClassification_StateChanging,
		"typed chat Command is state-changing");
	Expect(typedChat.GetChatAuthor() == L"Alice" && typedChat.GetChatMessage() == L"hello: world",
		"typed chat Command keeps Chat Author and message separate");
	cGameInteractionEvent typedSubmission(eGameInteractionEvent_LocalChatSubmitted, L"Daniel", L"hello");
	Expect(typedSubmission.GetChatAuthor() == L"Daniel" && typedSubmission.GetChatMessage() == L"hello",
		"typed local-submission Event keeps Chat Author and message separate");

	Expect(cGameInteractionCommand(eGameInteractionCommand_GetCustomStories).GetClassification() ==
		eGameInteractionCommandClassification_Observational, "Custom Story listing is observational");
	Expect(cGameInteractionCommand(eGameInteractionCommand_StartCustomStory, L"mp-test-cs").GetClassification() ==
		eGameInteractionCommandClassification_StateChanging, "Custom Story start is state-changing");

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

	SendCommands(peer, gateway, adapter, "chat:Alice:hello: from elsewhere\n");
	Expect(Receive(peer) == "RESPONSE:chat:message displayed\n",
		"valid chat Command receives the success Response");
	Expect(adapter.mlDisplayedChatEntries == 1 && adapter.msDisplayedAuthor == L"Alice" &&
		adapter.msDisplayedMessage == L"hello: from elsewhere",
		"valid chat Command displays exactly one whole Chat Entry");
	SendCommands(peer, gateway, adapter, "chat: :hello\nchat:Alice: \n");
	Expect(Receive(peer) == "RESPONSE:chat:invalid author\nRESPONSE:chat:invalid message\n",
		"invalid chat fields receive stable outcomes");
	Expect(adapter.mlDisplayedChatEntries == 1, "invalid chat Commands do not display partial entries");
	adapter.mbChatAvailable = false;
	SendCommands(peer, gateway, adapter, "chat:Alice:hello\n");
	Expect(Receive(peer) == "RESPONSE:chat:unavailable\n", "unavailable chat UI receives a stable outcome");
	Expect(adapter.mlDisplayedChatEntries == 1, "unavailable chat UI displays no entry");
	adapter.mbChatAvailable = true;

	adapter.mvCustomStories.push_back(cGameInteractionCustomStory(L"mp-test-cs", L"Amnesia Multiplayer Test"));
	adapter.mvCustomStories.push_back(cGameInteractionCustomStory(L"other", L"Other"));
	SendCommands(peer, gateway, adapter, "getcustomstories\n");
	Expect(Receive(peer) == "RESPONSE:getcustomstories:mp-test-cs|Amnesia Multiplayer Test\tother|Other\n",
		"Custom Story listing reports every installed Custom Story");

	adapter.mPeer = peer;
	adapter.msInvalidCustomStory = L"broken";
	SendCommands(peer, gateway, adapter, "startcustomstory:missing\nstartcustomstory:broken\n");
	Expect(Receive(peer) == "RESPONSE:startcustomstory:not found\nRESPONSE:startcustomstory:invalid\n",
		"unknown and invalid Custom Stories receive stable outcomes");
	adapter.mbInMainMenu = false;
	SendCommands(peer, gateway, adapter, "startcustomstory:mp-test-cs\n");
	Expect(Receive(peer) == "RESPONSE:startcustomstory:not in main menu\n",
		"Custom Story start outside the main menu is rejected");
	Expect(adapter.mvStartedCustomStories.empty(), "rejected Custom Story starts change no game state");
	adapter.mbInMainMenu = true;

	const std::string starts = "startcustomstory:mp-test-cs\nstartcustomstory:other\n";
	Expect(send(peer, starts.data(), static_cast<int>(starts.size()), 0) ==
		static_cast<int>(starts.size()), "Peer sends Custom Story starts");
	gateway.Update(adapter);
	Expect(adapter.mvStartedCustomStories.empty(), "accepted Custom Story start is deferred past Command processing");
	gateway.Update(adapter);
	Expect(adapter.mvStartedCustomStories.size() == 1 && adapter.mvStartedCustomStories[0] == L"mp-test-cs",
		"accepted Custom Story start is performed once on a later update");
	Expect(adapter.mbResponseDeliveredBeforeStart, "starting Response is delivered before the start is performed");
	Expect(Receive(peer) == "RESPONSE:startcustomstory:starting\nRESPONSE:startcustomstory:not in main menu\n",
		"a second start while one is pending is rejected");
	gateway.Update(adapter);
	Expect(adapter.mvStartedCustomStories.size() == 1, "rejected pending start is never performed");

	Expect(send(peer, "get", 3, 0) == 3, "Peer sends a fragmented Command prefix");
	gateway.Update(adapter);
	SendCommands(peer, gateway, adapter, "map\nunknown\n");
	Expect(Receive(peer) == "RESPONSE:getmap:maps/main/level01.map\nWARNING:Unknown command\n",
		"gateway buffers fragmented input and separates coalesced Commands");

	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_MapChanged, "maps/main/level02.map"));
	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_ScriptCallObserved, "OnEnter()"));
	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted, L"Daniel", L"hi: all"));
	gateway.Report(cGameInteractionEvent(eGameInteractionEvent_CustomStoryStarted, L"mp-test-cs"));
	gateway.Update(adapter);
	Expect(Receive(peer) == "EVENT:MapChanged:maps/main/level02.map\nSCRIPT_CALL:OnEnter()\nEVENT:CHAT:Daniel:hi: all\n"
		"EVENT:CustomStoryStarted:mp-test-cs\n",
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
