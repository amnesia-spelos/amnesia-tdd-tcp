#ifndef PEER_SETTING_MODEL_H
#define PEER_SETTING_MODEL_H

#include <string>
#include <vector>

// What a settingannounce Command announces. Every field is UTF-8.
struct cPeerSettingAnnouncement
{
	std::string msKey;
	std::string msLabel;
	std::string msDefault;
	// Empty when the Peer gave none.
	std::string msDescription;
};

// A Peer Setting as the options tab shows it, with its effective value: the stored value if the game has
// one, otherwise the announced default.
struct cPeerSetting
{
	cPeerSetting() : mlSession(0) {}
	unsigned int mlSession;
	std::string msKey;
	std::string msLabel;
	std::string msDefault;
	std::string msDescription;
	std::string msValue;
};

// A value the player confirmed for a Setting Key.
struct cPeerSettingEdit
{
	cPeerSettingEdit(const std::string& asKey = std::string(), const std::string& asValue = std::string())
		: msKey(asKey), msValue(asValue) {}
	std::string msKey;
	std::string msValue;
};

// A settingchanged Event the owning Session must be sent.
struct cPeerSettingChange
{
	cPeerSettingChange() : mlSession(0) {}
	unsigned int mlSession;
	std::string msKey;
	std::string msValue;
};

// Where the game remembers the values the player changed, by Setting Key.
class iPeerSettingStore
{
public:
	virtual ~iPeerSettingStore() {}
	virtual bool TryLoadValue(const std::string& asKey, std::string& asValue) = 0;
	virtual void StoreValue(const std::string& asKey, const std::string& asValue) = 0;
};

enum ePeerSettingOutcome
{
	ePeerSettingOutcome_Success,
	// Another live Session owns the Setting Key.
	ePeerSettingOutcome_Duplicate,
	// The announce would exceed a cap.
	ePeerSettingOutcome_Unavailable,
	// The Session has not announced the Setting Key.
	ePeerSettingOutcome_NotFound
};

// The Peer Settings of every live Session, identified by a Session number the caller chooses.
class cPeerSettingModel
{
public:
	static const size_t kMaximumSettingsPerSession = 16;
	static const size_t kMaximumSettings = 32;

	// Without a store, nothing is loaded or remembered.
	explicit cPeerSettingModel(iPeerSettingStore* apStore = NULL);
	void SetStore(iPeerSettingStore* apStore) { mpStore = apStore; }

	// Announcing a Setting Key the Session already owns updates its label, default, and description.
	// On success the value is the effective value, which the Session counts as told.
	ePeerSettingOutcome Announce(unsigned int alSession, const cPeerSettingAnnouncement& aAnnouncement,
		std::string& asValue);
	ePeerSettingOutcome ReadValue(unsigned int alSession, const std::string& asKey, std::string& asValue);
	// Withdraws the Session's Peer Settings. Their stored values stay.
	void EndSession(unsigned int alSession);

	// In announcement order.
	const std::vector<cPeerSetting>& GetSettings() const { return mvSettings; }
	// Changes whenever the list or a value in it changes.
	unsigned int GetRevision() const { return mlRevision; }

	// The player confirmed these values. A value that differs from its setting's effective value is
	// stored; edits of withdrawn settings and invalid values are ignored. Returns, for each owned key whose
	// effective value differs from the one its Session was last told, the settingchanged to send.
	std::vector<cPeerSettingChange> Confirm(const std::vector<cPeerSettingEdit>& avEdits);

	// [a-z0-9_.-], 1-64 characters.
	static bool IsValidKey(const std::string& asKey);
	// Valid UTF-8 without tabs or newlines: a label of 1-48 Unicode scalars, a description or value of at
	// most 256.
	static bool IsValidLabel(const std::string& asLabel);
	static bool IsValidDescription(const std::string& asDescription);
	static bool IsValidValue(const std::string& asValue);

private:
	struct cEntry
	{
		cEntry() : mbStored(false) {}
		cPeerSetting mSetting;
		bool mbStored;
		std::string msStoredValue;
		std::string msLastTold;
	};

	cEntry* Find(const std::string& asKey);
	static void UpdateEffectiveValue(cEntry& aEntry);
	void Publish();

	iPeerSettingStore* mpStore;
	std::vector<cEntry> mvEntries;
	std::vector<cPeerSetting> mvSettings;
	unsigned int mlRevision;
};

#endif
