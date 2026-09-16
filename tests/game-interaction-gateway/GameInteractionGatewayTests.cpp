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

		bool mbMapLoaded;
		cGameInteractionPosition mPosition;
		cGameInteractionRotation mRotation;
		std::string msMapFile;
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

	adapter.mbMapLoaded = false;
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
