#include "GameInteractionGateway.h"
#include "GameInteractionTransport.h"
#include "GameInteractionProtocolVersion2.h"
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
	: mType(aType), msData(asData), mlProtocolVersion(0), mlCapabilities(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asChatAuthor, const std::wstring& asChatMessage)
	: mType(aType), msChatAuthor(asChatAuthor), msChatMessage(asChatMessage), mlProtocolVersion(0),
	  mlCapabilities(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	const std::wstring& asCustomStoryIdentifier)
	: mType(aType), msCustomStoryIdentifier(asCustomStoryIdentifier), mlProtocolVersion(0), mlCapabilities(0)
{
}

cGameInteractionCommand::cGameInteractionCommand(eGameInteractionCommandType aType,
	unsigned int alProtocolVersion, unsigned int alCapabilities)
	: mType(aType), mlProtocolVersion(alProtocolVersion), mlCapabilities(alCapabilities)
{
}

eGameInteractionCommandClassification cGameInteractionCommand::GetClassification() const
{
	return mType == eGameInteractionCommand_ExecuteScript || mType == eGameInteractionCommand_Chat ||
		mType == eGameInteractionCommand_StartCustomStory || mType == eGameInteractionCommand_AvatarCreate ||
		mType == eGameInteractionCommand_AvatarRemove || mType == eGameInteractionCommand_AvatarCollision ||
		mType == eGameInteractionCommand_AvatarPose ?
		eGameInteractionCommandClassification_StateChanging :
		eGameInteractionCommandClassification_Observational;
}

cGameInteractionResponse::cGameInteractionResponse(eGameInteractionCommandType aCommandType,
	eGameInteractionResponseType aType, eGameInteractionCommandOutcome aOutcome)
	: mCommandType(aCommandType), mType(aType), mOutcome(aOutcome), mlCapabilities(0)
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

	const unsigned int kSupportedProtocolVersion = 2;
	const unsigned int kSupportedCapabilities =
		eGameInteractionCapability_Avatars | eGameInteractionCapability_LocalPose;

	struct cSessionProtocol
	{
		cSessionProtocol()
			: mbNegotiated(false), mbCommandProcessed(false), mlGrantedCapabilities(eGameInteractionCapability_None) {}
		bool mbNegotiated;
		// Set by any Command other than a failed negotiation, after which negotiating is too late.
		bool mbCommandProcessed;
		unsigned int mlGrantedCapabilities;
	};

	eGameInteractionCommandOutcome NegotiationOutcomeFor(const cGameInteractionCommand& aCommand,
		const cSessionProtocol& aSession)
	{
		if (aSession.mbNegotiated) return eGameInteractionCommandOutcome_AlreadyNegotiated;
		if (aSession.mbCommandProcessed) return eGameInteractionCommandOutcome_NegotiationTooLate;
		if (aCommand.GetProtocolVersion() == 0) return eGameInteractionCommandOutcome_Invalid;
		if (aCommand.GetProtocolVersion() != kSupportedProtocolVersion)
			return eGameInteractionCommandOutcome_UnsupportedProtocolVersion;
		return eGameInteractionCommandOutcome_Success;
	}

	// A failed first negotiation leaves the Session on the legacy protocol and may be retried, so a
	// Peer can fall back from a Protocol Version this game does not support.
	cGameInteractionResponse NegotiateProtocol(const cGameInteractionCommand& aCommand, cSessionProtocol& aSession)
	{
		cGameInteractionResponse response(aCommand.GetType(), eGameInteractionResponse_ProtocolNegotiated,
			NegotiationOutcomeFor(aCommand, aSession));
		if (response.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
		aSession.mbNegotiated = true;
		aSession.mlGrantedCapabilities = aCommand.GetCapabilities() & kSupportedCapabilities;
		response.SetCapabilities(aSession.mlGrantedCapabilities);
		return response;
	}

	unsigned int RequiredCapability(eGameInteractionCommandType aType)
	{
		switch (aType)
		{
		case eGameInteractionCommand_AvatarCreate:
		case eGameInteractionCommand_AvatarRemove:
		case eGameInteractionCommand_AvatarCollision:
		case eGameInteractionCommand_AvatarPose:
			return eGameInteractionCapability_Avatars;
		case eGameInteractionCommand_LocalPose:
			return eGameInteractionCapability_LocalPose;
		default:
			return eGameInteractionCapability_None;
		}
	}

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
		iGameInteractionGameAdapter& aGameAdapter, cSessionProtocol& aSession,
		cPendingCustomStoryStart& aPendingStart)
	{
		if (aCommand.GetType() != eGameInteractionCommand_NegotiateProtocol) aSession.mbCommandProcessed = true;
		const unsigned int requiredCapability = RequiredCapability(aCommand.GetType());
		if ((aSession.mlGrantedCapabilities & requiredCapability) != requiredCapability)
			return cGameInteractionResponse(aCommand.GetType(), eGameInteractionResponse_Rejected,
				eGameInteractionCommandOutcome_CapabilityNotGranted);
		switch (aCommand.GetType())
		{
		case eGameInteractionCommand_NegotiateProtocol:
			return NegotiateProtocol(aCommand, aSession);
		case eGameInteractionCommand_AvatarCreate:
		case eGameInteractionCommand_AvatarRemove:
		case eGameInteractionCommand_AvatarCollision:
		case eGameInteractionCommand_AvatarPose:
		case eGameInteractionCommand_LocalPose:
			// Granted Capabilities gain their behavior in #30 and #31; until then their Commands are unknown.
			return cGameInteractionResponse(eGameInteractionCommand_Unknown, eGameInteractionResponse_Rejected);
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
	cSessionProtocol mSession;

	// The legacy adapter serves a Session until it negotiates; its negotiation Command is the only
	// Protocol Version 2 line a legacy Session understands.
	cGameInteractionCommand ParseCommand(const std::string& asLine) const
	{
		if (mSession.mbNegotiated || cGameInteractionProtocolVersion2::IsNegotiation(asLine))
			return cGameInteractionProtocolVersion2::ParseCommand(asLine);
		return cLegacyGameInteractionProtocol::ParseCommand(asLine);
	}

	std::string SerializeResponse(const cGameInteractionResponse& aResponse) const
	{
		if (mSession.mbNegotiated || aResponse.GetCommandType() == eGameInteractionCommand_NegotiateProtocol)
			return cGameInteractionProtocolVersion2::SerializeResponse(aResponse);
		return cLegacyGameInteractionProtocol::SerializeResponse(aResponse);
	}

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
		mpImplementation->mSession = cSessionProtocol();
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			cLegacyGameInteractionProtocol::Greeting()));
	}
	else if (event == eGameInteractionTransportEvent_PeerDisconnected)
	{
		mpImplementation->mInboundLines.Clear();
		mpImplementation->mSession = cSessionProtocol();
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
		const cGameInteractionCommand command = mpImplementation->ParseCommand(commandText);
		const cGameInteractionResponse response = ExecuteCommand(command, aGameAdapter,
			mpImplementation->mSession, mpImplementation->mPendingCustomStoryStart);
		mpImplementation->mTransport.QueueBytes(cLegacyGameInteractionProtocol::ToWireLine(
			mpImplementation->SerializeResponse(response)));
	}
	// Delivering Responses now rather than on the next update removes a tick of latency.
	mpImplementation->mTransport.Flush();

	if (mpImplementation->mInboundLines.HasExceededLineLimit())
	{
		mpImplementation->mTransport.DisconnectPeer("Peer exceeded the inbound line length limit");
		mpImplementation->mInboundLines.Clear();
		mpImplementation->mSession = cSessionProtocol();
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
