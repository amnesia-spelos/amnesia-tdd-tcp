---
status: accepted
---

# Workshop map paths use a `workshop/` prefix

Custom Stories subscribed through Steam Workshop live outside the game's install folder, in `steamapps/workshop/content/57300/<item number>/`, so a Map Path relative to the install folder cannot name their maps. A map inside a Workshop item is instead named relative to the Workshop folder, under a fixed `workshop/` prefix, for example `workshop/3040083107/maps/sphw_00_Proxy.map`. Without the Steam API, the game finds that folder from a `Directories` entry in the init config when set, and otherwise derives it from the install folder, since Steam keeps Workshop content in the same library as the game.

## Considered Options

- A path relative to the install folder through `..` (`../../workshop/content/57300/...`) would keep the old definition literally. It was rejected because it depends on Steam's library layout and breaks when the Workshop folder is configured elsewhere, for example in a development build.
- Naming every Custom Story map as if it were under `custom_stories/<identifier>/` was rejected. It hides where the map lives and gains nothing, because a local copy of a Workshop story has a different Custom Story Identifier anyway.

## Consequences

- Peers compare Map Paths as opaque strings, so a Map Path is stable once Peers store or exchange it. Changing the prefix later would break Peers that remember Map Paths.
- No folder named `workshop` exists in the install folder, so a prefixed Map Path cannot collide with an installed one.
