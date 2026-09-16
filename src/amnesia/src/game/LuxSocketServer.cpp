#include "LuxSocketServer.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"

namespace
{
	class cLuxGameInteractionGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		virtual bool IsMapLoaded() const
		{
			return gpBase->mpMapHandler && gpBase->mpMapHandler->GetCurrentMap();
		}

		virtual cGameInteractionPosition GetPosition() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			const cVector3f position = pCharBody->GetFeetPosition();
			return cGameInteractionPosition(position.x, position.y, position.z);
		}

		virtual cGameInteractionRotation GetRotation() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			return cGameInteractionRotation(pCharBody->GetYaw(), pCharBody->GetPitch());
		}

		virtual std::string GetMapFile() const
		{
			return gpBase->mpMapHandler->GetCurrentMap()->GetFileName();
		}

		virtual void RunScript(const std::string& asScript)
		{
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(asScript);
		}
	};
}

cLuxSocketServer::cLuxSocketServer()
    : iLuxUpdateable("LuxSocketServer")
{
	mHost = "127.0.0.1";
	mPort = 5150;

	InitSocket();
    Log("cLuxSocketServer created!\n");
}

bool cLuxSocketServer::InitSocket()
{
	if (!mGateway.Listen(mHost, mPort))
	{
		Log("Game Interaction Protocol listener failed: %s\n", mGateway.GetDiagnostic().c_str());
		return false;
	}
	Log("Socket listening on %s:%d\n", mHost.c_str(), mPort);
	return true;
}

void cLuxSocketServer::Update(float afTimeStep)
{
	cLuxGameInteractionGameAdapter gameAdapter;
	mGateway.Update(gameAdapter);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::PublishEvent(const cGameInteractionEvent& aEvent)
{
	mGateway.Report(aEvent);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::LogNewGatewayDiagnostic()
{
	const tString& diagnostic = mGateway.GetDiagnostic();
	if (!diagnostic.empty() && diagnostic != mLastGatewayDiagnostic)
		Log("Game Interaction Protocol: %s\n", diagnostic.c_str());
	mLastGatewayDiagnostic = diagnostic;
}

void cLuxSocketServer::SetConnectionSettings(const tString& host, int port)
{
    mHost = host;
    mPort = port;

	Log("LuxSocketServer config changed: re-init\n");
	mGateway.Shutdown();
	mLastGatewayDiagnostic.clear();
    InitSocket();
}

cLuxSocketServer::~cLuxSocketServer()
{
	mGateway.Shutdown();
    Log("cLuxSocketServer destroyed!\n");
}
