#include "GameInteractionGateway.h"
#include "GameInteractionTransport.h"
#include "LegacyGameInteractionProtocol.h"

#include <vector>

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::string& asData)
	: mType(aType), msData(asData)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::wstring& asChatAuthor, const std::wstring& asChatMessage)
	: mType(aType), msChatAuthor(asChatAuthor), msChatMessage(asChatMessage)
{
}

cGameInteractionEvent::cGameInteractionEvent(eGameInteractionEventType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::string& asData)
	: mType(aType), msData(asData)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asChatAuthor, const std::wstring& asChatMessage)
	: mType(aType), msChatAuthor(asChatAuthor), msChatMessage(asChatMessage)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier)
{
}

eGameInteractionCommandClassification cGameInteractionCommand::GetClassification() const
{
	return mType == eGameInteractionCommand_ExecuteScript || mType == eGameInteractionCommand_Chat ||
		mType == eGameInteractionCommand_StartCustomStory ?
		eGameInteractionCommandClassification_StateChanging :
		eGameInteractionCommandClassification_Observational;
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome)
{
}

namespace
{
	struct cPendingCustomStoryStart
	{
		cPendingCustomStoryStart() : mbPending(false) {}
		bool mbPending;
		std::wstring msIdentifier;
	};

	eGameInteractionCommandOutcome OutcomeFor(eGameInteractionCustomStoryAvailability aAvailability)
	{
		switch (aAvailability)
		{
		case eGameInteractionCustomStoryAvailability_Available: return eGameInteractionCommandOutcome_Success;
		case eGameInteractionCustomStoryAvailability_NotFound: return eGameInteractionCommandOutcome_CustomStoryNotFound;
		case eGameInteractionCustomStoryAvailability_Invalid: return eGameInteractionCommandOutcome_CustomStoryInvalid;
		default: return eGameInteractionCommandOutcome_NotInMainMenu;
		}
	}

	cGameInteractionResponse ExecuteCommand(const cGameInteractionCommand& aCommand,
		iGameInteractionGameAdapter& aGameAdapter, cPendingCustomStoryStart& aPendingStart)
	{
		switch (aCommand.GetType())
		{
		case eGameInteractionCommand_GetPosition:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Position,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetPosition(aGameAdapter.GetPosition());
			return response;
		}
		case eGameInteractionCommand_GetRotation:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Rotation,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetRotation(aGameAdapter.GetRotation());
			return response;
		}
		case eGameInteractionCommand_GetPositionRotation:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_PositionRotation,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
			response.SetPosition(aGameAdapter.GetPosition());
			response.SetRotation(aGameAdapter.GetRotation());
			return response;
		}
		case eGameInteractionCommand_GetMap:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_Map,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				response.SetMapFile(aGameAdapter.GetMapFile());
			return response;
		}
		case eGameInteractionCommand_ExecuteScript:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_ScriptExecuted,
				aGameAdapter.IsMapLoaded() ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_MapNotLoaded);
			if (response.GetOutcome() == eGameInteractionCommandOutcome_Success)
				aGameAdapter.RunScript(aCommand.GetData());
			return response;
		}
		case eGameInteractionCommand_Chat:
		{
			cChatEntry entry;
			const eChatEntryValidation validation = cChatModel::TryCreateEntry(
				aCommand.GetChatAuthor(), aCommand.GetChatMessage(), entry);
			if (validation == eChatEntryValidation_InvalidAuthor)
				return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
					eGameInteractionCommandOutcome_InvalidAuthor);
			if (validation == eChatEntryValidation_InvalidMessage)
				return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
					eGameInteractionCommandOutcome_InvalidMessage);
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_ChatDisplayed,
				aGameAdapter.DisplayChatEntry(entry) ? eGameInteractionCommandOutcome_Success :
					eGameInteractionCommandOutcome_Unavailable);
		}
		case eGameInteractionCommand_GetCustomStories:
		{
			cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_CustomStories);
			response.SetCustomStories(aGameAdapter.GetCustomStories());
			return response;
		}
		case eGameInteractionCommand_StartCustomStory:
		{
			// A pending start will leave the main menu, so a second start cannot be accepted.
			const eGameInteractionCommandOutcome outcome = aPendingStart.mbPending ?
				eGameInteractionCommandOutcome_NotInMainMenu :
				OutcomeFor(aGameAdapter.GetCustomStoryAvailability(aCommand.GetCustomStoryIdentifier()));
			if (outcome == eGameInteractionCommandOutcome_Success)
			{
				aPendingStart.mbPending = true;
				aPendingStart.msIdentifier = aCommand.GetCustomStoryIdentifier();
			}
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_CustomStoryStarting, outcome);
		}
		default:
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Pong);
		}
	}
}

