#include "PeerSettingForm.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	cPeerSetting Setting(unsigned int alSession, const std::string& asKey, const std::string& asValue,
		const std::string& asLabel = "Label", const std::string& asDescription = std::string())
	{
		cPeerSetting setting;
		setting.mlSession = alSession;
		setting.msKey = asKey;
		setting.msLabel = asLabel;
		setting.msDefault = asValue;
		setting.msDescription = asDescription;
		setting.msValue = asValue;
		return setting;
	}

	void ResettingShowsEveryEffectiveValue()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player", "Username", "Your name"));
		settings.push_back(Setting(1, "multimnesia.color", "FF0000", "Color"));
		cPeerSettingForm form;
		form.Reset(settings);
		const std::vector<cPeerSettingFormRow>& rows = form.GetRows();
		Expect(rows.size() == 2 && rows[0].msKey == "multimnesia.username" && rows[0].msLabel == "Username" &&
			rows[0].msDescription == "Your name" && rows[0].msText == "Player" && rows[1].msText == "FF0000",
			"a reset shows a row per setting in announcement order with its effective value");
		Expect(rows[0].mlPreviousIndex == -1 && rows[1].mlPreviousIndex == -1, "every row of a reset is new");
		Expect(form.GetEdits().empty(), "untouched rows are no edits");
	}

	void TypedRowsAreTheEdits()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player"));
		settings.push_back(Setting(1, "multimnesia.color", "FF0000"));
		settings.push_back(Setting(1, "multimnesia.team", "red"));
		cPeerSettingForm form;
		form.Reset(settings);
		form.SetText(1, "00FF00");
		form.SetText(2, "blue");
		form.SetText(2, "red");
		const std::vector<cPeerSettingEdit> edits = form.GetEdits();
		Expect(edits.size() == 1 && edits[0].msKey == "multimnesia.color" && edits[0].msValue == "00FF00",
			"only a row whose text differs from its value is an edit");
		Expect(form.GetRows()[1].msText == "00FF00", "a row holds what the player typed");

		form.Reset(settings);
		Expect(form.GetEdits().empty() && form.GetRows()[1].msText == "FF0000", "a reset discards edits");
	}

	void AnAnnounceAddsARowAndKeepsTheOthers()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player"));
		cPeerSettingForm form;
		form.Reset(settings);
		form.SetText(0, "Daniel");
		settings.push_back(Setting(1, "multimnesia.color", "FF0000", "Color", "Your color"));
		form.Sync(settings);
		const std::vector<cPeerSettingFormRow>& rows = form.GetRows();
		Expect(rows.size() == 2 && rows[0].mlPreviousIndex == 0 && rows[0].msText == "Daniel",
			"a row still announced keeps its place and what the player typed");
		Expect(rows[1].mlPreviousIndex == -1 && rows[1].msKey == "multimnesia.color" && rows[1].msLabel == "Color" &&
			rows[1].msDescription == "Your color" && rows[1].msText == "FF0000",
			"an announced setting adds a new row showing its effective value");
	}

	void AReAnnounceUpdatesTheRowButKeepsTheTypedText()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player", "Name", "Old"));
		cPeerSettingForm form;
		form.Reset(settings);
		form.SetText(0, "Daniel");
		settings[0] = Setting(1, "multimnesia.username", "Somebody", "Username", "New");
		form.Sync(settings);
		const cPeerSettingFormRow& row = form.GetRows()[0];
		Expect(row.msLabel == "Username" && row.msDescription == "New", "a re-announce updates the label and description");
		Expect(row.msText == "Daniel", "a re-announce keeps what the player typed");
		const std::vector<cPeerSettingEdit> edits = form.GetEdits();
		Expect(edits.size() == 1 && edits[0].msValue == "Daniel", "the typed text stays an edit");
	}

	void AnUntouchedRowFollowsANewDefault()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player"));
		cPeerSettingForm form;
		form.Reset(settings);
		settings[0] = Setting(1, "multimnesia.username", "Somebody");
		form.Sync(settings);
		Expect(form.GetRows()[0].msText == "Somebody", "an untouched row shows the new effective value");
		Expect(form.GetEdits().empty(), "following a new default is no edit");
	}

	void AnEndedSessionsRowsAreDroppedWithTheirEdits()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "multimnesia.username", "Player"));
		settings.push_back(Setting(1, "multimnesia.color", "FF0000"));
		cPeerSettingForm form;
		form.Reset(settings);
		form.SetText(0, "Daniel");
		form.SetText(1, "00FF00");
		settings.clear();
		form.Sync(settings);
		Expect(form.GetRows().empty() && form.GetEdits().empty(), "withdrawn rows disappear with their edits");

		settings.push_back(Setting(2, "multimnesia.color", "FF0000"));
		form.Sync(settings);
		Expect(form.GetRows().size() == 1 && form.GetRows()[0].mlPreviousIndex == -1 &&
			form.GetRows()[0].msText == "FF0000" && form.GetEdits().empty(),
			"a later Session's setting is a new row, without the ended Session's edit");
	}

	void AWithdrawnRowLeavesTheOthersInPlace()
	{
		std::vector<cPeerSetting> settings;
		settings.push_back(Setting(1, "first.a", "a"));
		settings.push_back(Setting(2, "second.b", "b"));
		cPeerSettingForm form;
		form.Reset(settings);
		form.SetText(1, "typed");
		settings.erase(settings.begin());
		form.Sync(settings);
		Expect(form.GetRows().size() == 1 && form.GetRows()[0].mlPreviousIndex == 1 &&
			form.GetRows()[0].msText == "typed", "a remaining row says where it was and keeps its text");
	}
}

int main()
{
	ResettingShowsEveryEffectiveValue();
	TypedRowsAreTheEdits();
	AnAnnounceAddsARowAndKeepsTheOthers();
	AReAnnounceUpdatesTheRowButKeepsTheTypedText();
	AnUntouchedRowFollowsANewDefault();
	AnEndedSessionsRowsAreDroppedWithTheirEdits();
	AWithdrawnRowLeavesTheOthersInPlace();
	std::cout << "Peer Setting form tests passed.\n";
	return 0;
}
