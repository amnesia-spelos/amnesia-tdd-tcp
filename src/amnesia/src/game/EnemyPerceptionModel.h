#ifndef ENEMY_PERCEPTION_MODEL_H
#define ENEMY_PERCEPTION_MODEL_H
#include <map>
#include <string>
#include <vector>

// Empty identifier denotes the local player; every Avatar keeps its Session identifier.
struct cEnemyPerceptionCandidate
{
	cEnemyPerceptionCandidate(const std::string& id, bool perceived, float distance, int checks = 3)
		: msIdentifier(id), mbPerceived(perceived), mfDistance(distance), mlRequiredChecks(checks) {}
	std::string msIdentifier;
	bool mbPerceived;
	float mfDistance;
	int mlRequiredChecks;
};

// One enemy's consecutive sight checks and sticky target. World queries stay in the game adapter.
class cEnemyPerceptionModel
{
public:
	static float SightRange(float sight, float darkness, float crouchMultiplier, bool crouch,
		bool lantern, float light, float speed, float darknessThreshold, bool skipHandicaps)
	{
		float range = lantern ? sight * 1.5f : sight;
		if (!lantern && !skipHandicaps)
		{
			if (light < darknessThreshold && speed < 0.05f) range = darkness;
			if (crouch) range *= crouchMultiplier;
		}
		return range;
	}
	cEnemyPerceptionModel(bool localVisible = false) : mbVisible(localVisible)
	{ if (localVisible) mCounts[""] = 3; }
	void Update(const std::vector<cEnemyPerceptionCandidate>& players)
	{
		std::map<std::string, int> counts;
		const cEnemyPerceptionCandidate* best = NULL;
		const cEnemyPerceptionCandidate* current = NULL;
		for (size_t i = 0; i < players.size(); ++i)
		{
			const cEnemyPerceptionCandidate& player = players[i];
			int count = player.mbPerceived ? mCounts[player.msIdentifier] + 1 : 0;
			if (count > player.mlRequiredChecks) count = player.mlRequiredChecks;
			counts[player.msIdentifier] = count;
			if (count < player.mlRequiredChecks) continue;
			if (!best || player.mfDistance < best->mfDistance) best = &player;
			if (mbVisible && player.msIdentifier == msTarget) current = &player;
		}
		mCounts.swap(counts);
		if (current) best = current;
		mbVisible = best != NULL;
		if (best) msTarget = best->msIdentifier;
	}
	// A nearby enemy may report a target without granting sight of it.
	void RememberTarget(const std::string& identifier) { msTarget = identifier; mbVisible = false; mCounts.clear(); }
	bool IsVisible() const { return mbVisible; }
	const std::string& GetTarget() const { return msTarget; }
private:
	std::map<std::string, int> mCounts;
	std::string msTarget;
	bool mbVisible;
};
#endif
