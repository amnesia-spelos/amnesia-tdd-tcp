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
	// Peer Settings, for the options tab: the connected Peers' settings in announcement order, a revision
	// that changes whenever the list or a value in it changes, and the values the player confirmed with OK.
	// Confirming stores the changed values in the main config and tells their Peers.
	const std::vector<cPeerSetting>& GetPeerSettings() const { return mGateway.GetPeerSettings(); }
	unsigned int GetPeerSettingsRevision() const { return mGateway.GetPeerSettingsRevision(); }
	void ConfirmPeerSettings(const std::vector<cPeerSettingEdit>& avEdits);
private:
	cGameInteractionGateway mGateway;
	iPeerSettingStore* mpPeerSettingStore;
	
	tString mHost;
	tString mLastGatewayDiagnostic;
	int mPort;

	bool InitSocket();
	void LogNewGatewayDiagnostic();
};

#endif
