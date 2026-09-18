#include "GameInteractionTransport.h"

#include <ws2tcpip.h>

namespace
{
	const std::string::size_type kMaximumQueuedBytes = 1024 * 1024;
	const int kMaximumBytesReceivedPerUpdate = 64 * 1024;
	const int kMaximumBytesSentPerUpdate = 64 * 1024;
}

cGameInteractionTransport::cGameInteractionTransport()
	: mListenSocket(INVALID_SOCKET), mPeerSocket(INVALID_SOCKET), mbWinsockStarted(false), mlPort(0),
	  mlOutboundOffset(0)
{
}

cGameInteractionTransport::~cGameInteractionTransport()
{
	Shutdown();
}

bool cGameInteractionTransport::Listen(const std::string& asHost, int alPort)
{
	Shutdown();
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		msDiagnostic = "Winsock startup failed";
		return false;
	}
	mbWinsockStarted = true;

	mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mListenSocket == INVALID_SOCKET)
	{
		msDiagnostic = "Socket creation failed";
		Shutdown();
		return false;
	}

	u_long nonBlocking = 1;
	if (ioctlsocket(mListenSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
	{
		msDiagnostic = "Could not make listener non-blocking";
		Shutdown();
		return false;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	if (InetPtonA(AF_INET, asHost.c_str(), &address.sin_addr) != 1)
	{
		msDiagnostic = "Configured host is not an IPv4 address";
		Shutdown();
		return false;
	}
	address.sin_port = htons(static_cast<u_short>(alPort));
	if (bind(mListenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
		listen(mListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		msDiagnostic = "Could not bind or listen on configured endpoint";
		Shutdown();
		return false;
	}

	if (alPort == 0)
	{
		int length = sizeof(address);
		if (getsockname(mListenSocket, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR)
		{
			msDiagnostic = "Could not discover listening endpoint";
			Shutdown();
			return false;
		}
		mlPort = ntohs(address.sin_port);
	}
	else mlPort = alPort;
	msDiagnostic.clear();
	return true;
}

void cGameInteractionTransport::Shutdown()
{
	if (mPeerSocket != INVALID_SOCKET) closesocket(mPeerSocket);
	if (mListenSocket != INVALID_SOCKET) closesocket(mListenSocket);
	mPeerSocket = INVALID_SOCKET;
	mListenSocket = INVALID_SOCKET;
	msOutboundBytes.clear();
	mlOutboundOffset = 0;
	mlPort = 0;
	if (mbWinsockStarted) WSACleanup();
	mbWinsockStarted = false;
}

eGameInteractionTransportEvent cGameInteractionTransport::Update(std::vector<std::string>& avReceivedBytes)
{
	eGameInteractionTransportEvent event = eGameInteractionTransportEvent_None;
	if (mPeerSocket == INVALID_SOCKET && mListenSocket != INVALID_SOCKET)
	{
		SOCKET peer = accept(mListenSocket, NULL, NULL);
		if (peer != INVALID_SOCKET)
		{
			u_long nonBlocking = 1;
			if (ioctlsocket(peer, FIONBIO, &nonBlocking) == SOCKET_ERROR)
			{
				closesocket(peer);
				msDiagnostic = "Could not make Peer non-blocking";
			}
			else
			{
				mPeerSocket = peer;
				event = eGameInteractionTransportEvent_PeerConnected;
				msDiagnostic.clear();
				// Nagle would hold small Responses and State Updates back until the Peer acknowledges earlier ones.
				const BOOL noDelay = TRUE;
				if (setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay),
					sizeof(noDelay)) == SOCKET_ERROR)
					msDiagnostic = "Could not disable Nagle's algorithm for Peer";
			}
		}
		else if (WSAGetLastError() != WSAEWOULDBLOCK)
			msDiagnostic = "Could not accept Peer";
	}

	if (mPeerSocket != INVALID_SOCKET)
	{
		ReceiveBytes(avReceivedBytes);
		Flush();
		if (mPeerSocket == INVALID_SOCKET) event = eGameInteractionTransportEvent_PeerDisconnected;
	}
	return event;
}

void cGameInteractionTransport::QueueBytes(const std::string& asBytes)
{
	if (mPeerSocket == INVALID_SOCKET) return;
	if (msOutboundBytes.size() - mlOutboundOffset + asBytes.size() > kMaximumQueuedBytes)
	{
		DisconnectPeer("Peer delivery queue limit exceeded");
		return;
	}
	msOutboundBytes.append(asBytes);
}

void cGameInteractionTransport::DisconnectPeer(const char* apDiagnostic)
{
	closesocket(mPeerSocket);
	mPeerSocket = INVALID_SOCKET;
	msOutboundBytes.clear();
	mlOutboundOffset = 0;
	msDiagnostic = apDiagnostic;
}

void cGameInteractionTransport::ReceiveBytes(std::vector<std::string>& avReceivedBytes)
{
	char buffer[8192];
	int remainingBudget = kMaximumBytesReceivedPerUpdate;
	while (remainingBudget > 0)
	{
		const int bufferSize = static_cast<int>(sizeof(buffer));
		const int requestedBytes = remainingBudget < bufferSize ? remainingBudget : bufferSize;
		const int received = recv(mPeerSocket, buffer, requestedBytes, 0);
		if (received > 0)
		{
			avReceivedBytes.push_back(std::string(buffer, received));
			remainingBudget -= received;
		}
		else if (received == 0) { DisconnectPeer("Peer disconnected"); return; }
		else if (WSAGetLastError() == WSAEWOULDBLOCK) break;
		else { DisconnectPeer("Peer receive failed"); return; }
	}
}

void cGameInteractionTransport::Flush()
{
	if (mPeerSocket == INVALID_SOCKET) return;
	int remainingBudget = kMaximumBytesSentPerUpdate;
	while (mlOutboundOffset < msOutboundBytes.size() && remainingBudget > 0)
	{
		const int remainingBytes = static_cast<int>(msOutboundBytes.size() - mlOutboundOffset);
		const int attemptedBytes = remainingBytes < remainingBudget ? remainingBytes : remainingBudget;
		const int sent = send(mPeerSocket, msOutboundBytes.data() + mlOutboundOffset,
			attemptedBytes, 0);
		if (sent > 0) { mlOutboundOffset += sent; remainingBudget -= sent; }
		else if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return;
		else { DisconnectPeer("Peer delivery failed"); return; }
	}
	if (mlOutboundOffset == msOutboundBytes.size())
	{
		msOutboundBytes.clear();
		mlOutboundOffset = 0;
	}
}
