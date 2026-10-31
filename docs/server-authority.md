# Server authority and cheat resistance

Hexaverse is built so that a modified client gains almost nothing. The server is **fully authoritative**: it owns the only real copy of the game and resolves every round itself. It also keeps track of what each faction **can see right now and has seen in the past**, and it only sends a client what that client's faction is allowed to know. Information that never reaches a client can't be revealed by a map hack, a memory reader or a patched renderer.

This page describes how the code does that, with references to the source.

## The server owns the game

- **One real world.** The server holds the actual terrain (`World::actual`), every entity, every faction's resources and research state. Clients hold a replica that is built only from what the server sends them (see [architecture.md](architecture.md)).
- **Clients send requests, not results.** A client can only ask for things: queue a task, cancel one, enable a structure, research a topic, end the round. The server works out what happens. Attacks, movement, terraforming, damage, production and research are all resolved on the server in the structure, attack and movement phases (`server/components/evolver.*`). Clients only animate the results they're sent.
- **Every command is checked.** Commands are handled in `server/components/networkChannels.cpp`. Before any change, `CheckCommandModeGetAccessibleEntity()` makes sure that:
  - the game is in the command phase (`CheckCommandMode()`),
  - the entity exists (ghosts, markers and remembered contacts can't be commanded),
  - the requesting client's faction **owns** it and is still alive (`Network::CanControl()` in `server/components/state/networkState.cpp`),
  - the entity is placed in the world.

  Each handler then checks the specific command: whether the unit can perform the operation at all, the task queue limit (`Objects::MaxTasks`), the terrain layer bounds for digging and building, and so on. A rejected command is logged as `commandRejected` and changes nothing.
- **AI players get no special access.** Lua AIs run in a separate Console Client that the server starts with an access token for its faction (`server/components/aiProcess.cpp`). It connects over the normal protocol, so it receives the same filtered view as a human player and its commands pass the same checks.

## The server tracks what each faction can see

Each faction (`clientServerShared/objects/faction.h`) carries its own view of the world, kept on the server:

| Member | What it tracks |
|---|---|
| `knownWorld` (`World::Data`) | The faction's own copy of the terrain. Each cell knows whether it is **currently covered by the faction's sensors** (`IsCurrentlyVisible()`). Cells that aren't covered keep the content the faction **last saw** there. |
| `seeing` | For every foreign entity the faction has spotted: whether it is visible **now**, plus a timeline of when it appeared and disappeared during the current round (`SeeingReference::reverseVisibilityChange`). |
| `knownResources` | Which natural-resource deposits the faction has found with surveyors. |

The sensors that feed this are the faction's own structures and units, plus its allies' if the alliance shares sensors (`Faction::CollectSensors()`, `Faction::CheckSensory()`). Units below ground, on the surface and in the air are checked differently: ground units against the visible cells of `knownWorld` (`Faction::GridCanSee()`), and aircraft against sensor spheres (`CountOffGridCoverage()`).

Whenever something moves, is built or sensors change, the server calls `HandlePotentialAppearance()` and `HandlePotentialDisappearance()`. A change in visibility is recorded with the exact time inside the round (`SignalAppearance()` / `SignalDisappearance()` in `faction.cpp`). That timing matters: during the movement phase a unit can drive into view and out again, and the server knows exactly which part of the motion each faction may see (`Faction::CanSee(entity, time)`).

## Only allowed information is sent

All outgoing game updates are filtered per recipient (`server/server.cpp`, `server/components/state/gameState.cpp`, `server/components/state/worldState.cpp`):

- **Entities appear and vanish.** A foreign unit is sent to a client only when it comes into sensor range (`SignalAppearanceToHostileFaction`). When it leaves, the client is told it vanished (`SignalDisappearanceToHostileFaction`), and no further updates follow.
- **State, motion and turret updates** go only to clients that can see the entity at that moment: `Network::CanSee(client, entity, time)` is checked before every entity state, `mobileEntityMotion`, `entityTurretRotation` and removal message.
- **Foreign units are described with less detail.** `Game::Network::GetExposition()` picks what goes into each description. Your own units are described in full. Hostile units get only what can be observed, such as type, position, health and EMP damage: no task queue, no production queue, no sleep or automation state, no stored resources (`Expose` in `gameState.h`). Allies get a middle level, or full detail if the alliance enables *See Unit Details*.
- **Terrain is sent as each faction knows it.** Terrain changes are compared against each faction's `knownWorld` and sent only where that faction's sensors cover the cells (`Data::ProgressiveUpdate()`, `CheckAllFactionCoverageAndSendAsNeeded()`). Terrain dug out or built up out of sight stays the way the faction last saw it until a sensor looks again. At the start of a game, unexplored terrain is sent obscured according to the `World/worldDiscovery` setting (`None`, `Water`, `Liquids` or `FullTerrain`).
- **Explosions** are sent only to factions that can see part of the blast volume (`SignalExplosion()`).
- **Hidden attackers stay hidden.** If you're hit by a unit you can't see, you get a *vague* trajectory: the turret type and a source position moved randomly by up to 20% of the turret's range. The exact attack (`preciseAttackTrajectory`) goes only to clients that can see the attacker.
- **Resource deposits** are revealed only through the faction's own surveyors (`UpdateResourceCoverage()`).

Ghosts, the last known positions of units that went out of sight, are made by the client from what it was told before. The server doesn't keep sending positions of units you can no longer see.

## What a cheating client could still do

Because the server decides everything and sends only what may be seen, a modified client can't:

- see units, structures, terrain changes or resources outside its faction's sensor coverage,
- read the orders, production or stores of enemy units,
- move, attack or build faster or farther than the rules allow, or control units it doesn't own.

What remains is what any client can do legitimately: show the information it was given in a different way, or automate commands (which the Lua AI API already allows).

## Exceptions set by the server host

Some information is shared on purpose, set by the host or by the players:

- **Spectators** see everything. Spectator connections are accepted only from `localhost` (`networkChannels.cpp`).
- **Alliances** can share sensors and unit details with their members (alliance settings *Share Sensors* and *See Unit Details*).
- `Rules/doomsdayWeapons` is `Expose` by default: everyone is told about doomsday explosions and their craters, even out of sight. `Conceal` treats them like ordinary weapons.
- `Rules/exposeActionLeftToHostileFactions` is `true` by default: hostile factions see how much attack and movement initiative a visible unit has left. Set it to `false` to keep that private.
- `World/worldDiscovery = FullTerrain` reveals the initial terrain of the whole planet (but still not later changes made out of sight).
