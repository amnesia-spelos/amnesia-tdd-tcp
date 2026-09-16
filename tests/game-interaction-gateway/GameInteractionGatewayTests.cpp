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
}

int main()
{
	cGameInteractionGateway gateway;
	const cGameInteractionCommand command(eGameInteractionCommand_Ping);

	const cGameInteractionResponse response = gateway.Handle(command);

	Expect(response.GetCommandType() == eGameInteractionCommand_Ping,
		"Response identifies the handled Command");
	Expect(response.GetType() == eGameInteractionResponse_Pong,
		"ping produces a typed pong Response");

	std::cout << "Game Interaction Protocol gateway ping case passed\n";
	return 0;
}
