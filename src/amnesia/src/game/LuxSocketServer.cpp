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
	mListenSocket = INVALID_SOCKET;
    mClientSocket = INVALID_SOCKET;

	InitSocket();
    Log("cLuxSocketServer created!\n");
}

bool cLuxSocketServer::InitSocket()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        Log("WSAStartup failed\n");
        return false;
    }

    mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListenSocket == INVALID_SOCKET)
    {
        Log("Socket creation failed\n");
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(mListenSocket, FIONBIO, &nonBlocking);

    sockaddr_in service;
    service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(mHost.c_str());
	service.sin_port = htons(mPort);

    if (bind(mListenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
    {
        Log("Bind failed\n");
        closesocket(mListenSocket);
        WSACleanup();
        return false;
    }

    if (listen(mListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        Log("Listen failed\n");
        closesocket(mListenSocket);
        WSACleanup();
        return false;
    }

    Log("Socket listening on %s:%d\n", mHost.c_str(), mPort);
    return true;
}

void cLuxSocketServer::ShutdownSocket()
{
    if (mClientSocket != INVALID_SOCKET)
    {
        closesocket(mClientSocket);
        mClientSocket = INVALID_SOCKET;
    }

    if (mListenSocket != INVALID_SOCKET)
    {
        closesocket(mListenSocket);
        mListenSocket = INVALID_SOCKET;
    }

    WSACleanup();
}

void cLuxSocketServer::Update(float afTimeStep)
{
	// Accept client if not already connected
    if (mClientSocket == INVALID_SOCKET)
    {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(mListenSocket, (SOCKADDR*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET)
        {
            Log("Client connected!\n");
            mClientSocket = clientSocket;
			mInboundLines.Clear();

            SendMessage(cLegacyGameInteractionProtocol::Greeting());
        }
    }

    // Handle incoming data
    if (mClientSocket != INVALID_SOCKET)
    {
        char buffer[8192];
        int bytesReceived = recv(mClientSocket, buffer, sizeof(buffer), 0);

        if (bytesReceived > 0)
        {
			mInboundLines.Append(buffer, bytesReceived);
			cLuxLegacyGameAdapter gameAdapter;
			cLegacyGameInteractionProtocol protocol(gameAdapter);
			std::string command;
			while (mInboundLines.TryPopLine(command))
			{
				Log("Client says: %s\n", command.c_str());
				SendMessage(protocol.HandleCommand(command));
			}
        }
        else if (bytesReceived == 0 || (bytesReceived == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
        {
            Log("Client disconnected.\n");
            closesocket(mClientSocket);
            mClientSocket = INVALID_SOCKET;
			mInboundLines.Clear();
        }
    }
}

void cLuxSocketServer::SendMessage(const tString& message)
{
    if (mClientSocket != INVALID_SOCKET)
    {
        tString safeMessage = cLegacyGameInteractionProtocol::ToWireLine(message);

        send(mClientSocket, safeMessage.c_str(), (int)safeMessage.length(), 0);
    }
}

void cLuxSocketServer::SetConnectionSettings(const tString& host, int port)
{
    mHost = host;
    mPort = port;

	Log("LuxSocketServer config changed: re-init\n");
	ShutdownSocket();
    InitSocket();
}

cLuxSocketServer::~cLuxSocketServer()
{
	ShutdownSocket();
    Log("cLuxSocketServer destroyed!\n");
}
