#include "PeerSettingForm.h"

namespace
{
	cPeerSettingFormRow NewRow(const cPeerSetting& aSetting)
	{
		cPeerSettingFormRow row;
		row.mlSession = aSetting.mlSession;
		row.msKey = aSetting.msKey;
		row.msLabel = aSetting.msLabel;
		row.msDescription = aSetting.msDescription;
		row.msValue = aSetting.msValue;
		row.msText = aSetting.msValue;
		return row;
	}
}

void cPeerSettingForm::Reset(const std::vector<cPeerSetting>& avSettings)
{
	mvRows.clear();
	for (size_t index = 0; index < avSettings.size(); ++index) mvRows.push_back(NewRow(avSettings[index]));
}

void cPeerSettingForm::Sync(const std::vector<cPeerSetting>& avSettings)
{
	std::vector<cPeerSettingFormRow> rows;
	for (size_t index = 0; index < avSettings.size(); ++index)
	{
		const cPeerSetting& setting = avSettings[index];
		cPeerSettingFormRow row = NewRow(setting);
		for (size_t previous = 0; previous < mvRows.size(); ++previous)
		{
			const cPeerSettingFormRow& old = mvRows[previous];
			if (old.msKey != setting.msKey || old.mlSession != setting.mlSession) continue;
			row.mlPreviousIndex = static_cast<int>(previous);
			if (old.msText != old.msValue) row.msText = old.msText;
			break;
		}
		rows.push_back(row);
	}
	mvRows.swap(rows);
}

void cPeerSettingForm::SetText(size_t alIndex, const std::string& asText)
{
	if (alIndex < mvRows.size()) mvRows[alIndex].msText = asText;
}

std::vector<cPeerSettingEdit> cPeerSettingForm::GetEdits() const
{
	std::vector<cPeerSettingEdit> edits;
	for (size_t index = 0; index < mvRows.size(); ++index)
		if (mvRows[index].msText != mvRows[index].msValue)
			edits.push_back(cPeerSettingEdit(mvRows[index].msKey, mvRows[index].msText));
	return edits;
}
