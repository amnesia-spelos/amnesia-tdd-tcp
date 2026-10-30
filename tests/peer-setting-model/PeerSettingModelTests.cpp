#include "PeerSettingModel.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	class cFakeStore : public iPeerSettingStore
	{
	public:
		cFakeStore() : mlWrites(0) {}
		virtual bool TryLoadValue(const std::string& asKey, std::string& asValue)
		{
			std::map<std::string, std::string>::const_iterator value = mValues.find(asKey);
			if (value == mValues.end()) return false;
			asValue = value->second;
			return true;
		}
		virtual void StoreValue(const std::string& asKey, const std::string& asValue)
		{
			mValues[asKey] = asValue;
			++mlWrites;
		}
		std::map<std::string, std::string> mValues;
		int mlWrites;
	};

	cPeerSettingAnnouncement Announcement(const std::string& asKey, const std::string& asDefault,
		const std::string& asLabel = "Label", const std::string& asDescription = std::string())
	{
		cPeerSettingAnnouncement announcement;
		announcement.msKey = asKey;
		announcement.msLabel = asLabel;
		announcement.msDefault = asDefault;
		announcement.msDescription = asDescription;
		return announcement;
	}

	void AnnouncingGivesTheStoredValueOrElseTheDefault()
	{
		cFakeStore store;
		store.mValues["multimnesia.color"] = "FF0000";
		cPeerSettingModel model(&store);
		std::string value;
		Expect(model.Announce(1, Announcement("multimnesia.username", "Player"), value) == ePeerSettingOutcome_Success &&
			value == "Player", "a key without a stored value starts at its default");
		Expect(model.Announce(1, Announcement("multimnesia.color", "FFFFFF"), value) == ePeerSettingOutcome_Success &&
			value == "FF0000", "a stored value wins over the announced default");
		Expect(model.GetSettings().size() == 2 && model.GetSettings()[0].msKey == "multimnesia.username" &&
			model.GetSettings()[0].msValue == "Player" && model.GetSettings()[1].msValue == "FF0000",
			"the settings are listed in announcement order with their effective values");
		Expect(store.mlWrites == 0, "announcing stores nothing");
	}

	void ReannouncingUpdatesAndAnotherSessionsKeyIsADuplicate()
	{
		cFakeStore store;
		cPeerSettingModel model(&store);
		std::string value;
		model.Announce(1, Announcement("a.first", "one", "First"), value);
		model.Announce(1, Announcement("a.second", "two"), value);
		const unsigned int revision = model.GetRevision();
		Expect(model.Announce(1, Announcement("a.first", "uno", "Primero", "Tip"), value) == ePeerSettingOutcome_Success &&
			value == "uno", "re-announcing an owned key succeeds with the new default as its effective value");
		Expect(model.GetSettings().size() == 2 && model.GetSettings()[0].msKey == "a.first" &&
			model.GetSettings()[0].msLabel == "Primero" && model.GetSettings()[0].msDescription == "Tip" &&
			model.GetSettings()[0].msDefault == "uno", "re-announcing updates in place, keeping announcement order");
		Expect(model.GetRevision() != revision, "a re-announce changes the list's revision");

		value = "unchanged";
		Expect(model.Announce(2, Announcement("a.first", "other"), value) == ePeerSettingOutcome_Duplicate &&
			value == "unchanged", "a key another live Session owns is a duplicate");
		Expect(model.GetSettings().size() == 2 && model.GetSettings()[0].mlSession == 1 &&
			model.GetSettings()[0].msDefault == "uno", "a duplicate announce changes nothing");
	}

	std::string Numbered(const std::string& asPrefix, int alNumber)
	{
		return asPrefix + static_cast<char>('a' + alNumber / 26) + static_cast<char>('a' + alNumber % 26);
	}

	void AnnouncesAreCappedPerSessionAndInTotal()
	{
		cPeerSettingModel model;
		std::string value;
		for (int index = 0; index < 16; ++index)
			Expect(model.Announce(1, Announcement(Numbered("one.", index), "x"), value) == ePeerSettingOutcome_Success,
				"a Session can announce 16 Peer Settings");
		Expect(model.Announce(1, Announcement("one.extra", "x"), value) == ePeerSettingOutcome_Unavailable,
			"a 17th Peer Setting of one Session is unavailable");
		Expect(model.Announce(1, Announcement(Numbered("one.", 3), "y"), value) == ePeerSettingOutcome_Success,
			"re-announcing an owned key at the cap still succeeds");
		for (int index = 0; index < 16; ++index)
			model.Announce(2, Announcement(Numbered("two.", index), "x"), value);
		Expect(model.GetSettings().size() == 32, "two Sessions reach 32 Peer Settings");
		Expect(model.Announce(3, Announcement("three.first", "x"), value) == ePeerSettingOutcome_Unavailable,
			"a 33rd Peer Setting across live Sessions is unavailable");
		Expect(model.GetSettings().size() == 32, "an unavailable announce adds nothing");
	}

	void OnlyTheOwningSessionReadsAValue()
	{
		cFakeStore store;
		store.mValues["a.stored"] = "kept";
		cPeerSettingModel model(&store);
		std::string value;
		model.Announce(1, Announcement("a.stored", "default"), value);
		Expect(model.ReadValue(1, "a.stored", value) == ePeerSettingOutcome_Success && value == "kept",
			"the owning Session reads the effective value");
		Expect(model.ReadValue(2, "a.stored", value) == ePeerSettingOutcome_NotFound,
			"a Peer cannot read another Peer's settings");
		Expect(model.ReadValue(1, "a.missing", value) == ePeerSettingOutcome_NotFound,
			"a key the Session never announced is not found");
	}

	void EndingASessionWithdrawsItsSettingsButKeepsStoredValues()
	{
		cFakeStore store;
		cPeerSettingModel model(&store);
		std::string value;
		model.Announce(1, Announcement("a.name", "Player"), value);
		model.Announce(2, Announcement("b.name", "Other"), value);
		std::vector<cPeerSettingEdit> edits;
		edits.push_back(cPeerSettingEdit("a.name", "Spelos"));
		model.Confirm(edits);
		const unsigned int revision = model.GetRevision();
		model.EndSession(1);
		Expect(model.GetSettings().size() == 1 && model.GetSettings()[0].msKey == "b.name",
			"a Session's settings are withdrawn when it ends");
		Expect(model.GetRevision() != revision, "a withdrawal changes the list's revision");
		Expect(model.ReadValue(1, "a.name", value) == ePeerSettingOutcome_NotFound, "a withdrawn key is not found");
		Expect(model.Announce(3, Announcement("a.name", "Fresh"), value) == ePeerSettingOutcome_Success &&
			value == "Spelos", "another Session can announce the key and gets the stored value");
		Expect(store.mValues["a.name"] == "Spelos", "the stored value survives the Session");
	}

	void ConfirmStoresOnlyChangedValuesAndKeepsThemStored()
	{
		cFakeStore store;
		cPeerSettingModel model(&store);
		std::string value;
		model.Announce(1, Announcement("a.name", "Player"), value);
		model.Announce(1, Announcement("a.color", "FFFFFF"), value);
		std::vector<cPeerSettingEdit> edits;
		edits.push_back(cPeerSettingEdit("a.name", "Player"));
		edits.push_back(cPeerSettingEdit("a.color", "00FF00"));
		model.Confirm(edits);
		Expect(store.mValues.size() == 1 && store.mValues["a.color"] == "00FF00",
			"only a changed value is stored, so an untouched setting keeps following its default");
		Expect(model.Announce(1, Announcement("a.name", "Guest"), value) == ePeerSettingOutcome_Success &&
			value == "Guest", "an untouched setting follows a new default");

		edits.clear();
		edits.push_back(cPeerSettingEdit("a.color", "FFFFFF"));
		model.Confirm(edits);
		Expect(store.mValues["a.color"] == "FFFFFF" && store.mlWrites == 2,
			"setting a stored value back to the default text stores it again");
		Expect(model.Announce(1, Announcement("a.color", "000000"), value) == ePeerSettingOutcome_Success &&
			value == "FFFFFF", "a value set back to the default text stays stored and wins over a new default");

		edits.clear();
		edits.push_back(cPeerSettingEdit("a.withdrawn", "x"));
		edits.push_back(cPeerSettingEdit("a.name", "tab\there"));
		model.Confirm(edits);
		Expect(store.mlWrites == 2, "edits of unknown keys and invalid values are ignored");
	}

	void ConfirmTellsEachOwningSessionOnlyWhatItWasNotLastTold()
	{
		cFakeStore store;
		cPeerSettingModel model(&store);
		std::string value;
		model.Announce(1, Announcement("a.name", "Player"), value);
		model.Announce(1, Announcement("a.color", "FFFFFF"), value);
		model.Announce(2, Announcement("b.name", "Other"), value);
		std::vector<cPeerSettingEdit> edits;
		edits.push_back(cPeerSettingEdit("a.name", "Spelos"));
		edits.push_back(cPeerSettingEdit("a.color", "FFFFFF"));
		edits.push_back(cPeerSettingEdit("b.name", "Daniel"));
		std::vector<cPeerSettingChange> changes = model.Confirm(edits);
		Expect(changes.size() == 2 && changes[0].mlSession == 1 && changes[0].msKey == "a.name" &&
			changes[0].msValue == "Spelos" && changes[1].mlSession == 2 && changes[1].msKey == "b.name" &&
			changes[1].msValue == "Daniel", "each changed key is reported to its owning Session");
		Expect(model.GetSettings()[0].msValue == "Spelos", "the list shows the confirmed value");

		changes = model.Confirm(edits);
		Expect(changes.empty(), "confirming the same values again tells nobody");

		edits.clear();
		edits.push_back(cPeerSettingEdit("a.name", "Third"));
		model.Confirm(edits);
		edits[0].msValue = "Spelos";
		changes = model.Confirm(edits);
		Expect(changes.size() == 1 && changes[0].msValue == "Spelos",
			"a value is reported against the one last given, by the earlier settingchanged");
	}

	std::string Repeated(const std::string& asText, size_t aCount)
	{
		std::string result;
		for (size_t index = 0; index < aCount; ++index) result += asText;
		return result;
	}

	void FieldsKeepToTheirLimits()
	{
		Expect(cPeerSettingModel::IsValidKey("multimnesia.user_name-2"), "a key of [a-z0-9_.-] is valid");
		Expect(cPeerSettingModel::IsValidKey(Repeated("k", 64)) && !cPeerSettingModel::IsValidKey(Repeated("k", 65)),
			"a key is at most 64 characters");
		Expect(!cPeerSettingModel::IsValidKey("") && !cPeerSettingModel::IsValidKey("Upper") &&
			!cPeerSettingModel::IsValidKey("a key") && !cPeerSettingModel::IsValidKey("a:key"),
			"an empty key or one with other characters is invalid");

		const std::string scalar = "\xF0\x9F\x98\x80";
		Expect(cPeerSettingModel::IsValidLabel(Repeated(scalar, 48)) && !cPeerSettingModel::IsValidLabel(Repeated(scalar, 49)),
			"a label is at most 48 Unicode scalars");
		Expect(!cPeerSettingModel::IsValidLabel(""), "a label is not empty");
		Expect(cPeerSettingModel::IsValidValue("") && cPeerSettingModel::IsValidDescription(""),
			"a value and a description may be empty");
		Expect(cPeerSettingModel::IsValidValue(Repeated("\xC3\xA9", 256)) && !cPeerSettingModel::IsValidValue(Repeated("v", 257)),
			"a value is at most 256 Unicode scalars");
		Expect(cPeerSettingModel::IsValidDescription(Repeated("d", 256)) &&
			!cPeerSettingModel::IsValidDescription(Repeated("d", 257)), "a description is at most 256 Unicode scalars");
		Expect(!cPeerSettingModel::IsValidValue("a\tb") && !cPeerSettingModel::IsValidLabel("a\nb") &&
			!cPeerSettingModel::IsValidDescription("a\rb"), "no field holds a tab or newline");
		Expect(cPeerSettingModel::IsValidValue("spaces and: colons"), "spaces and colons are ordinary text");
		Expect(!cPeerSettingModel::IsValidValue("\xC3\x28") && !cPeerSettingModel::IsValidValue("\xC0\xAF") &&
			!cPeerSettingModel::IsValidValue("\xED\xA0\x80") && !cPeerSettingModel::IsValidValue("\xE2\x82"),
			"malformed, overlong, surrogate, and truncated UTF-8 is invalid");
	}
}

int main()
{
	AnnouncingGivesTheStoredValueOrElseTheDefault();
	ReannouncingUpdatesAndAnotherSessionsKeyIsADuplicate();
	AnnouncesAreCappedPerSessionAndInTotal();
	OnlyTheOwningSessionReadsAValue();
	EndingASessionWithdrawsItsSettingsButKeepsStoredValues();
	ConfirmStoresOnlyChangedValuesAndKeepsThemStored();
	ConfirmTellsEachOwningSessionOnlyWhatItWasNotLastTold();
	FieldsKeepToTheirLimits();
	return 0;
}
