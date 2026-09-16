#ifndef LUX_SOCKET_SERVER_H
#define LUX_SOCKET_SERVER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"
#include "GameInteractionTransport.h"
#include "LegacyGameInteractionProtocol.h"

class cLuxSocketServer : public iLuxUpdateable
{
public:
    cLuxSocketServer();
    ~cLuxSocketServer();

    void Update(float afTimeStep);
	void PublishEvent(const cGameInteractionEvent& aEvent);
	void SetConnectionSettings(const tString& host, int port);
	const tString& GetHost() const { return mHost; }
	int GetPort() const { return mPort; }
private:
	cGameInteractionGateway mGateway;
	cGameInteractionTransport mTransport;
	cGameInteractionLineBuffer mInboundLines;
	
	tString mHost;
	int mPort;

	bool InitSocket();
	void SendMessage(const tString& message);
};

#endif
