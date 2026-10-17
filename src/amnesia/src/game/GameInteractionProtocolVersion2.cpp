#include "GameInteractionProtocolVersion2.h"
#include "LegacyGameInteractionProtocol.h"

#include <cfloat>
#include <cmath>
#include <cstdio>

namespace
{
	const double kNumberScale = 10000.0;
	// Keeps scaled values inside long long; Poses are far smaller than this.
	const double kLargestFormattedMagnitude = 1.0e14;
	// Fifteen digits stay exact in a double mantissa, so dividing by a power of ten rounds once.
	const int kMaximumNumberDigits = 15;
	const std::string::size_type kMaximumAvatarIdentifierLength = 32;
	const std::string::size_type kMaximumItemNameLength = 64;
	const std::string::size_type kMaximumPlacementMapPathLength = 1024;

	const char* const kNegotiationKeyword = "protocol";
	const std::string kNegotiationPrefix = std::string(kNegotiationKeyword) + " ";
	const unsigned long long kMaximumProtocolVersion = 0xFFFFFFFFu;
	const unsigned long long kLargestParsedSubscriptionRate = 0xFFFFFFFFu;
	const unsigned long long kMaximumTimeMs = 0xFFFFFFFFFFFFFFFFull;
	const unsigned long long kMaximumTeleportCounter = 0xFFFFFFFFu;
	const unsigned long long kLargestEntityIdentifier = 2147483647ull;
	const unsigned long long kLargestNegatedEntityIdentifier = 2147483648ull;
	const unsigned long long kMaximumBodyCount = 32;
	// Episodes, deaths, and checkpoint counts fit in a signed 32-bit integer.
	const unsigned long long kMaximumSharedDeathNumber = 0x7FFFFFFF;
	const double kMaximumBodyStateMagnitude = 1.0e9;
	const double kOrientationLengthTolerance = 0.01;

	struct cCapabilityName
	{
		eGameInteractionCapability mCapability;
		const char* mpName;
	};

	// Also the canonical order in which granted Capabilities are reported.
	const cCapabilityName kCapabilityNames[] =
	{
		{ eGameInteractionCapability_Avatars, "avatars" },
		{ eGameInteractionCapability_LocalPose, "localpose" },
		{ eGameInteractionCapability_Interactions, "interactions" },
		{ eGameInteractionCapability_SharedCallbacks, "sharedcallbacks" },
		{ eGameInteractionCapability_PlayerPlace, "playerplace" },
		{ eGameInteractionCapability_Enemies, "enemies" }
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
		{ eGameInteractionCommand_Chat, "chatentry" },
		{ eGameInteractionCommand_AvatarCreate, "avatarcreate" },
		{ eGameInteractionCommand_AvatarRemove, "avatarremove" },
		{ eGameInteractionCommand_AvatarCollision, "avatarcollision" },
		{ eGameInteractionCommand_AvatarEnemySight, "avatarenemysight" },
		{ eGameInteractionCommand_AvatarPose, "avatarpose" },
		{ eGameInteractionCommand_LocalPose, "localpose" },
		{ eGameInteractionCommand_ReportedBodies, "reportedbodies" },
		{ eGameInteractionCommand_EntityDrive, "entitydrive" },
		{ eGameInteractionCommand_EntityBodies, "entitybodies" },
		{ eGameInteractionCommand_EntityInteracting, "entityinteracting" },
		{ eGameInteractionCommand_EntityBreak, "entitybreak" },
		{ eGameInteractionCommand_EntityRelease, "entityrelease" },
		{ eGameInteractionCommand_SharedCallback, "sharedcallback" },
		{ eGameInteractionCommand_ButtonState, "buttonstate" },
		{ eGameInteractionCommand_LampState, "lampstate" },
		{ eGameInteractionCommand_PlayerPlace, "playerplace" },
		{ eGameInteractionCommand_EnemyPoses, "enemyposes" },
		{ eGameInteractionCommand_EnemyDrive, "enemydrive" },
		{ eGameInteractionCommand_EnemyPose, "enemypose" },
		{ eGameInteractionCommand_EnemyRelease, "enemyrelease" },
		{ eGameInteractionCommand_EnemyAttack, "enemyattack" },
		{ eGameInteractionCommand_SharedDeath, "shareddeath" }
	};
	const int kCommandKeywordCount = sizeof(kCommandKeywords) / sizeof(kCommandKeywords[0]);

	struct cSharedCallbackKindName
	{
		eGameInteractionSharedCallbackKind mKind;
		const char* mpName;
	};

