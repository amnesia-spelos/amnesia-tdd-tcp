#include "EnemyPerceptionModel.h"
#include <cstdlib>
#include <iostream>
void Expect(bool value, const char* message) { if (!value) { std::cerr << message << "\n"; exit(1); } }
int main()
{
	Expect(cEnemyPerceptionModel::SightRange(20, 5, 0.5f, false, false, 0.1f, 0, 0.35f, false) == 5,
		"still player in darkness has darkness range");
	Expect(cEnemyPerceptionModel::SightRange(20, 5, 0.5f, true, false, 0.1f, 0, 0.35f, false) == 2.5f,
		"crouch reduces dark range");
	Expect(cEnemyPerceptionModel::SightRange(20, 5, 0.5f, true, true, 0.1f, 0, 0.35f, false) == 30,
		"raised lantern overrides darkness and crouch without accumulating range");
	cEnemyPerceptionModel model;
	std::vector<cEnemyPerceptionCandidate> players;
	players.push_back(cEnemyPerceptionCandidate("avatar", true, 10));
	model.Update(players); model.Update(players);
	Expect(!model.IsVisible(), "sight needs three consecutive checks");
	model.Update(players);
	Expect(model.IsVisible() && model.GetTarget() == "avatar", "enemy acquires Avatar");
	players.push_back(cEnemyPerceptionCandidate("", true, 2));
	model.Update(players); model.Update(players); model.Update(players);
	Expect(model.GetTarget() == "avatar", "closer visible player does not steal target");
	players[0].mbPerceived = false;
	model.Update(players);
	Expect(model.GetTarget().empty() && model.IsVisible(), "lost target switches to best perceived player");
	players.clear(); model.Update(players);
	Expect(!model.IsVisible(), "departure removes perception");
	cEnemyPerceptionModel existingLocal(true);
	players.push_back(cEnemyPerceptionCandidate("", true, 10));
	players.push_back(cEnemyPerceptionCandidate("nearer", true, 1));
	existingLocal.Update(players); existingLocal.Update(players); existingLocal.Update(players);
	Expect(existingLocal.GetTarget().empty(), "opt-in preserves an already visible local target");
	players[0].mbPerceived = false; existingLocal.Update(players);
	Expect(existingLocal.GetTarget() == "nearer", "local target can be lost in favor of an Avatar");
	cEnemyPerceptionModel water;
	players.clear(); players.push_back(cEnemyPerceptionCandidate("water-avatar", true, 20, 1));
	water.Update(players);
	Expect(water.IsVisible(), "Kaernk height detection is immediate");
	cEnemyPerceptionModel helped;
	helped.RememberTarget("reported-avatar");
	Expect(!helped.IsVisible() && helped.GetTarget() == "reported-avatar", "help remembers an Avatar without inventing sight");
	std::vector<cEnemyPerceptionCandidate> others;
	Expect(cEnemyPerceptionModel::ShownPlayer(false, others).empty(), "a local trigger shows the enemy the local player");
	Expect(cEnemyPerceptionModel::ShownPlayer(true, others).empty(),
		"another player's trigger with no Avatar on the map still shows the local player");
	others.push_back(cEnemyPerceptionCandidate("far", false, 30));
	others.push_back(cEnemyPerceptionCandidate("near", false, 4));
	Expect(cEnemyPerceptionModel::ShownPlayer(true, others) == "near",
		"another player's trigger shows the enemy the nearest Avatar, seen or not");
	Expect(cEnemyPerceptionModel::ShownPlayer(false, others).empty(), "Avatars never take a local trigger");
	std::vector<float> avatarDistancesSqr;
	Expect(cEnemyPerceptionModel::HallucinationEnds(2, 3.9f, avatarDistancesSqr),
		"the local player inside the end distance ends a hallucination alone");
	Expect(!cEnemyPerceptionModel::HallucinationEnds(2, 4, avatarDistancesSqr),
		"exactly the end distance does not end it, as vanilla");
	avatarDistancesSqr.push_back(400);
	avatarDistancesSqr.push_back(1);
	Expect(cEnemyPerceptionModel::HallucinationEnds(2, 10000, avatarDistancesSqr),
		"an Avatar inside the end distance ends it while the local player is far");
	avatarDistancesSqr.pop_back();
	Expect(!cEnemyPerceptionModel::HallucinationEnds(2, 10000, avatarDistancesSqr),
		"nobody inside the end distance keeps it");
	Expect(cEnemyPerceptionModel::HallucinationEnds(2, 1, avatarDistancesSqr),
		"a far Avatar does not keep it from the local player");
	std::cout << "Enemy perception cases passed\n";
}
