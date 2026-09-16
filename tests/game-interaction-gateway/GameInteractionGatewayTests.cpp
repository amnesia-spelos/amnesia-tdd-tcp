#include "GameInteractionGateway.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	class cFakeGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		cFakeGameAdapter()
			: mbMapLoaded(true), mPosition(1.25f, -2.5f, 3.75f),
			mRotation(1.570796325f, -0.7853981625f), msMapFile("maps/main/level01.map")
		{
		}

		virtual bool IsMapLoaded() const { return mbMapLoaded; }
		virtual cGameInteractionPosition GetPosition() const { return mPosition; }
		virtual cGameInteractionRotation GetRotation() const { return mRotation; }
		virtual std::string GetMapFile() const { return msMapFile; }
		virtual void RunScript(const std::string& asScript) { msExecutedScript = asScript; }

		bool mbMapLoaded;
		cGameInteractionPosition mPosition;
		cGameInteractionRotation mRotation;
		std::string msMapFile;
		std::string msExecutedScript;
	};

	void ExpectSuccess(const cGameInteractionResponse& aResponse,
		eGameInteractionCommandType aCommand, eGameInteractionResponseType aType)
	{
		Expect(aResponse.GetCommandType() == aCommand, "Response identifies the handled Command");
		Expect(aResponse.GetType() == aType, "Response has the expected typed value");
		Expect(aResponse.GetOutcome() == eGameInteractionCommandOutcome_Success,
			"Response has a successful Command Outcome");
	}
}

int main()
{
	cFakeGameAdapter adapter;
	cGameInteractionGateway gateway;

	gateway.BeginLegacySession();
	gateway.Publish(cGameInteractionEvent(eGameInteractionEvent_MapChanged,
		"maps/main/level02.map"));
	cGameInteractionEvent publishedEvent;
	Expect(gateway.TryTakePublishedEvent(publishedEvent), "active Session receives a published Event");
	Expect(publishedEvent.GetType() == eGameInteractionEvent_MapChanged,
		"published Event retains its typed meaning");
	Expect(publishedEvent.GetData() == "maps/main/level02.map",
		"published map-change Event retains its map file");
	Expect(!gateway.TryTakePublishedEvent(publishedEvent), "published Event is delivered once");
	gateway.Publish(cGameInteractionEvent(eGameInteractionEvent_ScriptCallObserved,
		"OnCollide(\"Player\", \"Door\", 1)"));
	Expect(gateway.TryTakePublishedEvent(publishedEvent), "script-call observation is published");
	Expect(publishedEvent.GetType() == eGameInteractionEvent_ScriptCallObserved,
		"script-call observation retains its distinct typed meaning");
	Expect(publishedEvent.GetData() == "OnCollide(\"Player\", \"Door\", 1)",
		"script-call observation retains its script call");
	gateway.EndSession();
	gateway.Publish(cGameInteractionEvent(eGameInteractionEvent_MapChanged, "not-delivered.map"));
	Expect(!gateway.TryTakePublishedEvent(publishedEvent), "Event without an active Session is discarded");
	gateway.BeginLegacySession();

	const cGameInteractionResponse ping = gateway.Handle(cGameInteractionCommand(eGameInteractionCommand_Ping), adapter);
	ExpectSuccess(ping, eGameInteractionCommand_Ping, eGameInteractionResponse_Pong);

	const cGameInteractionResponse position = gateway.Handle(cGameInteractionCommand(eGameInteractionCommand_GetPosition), adapter);
	ExpectSuccess(position, eGameInteractionCommand_GetPosition, eGameInteractionResponse_Position);
	Expect(position.GetPosition().mfX == 1.25f && position.GetPosition().mfY == -2.5f &&
		position.GetPosition().mfZ == 3.75f, "getpos returns the adapter's position");

	const cGameInteractionResponse rotation = gateway.Handle(cGameInteractionCommand(eGameInteractionCommand_GetRotation), adapter);
	ExpectSuccess(rotation, eGameInteractionCommand_GetRotation, eGameInteractionResponse_Rotation);
	Expect(rotation.GetRotation().mfYawRadians == 1.570796325f &&
		rotation.GetRotation().mfPitchRadians == -0.7853981625f,
		"getrot returns the adapter's rotation");

	const cGameInteractionResponse pose = gateway.Handle(cGameInteractionCommand(eGameInteractionCommand_GetPositionRotation), adapter);
	ExpectSuccess(pose, eGameInteractionCommand_GetPositionRotation, eGameInteractionResponse_PositionRotation);
	Expect(pose.GetPosition().mfX == 1.25f && pose.GetRotation().mfPitchRadians == -0.7853981625f,
		"getposrot returns the adapter's position and rotation");

	const cGameInteractionResponse map = gateway.Handle(cGameInteractionCommand(eGameInteractionCommand_GetMap), adapter);
	ExpectSuccess(map, eGameInteractionCommand_GetMap, eGameInteractionResponse_Map);
	Expect(map.GetMapFile() == "maps/main/level01.map", "getmap returns the adapter's map file");

	const std::string script = "SetLocalVarInt(\"lever\", 1):with:colons";
	const cGameInteractionCommand execute(eGameInteractionCommand_ExecuteScript, script);
	Expect(execute.GetClassification() == eGameInteractionCommandClassification_StateChanging,
		"exec is classified as a state-changing Command");
	const cGameInteractionResponse execution = gateway.Handle(execute, adapter);
	ExpectSuccess(execution, eGameInteractionCommand_ExecuteScript, eGameInteractionResponse_ScriptExecuted);
	Expect(adapter.msExecutedScript == script, "exec passes arbitrary script text through the game adapter");

	adapter.mbMapLoaded = false;
	adapter.msExecutedScript.clear();
	const cGameInteractionResponse executionWithoutMap = gateway.Handle(execute, adapter);
	Expect(executionWithoutMap.GetCommandType() == eGameInteractionCommand_ExecuteScript,
		"map-not-loaded exec Response identifies the handled Command");
	Expect(executionWithoutMap.GetType() == eGameInteractionResponse_ScriptExecuted,
		"map-not-loaded exec retains its typed Response");
	Expect(executionWithoutMap.GetOutcome() == eGameInteractionCommandOutcome_MapNotLoaded,
		"map-not-loaded exec has a stable Command Outcome");
	Expect(adapter.msExecutedScript.empty(), "exec does not reach the game adapter without a loaded map");

	const eGameInteractionCommandType observationalCommands[] = {
		eGameInteractionCommand_GetPosition,
		eGameInteractionCommand_GetRotation,
		eGameInteractionCommand_GetPositionRotation,
		eGameInteractionCommand_GetMap
	};
	for (int index = 0; index < 4; ++index)
	{
		const cGameInteractionResponse response = gateway.Handle(cGameInteractionCommand(observationalCommands[index]), adapter);
		Expect(response.GetCommandType() == observationalCommands[index],
			"map-not-loaded Response identifies the handled Command");
		Expect(response.GetOutcome() == eGameInteractionCommandOutcome_MapNotLoaded,
			"missing map has a stable Command Outcome");
	}

	std::cout << "Game Interaction Protocol gateway observational cases passed\n";
	return 0;
}
