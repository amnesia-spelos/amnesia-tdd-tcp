#ifndef GAME_INTERACTION_TRANSPORT_H
#define GAME_INTERACTION_TRANSPORT_H

#include <winsock2.h>

#include <string>
#include <vector>

enum eGameInteractionTransportEvent
{
	eGameInteractionTransportEvent_None,
	eGameInteractionTransportEvent_PeerConnected,
	eGameInteractionTransportEvent_PeerDisconnected
};

class cGameInteractionTransport
{
public:
	cGameInteractionTransport();
	~cGameInteractionTransport();

	bool Listen(const std::string& asHost, int alPort);
	void Shutdown();
	eGameInteractionTransportEvent Update(std::vector<std::string>& avReceivedBytes);
	void QueueBytes(const std::string& asBytes);
	void Flush();
	void DisconnectPeer(const char* apDiagnostic);

	int GetPort() const { return mlPort; }
	bool HasPeer() const { return mPeerSocket != INVALID_SOCKET; }
	std::string::size_type GetPendingDeliveryByteCount() const { return msOutboundBytes.size() - mlOutboundOffset; }
	const std::string& GetDiagnostic() const { return msDiagnostic; }

private:
	SOCKET mListenSocket;
	SOCKET mPeerSocket;
	bool mbWinsockStarted;
	int mlPort;
	std::string msOutboundBytes;
	std::string::size_type mlOutboundOffset;
	std::string msDiagnostic;

	void ReceiveBytes(std::vector<std::string>& avReceivedBytes);
};

#endif