	const cSharedCallbackKindName kSharedCallbackKindNames[] =
	{
		{ eGameInteractionSharedCallbackKind_PlayerCollide, "playercollide" },
		{ eGameInteractionSharedCallbackKind_EntityCollide, "entitycollide" },
		{ eGameInteractionSharedCallbackKind_Interact, "interact" },
		{ eGameInteractionSharedCallbackKind_UseItem, "useitem" },
		{ eGameInteractionSharedCallbackKind_Pickup, "pickup" },
		{ eGameInteractionSharedCallbackKind_Flashback, "flashback" },
		{ eGameInteractionSharedCallbackKind_Combine, "combine" },
		{ eGameInteractionSharedCallbackKind_StickyAttach, "stickyattach" }
	};
	const int kSharedCallbackKindNameCount = sizeof(kSharedCallbackKindNames) / sizeof(kSharedCallbackKindNames[0]);

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
		case eGameInteractionCommandOutcome_AvatarExists: return "exists";
		case eGameInteractionCommandOutcome_AvatarLimitReached: return "limit";
		case eGameInteractionCommandOutcome_AvatarModelNotFound: return "model-not-found";
		case eGameInteractionCommandOutcome_AvatarNotFound: return "not-found";
		case eGameInteractionCommandOutcome_WrongMap: return "wrong-map";
		case eGameInteractionCommandOutcome_EntityNotFound: return "not-found";
		case eGameInteractionCommandOutcome_EntityNotHoldable: return "not-holdable";
		case eGameInteractionCommandOutcome_CallbackNotFound: return "not-found";
		case eGameInteractionCommandOutcome_EntityWrongType: return "wrong-type";
		case eGameInteractionCommandOutcome_InvalidAuthor: return "invalid-author";
		case eGameInteractionCommandOutcome_InvalidMessage: return "invalid-message";
		case eGameInteractionCommandOutcome_Unavailable: return "unavailable";
		case eGameInteractionCommandOutcome_Duplicate: return "duplicate";
		case eGameInteractionCommandOutcome_Stale: return "stale";
		default: return "invalid";
		}
	}

	const char* EndingToken(eGameInteractionEnding aEnding)
	{
		switch (aEnding)
		{
		case eGameInteractionEnding_Thrown: return "thrown";
		case eGameInteractionEnding_TooFar: return "too-far";
		case eGameInteractionEnding_Destroyed: return "destroyed";
		default: return "released";
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
	bool TryParseSubscriptionRate(const std::string& asText, unsigned int& alRate)
	{
		if (asText.empty() || asText.find_first_not_of("0123456789") != std::string::npos) return false;
		unsigned long long rate = kLargestParsedSubscriptionRate;
		cGameInteractionProtocolVersion2::TryParseUnsignedInteger(asText, kLargestParsedSubscriptionRate, rate);
		alRate = static_cast<unsigned int>(rate);
		return true;
	}

	// localpose and reportedbodies: subscribe <hz> or unsubscribe.
	cGameInteractionCommand ParseSubscription(eGameInteractionCommandType aType, const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		std::string request;
		if (reader.TryReadField(request) && request == "unsubscribe" && reader.IsAtEnd())
			return cGameInteractionCommand(aType, eGameInteractionSubscriptionRequest_Unsubscribe, 0);
		std::string rateField;
		unsigned int rate = 0;
		if (request == "subscribe" && reader.TryReadField(rateField) && reader.IsAtEnd() &&
			TryParseSubscriptionRate(rateField, rate))
			return cGameInteractionCommand(aType, eGameInteractionSubscriptionRequest_Subscribe, rate);
		return cGameInteractionCommand(aType, eGameInteractionSubscriptionRequest_Invalid, 0);
	}

	// Keeps the Avatar Identifier on the request whenever its field is valid.
	bool TryReadAvatarIdentifier(cGameInteractionFieldReader& aReader, cGameInteractionAvatarRequest& aRequest)
	{
		std::string identifier;
		if (!aReader.TryReadField(identifier) || !cGameInteractionProtocolVersion2::IsValidAvatarIdentifier(identifier))
			return false;
		aRequest.msIdentifier = identifier;
		return true;
	}

	bool TryReadNumber(cGameInteractionFieldReader& aReader, float& afValue)
	{
		std::string field;
		double value = 0.0;
		if (!aReader.TryReadField(field) || !cGameInteractionProtocolVersion2::TryParseNumber(field, value)) return false;
		afValue = static_cast<float>(value);
		return true;
	}

	bool TryReadUnsignedInteger(cGameInteractionFieldReader& aReader, unsigned long long alMaximum,
		unsigned long long& alValue)
	{
		std::string field;
		return aReader.TryReadField(field) &&
			cGameInteractionProtocolVersion2::TryParseUnsignedInteger(field, alMaximum, alValue);
	}

	bool TryReadFlag(cGameInteractionFieldReader& aReader, bool& abValue)
	{
		std::string field;
		if (!aReader.TryReadField(field) || (field != "0" && field != "1")) return false;
		abValue = field == "1";
		return true;
	}

	const char* FormatFlag(bool abValue)
	{
		return abValue ? "1" : "0";
	}

	// avatarcreate <id> [<entityFile>], avatarremove <id>, avatarcollision <id> <0|1>, and
	// avatarpose <id> <timeMs> <teleportCounter> <x> <y> <z> <yaw> <pitch> <crouch> <lantern> <map>.
	cGameInteractionCommand ParseAvatarCommand(eGameInteractionCommandType aType, const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionAvatarRequest request;
		bool valid = TryReadAvatarIdentifier(reader, request);
		if (valid && aType == eGameInteractionCommand_AvatarCreate && !reader.IsAtEnd())
			valid = reader.TryReadRest(request.msEntityFile);
		else if (valid && aType == eGameInteractionCommand_AvatarCollision)
			valid = TryReadFlag(reader, request.mbCollides);
		else if (valid && aType == eGameInteractionCommand_AvatarEnemySight)
			valid = TryReadFlag(reader, request.mbEnemySight);
		else if (valid && aType == eGameInteractionCommand_AvatarPose)
		{
			cGameInteractionPose& pose = request.mPose;
			unsigned long long teleportCounter = 0;
			valid = TryReadUnsignedInteger(reader, kMaximumTimeMs, pose.mlTimeMs) &&
				TryReadUnsignedInteger(reader, kMaximumTeleportCounter, teleportCounter) &&
				TryReadNumber(reader, pose.mFeetPosition.mfX) && TryReadNumber(reader, pose.mFeetPosition.mfY) &&
				TryReadNumber(reader, pose.mFeetPosition.mfZ) && TryReadNumber(reader, pose.mfBodyYawDegrees) &&
				TryReadNumber(reader, pose.mfCameraPitchDegrees) && TryReadFlag(reader, pose.mbCrouching) &&
				TryReadFlag(reader, pose.mbLanternRaised) &&
				reader.TryReadRest(pose.msMapFile);
			pose.mlTeleportCounter = static_cast<unsigned int>(teleportCounter);
		}
		request.mbValid = valid && reader.IsAtEnd();
		return cGameInteractionCommand(aType, request);
	}

	cGameInteractionCommand ParseEnemyCommand(eGameInteractionCommandType aType, const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string field;
		reader.TryReadField(field);
		cGameInteractionEnemyRequest request;
		cGameInteractionEnemyPose& enemy = request.mEnemy;
		cGameInteractionPose& pose = enemy.mPose;
		bool valid = reader.TryReadField(field) &&
			cGameInteractionProtocolVersion2::TryParseEntityIdentifier(field, enemy.mlEntityId);
		if (valid && aType == eGameInteractionCommand_EnemyPose)
		{
			unsigned long long teleport = 0;
			valid = TryReadUnsignedInteger(reader, kMaximumTimeMs, pose.mlTimeMs) &&
				TryReadUnsignedInteger(reader, kMaximumTeleportCounter, teleport) &&
				TryReadNumber(reader, pose.mFeetPosition.mfX) && TryReadNumber(reader, pose.mFeetPosition.mfY) &&
				TryReadNumber(reader, pose.mFeetPosition.mfZ) && TryReadNumber(reader, pose.mfBodyYawDegrees) &&
				reader.TryReadField(enemy.msClip) && enemy.msClip.size() <= 128 &&
				TryReadNumber(reader, enemy.mfAnimationTime) && enemy.mfAnimationTime >= 0 &&
				enemy.mfAnimationTime <= 1.0e9f && TryReadFlag(reader, enemy.mbLoop);
			pose.mlTeleportCounter = static_cast<unsigned int>(teleport);
			if (enemy.msClip == "-") enemy.msClip.clear();
			valid = valid && std::fabs(pose.mFeetPosition.mfX) <= 1.0e9f &&
				std::fabs(pose.mFeetPosition.mfY) <= 1.0e9f && std::fabs(pose.mFeetPosition.mfZ) <= 1.0e9f &&
				std::fabs(pose.mfBodyYawDegrees) <= 1.0e9f;
		}
		request.mbValid = valid && reader.TryReadRest(pose.msMapFile) &&
			pose.msMapFile.size() <= kMaximumPlacementMapPathLength && reader.IsAtEnd();
		return cGameInteractionCommand(aType, request);
	}

	int HexDigitValue(char acCharacter)
	{
		if (IsDigit(acCharacter)) return acCharacter - '0';
		if (acCharacter >= 'a' && acCharacter <= 'f') return acCharacter - 'a' + 10;
		if (acCharacter >= 'A' && acCharacter <= 'F') return acCharacter - 'A' + 10;
		return -1;
	}

	// `-` keeps the part's default color; anything else must be RRGGBB in hex of either case.
	bool TryReadChatColor(cGameInteractionFieldReader& aReader, cChatColor& aColor)
	{
		std::string field;
		if (!aReader.TryReadField(field)) return false;
		if (field == "-")
		{
			aColor = cChatColor();
			return true;
		}
		if (field.size() != 6) return false;
		unsigned char channels[3];
		for (int channel = 0; channel < 3; ++channel)
		{
			const int high = HexDigitValue(field[channel * 2]);
			const int low = HexDigitValue(field[channel * 2 + 1]);
			if (high < 0 || low < 0) return false;
			channels[channel] = static_cast<unsigned char>(high * 16 + low);
		}
		aColor = cChatColor(channels[0], channels[1], channels[2]);
		return true;
	}

	// Malformed UTF-8 becomes a control character, which the chat model rejects for that part.
	std::wstring DecodeChatText(const std::string& asText)
	{
		std::wstring text;
		if (!cLegacyGameInteractionProtocol::TryDecodeUtf8(asText, text)) text.assign(1, static_cast<wchar_t>(1));
		return text;
	}

	// chatentry <authorColor> <messageColor> <Author>:<message>. The first ':' ends the Chat Author.
	cGameInteractionCommand ParseChatEntry(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionChatRequest request;
		std::string text;
		const bool valid = TryReadChatColor(reader, request.mAuthorColor) &&
			TryReadChatColor(reader, request.mMessageColor) && reader.TryReadRest(text);
		const std::string::size_type delimiter = text.find(':');
		request.mbValid = valid && delimiter != std::string::npos;
		if (request.mbValid)
		{
			request.msAuthor = DecodeChatText(text.substr(0, delimiter));
			request.msMessage = DecodeChatText(text.substr(delimiter + 1));
		}
		return cGameInteractionCommand(eGameInteractionCommand_Chat, request);
	}

	bool TryReadEntityIdentifier(cGameInteractionFieldReader& aReader, int& alIdentifier)
	{
		std::string field;
		return aReader.TryReadField(field) &&
			cGameInteractionProtocolVersion2::TryParseEntityIdentifier(field, alIdentifier);
	}

	bool TryReadBoundedNumber(cGameInteractionFieldReader& aReader, double& afValue)
	{
		std::string field;
		return aReader.TryReadField(field) && cGameInteractionProtocolVersion2::TryParseNumber(field, afValue) &&
			afValue >= -kMaximumBodyStateMagnitude && afValue <= kMaximumBodyStateMagnitude;
	}

	bool TryReadBoundedVector(cGameInteractionFieldReader& aReader, float& afX, float& afY, float& afZ)
	{
		double x = 0.0, y = 0.0, z = 0.0;
		if (!TryReadBoundedNumber(aReader, x) || !TryReadBoundedNumber(aReader, y) || !TryReadBoundedNumber(aReader, z))
			return false;
		afX = static_cast<float>(x);
		afY = static_cast<float>(y);
		afZ = static_cast<float>(z);
		return true;
	}

	bool TryReadEnemyAttackKind(cGameInteractionFieldReader& aReader, eGameInteractionEnemyAttackKind& aKind)
	{
		std::string field;
		if (!aReader.TryReadField(field)) return false;
		if (field == "normal") aKind = eGameInteractionEnemyAttackKind_Normal;
		else if (field == "breakdoor") aKind = eGameInteractionEnemyAttackKind_BreakDoor;
		else return false;
		return true;
	}

	cGameInteractionCommand ParseEnemyAttack(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string field;
		reader.TryReadField(field);
		cGameInteractionEnemyAttackRequest request;
		cGameInteractionEnemyAttack& attack = request.mAttack;
		cGameInteractionVector& size = attack.mBoxSize;
		cGameInteractionVector& offset = attack.mBoxOffset;
		request.mbValid = reader.TryReadField(field) &&
			cGameInteractionProtocolVersion2::TryParseEntityIdentifier(field, attack.mlEntityId) &&
			TryReadEnemyAttackKind(reader, attack.mKind) &&
			TryReadBoundedVector(reader, size.mfX, size.mfY, size.mfZ) &&
			size.mfX > 0 && size.mfY > 0 && size.mfZ > 0 &&
			TryReadBoundedVector(reader, offset.mfX, offset.mfY, offset.mfZ) &&
			reader.TryReadRest(attack.msMapFile) && attack.msMapFile.size() <= kMaximumPlacementMapPathLength &&
			reader.IsAtEnd();
		return cGameInteractionCommand(eGameInteractionCommand_EnemyAttack, request);
	}

	// The orientation is kept as sent; only its length is checked.
	bool TryReadOrientation(cGameInteractionFieldReader& aReader, cGameInteractionQuaternion& aOrientation)
	{
		double x = 0.0, y = 0.0, z = 0.0, w = 0.0;
		if (!TryReadBoundedNumber(aReader, x) || !TryReadBoundedNumber(aReader, y) ||
			!TryReadBoundedNumber(aReader, z) || !TryReadBoundedNumber(aReader, w))
			return false;
		const double length = sqrt(x * x + y * y + z * z + w * w);
		if (length < 1.0 - kOrientationLengthTolerance || length > 1.0 + kOrientationLengthTolerance) return false;
		aOrientation = cGameInteractionQuaternion(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
			static_cast<float>(w));
		return true;
	}

	// <x> <y> <z> <qx> <qy> <qz> <qw> <vx> <vy> <vz> <wx> <wy> <wz>
	bool TryReadBodyState(cGameInteractionFieldReader& aReader, cGameInteractionBodyState& aState)
	{
		cGameInteractionPosition& position = aState.mPosition;
		cGameInteractionVector& linear = aState.mLinearVelocity;
		cGameInteractionVector& angular = aState.mAngularVelocity;
		return TryReadBoundedVector(aReader, position.mfX, position.mfY, position.mfZ) &&
			TryReadOrientation(aReader, aState.mOrientation) &&
			TryReadBoundedVector(aReader, linear.mfX, linear.mfY, linear.mfZ) &&
			TryReadBoundedVector(aReader, angular.mfX, angular.mfY, angular.mfZ);
	}

	// <timeMs> <count> followed by <count> entries of <entityId> <bodyId> <state>.
	bool TryReadBodySamples(cGameInteractionFieldReader& aReader, cGameInteractionBodySamples& aSamples)
	{
		unsigned long long count = 0;
		if (!TryReadUnsignedInteger(aReader, kMaximumTimeMs, aSamples.mlTimeMs) ||
			!TryReadUnsignedInteger(aReader, kMaximumBodyCount, count))
			return false;
		for (unsigned long long index = 0; index < count; ++index)
		{
			cGameInteractionBodySample sample;
			if (!TryReadEntityIdentifier(aReader, sample.mlEntityId) || !TryReadEntityIdentifier(aReader, sample.mlBodyId) ||
				!TryReadBodyState(aReader, sample.mState))
				return false;
			aSamples.mvBodies.push_back(sample);
		}
		return true;
	}

	// entitydrive <entityId> <map>, entitybodies <timeMs> <count> [<entry>...] <map>,
	// entityinteracting <entityId> <0|1> <map>, entitybreak <entityId> <state> <map>, and
	// entityrelease <entityId> <map>.
	cGameInteractionCommand ParseEntityCommand(eGameInteractionCommandType aType, const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionEntityRequest request;
		bool valid = aType == eGameInteractionCommand_EntityBodies ? TryReadBodySamples(reader, request.mBodies) :
			TryReadEntityIdentifier(reader, request.mlEntityId);
		if (valid && aType == eGameInteractionCommand_EntityInteracting)
			valid = TryReadFlag(reader, request.mbInteracting);
		else if (valid && aType == eGameInteractionCommand_EntityBreak)
			valid = TryReadBodyState(reader, request.mState);
		valid = valid && reader.TryReadRest(request.msMapFile);
		request.mBodies.msMapFile = request.msMapFile;
		request.mbValid = valid && reader.IsAtEnd();
		return cGameInteractionCommand(aType, request);
	}

	cGameInteractionCommand ParseEntityState(const std::string& asLine, eGameInteractionCommandType aType)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionEntityStateRequest request;
		request.mState.mKind = aType == eGameInteractionCommand_ButtonState ?
			eGameInteractionEntityStateKind_Button : eGameInteractionEntityStateKind_Lamp;
		request.mbValid = TryReadEntityIdentifier(reader, request.mState.mlEntityId) &&
			TryReadFlag(reader, request.mState.mbState) && reader.TryReadRest(request.mState.msMapFile) && reader.IsAtEnd();
		return cGameInteractionCommand(aType, request);
	}

	// playerplace <x> <y> <z> <yaw> <crouch> <map>
	cGameInteractionCommand ParsePlayerPlace(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionPlacementRequest request;
		cGameInteractionPlacement& placement = request.mPlacement;
		double yaw = 0.0;
		request.mbValid = TryReadBoundedVector(reader, placement.mFeetPosition.mfX, placement.mFeetPosition.mfY,
			placement.mFeetPosition.mfZ) && TryReadBoundedNumber(reader, yaw) &&
			TryReadFlag(reader, placement.mbCrouching) && reader.TryReadRest(placement.msMapFile) &&
			placement.msMapFile.size() <= kMaximumPlacementMapPathLength && reader.IsAtEnd();
		placement.mfBodyYawDegrees = static_cast<float>(yaw);
		return cGameInteractionCommand(eGameInteractionCommand_PlayerPlace, request);
	}

	// shareddeath <episode> <death> <count> <checkPoint> <map>
	cGameInteractionCommand ParseSharedDeath(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionSharedDeathRequest request;
		cSharedDeathEpisode& episode = request.mEpisode;
		unsigned long long episodeNumber = 0, death = 0, count = 0;
		request.mbValid = TryReadUnsignedInteger(reader, kMaximumSharedDeathNumber, episodeNumber) && episodeNumber > 0 &&
			TryReadUnsignedInteger(reader, kMaximumSharedDeathNumber, death) &&
			TryReadUnsignedInteger(reader, kMaximumSharedDeathNumber, count) &&
			reader.TryReadField(episode.msCheckPoint) &&
			cGameInteractionProtocolVersion2::IsValidItemName(episode.msCheckPoint) &&
			reader.TryReadRest(episode.msMapFile) && episode.msMapFile.size() <= kMaximumPlacementMapPathLength &&
			reader.IsAtEnd();
		episode.mlEpisode = static_cast<unsigned int>(episodeNumber);
		episode.mlDeath = static_cast<int>(death);
		episode.mlCount = static_cast<int>(count);
		return cGameInteractionCommand(eGameInteractionCommand_SharedDeath, request);
	}

	bool TryReadSharedCallbackKind(cGameInteractionFieldReader& aReader, eGameInteractionSharedCallbackKind& aKind)
	{
		std::string field;
		if (!aReader.TryReadField(field)) return false;
		for (int index = 0; index < kSharedCallbackKindNameCount; ++index)
		{
			if (field != kSharedCallbackKindNames[index].mpName) continue;
			aKind = kSharedCallbackKindNames[index].mKind;
			return true;
		}
		return false;
	}

	bool TryReadCollideState(cGameInteractionFieldReader& aReader, int& alState)
	{
		std::string field;
		if (!aReader.TryReadField(field) || (field != "1" && field != "-1")) return false;
		alState = field == "1" ? 1 : -1;
		return true;
	}

	bool TryReadItemName(cGameInteractionFieldReader& aReader, std::string& asItem)
	{
		return aReader.TryReadField(asItem) && cGameInteractionProtocolVersion2::IsValidItemName(asItem);
	}

	// sharedcallback playercollide <entityId> <state> <map>, entitycollide <parentId> <entityId> <state> <map>,
	// interact <entityId> <map>, useitem <entityId> <item> <map>, pickup <entityId> <map>,
	// flashback <entityId> <map>, combine <item> <otherItem> <map>, and stickyattach <areaId> <entityId> <bodyId> <map>.
	cGameInteractionCommand ParseSharedCallback(const std::string& asLine)
	{
		cGameInteractionFieldReader reader(asLine);
		std::string keyword;
		reader.TryReadField(keyword);
		cGameInteractionSharedCallbackRequest request;
		cGameInteractionSharedCallback& callback = request.mCallback;
		bool valid = TryReadSharedCallbackKind(reader, callback.mKind);
		if (valid && callback.mKind == eGameInteractionSharedCallbackKind_Combine)
			valid = TryReadItemName(reader, callback.msItem) && TryReadItemName(reader, callback.msOtherItem);
		else
		{
			if (valid && (callback.mKind == eGameInteractionSharedCallbackKind_EntityCollide ||
				callback.mKind == eGameInteractionSharedCallbackKind_StickyAttach))
				valid = TryReadEntityIdentifier(reader, callback.mlParentEntityId);
			valid = valid && TryReadEntityIdentifier(reader, callback.mlEntityId);
		}
		if (valid && (callback.mKind == eGameInteractionSharedCallbackKind_PlayerCollide ||
			callback.mKind == eGameInteractionSharedCallbackKind_EntityCollide))
			valid = TryReadCollideState(reader, callback.mlState);
		else if (valid && callback.mKind == eGameInteractionSharedCallbackKind_UseItem)
			valid = TryReadItemName(reader, callback.msItem);
		else if (valid && callback.mKind == eGameInteractionSharedCallbackKind_StickyAttach)
			valid = TryReadEntityIdentifier(reader, callback.mlBodyId);
		valid = valid && reader.TryReadRest(callback.msMapFile);
		request.mbValid = valid && reader.IsAtEnd();
		return cGameInteractionCommand(eGameInteractionCommand_SharedCallback, request);
	}

	// A writer keeps every number finite and within the reader's bound.
	double BoundedNumber(float afValue)
	{
		const double value = afValue;
		if (value != value || value > DBL_MAX || value < -DBL_MAX) return 0.0;
		if (value > kMaximumBodyStateMagnitude) return kMaximumBodyStateMagnitude;
		if (value < -kMaximumBodyStateMagnitude) return -kMaximumBodyStateMagnitude;
		return value;
	}

	std::string FormatBoundedVector(float afX, float afY, float afZ)
	{
		return cGameInteractionProtocolVersion2::FormatNumber(BoundedNumber(afX)) + " " +
			cGameInteractionProtocolVersion2::FormatNumber(BoundedNumber(afY)) + " " +
			cGameInteractionProtocolVersion2::FormatNumber(BoundedNumber(afZ));
	}

	const char* EnemyAttackKindToken(eGameInteractionEnemyAttackKind aKind)
	{
		return aKind == eGameInteractionEnemyAttackKind_BreakDoor ? "breakdoor" : "normal";
	}

	std::string FormatEnemyAttack(const cGameInteractionEnemyAttack& aAttack)
	{
		const cGameInteractionVector& size = aAttack.mBoxSize;
		const cGameInteractionVector& offset = aAttack.mBoxOffset;
		return cGameInteractionProtocolVersion2::FormatEntityIdentifier(aAttack.mlEntityId) + " " + EnemyAttackKindToken(aAttack.mKind) + " " +
			FormatBoundedVector(size.mfX, size.mfY, size.mfZ) + " " +
			FormatBoundedVector(offset.mfX, offset.mfY, offset.mfZ) + " " + aAttack.msMapFile;
	}

	// Normalized, so a reader never rejects it; one without a direction is written as the identity.
	std::string FormatOrientation(const cGameInteractionQuaternion& aOrientation)
	{
		double components[4] = { aOrientation.mfX, aOrientation.mfY, aOrientation.mfZ, aOrientation.mfW };
		double lengthSquared = 0.0;
		for (int index = 0; index < 4; ++index) lengthSquared += components[index] * components[index];
		const double length = sqrt(lengthSquared);
		const bool hasDirection = length > 0.0 && length <= DBL_MAX;
		std::string text;
		for (int index = 0; index < 4; ++index)
		{
			const double identity = index == 3 ? 1.0 : 0.0;
			text += (index == 0 ? "" : " ") +
				cGameInteractionProtocolVersion2::FormatNumber(hasDirection ? components[index] / length : identity);
		}
		return text;
	}

	std::string FormatEntityEvent(const char* apKeyword, const cGameInteractionEntityEvent& aEvent,
		const std::string& asFields)
	{
		return std::string("EVENT ") + apKeyword + " " +
			cGameInteractionProtocolVersion2::FormatEntityIdentifier(aEvent.mlEntityId) + asFields + " " +
			aEvent.msMapFile;
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

// Printable ASCII without spaces, so an item is one field and every game reads it the same.
bool cGameInteractionProtocolVersion2::IsValidItemName(const std::string& asItem)
{
	if (asItem.empty() || asItem.size() > kMaximumItemNameLength) return false;
	for (std::string::size_type index = 0; index < asItem.size(); ++index)
	{
		const unsigned char character = static_cast<unsigned char>(asItem[index]);
		if (character <= ' ' || character > '~') return false;
	}
	return true;
}

std::string cGameInteractionProtocolVersion2::FormatCheckPointName(const std::string& asName)
{
	return IsValidItemName(asName) ? asName : "-";
}

namespace
{
	std::string FormatCheckPoint(const cGameInteractionCheckPoint& aCheckPoint)
	{
		return cGameInteractionProtocolVersion2::FormatEntityIdentifier(aCheckPoint.mlCount) + (aCheckPoint.mbCallback ? " 1 " : " 0 ") +
			cGameInteractionProtocolVersion2::FormatCheckPointName(aCheckPoint.msName) + " " + aCheckPoint.msMapFile;
	}
}

std::string cGameInteractionProtocolVersion2::SerializeCheckPoint(const cGameInteractionCheckPoint& aCheckPoint)
{
	return "EVENT checkpoint " + FormatCheckPoint(aCheckPoint);
}

std::string cGameInteractionProtocolVersion2::SerializePlayerDeath(int alDeath,
	const cGameInteractionCheckPoint& aCheckPoint)
{
	return "EVENT playerdeath " + FormatEntityIdentifier(alDeath) + " " + FormatCheckPoint(aCheckPoint);
}

std::string cGameInteractionProtocolVersion2::FormatSharedCallback(const cGameInteractionSharedCallback& aCallback)
{
	const char* kind = "interact";
	for (int index = 0; index < kSharedCallbackKindNameCount; ++index)
		if (kSharedCallbackKindNames[index].mKind == aCallback.mKind) kind = kSharedCallbackKindNames[index].mpName;
	std::string fields = std::string(kind) + " ";
	if (aCallback.mKind == eGameInteractionSharedCallbackKind_Combine)
		return fields + aCallback.msItem + " " + aCallback.msOtherItem + " " + aCallback.msMapFile;
	if (aCallback.mKind == eGameInteractionSharedCallbackKind_EntityCollide ||
		aCallback.mKind == eGameInteractionSharedCallbackKind_StickyAttach)
		fields += FormatEntityIdentifier(aCallback.mlParentEntityId) + " ";
	fields += FormatEntityIdentifier(aCallback.mlEntityId);
	if (aCallback.mKind == eGameInteractionSharedCallbackKind_PlayerCollide ||
		aCallback.mKind == eGameInteractionSharedCallbackKind_EntityCollide)
		fields += aCallback.mlState > 0 ? " 1" : " -1";
	else if (aCallback.mKind == eGameInteractionSharedCallbackKind_UseItem)
		fields += " " + aCallback.msItem;
	else if (aCallback.mKind == eGameInteractionSharedCallbackKind_StickyAttach)
		fields += " " + FormatEntityIdentifier(aCallback.mlBodyId);
	return fields + " " + aCallback.msMapFile;
}

std::string cGameInteractionProtocolVersion2::FormatEntityIdentifier(int alIdentifier)
{
	char formatted[16];
	sprintf(formatted, "%d", alIdentifier);
	return formatted;
}

bool cGameInteractionProtocolVersion2::TryParseEntityIdentifier(const std::string& asText, int& alIdentifier)
{
	const bool negative = !asText.empty() && asText[0] == '-';
	unsigned long long magnitude = 0;
	if (!TryParseUnsignedInteger(asText.substr(negative ? 1 : 0),
		negative ? kLargestNegatedEntityIdentifier : kLargestEntityIdentifier, magnitude))
		return false;
	alIdentifier = static_cast<int>(negative ? -static_cast<long long>(magnitude) : static_cast<long long>(magnitude));
	return true;
}

std::string cGameInteractionProtocolVersion2::FormatBodyState(const cGameInteractionBodyState& aState)
{
	return FormatBoundedVector(aState.mPosition.mfX, aState.mPosition.mfY, aState.mPosition.mfZ) + " " +
		FormatOrientation(aState.mOrientation) + " " +
		FormatBoundedVector(aState.mLinearVelocity.mfX, aState.mLinearVelocity.mfY, aState.mLinearVelocity.mfZ) + " " +
		FormatBoundedVector(aState.mAngularVelocity.mfX, aState.mAngularVelocity.mfY, aState.mAngularVelocity.mfZ);
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
	if (type == eGameInteractionCommand_EnemyDrive || type == eGameInteractionCommand_EnemyPose ||
		type == eGameInteractionCommand_EnemyRelease) return ParseEnemyCommand(type, asLine);
	if (type == eGameInteractionCommand_EnemyAttack) return ParseEnemyAttack(asLine);
	if (type == eGameInteractionCommand_SharedDeath) return ParseSharedDeath(asLine);
	if (type == eGameInteractionCommand_Chat) return ParseChatEntry(asLine);
	if (type == eGameInteractionCommand_SharedCallback) return ParseSharedCallback(asLine);
	if (type == eGameInteractionCommand_PlayerPlace) return ParsePlayerPlace(asLine);
	if (type == eGameInteractionCommand_ButtonState || type == eGameInteractionCommand_LampState)
		return ParseEntityState(asLine, type);
	if (type == eGameInteractionCommand_LocalPose || type == eGameInteractionCommand_ReportedBodies ||
		type == eGameInteractionCommand_EnemyPoses)
		return ParseSubscription(type, asLine);
	if (type == eGameInteractionCommand_EntityDrive || type == eGameInteractionCommand_EntityBodies ||
		type == eGameInteractionCommand_EntityInteracting || type == eGameInteractionCommand_EntityBreak ||
		type == eGameInteractionCommand_EntityRelease)
		return ParseEntityCommand(type, asLine);
	if (type == eGameInteractionCommand_AvatarCreate || type == eGameInteractionCommand_AvatarRemove ||
		type == eGameInteractionCommand_AvatarCollision || type == eGameInteractionCommand_AvatarEnemySight || type == eGameInteractionCommand_AvatarPose)
		return ParseAvatarCommand(type, asLine);
	std::string fields;
	reader.TryReadRest(fields);
	return cGameInteractionCommand(type, fields);
}

std::string cGameInteractionProtocolVersion2::SerializeResponse(const cGameInteractionResponse& aResponse)
{
	const char* keyword = KeywordFor(aResponse.GetCommandType());
	if (!keyword) return cLegacyGameInteractionProtocol::SerializeResponse(aResponse);

	std::string response = std::string("RESPONSE ") + keyword + " " + OutcomeToken(aResponse.GetOutcome());
	if (aResponse.GetType() == eGameInteractionResponse_Avatar)
		return aResponse.GetAvatarIdentifier().empty() ? response : response + " " + aResponse.GetAvatarIdentifier();
	if (aResponse.GetType() == eGameInteractionResponse_Entity)
		return aResponse.NamesEntity() ? response + " " + FormatEntityIdentifier(aResponse.GetEntityId()) : response;
	if (aResponse.GetOutcome() != eGameInteractionCommandOutcome_Success) return response;
	if (aResponse.GetType() == eGameInteractionResponse_ProtocolNegotiated)
	{
		response += " 2";
		for (int index = 0; index < kCapabilityNameCount; ++index)
			if (aResponse.GetCapabilities() & kCapabilityNames[index].mCapability)
				response += std::string(" ") + kCapabilityNames[index].mpName;
	}
	else if (aResponse.GetType() == eGameInteractionResponse_Subscription)
	{
		if (aResponse.GetSubscriptionRequest() == eGameInteractionSubscriptionRequest_Unsubscribe)
			return response + " unsubscribe";
		char rate[16];
		sprintf(rate, " subscribe %u", aResponse.GetSubscriptionRate());
		response += rate;
	}
	return response;
}

// The report never exceeds the entry limit, but a writer keeps to it regardless.
std::string cGameInteractionProtocolVersion2::SerializeReportedBodies(const cGameInteractionBodySamples& aBodies)
{
	const size_t count = aBodies.mvBodies.size() < kMaximumBodyCount ? aBodies.mvBodies.size() :
		static_cast<size_t>(kMaximumBodyCount);
	char clockFields[64];
	sprintf(clockFields, "STATE reportedbodies %llu %u", aBodies.mlTimeMs, static_cast<unsigned int>(count));
	std::string stateUpdate = clockFields;
	for (size_t index = 0; index < count; ++index)
	{
		const cGameInteractionBodySample& sample = aBodies.mvBodies[index];
		stateUpdate += " " + FormatEntityIdentifier(sample.mlEntityId) + " " + FormatEntityIdentifier(sample.mlBodyId) +
			" " + FormatBodyState(sample.mState);
	}
	return stateUpdate + " " + aBodies.msMapFile;
}

std::string cGameInteractionProtocolVersion2::SerializeEvent(const cGameInteractionEvent& aEvent)
{
	const cGameInteractionEntityEvent& entityEvent = aEvent.GetEntityEvent();
	const std::string body = " " + FormatEntityIdentifier(entityEvent.mlBodyId);
	switch (aEvent.GetType())
	{
	case eGameInteractionEvent_InteractionStarted:
		return FormatEntityEvent("interactionstart", entityEvent, body);
	case eGameInteractionEvent_InteractionEnded:
		return FormatEntityEvent("interactionend", entityEvent, body + " " + EndingToken(entityEvent.mEnding));
	case eGameInteractionEvent_ReportContact:
		return FormatEntityEvent("reportcontact", entityEvent, std::string());
	case eGameInteractionEvent_ReportSettled:
		return FormatEntityEvent("reportsettled", entityEvent, std::string());
	case eGameInteractionEvent_EntityReleased:
		return FormatEntityEvent("entityreleased", entityEvent, std::string());
	case eGameInteractionEvent_ReportBroke:
		return FormatEntityEvent("reportbroke", entityEvent, " " + FormatBodyState(entityEvent.mState));
	case eGameInteractionEvent_MapEntered:
		return "EVENT mapentered " + aEvent.GetData();
	case eGameInteractionEvent_ButtonState:
	case eGameInteractionEvent_LampState:
	{
		const cGameInteractionEntityState& state = aEvent.GetEntityState();
		return std::string("EVENT ") + (aEvent.GetType() == eGameInteractionEvent_ButtonState ? "buttonstate " : "lampstate ") +
			FormatEntityIdentifier(state.mlEntityId) + (state.mbState ? " 1 " : " 0 ") + state.msMapFile;
	}
	case eGameInteractionEvent_SharedCallback:
		return "EVENT sharedcallback " + FormatSharedCallback(aEvent.GetSharedCallback());
	case eGameInteractionEvent_EnemyAttack:
		return "EVENT enemyattack " + FormatEnemyAttack(aEvent.GetEnemyAttack());
	default:
		return std::string();
	}
}

std::string cGameInteractionProtocolVersion2::SerializeLocalPose(const cGameInteractionPose& aPose)
{
	char clockFields[64];
	sprintf(clockFields, "STATE localpose %llu %u ", aPose.mlTimeMs, aPose.mlTeleportCounter);
	return clockFields + FormatNumber(aPose.mFeetPosition.mfX) + " " + FormatNumber(aPose.mFeetPosition.mfY) +
		" " + FormatNumber(aPose.mFeetPosition.mfZ) + " " + FormatNumber(aPose.mfBodyYawDegrees) + " " +
		FormatNumber(aPose.mfCameraPitchDegrees) + " " + FormatFlag(aPose.mbCrouching) + " " +
		FormatFlag(aPose.mbLanternRaised) + " " + aPose.msMapFile;
}

std::string cGameInteractionProtocolVersion2::SerializeEnemyPose(const cGameInteractionEnemyPose& aEnemy)
{
	const cGameInteractionPose& pose = aEnemy.mPose;
	char clock[64];
	sprintf(clock, " %llu %u ", pose.mlTimeMs, pose.mlTeleportCounter);
	return "STATE enemypose " + FormatEntityIdentifier(aEnemy.mlEntityId) + clock +
		FormatNumber(pose.mFeetPosition.mfX) + " " + FormatNumber(pose.mFeetPosition.mfY) + " " +
		FormatNumber(pose.mFeetPosition.mfZ) + " " + FormatNumber(pose.mfBodyYawDegrees) + " " +
		(aEnemy.msClip.empty() ? "-" : aEnemy.msClip) + " " + FormatNumber(aEnemy.mfAnimationTime) + " " +
		(aEnemy.mbLoop ? "1" : "0") + " " + pose.msMapFile;
}
