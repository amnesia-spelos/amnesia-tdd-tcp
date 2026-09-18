#ifndef GAME_INTERACTION_PROTOCOL_VERSION_2_H
#define GAME_INTERACTION_PROTOCOL_VERSION_2_H

#include "GameInteractionGateway.h"

#include <string>

// Reads a Protocol Version 2 line: single-space-separated fields, optionally ending with a
// rest-of-line field (a map path) that may itself contain spaces and ':'.
class cGameInteractionFieldReader
{
public:
	explicit cGameInteractionFieldReader(const std::string& asLine);
	bool TryReadField(std::string& asField);
	bool TryReadRest(std::string& asRest);
	bool IsAtEnd() const;

private:
	std::string msLine;
	std::string::size_type mPosition;
};

// Wire format of a Session that negotiated Protocol Version 2. It is a superset of the legacy
// protocol, so Commands and Responses it does not own are delegated to the legacy adapter.
class cGameInteractionProtocolVersion2
{
public:
	static bool IsNegotiation(const std::string& asLine);
	static cGameInteractionCommand ParseCommand(const std::string& asLine);
	static std::string SerializeResponse(const cGameInteractionResponse& aResponse);
	static std::string SerializeLocalPose(const cGameInteractionPose& aPose);

	static std::string FormatNumber(double afValue);
	static bool TryParseNumber(const std::string& asText, double& afValue);
	static bool TryParseUnsignedInteger(const std::string& asText, unsigned long long alMaximum,
		unsigned long long& alValue);
	static bool IsValidAvatarIdentifier(const std::string& asIdentifier);
};

#endif
