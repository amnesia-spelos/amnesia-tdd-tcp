#include "LuxSocketServer.h"
#include "LegacyGameInteractionProtocol.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"

namespace
{
	class cLuxLegacyGameAdapter : public iLegacyGameAdapter
	{
	public:
		virtual bool IsMapLoaded() const
		{
			return gpBase->mpMapHandler && gpBase->mpMapHandler->GetCurrentMap();
		}

		virtual cLegacyPeerState GetPeerState() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			const cVector3f position = pCharBody->GetFeetPosition();
			cLegacyPeerState state = { true, position.x, position.y, position.z,
				pCharBody->GetYaw(), pCharBody->GetPitch(), "" };
			return state;
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
	if (!mTransport.Listen(mHost, mPort))
	{
		Log("Game Interaction Protocol listener failed: %s\n", mTransport.GetDiagnostic().c_str());
		return false;
	}
	Log("Socket listening on %s:%d\n", mHost.c_str(), mPort);
	return true;
}

void cLuxSocketServer::Update(float afTimeStep)
{
	std::vector<std::string> receivedBytes;
	const eGameInteractionTransportEvent event = mTransport.Update(receivedBytes);
	if (event == eGameInteractionTransportEvent_PeerConnected)
	{
		Log("Peer connected!\n");
		mInboundLines.Clear();
		SendMessage(cLegacyGameInteractionProtocol::Greeting());
	}
	else if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		mInboundLines.Clear();
		Log("Peer disconnected: %s\n", mTransport.GetDiagnostic().c_str());
	}

	for (std::vector<std::string>::const_iterator bytes = receivedBytes.begin(); bytes != receivedBytes.end(); ++bytes)
		mInboundLines.Append(bytes->data(), bytes->size());
	std::vector<std::string> commands;
	std::string command;
	while (mInboundLines.TryPopLine(command)) commands.push_back(command);

	cLuxLegacyGameAdapter gameAdapter;
	cLegacyGameInteractionProtocol protocol(gameAdapter);
	for (std::vector<std::string>::const_iterator command = commands.begin(); command != commands.end(); ++command)
	{
		Log("Peer says: %s\n", command->c_str());
		SendMessage(protocol.HandleCommand(*command));
	}
}

void cLuxSocketServer::SendMessage(const tString& message)
{
	const bool hadPeer = mTransport.HasPeer();
	mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(message));
	if (hadPeer && !mTransport.HasPeer())
		Log("Game Interaction Protocol delivery failed: %s\n", mTransport.GetDiagnostic().c_str());
}

void cLuxSocketServer::SetConnectionSettings(const tString& host, int port)
{
    mHost = host;
    mPort = port;

	Log("LuxSocketServer config changed: re-init\n");
	mTransport.Shutdown();
    InitSocket();
}

cLuxSocketServer::~cLuxSocketServer()
{
	mTransport.Shutdown();
    Log("cLuxSocketServer destroyed!\n");
}
