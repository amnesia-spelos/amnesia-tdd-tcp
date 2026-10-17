#ifndef SHARED_DEATH_MODEL_H
#define SHARED_DEATH_MODEL_H
#include <string>

// How a death treats the map's enemies at its checkpoint.
enum eSharedDeathPolicy
{
	// Single-player behavior: every enemy the game simulates is deactivated and reset.
	eSharedDeathPolicy_Vanilla,
	// No death callback while another player shares the map: Shared Enemies survive the death.
	eSharedDeathPolicy_PreserveEnemies,
	// A death callback while another player shares the map: every player dies in one Shared Death, and
	// driven copies are reset with the enemies they copy.
	eSharedDeathPolicy_Coordinated
};

// One Shared Death as the Session Host's Game Peer decided it. The death is the game's own death number
// this episode answers, or 0 when it asks the game to kill its player.
struct cSharedDeathEpisode
{
	cSharedDeathEpisode() : mlEpisode(0), mlDeath(0), mlCount(0) {}
	unsigned int mlEpisode;
	int mlDeath;
	// The pre-increment checkpoint count every game passes to its own death callback.
	int mlCount;
	std::string msCheckPoint;
	std::string msMapFile;
};

enum eSharedDeathOutcome
{
	// The player is alive, so the game kills it; that death belongs to the episode.
	eSharedDeathOutcome_Kill,
	// The episode attached to the death in progress.
	eSharedDeathOutcome_Joined,
	// The episode, or a newer one, was already applied.
	eSharedDeathOutcome_Duplicate,
	// The episode answers a death that already reached its checkpoint, or never happened.
	eSharedDeathOutcome_Stale
};

// What a death's checkpoint does: the count the callback receives and how enemies are reset.
struct cSharedDeathCheckPoint
{
	cSharedDeathCheckPoint() : mlCount(0), mReset(eSharedDeathPolicy_Vanilla), mbAgreed(false) {}
	int mlCount;
	eSharedDeathPolicy mReset;
	// Whether the count is the episode's rather than the game's own.
	bool mbAgreed;
};

// The local player's deaths within one Session, and the Shared Death episodes that answer them.
class cSharedDeathModel
{
public:
	cSharedDeathModel() : mlDeaths(0), mlDeath(0), mlLastEpisode(0), mbDying(false), mPolicy(eSharedDeathPolicy_Vanilla), mbHasEpisode(false), mbKilling(false),
		mbWaiting(false), mfWaitStartMs(0) {}

	// The other player is on this map exactly when the death is shared at all.
	static eSharedDeathPolicy SelectPolicy(bool abOtherPlayerOnMap, bool abCallback)
	{
		if (!abOtherPlayerOnMap) return eSharedDeathPolicy_Vanilla;
		return abCallback ? eSharedDeathPolicy_Coordinated : eSharedDeathPolicy_PreserveEnemies;
	}

	// The local player started dying. Returns the death's number to report, or 0 when an episode killed
	// the player, so the death is not reported back.
	int BeginDeath(eSharedDeathPolicy aPolicy)
	{
		mbDying = true;
		mPolicy = aPolicy;
		mbWaiting = false;
		if (mbKilling)
		{
			mbKilling = false;
			mbHasEpisode = true;
			mlDeath = 0;
			return 0;
		}
		mbHasEpisode = false;
		mlDeath = ++mlDeaths;
		return mlDeath;
	}

	// Episodes are numbered in the order the Session Host's Game Peer decided them, so an episode no newer
	// than one already applied is a delayed or repeated delivery. An episode answering a death never
	// kills; one asking for a death kills only a living player.
	eSharedDeathOutcome ApplyEpisode(const cSharedDeathEpisode& aEpisode)
	{
		if (aEpisode.mlEpisode <= mlLastEpisode) return eSharedDeathOutcome_Duplicate;
		mlLastEpisode = aEpisode.mlEpisode;
		const bool answersCurrentDeath = aEpisode.mlDeath == 0 || aEpisode.mlDeath == mlDeath;
		if (mbDying && !mbHasEpisode && answersCurrentDeath)
		{
			mEpisode = aEpisode;
			mbHasEpisode = true;
			return eSharedDeathOutcome_Joined;
		}
		if (mbDying || mbKilling || aEpisode.mlDeath != 0) return eSharedDeathOutcome_Stale;
		mEpisode = aEpisode;
		mbKilling = true;
		return eSharedDeathOutcome_Kill;
	}

	// The game could not kill its player for the episode, so no death of its own belongs to it.
	void AbandonKill() { mbKilling = false; }

	// Whether the death screen stays up at the checkpoint: a coordinated death waits for its episode, so
	// its callback can receive the agreed count, but never longer than kCheckPointWaitMs from the first ask.
	bool HoldsCheckPoint(double afTimeMs)
	{
		if (!mbDying || mbHasEpisode || mPolicy != eSharedDeathPolicy_Coordinated) return false;
		if (!mbWaiting)
		{
			mbWaiting = true;
			mfWaitStartMs = afTimeMs;
		}
		return afTimeMs - mfWaitStartMs < kCheckPointWaitMs;
	}

	// Whether a death in progress must leave Shared Enemies alone, including their reactions to it.
	bool PreservesEnemies() const
	{
		return mbDying && !mbHasEpisode && mPolicy == eSharedDeathPolicy_PreserveEnemies;
	}

	// A Map Visit ended, taking any death in progress and pending kill with it.
	void EndMapVisit()
	{
		mbDying = false;
		mbKilling = false;
	}

	// The death reached its checkpoint, whose current registration this is. A death in an episode resets
	// coordinated and takes the agreed count when this game's registration is the episode's.
	cSharedDeathCheckPoint CompleteCheckPoint(const std::string& asCheckPoint, bool abCallback, int alCount)
	{
		cSharedDeathCheckPoint result;
		result.mlCount = alCount;
		result.mReset = mbDying ? mPolicy : eSharedDeathPolicy_Vanilla;
		if (mbDying && mbHasEpisode)
		{
			result.mReset = eSharedDeathPolicy_Coordinated;
			result.mbAgreed = abCallback && asCheckPoint == mEpisode.msCheckPoint;
			if (result.mbAgreed) result.mlCount = mEpisode.mlCount;
		}
		mbDying = false;
		return result;
	}

private:
	int mlDeaths;
	// The death in progress, or 0 when an episode killed the player.
	int mlDeath;
	unsigned int mlLastEpisode;
	bool mbDying;
	eSharedDeathPolicy mPolicy;
	bool mbHasEpisode;
	// An episode is killing the living player, whose death it then owns.
	bool mbKilling;
	cSharedDeathEpisode mEpisode;
	bool mbWaiting;
	double mfWaitStartMs;
	static const int kCheckPointWaitMs = 3000;
};
#endif
