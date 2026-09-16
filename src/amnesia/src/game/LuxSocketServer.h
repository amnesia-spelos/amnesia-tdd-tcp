#ifndef LUX_SOCKET_SERVER_H
#define LUX_SOCKET_SERVER_H

#include "LuxBase.h"
#include "GameInteractionGateway.h"

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
	
	tString mHost;
	tString mLastGatewayDiagnostic;
	int mPort;

	bool InitSocket();
	void LogNewGatewayDiagnostic();
};

#endif
