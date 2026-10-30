#ifndef PEER_SETTING_FORM_H
#define PEER_SETTING_FORM_H

#include "PeerSettingModel.h"

#include <string>
#include <vector>

// A Peer Setting's row in the options tab. Every field is UTF-8.
struct cPeerSettingFormRow
{
	cPeerSettingFormRow() : mlSession(0), mlPreviousIndex(-1) {}
	unsigned int mlSession;
	std::string msKey;
	std::string msLabel;
	std::string msDescription;
	// The effective value the row last showed.
	std::string msValue;
	// What the text box holds.
	std::string msText;
	// The row's index before the last Reset or Sync, or -1 when it is new.
	int mlPreviousIndex;
};

// The Peer Settings rows of an open options window, kept in step with the live Sessions' settings.
class cPeerSettingForm
{
public:
	// Every row shows its effective value, discarding any edit.
	void Reset(const std::vector<cPeerSetting>& avSettings);
	// Follows the settings as they change while the window is open. A row a Session still announces keeps
	// what the player typed, and an untouched row shows its new effective value. A withdrawn row is dropped
	// with its edit.
	void Sync(const std::vector<cPeerSetting>& avSettings);
	// The player typed in a row.
	void SetText(size_t alIndex, const std::string& asText);

	// In announcement order.
	const std::vector<cPeerSettingFormRow>& GetRows() const { return mvRows; }
	// The rows the player changed, as the values to confirm with OK.
	std::vector<cPeerSettingEdit> GetEdits() const;

private:
	std::vector<cPeerSettingFormRow> mvRows;
};

#endif
