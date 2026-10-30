#include "PeerSettingModel.h"

cPeerSettingModel::cPeerSettingModel(iPeerSettingStore* apStore) : mpStore(apStore), mlRevision(0)
{
}

ePeerSettingOutcome cPeerSettingModel::Announce(unsigned int alSession,
	const cPeerSettingAnnouncement& aAnnouncement, std::string& asValue)
{
	cEntry* existing = Find(aAnnouncement.msKey);
	if (existing && existing->mSetting.mlSession != alSession) return ePeerSettingOutcome_Duplicate;
	if (!existing)
	{
		size_t sessionCount = 0;
		for (size_t index = 0; index < mvEntries.size(); ++index)
			if (mvEntries[index].mSetting.mlSession == alSession) ++sessionCount;
		if (sessionCount >= kMaximumSettingsPerSession || mvEntries.size() >= kMaximumSettings)
			return ePeerSettingOutcome_Unavailable;
		cEntry entry;
		entry.mSetting.mlSession = alSession;
		entry.mSetting.msKey = aAnnouncement.msKey;
		entry.mbStored = mpStore && mpStore->TryLoadValue(aAnnouncement.msKey, entry.msStoredValue);
		mvEntries.push_back(entry);
		existing = &mvEntries.back();
	}
	existing->mSetting.msLabel = aAnnouncement.msLabel;
	existing->mSetting.msDefault = aAnnouncement.msDefault;
	existing->mSetting.msDescription = aAnnouncement.msDescription;
	UpdateEffectiveValue(*existing);
	existing->msLastTold = existing->mSetting.msValue;
	asValue = existing->msLastTold;
	Publish();
	return ePeerSettingOutcome_Success;
}

ePeerSettingOutcome cPeerSettingModel::ReadValue(unsigned int alSession, const std::string& asKey,
	std::string& asValue)
{
	cEntry* entry = Find(asKey);
	if (!entry || entry->mSetting.mlSession != alSession) return ePeerSettingOutcome_NotFound;
	entry->msLastTold = entry->mSetting.msValue;
	asValue = entry->msLastTold;
	return ePeerSettingOutcome_Success;
}

void cPeerSettingModel::EndSession(unsigned int alSession)
{
	const size_t count = mvEntries.size();
	for (size_t index = mvEntries.size(); index > 0; --index)
		if (mvEntries[index - 1].mSetting.mlSession == alSession) mvEntries.erase(mvEntries.begin() + (index - 1));
	if (mvEntries.size() != count) Publish();
}

std::vector<cPeerSettingChange> cPeerSettingModel::Confirm(const std::vector<cPeerSettingEdit>& avEdits)
{
	bool stored = false;
	for (size_t index = 0; index < avEdits.size(); ++index)
	{
		cEntry* entry = Find(avEdits[index].msKey);
		if (!entry || entry->mSetting.msValue == avEdits[index].msValue || !IsValidValue(avEdits[index].msValue))
			continue;
		entry->mbStored = true;
		entry->msStoredValue = avEdits[index].msValue;
		if (mpStore) mpStore->StoreValue(entry->mSetting.msKey, entry->msStoredValue);
		UpdateEffectiveValue(*entry);
		stored = true;
	}

	std::vector<cPeerSettingChange> changes;
	for (size_t index = 0; index < mvEntries.size(); ++index)
	{
		cEntry& entry = mvEntries[index];
		if (entry.mSetting.msValue == entry.msLastTold) continue;
		entry.msLastTold = entry.mSetting.msValue;
		cPeerSettingChange change;
		change.mlSession = entry.mSetting.mlSession;
		change.msKey = entry.mSetting.msKey;
		change.msValue = entry.msLastTold;
		changes.push_back(change);
	}
	if (stored || !changes.empty()) Publish();
	return changes;
}

namespace
{
	const std::string::size_type kMaximumKeyLength = 64;
	const size_t kMaximumLabelLength = 48;
	const size_t kMaximumTextLength = 256;

	// Counts the Unicode scalars of valid UTF-8 text without tabs or newlines; false for any other text.
	bool TryCountScalars(const std::string& asText, size_t& alCount)
	{
		alCount = 0;
		for (std::string::size_type index = 0; index < asText.size(); ++alCount)
		{
			const unsigned char lead = static_cast<unsigned char>(asText[index]);
			if (lead == '\t' || lead == '\n' || lead == '\r') return false;
			int length = 1;
			unsigned int scalar = lead;
			if (lead >= 0xF0 && lead <= 0xF4) { length = 4; scalar = lead & 0x07; }
			else if (lead >= 0xE0) { length = 3; scalar = lead & 0x0F; }
			else if (lead >= 0xC2 && lead < 0xE0) { length = 2; scalar = lead & 0x1F; }
			else if (lead >= 0x80) return false;
			if (asText.size() - index < static_cast<std::string::size_type>(length)) return false;
			for (int offset = 1; offset < length; ++offset)
			{
				const unsigned char continuation = static_cast<unsigned char>(asText[index + offset]);
				if ((continuation & 0xC0) != 0x80) return false;
				scalar = (scalar << 6) | (continuation & 0x3F);
			}
			// Overlong forms, surrogates, and values beyond Unicode are malformed.
			if ((length == 3 && (scalar < 0x800 || (scalar >= 0xD800 && scalar <= 0xDFFF))) ||
				(length == 4 && (scalar < 0x10000 || scalar > 0x10FFFF)))
				return false;
			index += length;
		}
		return true;
	}

	bool IsValidText(const std::string& asText, size_t alMinimum, size_t alMaximum)
	{
		size_t count = 0;
		return TryCountScalars(asText, count) && count >= alMinimum && count <= alMaximum;
	}
}

bool cPeerSettingModel::IsValidKey(const std::string& asKey)
{
	if (asKey.empty() || asKey.size() > kMaximumKeyLength) return false;
	for (std::string::size_type index = 0; index < asKey.size(); ++index)
	{
		const char character = asKey[index];
		if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
			character == '_' || character == '.' || character == '-'))
			return false;
	}
	return true;
}

bool cPeerSettingModel::IsValidLabel(const std::string& asLabel)
{
	return IsValidText(asLabel, 1, kMaximumLabelLength);
}

bool cPeerSettingModel::IsValidDescription(const std::string& asDescription)
{
	return IsValidText(asDescription, 0, kMaximumTextLength);
}

bool cPeerSettingModel::IsValidValue(const std::string& asValue)
{
	return IsValidText(asValue, 0, kMaximumTextLength);
}

cPeerSettingModel::cEntry* cPeerSettingModel::Find(const std::string& asKey)
{
	for (size_t index = 0; index < mvEntries.size(); ++index)
		if (mvEntries[index].mSetting.msKey == asKey) return &mvEntries[index];
	return NULL;
}

void cPeerSettingModel::UpdateEffectiveValue(cEntry& aEntry)
{
	aEntry.mSetting.msValue = aEntry.mbStored ? aEntry.msStoredValue : aEntry.mSetting.msDefault;
}

void cPeerSettingModel::Publish()
{
	mvSettings.clear();
	for (size_t index = 0; index < mvEntries.size(); ++index) mvSettings.push_back(mvEntries[index].mSetting);
	++mlRevision;
}
