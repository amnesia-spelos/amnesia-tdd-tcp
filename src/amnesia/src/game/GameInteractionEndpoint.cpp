#include "GameInteractionEndpoint.h"

namespace
{
	const char* const kDefaultHost = "127.0.0.1";
	const int kDefaultPort = 5150;
	const int kHighestPort = 65535;

	// Digits only: atoi would read "51x50" as 51 and "abc" as 0.
	int ParsePort(const std::string& asPort)
	{
		if (asPort.empty()) return kDefaultPort;
		int port = 0;
		for (size_t i = 0; i < asPort.size(); ++i)
		{
			if (asPort[i] < '0' || asPort[i] > '9') return kDefaultPort;
			port = port * 10 + (asPort[i] - '0');
			if (port > kHighestPort) return kDefaultPort;
		}
		return port == 0 ? kDefaultPort : port;
	}
}

cGameInteractionEndpoint ParseGameInteractionEndpoint(const std::string& asHost, const std::string& asPort)
{
	cGameInteractionEndpoint endpoint;
	endpoint.msHost = asHost.empty() ? kDefaultHost : asHost;
	endpoint.mlPort = ParsePort(asPort);
	return endpoint;
}
