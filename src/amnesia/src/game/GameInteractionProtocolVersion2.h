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
	// Granted Capabilities that change a message's fields, such as lean, decide how it is read.
	static cGameInteractionCommand ParseCommand(const std::string& asLine,
		unsigned int alGrantedCapabilities = eGameInteractionCapability_None);
	static std::string SerializeResponse(const cGameInteractionResponse& aResponse);
	static std::string SerializeEnemyPose(const cGameInteractionEnemyPose& aPose);
	static std::string SerializeLocalPose(const cGameInteractionPose& aPose,
		unsigned int alGrantedCapabilities = eGameInteractionCapability_None);
	static std::string SerializeReportedBodies(const cGameInteractionBodySamples& aBodies);
	// The settingchanged Event, whose value is the rest of the line and may be empty.
	static std::string SerializeSettingChanged(const std::string& asKey, const std::string& asValue);
	// Only the interactions and sharedcallbacks Events have a Protocol Version 2 form.
	static std::string SerializeEvent(const cGameInteractionEvent& aEvent);
	// The fields a sharedcallback Event and Command share: <kind> <fields>... <map>.
	static std::string FormatSharedCallback(const cGameInteractionSharedCallback& aCallback);
	static bool IsValidItemName(const std::string& asItem);
	// A checkpoint name as one field: the name itself when it is a valid item name, otherwise -.
	static std::string FormatCheckPointName(const std::string& asName);
	static std::string SerializeCheckPoint(const cGameInteractionCheckPoint& aCheckPoint);
	static std::string SerializePlayerDeath(int alDeath, const cGameInteractionCheckPoint& aCheckPoint);

	static std::string FormatNumber(double afValue);
	static bool TryParseNumber(const std::string& asText, double& afValue);
	static bool TryParseUnsignedInteger(const std::string& asText, unsigned long long alMaximum,
		unsigned long long& alValue);
	static bool IsValidAvatarIdentifier(const std::string& asIdentifier);
	static std::string FormatEntityIdentifier(int alIdentifier);
	static bool TryParseEntityIdentifier(const std::string& asText, int& alIdentifier);
	// Writes the 13 fields of a body state within their bounds.
	static std::string FormatBodyState(const cGameInteractionBodyState& aState);
};

#endif
