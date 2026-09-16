#include "GameInteractionTransport.h"

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

	SOCKET ConnectPeer(const cGameInteractionTransport& aTransport)
	{
		SOCKET peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(static_cast<u_short>(aTransport.GetPort()));
		Expect(connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Peer connects");
		return peer;
	}

	void QueuedProtocolOutputRetainsFramingAndOrder()
	{
		cGameInteractionTransport transport;
		Expect(transport.Listen("127.0.0.1", 0), "transport listens on loopback");

		SOCKET peer = ConnectPeer(transport);

		std::vector<std::string> commands;
		Expect(transport.Update(commands) == eGameInteractionTransportEvent_PeerConnected, "Session begins");
		transport.QueueBytes("RESPONSE:one\nEVENT:two\n");
		transport.Update(commands);

		char received[64] = {};
		const int count = recv(peer, received, sizeof(received), 0);
		Expect(std::string(received, count) == "RESPONSE:one\nEVENT:two\n", "protocol output retains framing and order");

		closesocket(peer);
		transport.Shutdown();
	}

	void PartialDeliveryDisconnectAndRelistenPreserveAvailability()
	{
		cGameInteractionTransport transport;
		Expect(transport.Listen("127.0.0.1", 0), "relisten test transport listens");
		SOCKET firstPeer = ConnectPeer(transport);
		int receiveBufferSize = 1024;
		setsockopt(firstPeer, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&receiveBufferSize), sizeof(receiveBufferSize));
		std::vector<std::string> commands;
		transport.Update(commands);

		const std::string first(256 * 1024, 'a');
		const std::string second(256 * 1024, 'b');
		transport.QueueBytes(first + "\n");
		transport.QueueBytes(second + "\n");
		transport.Update(commands);
		Expect(transport.GetPendingDeliveryByteCount() > 0, "partial delivery retains unsent bytes");

		std::string delivered;
		char buffer[8192];
		while (delivered.size() < first.size() + second.size() + 2)
		{
			transport.Update(commands);
			const int count = recv(firstPeer, buffer, sizeof(buffer), 0);
			Expect(count > 0, "queued bytes remain available after a partial write");
			if (count <= 0) break;
			delivered.append(buffer, count);
		}
		Expect(delivered == first + "\n" + second + "\n", "partial delivery remains complete and ordered");

		closesocket(firstPeer);
		for (int index = 0; index < 50 && transport.HasPeer(); ++index) transport.Update(commands);
		Expect(!transport.HasPeer(), "orderly disconnect clears Session state");

		SOCKET laterPeer = ConnectPeer(transport);
		Expect(transport.Update(commands) == eGameInteractionTransportEvent_PeerConnected, "later Peer starts a new Session");
		closesocket(laterPeer);
		transport.Shutdown();
	}

	void SlowPeerIsDisconnectedAndSettingsCanRelisten()
	{
		cGameInteractionTransport transport;
		Expect(transport.Listen("127.0.0.1", 0), "slow-Peer transport listens");
		SOCKET slowPeer = ConnectPeer(transport);
		std::vector<std::string> commands;
		transport.Update(commands);
		transport.QueueBytes(std::string(1024 * 1024 + 1, 'x'));
		Expect(!transport.HasPeer(), "Peer exceeding delivery queue limit is disconnected");
		Expect(!transport.GetDiagnostic().empty(), "delivery failure is exposed through diagnostics");
		closesocket(slowPeer);

		Expect(transport.Listen("127.0.0.1", 0), "applying connection settings relistens");
		SOCKET laterPeer = ConnectPeer(transport);
		Expect(transport.Update(commands) == eGameInteractionTransportEvent_PeerConnected, "relisten accepts a new Session");
		closesocket(laterPeer);
		transport.Shutdown();
	}
}

int main()
{
	QueuedProtocolOutputRetainsFramingAndOrder();
	PartialDeliveryDisconnectAndRelistenPreserveAvailability();
	SlowPeerIsDisconnectedAndSettingsCanRelisten();
	if (gFailures != 0) return 1;
	std::cout << "Game Interaction Protocol delivery tests passed\n";
	return 0;
}
