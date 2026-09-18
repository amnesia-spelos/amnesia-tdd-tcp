#include "GameInteractionProtocolVersion2.h"
#include "LegacyGameInteractionProtocol.h"

#include <cstdio>

namespace
{
	const double kNumberScale = 10000.0;
	// Keeps scaled values inside long long; Poses are far smaller than this.
	const double kLargestFormattedMagnitude = 1.0e14;
	// Fifteen digits stay exact in a double mantissa, so dividing by a power of ten rounds once.
	const int kMaximumNumberDigits = 15;
	const std::string::size_type kMaximumAvatarIdentifierLength = 32;

	const char* const kNegotiationKeyword = "protocol";
	const std::string kNegotiationPrefix = std::string(kNegotiationKeyword) + " ";
	const unsigned long long kMaximumProtocolVersion = 0xFFFFFFFFu;
	const unsigned long long kLargestParsedLocalPoseRate = 0xFFFFFFFFu;

	struct cCapabilityName
	{
		eGameInteractionCapability mCapability;
		const char* mpName;
	};

	// Also the canonical order in which granted Capabilities are reported.
	const cCapabilityName kCapabilityNames[] =
	{
		{ eGameInteractionCapability_Avatars, "avatars" },
		{ eGameInteractionCapability_LocalPose, "localpose" }
	};
	const int kCapabilityNameCount = sizeof(kCapabilityNames) / sizeof(kCapabilityNames[0]);

	struct cCommandKeyword
	{
		eGameInteractionCommandType mType;
		const char* mpKeyword;
	};

	const cCommandKeyword kCommandKeywords[] =
	{
		{ eGameInteractionCommand_NegotiateProtocol, kNegotiationKeyword },
		{ eGameInteractionCommand_AvatarCreate, "avatarcreate" },
		{ eGameInteractionCommand_AvatarRemove, "avatarremove" },
		{ eGameInteractionCommand_AvatarCollision, "avatarcollision" },
		{ eGameInteractionCommand_AvatarPose, "avatarpose" },
		{ eGameInteractionCommand_LocalPose, "localpose" }
	};
	const int kCommandKeywordCount = sizeof(kCommandKeywords) / sizeof(kCommandKeywords[0]);

	const char* KeywordFor(eGameInteractionCommandType aType)
	{
		for (int index = 0; index < kCommandKeywordCount; ++index)
			if (kCommandKeywords[index].mType == aType) return kCommandKeywords[index].mpKeyword;
		return NULL;
	}

	eGameInteractionCommandType CommandTypeFor(const std::string& asKeyword)
	{
		for (int index = 0; index < kCommandKeywordCount; ++index)
			if (asKeyword == kCommandKeywords[index].mpKeyword) return kCommandKeywords[index].mType;
		return eGameInteractionCommand_Unknown;
	}

	bool IsDigit(char acCharacter)
	{
		return acCharacter >= '0' && acCharacter <= '9';
	}

	const char* OutcomeToken(eGameInteractionCommandOutcome aOutcome)
	{
		switch (aOutcome)
		{
		case eGameInteractionCommandOutcome_Success: return "ok";
		case eGameInteractionCommandOutcome_UnsupportedProtocolVersion: return "unsupported-version";
		case eGameInteractionCommandOutcome_AlreadyNegotiated: return "already-negotiated";
		case eGameInteractionCommandOutcome_NegotiationTooLate: return "too-late";
		case eGameInteractionCommandOutcome_CapabilityNotGranted: return "not-granted";
		default: return "invalid";
		}
	}

	unsigned int CapabilityNamed(const std::string& asName)
	{
		for (int index = 0; index < kCapabilityNameCount; ++index)
			if (asName == kCapabilityNames[index].mpName) return kCapabilityNames[index].mCapability;
		return eGameInteractionCapability_None;
	}

	// Unknown Capability names are ignored so Peers can request Capabilities a newer game may support.
	cGameInteractionCommand ParseNegotiation(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string field;
		unsigned long long version = 0;
		reader.TryReadField(field);
		if (!reader.TryReadField(field) ||
			!cGameInteractionProtocolVersion2::TryParseUnsignedInteger(field, kMaximumProtocolVersion, version))
			version = 0;
		unsigned int capabilities = eGameInteractionCapability_None;
		while (version != 0 && !reader.IsAtEnd())
		{
			if (reader.TryReadField(field)) capabilities |= CapabilityNamed(field);
			else version = 0;
		}
		return cGameInteractionCommand(eGameInteractionCommand_NegotiateProtocol,
			static_cast<unsigned int>(version), capabilities);
	}

