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
	std::cout << "Enemy perception cases passed\n";
}
