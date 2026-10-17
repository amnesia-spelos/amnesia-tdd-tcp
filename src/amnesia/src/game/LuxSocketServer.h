#ifndef LUX_SOCKET_SERVER_H
#define LUX_SOCKET_SERVER_H

#include "LuxBase.h"
#include "LuxChatHandler.h"
#include "GameInteractionGateway.h"

class cLuxSocketServer : public iLuxUpdateable, public iLuxChatSubmissionSink
{
public:
    cLuxSocketServer();
    ~cLuxSocketServer();

    void Update(float afTimeStep);
	void DestroyWorldEntities(cLuxMap* apMap);
	void PublishEvent(const cGameInteractionEvent& aEvent);
	// Shared Deaths, for the local player's death and the current map's checkpoint.
	void BeginPlayerDeath();
	bool HoldsCheckPoint();
	cSharedDeathCheckPoint CompleteCheckPoint();
	bool PreservesSharedEnemies() const { return mGateway.PreservesSharedEnemies(); }
	bool SharesEnemies() const { return mGateway.SharesEnemies(); }
	virtual void ReportLocalChatEntry(const cChatEntry& aEntry);
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