	// Any decimal integer is a rate; the gateway clamps it, so rates beyond 32 bits saturate.
	bool TryParseLocalPoseRate(const std::string& asText, unsigned int& alRate)
	{
		if (asText.empty() || asText.find_first_not_of("0123456789") != std::string::npos) return false;
		unsigned long long rate = kLargestParsedLocalPoseRate;
		cGameInteractionProtocolVersion2::TryParseUnsignedInteger(asText, kLargestParsedLocalPoseRate, rate);
		alRate = static_cast<unsigned int>(rate);
		return true;
	}

	cGameInteractionCommand ParseLocalPose(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		std::string request;
		if (reader.TryReadField(request) && request == "unsubscribe" && reader.IsAtEnd())
			return cGameInteractionCommand(eGameInteractionCommand_LocalPose,
				eGameInteractionLocalPoseRequest_Unsubscribe, 0);
		std::string rateField;
		unsigned int rate = 0;
		if (request == "subscribe" && reader.TryReadField(rateField) && reader.IsAtEnd() &&
			TryParseLocalPoseRate(rateField, rate))
			return cGameInteractionCommand(eGameInteractionCommand_LocalPose,
				eGameInteractionLocalPoseRequest_Subscribe, rate);
		return cGameInteractionCommand(eGameInteractionCommand_LocalPose, eGameInteractionLocalPoseRequest_Invalid, 0);
	}
}

cGameInteractionFieldReader::cGameInteractionFieldReader(const std::string& asLine)
	: msLine(asLine), mPosition(0)
{
}

// A position past the line's length means no field remains; one equal to it means an empty field follows.
bool cGameInteractionFieldReader::TryReadField(std::string& asField)
{
	if (mPosition > msLine.size()) return false;
	std::string::size_type end = msLine.find(' ', mPosition);
	if (end == std::string::npos) end = msLine.size();
	if (end == mPosition) return false;
	asField = msLine.substr(mPosition, end - mPosition);
	mPosition = end + 1;
	return true;
}

bool cGameInteractionFieldReader::TryReadRest(std::string& asRest)
{
	if (mPosition >= msLine.size()) return false;
	asRest = msLine.substr(mPosition);
	mPosition = msLine.size() + 1;
	return true;
}

bool cGameInteractionFieldReader::IsAtEnd() const
{
	return mPosition > msLine.size();
}

// Formats through integers only, because the "C"-locale decimal point must not follow the system locale.
std::string cGameInteractionProtocolVersion2::FormatNumber(double afValue)
{
	if (!(afValue > -kLargestFormattedMagnitude && afValue < kLargestFormattedMagnitude))
		afValue = 0.0;
	const double scaled = afValue * kNumberScale;
	long long units = static_cast<long long>(scaled < 0.0 ? scaled - 0.5 : scaled + 0.5);
	const bool negative = units < 0;
	if (negative) units = -units;
	char formatted[64];
	sprintf(formatted, "%s%lld.%04lld", negative ? "-" : "", units / 10000, units % 10000);
	return formatted;
}

// Accepts only -?digits(.digits)? so parsing never follows the system locale.
bool cGameInteractionProtocolVersion2::TryParseNumber(const std::string& asText, double& afValue)
{
	std::string::size_type index = 0;
	const bool negative = index < asText.size() && asText[index] == '-';
	if (negative) ++index;

	double mantissa = 0.0;
	double divisor = 1.0;
	int digitCount = 0;
	bool inFraction = false;
	bool digitsBeforeSeparator = false;
	bool digitsAfterSeparator = false;
	for (; index < asText.size(); ++index)
	{
		const char character = asText[index];
		if (character == '.' && !inFraction && digitsBeforeSeparator) { inFraction = true; continue; }
		if (!IsDigit(character) || ++digitCount > kMaximumNumberDigits) return false;
		mantissa = mantissa * 10.0 + (character - '0');
		if (inFraction) { divisor *= 10.0; digitsAfterSeparator = true; }
		else digitsBeforeSeparator = true;
	}
	if (!digitsBeforeSeparator || (inFraction && !digitsAfterSeparator)) return false;
	afValue = (negative ? -mantissa : mantissa) / divisor;
	return true;
}

