#include "LuxSocketServer.h"
#include "LegacyGameInteractionProtocol.h"
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
		mGateway.BeginLegacySession();
		SendMessage(cLegacyGameInteractionProtocol::Greeting());
	}
	else if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		mInboundLines.Clear();
		mGateway.EndSession();
		Log("Peer disconnected: %s\n", mTransport.GetDiagnostic().c_str());
		// ReceiveBytes may have queued complete Commands before observing the orderly
		// disconnect. They still belong to the closed Session and must not execute.
		return;
	}

	for (std::vector<std::string>::const_iterator bytes = receivedBytes.begin(); bytes != receivedBytes.end(); ++bytes)
		mInboundLines.Append(bytes->data(), bytes->size());
	std::vector<std::string> commands;
	std::string command;
	while (mInboundLines.TryPopLine(command)) commands.push_back(command);

	cLuxGameInteractionGameAdapter gameAdapter;
	cLegacyGameInteractionProtocol protocol(mGateway, gameAdapter);
	for (std::vector<std::string>::const_iterator command = commands.begin();
		command != commands.end() && mGateway.CanStartQueuedCommand(); ++command)
	{
		Log("Peer says: %s\n", command->c_str());
		SendMessage(protocol.HandleCommand(*command));
	}

	cGameInteractionEvent publishedEvent;
	while (mGateway.TryTakePublishedEvent(publishedEvent))
		SendMessage(cLegacyGameInteractionProtocol::SerializeEvent(publishedEvent));
}

void cLuxSocketServer::PublishEvent(const cGameInteractionEvent& aEvent)
{
	mGateway.Publish(aEvent);
}

void cLuxSocketServer::SendMessage(const tString& message)
{
	const bool hadPeer = mTransport.HasPeer();
	mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(message));
	if (hadPeer && !mTransport.HasPeer())
	{
		mGateway.EndSession();
		Log("Game Interaction Protocol delivery failed: %s\n", mTransport.GetDiagnostic().c_str());
	}
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
