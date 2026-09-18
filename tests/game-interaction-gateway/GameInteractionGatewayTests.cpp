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
			mPeer(INVALID_SOCKET), mbResponseDeliveredBeforeStart(false),
			mLocalPoseAvailability(eGameInteractionLocalPoseAvailability_Live)
		{
			mLocalPose.mlTimeMs = 1000;
			mLocalPose.mFeetPosition = cGameInteractionPosition(1.25f, -2.5f, 3.75f);
			mLocalPose.mfBodyYawDegrees = 90.0f;
			mLocalPose.msMapFile = "maps/main/level01.map";
		}

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
		virtual eGameInteractionLocalPoseAvailability GetLocalPoseAvailability() const
		{
			return mLocalPoseAvailability;
		}
		virtual cGameInteractionLocalPose GetLocalPose() const { return mLocalPose; }

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
		eGameInteractionLocalPoseAvailability mLocalPoseAvailability;
		cGameInteractionLocalPose mLocalPose;
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

	std::string Exchange(SOCKET aPeer, cGameInteractionGateway& aGateway,
		cFakeGameAdapter& aAdapter, const std::string& asCommands)
	{
		SendCommands(aPeer, aGateway, aAdapter, asCommands);
		return Receive(aPeer);
	}

	SOCKET OpenSession(cGameInteractionGateway& aGateway, cFakeGameAdapter& aAdapter)
	{
		Expect(aGateway.Listen("127.0.0.1", 0), "gateway listens for a new Session");
		SOCKET peer = Connect(aGateway, aAdapter);
		Expect(Receive(peer) == "Hello, from Amnesia: The Dark Descent!\n", "every Session opens with the greeting");
		return peer;
	}

	void ProtocolVersion2NegotiationGrantsSupportedRequestedCapabilities()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenSession(gateway, adapter);
		Expect(Exchange(peer, gateway, adapter, "protocol 2 localpose teleport avatars localpose\n") ==
			"RESPONSE protocol ok 2 avatars localpose\n",
			"negotiation grants the supported requested Capabilities once each in canonical order");
		gateway.Report(cGameInteractionEvent(eGameInteractionEvent_MapChanged, "maps/main/level02.map"));
		gateway.Report(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted, L"Daniel", L"hi: all"));
		gateway.Update(adapter);
		Expect(Receive(peer) == "EVENT:MapChanged:maps/main/level02.map\nEVENT:CHAT:Daniel:hi: all\n",
			"legacy Events keep their wire form in a negotiated Session");
		closesocket(peer);
		gateway.Shutdown();
	}

	// Returns whatever arrives within the wait, or nothing.
	std::string ReceiveAvailable(SOCKET aPeer, long alMilliseconds)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(aPeer, &readable);
		timeval timeout = { 0, alMilliseconds * 1000 };
		if (select(0, &readable, NULL, NULL, &timeout) != 1) return std::string();
		char buffer[65536];
		const int count = recv(aPeer, buffer, sizeof(buffer), 0);
		return count > 0 ? std::string(buffer, count) : std::string();
	}

	std::string ReceiveLines(SOCKET aPeer, int alLineCount)
	{
		std::string received;
		for (int attempt = 0; attempt < 20; ++attempt)
		{
			size_t lines = 0;
			for (size_t index = 0; index < received.size(); ++index)
				if (received[index] == '\n') ++lines;
			if (lines >= static_cast<size_t>(alLineCount)) return received;
			received += ReceiveAvailable(aPeer, 100);
		}
		return received;
	}

	// The fake adapter's Pose as a State Update at the given time.
	std::string LocalPoseStateUpdate(const std::string& asTimeMs)
	{
		return "STATE localpose " + asTimeMs + " 0 1.2500 -2.5000 3.7500 90.0000 0.0000 0 maps/main/level01.map\n";
	}

	std::string UpdateAt(cGameInteractionGateway& aGateway, cFakeGameAdapter& aAdapter, SOCKET aPeer,
		unsigned long long alTimeMs)
	{
		aAdapter.mLocalPose.mlTimeMs = alTimeMs;
		aGateway.Update(aAdapter);
		return ReceiveAvailable(aPeer, 50);
	}

	SOCKET OpenLocalPoseSession(cGameInteractionGateway& aGateway, cFakeGameAdapter& aAdapter)
	{
		SOCKET peer = OpenSession(aGateway, aAdapter);
		Expect(Exchange(peer, aGateway, aAdapter, "protocol 2 localpose\n") == "RESPONSE protocol ok 2 localpose\n",
			"Session negotiates the localpose Capability");
		return peer;
	}

	void LocalPoseStateUpdatesFollowTheSubscribedRate()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenLocalPoseSession(gateway, adapter);
		SendCommands(peer, gateway, adapter, "localpose subscribe 20\n");
		Expect(ReceiveLines(peer, 2) == "RESPONSE localpose ok subscribe 20\n" + LocalPoseStateUpdate("1000"),
			"subscribing sends the current Pose right after the Response");
		Expect(UpdateAt(gateway, adapter, peer, 1049).empty(), "no State Update before the 20 Hz interval elapses");
		Expect(UpdateAt(gateway, adapter, peer, 1050) == LocalPoseStateUpdate("1050"),
			"a State Update follows once the interval elapses");
		Expect(UpdateAt(gateway, adapter, peer, 1110) == LocalPoseStateUpdate("1110"), "a late update still samples");
		Expect(UpdateAt(gateway, adapter, peer, 1149).empty(), "a late sample does not shift the schedule");
		Expect(UpdateAt(gateway, adapter, peer, 1150) == LocalPoseStateUpdate("1150"),
			"the schedule keeps the subscribed average rate");
		Expect(UpdateAt(gateway, adapter, peer, 1500) == LocalPoseStateUpdate("1500"),
			"a stalled clock resumes sampling");
		Expect(UpdateAt(gateway, adapter, peer, 1549).empty(), "after a stall the schedule restarts from the sample");

		SendCommands(peer, gateway, adapter, "localpose unsubscribe\n");
		Expect(ReceiveLines(peer, 1) == "RESPONSE localpose ok unsubscribe\n", "unsubscribing is answered");
		Expect(UpdateAt(gateway, adapter, peer, 5000).empty(), "unsubscribing stops State Updates");
		closesocket(peer);
		gateway.Shutdown();
	}

	void LocalPoseRateIsClamped()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenLocalPoseSession(gateway, adapter);
		SendCommands(peer, gateway, adapter, "localpose subscribe 1000\n");
		Expect(ReceiveLines(peer, 2) == "RESPONSE localpose ok subscribe 60\n" + LocalPoseStateUpdate("1000"),
			"a rate above 60 Hz is clamped to 60 Hz");
		Expect(UpdateAt(gateway, adapter, peer, 1016).empty(), "60 Hz does not sample within 16 ms");
		Expect(UpdateAt(gateway, adapter, peer, 1017) == LocalPoseStateUpdate("1017"), "60 Hz samples after 16.7 ms");

		SendCommands(peer, gateway, adapter, "localpose subscribe 0\n");
		Expect(ReceiveLines(peer, 2) == "RESPONSE localpose ok subscribe 1\n" + LocalPoseStateUpdate("1017"),
			"a rate below 1 Hz is clamped to 1 Hz and subscribing again samples at once");
		Expect(UpdateAt(gateway, adapter, peer, 2016).empty(), "1 Hz does not sample within a second");
		Expect(UpdateAt(gateway, adapter, peer, 2017) == LocalPoseStateUpdate("2017"), "1 Hz samples after a second");
		closesocket(peer);
		gateway.Shutdown();
	}

	void LocalPoseRequiresTheLocalPoseCapability()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenSession(gateway, adapter);
		Expect(Exchange(peer, gateway, adapter, "protocol 2 avatars\nlocalpose subscribe 20\n") ==
			"RESPONSE protocol ok 2 avatars\nRESPONSE localpose not-granted\n",
			"subscribing without the localpose Capability is not granted");
		Expect(UpdateAt(gateway, adapter, peer, 2000).empty(), "a Session without the Capability gets no State Update");
		closesocket(peer);
		gateway.Shutdown();

		SOCKET legacyPeer = OpenSession(gateway, adapter);
		Expect(Exchange(legacyPeer, gateway, adapter, "localpose subscribe 20\n") == "WARNING:Unknown command\n",
			"a Session that never negotiated does not know localpose");
		Expect(UpdateAt(gateway, adapter, legacyPeer, 3000).empty(), "a legacy Session gets no State Update");
		closesocket(legacyPeer);
		gateway.Shutdown();
	}

	void LocalPoseIsEmittedOnlyWhileAMapIsLoaded()
	{
		cFakeGameAdapter adapter;
		adapter.mLocalPoseAvailability = eGameInteractionLocalPoseAvailability_Unavailable;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenLocalPoseSession(gateway, adapter);
		SendCommands(peer, gateway, adapter, "localpose subscribe 20\n");
		Expect(ReceiveLines(peer, 1) == "RESPONSE localpose ok subscribe 20\n",
			"subscribing in the main menu or while loading is answered");
		Expect(UpdateAt(gateway, adapter, peer, 2000).empty(), "no State Update without a loaded map");
		adapter.mLocalPoseAvailability = eGameInteractionLocalPoseAvailability_Live;
		Expect(UpdateAt(gateway, adapter, peer, 2001) == LocalPoseStateUpdate("2001"),
			"State Updates begin once a map is loaded");
		adapter.mLocalPoseAvailability = eGameInteractionLocalPoseAvailability_Unavailable;
		Expect(UpdateAt(gateway, adapter, peer, 3000).empty(), "State Updates stop while the next map loads");
		closesocket(peer);
		gateway.Shutdown();
	}

	void SuspendedPlayEmitsTheLocalPoseOnlyWhenItChanged()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenLocalPoseSession(gateway, adapter);
		SendCommands(peer, gateway, adapter, "localpose subscribe 20\n");
		Expect(ReceiveLines(peer, 2) == "RESPONSE localpose ok subscribe 20\n" + LocalPoseStateUpdate("1000"),
			"a live Session receives the Pose");
		adapter.mLocalPoseAvailability = eGameInteractionLocalPoseAvailability_Suspended;
		Expect(UpdateAt(gateway, adapter, peer, 1050).empty(), "an unchanged Pose is not repeated while paused");
		Expect(UpdateAt(gateway, adapter, peer, 1500).empty(), "an unchanged Pose stays silent while paused");
		adapter.mLocalPose.mfCameraPitchDegrees = -10.0f;
		Expect(UpdateAt(gateway, adapter, peer, 1510) ==
			"STATE localpose 1510 0 1.2500 -2.5000 3.7500 90.0000 -10.0000 0 maps/main/level01.map\n",
			"a changed Pose is sent while in the inventory");
		adapter.mLocalPose.mlTeleportCounter = 1;
		Expect(UpdateAt(gateway, adapter, peer, 1520).empty(), "a changed Pose while paused still keeps the rate");
		Expect(UpdateAt(gateway, adapter, peer, 1560) ==
			"STATE localpose 1560 1 1.2500 -2.5000 3.7500 90.0000 -10.0000 0 maps/main/level01.map\n",
			"a teleport while paused is a changed Pose");
		adapter.mLocalPoseAvailability = eGameInteractionLocalPoseAvailability_Live;
		Expect(UpdateAt(gateway, adapter, peer, 1610) ==
			"STATE localpose 1610 1 1.2500 -2.5000 3.7500 90.0000 -10.0000 0 maps/main/level01.map\n",
			"an unchanged Pose is sent again once play resumes");
		closesocket(peer);
		gateway.Shutdown();
	}

	void SlowPeerHoldsOnlyTheNewestLocalPose()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		Expect(gateway.Listen("127.0.0.1", 0), "gateway listens for a slow Peer");
		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		const int smallBuffer = 4096;
		setsockopt(peer, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&smallBuffer), sizeof(smallBuffer));
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<u_short>(gateway.GetPort()));
		Expect(connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "slow Peer connects");
		gateway.Update(adapter);
		SendCommands(peer, gateway, adapter, "protocol 2 localpose\nlocalpose subscribe 60\n");

		// Far more Pose bytes than the 1 MiB delivery queue limit, none of which the Peer reads.
		const int poseCount = 30000;
		for (int pose = 1; pose <= poseCount; ++pose)
		{
			adapter.mLocalPose.mFeetPosition.mfX = static_cast<float>(pose);
			adapter.mLocalPose.mlTimeMs = 1000 + static_cast<unsigned long long>(pose) * 17;
			gateway.Update(adapter);
		}
		Expect(gateway.GetDiagnostic().empty(), "a Peer that does not read is not disconnected for Poses alone");

		const std::string newest =
			"STATE localpose 511000 0 30000.0000 -2.5000 3.7500 90.0000 0.0000 0 maps/main/level01.map\n";
		std::string received;
		for (int attempt = 0; attempt < 500 && (received.size() < newest.size() ||
			received.compare(received.size() - newest.size(), newest.size(), newest) != 0); ++attempt)
		{
			gateway.Update(adapter);
			received += ReceiveAvailable(peer, 10);
		}
		Expect(received.compare(0, 39, "Hello, from Amnesia: The Dark Descent!\n") == 0, "slow Peer gets the greeting");
		Expect(received.size() >= newest.size() &&
			received.compare(received.size() - newest.size(), newest.size(), newest) == 0,
			"once the Peer reads, the newest Pose arrives last");
		Expect(received.size() < 1024 * 1024, "undelivered Poses were replaced rather than queued");
		Expect(Exchange(peer, gateway, adapter, "ping\n") == "RESPONSE:ping:pong\n", "slow Peer's Session continues");
		closesocket(peer);
		gateway.Shutdown();
	}

	void LocalPoseSubscriptionEndsWithTheSession()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenLocalPoseSession(gateway, adapter);
		SendCommands(peer, gateway, adapter, "localpose subscribe 20\n");
		Expect(ReceiveLines(peer, 2) == "RESPONSE localpose ok subscribe 20\n" + LocalPoseStateUpdate("1000"),
			"first Session is subscribed");
		closesocket(peer);
		for (int update = 0; update < 50 && gateway.GetDiagnostic().empty(); ++update)
		{
			Sleep(10);
			gateway.Update(adapter);
		}

		SOCKET laterPeer = Connect(gateway, adapter);
		Expect(Receive(laterPeer) == "Hello, from Amnesia: The Dark Descent!\n", "a new Session starts");
		Expect(Exchange(laterPeer, gateway, adapter, "protocol 2 localpose\n") == "RESPONSE protocol ok 2 localpose\n",
			"the new Session negotiates localpose");
		Expect(UpdateAt(gateway, adapter, laterPeer, 5000).empty(), "the new Session does not inherit the subscription");
		closesocket(laterPeer);
		gateway.Shutdown();
	}

	void OverlongInboundLineDisconnectsThePeerWithAReason()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenSession(gateway, adapter);
		const std::string overlong = "ping\nexec:" + std::string(64 * 1024, 'x');
		Expect(send(peer, overlong.data(), static_cast<int>(overlong.size()), 0) ==
			static_cast<int>(overlong.size()), "Peer sends an overlong unterminated line");
		for (int update = 0; update < 50 && gateway.GetDiagnostic().empty(); ++update)
		{
			Sleep(10);
			gateway.Update(adapter);
		}
		Expect(gateway.GetDiagnostic().find("line length") != std::string::npos,
			"overlong line leaves a diagnostic naming the line length limit");
		Expect(Receive(peer) == "RESPONSE:ping:pong\n", "Commands before the overlong line are answered");
		char buffer[16];
		Expect(recv(peer, buffer, sizeof(buffer), 0) == 0, "overlong line disconnects the Peer");
		closesocket(peer);

		SOCKET laterPeer = Connect(gateway, adapter);
		Expect(Receive(laterPeer) == "Hello, from Amnesia: The Dark Descent!\n",
			"gateway accepts a new Session after the line length disconnect");
		Expect(Exchange(laterPeer, gateway, adapter, "ping\n") == "RESPONSE:ping:pong\n",
			"new Session does not inherit the overlong line");
		closesocket(laterPeer);
		gateway.Shutdown();
	}

	void ResponseIsDeliveredWithinTheUpdateThatProcessedItsCommand()
	{
		cFakeGameAdapter adapter;
		cGameInteractionGateway gateway;
		SOCKET peer = OpenSession(gateway, adapter);
		Expect(send(peer, "ping\n", 5, 0) == 5, "Peer sends a Command");
		Sleep(50);
		gateway.Update(adapter);
		Expect(Receive(peer) == "RESPONSE:ping:pong\n", "Response is flushed by the update that processed its Command");
		closesocket(peer);
		gateway.Shutdown();
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

	ProtocolVersion2NegotiationGrantsSupportedRequestedCapabilities();
	ResponseIsDeliveredWithinTheUpdateThatProcessedItsCommand();
	OverlongInboundLineDisconnectsThePeerWithAReason();
	LocalPoseStateUpdatesFollowTheSubscribedRate();
	LocalPoseRateIsClamped();
	LocalPoseRequiresTheLocalPoseCapability();
	LocalPoseIsEmittedOnlyWhileAMapIsLoaded();
	SuspendedPlayEmitsTheLocalPoseOnlyWhenItChanged();
	SlowPeerHoldsOnlyTheNewestLocalPose();
	LocalPoseSubscriptionEndsWithTheSession();
	std::cout << "Game Interaction Protocol gateway loopback cases passed\n";
	return 0;
}