bool cGameInteractionProtocolVersion2::IsValidAvatarIdentifier(const std::string& asIdentifier)
{
	if (asIdentifier.empty() || asIdentifier.size() > kMaximumAvatarIdentifierLength) return false;
	for (std::string::size_type index = 0; index < asIdentifier.size(); ++index)
	{
		const unsigned char character = static_cast<unsigned char>(asIdentifier[index]);
		if (character <= ' ' || character > '~' || character == ':') return false;
	}
	return true;
}

bool cGameInteractionProtocolVersion2::TryParseUnsignedInteger(const std::string& asText,
	unsigned long long alMaximum, unsigned long long& alValue)
{
	if (asText.empty()) return false;
	unsigned long long value = 0;
	for (std::string::size_type index = 0; index < asText.size(); ++index)
	{
		if (!IsDigit(asText[index])) return false;
		const unsigned long long digit = static_cast<unsigned long long>(asText[index] - '0');
		if (digit > alMaximum || value > (alMaximum - digit) / 10) return false;
		value = value * 10 + digit;
	}
	alValue = value;
	return true;
}

bool cGameInteractionProtocolVersion2::IsNegotiation(const std::string& asLine)
{
	return asLine == kNegotiationKeyword || asLine.compare(0, kNegotiationPrefix.size(), kNegotiationPrefix) == 0;
}

// Fields beyond the keyword of a Command without a typed form are kept whole for its Capability's
// behavior to parse.
cGameInteractionCommand cGameInteractionProtocolVersion2::ParseCommand(const std::string& asLine)
{
	if (IsNegotiation(asLine)) return ParseNegotiation(asLine);
	cGameInteractionFieldReader reader(asLine);
	std::string keyword;
	const eGameInteractionCommandType type = reader.TryReadField(keyword) ? CommandTypeFor(keyword) :
		eGameInteractionCommand_Unknown;
	if (type == eGameInteractionCommand_Unknown) return cLegacyGameInteractionProtocol::ParseCommand(asLine);
	if (type == eGameInteractionCommand_LocalPose) return ParseLocalPose(asLine);
	std::string fields;
	reader.TryReadRest(fields);
	return cGameInteractionCommand(type, fields);
}

std::string cGameInteractionProtocolVersion2::SerializeResponse(const cGameInteractionResponse& aResponse)
{
	const char* keyword = KeywordFor(aResponse.GetCommandType());
	if (!keyword) return cLegacyGameInteractionProtocol::SerializeResponse(aResponse);

	std::string response = std::string("RESPONSE ") + keyword + " " + OutcomeToken(aResponse.GetOutcome());
	if (aResponse.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
	if (aResponse.GetType() == eGameInteractionResponse_ProtocolNegotiated)
	{
		response += " 2";
		for (int index = 0; index < kCapabilityNameCount; ++index)
			if (aResponse.GetCapabilities() & kCapabilityNames[index].mCapability)
				response += std::string(" ") + kCapabilityNames[index].mpName;
	}
	else if (aResponse.GetType() == eGameInteractionResponse_LocalPoseSubscription)
	{
		if (aResponse.GetLocalPoseRequest() == eGameInteractionLocalPoseRequest_Unsubscribe)
			return response + " unsubscribe";
		char rate[16];
		sprintf(rate, " subscribe %u", aResponse.GetLocalPoseRate());
		response += rate;
	}
	return response;
}

std::string cGameInteractionProtocolVersion2::SerializeLocalPose(const cGameInteractionLocalPose& aPose)
{
	char clockFields[64];
	sprintf(clockFields, "STATE localpose %llu %u ", aPose.mlTimeMs, aPose.mlTeleportCounter);
	return clockFields + FormatNumber(aPose.mFeetPosition.mfX) + " " + FormatNumber(aPose.mFeetPosition.mfY) +
		" " + FormatNumber(aPose.mFeetPosition.mfZ) + " " + FormatNumber(aPose.mfBodyYawDegrees) + " " +
		FormatNumber(aPose.mfCameraPitchDegrees) + (aPose.mbCrouching ? " 1 " : " 0 ") + aPose.msMapFile;
}