class cGameInteractionGateway::cImplementation
{
public:
	cGameInteractionTransport mTransport;
	cGameInteractionLineBuffer mInboundLines;
	cPendingCustomStoryStart mPendingCustomStoryStart;

	// Runs after the transport has flushed the accepting Response and outside Command processing,
	// because starting a Custom Story blocks while its start map loads.
	void PerformPendingCustomStoryStart(iGameInteractionGameAdapter& aGameAdapter)
	{
		if (!mPendingCustomStoryStart.mbPending) return;
		const std::wstring identifier = mPendingCustomStoryStart.msIdentifier;
		mPendingCustomStoryStart = cPendingCustomStoryStart();
		aGameAdapter.StartCustomStory(identifier);
	}
};

cGameInteractionGateway::cGameInteractionGateway()
	: mpImplementation(new cImplementation)
{
}

cGameInteractionGateway::~cGameInteractionGateway()
{
	delete mpImplementation;
}

bool cGameInteractionGateway::Listen(const std::string& asHost, int alPort)
{
	mpImplementation->mInboundLines.Clear();
	return mpImplementation->mTransport.Listen(asHost, alPort);
}

void cGameInteractionGateway::Shutdown()
{
	mpImplementation->mInboundLines.Clear();
	mpImplementation->mTransport.Shutdown();
}

void cGameInteractionGateway::Update(iGameInteractionGameAdapter& aGameAdapter)
{
	std::vector<std::string> receivedBytes;
	const eGameInteractionTransportEvent event = mpImplementation->mTransport.Update(receivedBytes);
	if (event == eGameInteractionTransportEvent_PeerConnected)
	{
		mpImplementation->mInboundLines.Clear();
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cLegacyGameInteractionProtocol::Greeting()));
	}
	else if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		mpImplementation->mInboundLines.Clear();
	}

	mpImplementation->PerformPendingCustomStoryStart(aGameAdapter);
	if (event == eGameInteractionTransportEvent_PeerDisconnected) return;

	for (std::vector<std::string>::const_iterator bytes = receivedBytes.begin();
		bytes != receivedBytes.end(); ++bytes)
		mpImplementation->mInboundLines.Append(bytes->data(), bytes->size());

	std::string commandText;
	while (mpImplementation->mTransport.HasPeer() &&
		mpImplementation->mInboundLines.TryPopLine(commandText))
	{
		const cGameInteractionCommand command = cLegacyGameInteractionProtocol::ParseCommand(commandText);
		const cGameInteractionResponse response = ExecuteCommand(command, aGameAdapter,
			mpImplementation->mPendingCustomStoryStart);
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cLegacyGameInteractionProtocol::SerializeResponse(response)));
	}
}

void cGameInteractionGateway::Report(const cGameInteractionEvent& aEvent)
{
	if (!mpImplementation->mTransport.HasPeer()) return;
	mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
		cLegacyGameInteractionProtocol::SerializeEvent(aEvent)));
}

int cGameInteractionGateway::GetPort() const
{
	return mpImplementation->mTransport.GetPort();
}

const std::string& cGameInteractionGateway::GetDiagnostic() const
{
	return mpImplementation->mTransport.GetDiagnostic();
}
