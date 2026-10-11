#ifndef SHARED_CALLBACK_MODEL_H
#define SHARED_CALLBACK_MODEL_H

// A collide callback the game ran, or is asked to run, as the local game has its two sides.
struct cSharedCollide
{
	cSharedCollide() : mbParentIsPlayer(false), mbParentHeld(false), mbChildHeld(false) {}
	// The parent is the local player rather than an entity.
	bool mbParentIsPlayer;
	// Whether the local player Holds the parent or the child, so that the other games follow it.
	bool mbParentHeld;
	bool mbChildHeld;
};

// Decides which Map Script callbacks are Shared Callbacks, and keeps Shared Callback mode: while a
// Peer's Command runs a callback, nothing it runs is reported back and the player's control and camera
// stay the local player's (amnesia-multiplayer's ADR 0006, not this repository's).
class cSharedCallbackModel
{
public:
	cSharedCallbackModel() : mlRunDepth(0) {}

	// Brackets a run from a Peer's Command. Runs nest, as a callback may run another one.
	void BeginRun() { ++mlRunDepth; }
	void EndRun() { if (mlRunDepth > 0) --mlRunDepth; }

	// Whether an interact or use-item callback the game just ran is reported.
	bool ReportsCallback() const { return mlRunDepth == 0; }
	// A collide callback is shared when the local player's body is its parent, or when the local player
	// Holds either side, because the other games then follow that entity rather than collide it.
	bool ReportsCollide(const cSharedCollide& aCollide) const;
	// Whether a script may take the player's control or camera.
	bool AllowsPlayerControl() const { return mlRunDepth == 0; }

	// A collide run by a Peer's Command, for a registration with the given states filter (0 for both,
	// 1 or -1 for one). Returns whether its callback runs. An entity collide moves the registration's
	// edge to the state as the local check would, so the local check does not fire it again, and does not
	// run when the edge is already there. A Player collide's edge is the local player's body, which
	// another player's trigger does not move.
	static bool RunCollide(const cSharedCollide& aCollide, int alStatesFilter, int alState, bool& abColliding);

private:
	int mlRunDepth;
};

#endif // SHARED_CALLBACK_MODEL_H
