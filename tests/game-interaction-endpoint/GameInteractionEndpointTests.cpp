#include "GameInteractionEndpoint.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void ExpectEndpoint(const cGameInteractionEndpoint& aActual, const std::string& asHost, int alPort,
		const char* apDescription)
	{
		if (aActual.msHost == asHost && aActual.mlPort == alPort) return;
		std::cerr << "FAIL: " << apDescription << "\n  expected: " << asHost << ":" << alPort
			<< "\n  actual:   " << aActual.msHost << ":" << aActual.mlPort << "\n";
		exit(1);
	}
}

int main()
{
	ExpectEndpoint(ParseGameInteractionEndpoint("0.0.0.0", "6000"), "0.0.0.0", 6000,
		"a stored host and port are read as they are");

	ExpectEndpoint(ParseGameInteractionEndpoint("", ""), "127.0.0.1", 5150,
		"a fresh config listens on the default endpoint");

	const char* const invalidPorts[] = { "abc", "51x50", "-1", "0", "65536", "99999999999", " 5150" };
	for (size_t i = 0; i < sizeof(invalidPorts) / sizeof(invalidPorts[0]); ++i)
	{
		const std::string description = std::string("the port '") + invalidPorts[i] + "' falls back to the default";
		ExpectEndpoint(ParseGameInteractionEndpoint("0.0.0.0", invalidPorts[i]), "0.0.0.0", 5150, description.c_str());
	}

	ExpectEndpoint(ParseGameInteractionEndpoint("0.0.0.0", "65535"), "0.0.0.0", 65535,
		"the highest TCP port is kept");

	std::cout << "Game Interaction Endpoint cases passed\n";
	return 0;
}
