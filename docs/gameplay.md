# Gameplay concepts

This is a compact overview of the rules, aimed at people working on the code. The full player wiki, with every unit, its stats and the controls, is in [`documentation/documentation/`](../documentation/documentation/home.txt). That wiki is also rendered in-game. Links below point into it.

## The pitch

> *eXplore, eXpand, eXploit, eXterminate, eXcavate.*
> Hexaverse is a turn-based, 4X multiplayer strategy game. All matches take place on volumetric planets. Use your terraformers to build walls and bridges, excavate tunnels, or submerge your bases beneath mountains, only to see them nuked from orbit.

## Rounds and phases

The game runs in rounds, and every player acts **at the same time**. Each round passes through the phases in `RuntimeConfig::Phase` (`clientServerShared/runtimeConfig.h`):

| Phase | What happens |
|---|---|
| `CommandPhase` | Players (and AI clients) queue and edit unit commands. The phase ends when every connected player has pressed *End round* or the round timer runs out. The timer is paused in round 1 and while nobody is connected. |
| `StructurePhase` | Resources are distributed across compounds, and production and research advance. |
| `AttackPhase` | Attack commands run in order of unit **initiative**. Offensive units can queue safe attacks automatically when targets are in range. |
| `MovementPhase` | Movement and operations (terraforming, building, repairing) run, also by initiative. Towers can fire at hostiles moving through their range. |
| `AnimationPhase` | Clients animate everything that happened (at most ~10 s, sped up if needed). Then the next command phase starts. |
| `EndOfGame` | The game is over (for example, the Nexus was conquered). |

Attacks come before movement: *you can't run if you're dead.* Every unit has an **action pool** (100% attack, 100% movement). Each operation uses part of it, and an operation that would take it below zero is postponed to the next round. See [round](../documentation/documentation/concepts/round.txt), [initiative](../documentation/documentation/concepts/initiative.txt) and [action pool](../documentation/documentation/concepts/action%20pool.txt).

## Terrain

- A planet is a **volumetric hex grid**: hexagonal cells cover the sphere (a few cells are pentagons, as on any sphere tiling), stacked into **32 layers**.
- There are two grids. A high-resolution **terrain grid** for what you see, and a coarser **unit grid** for where units can stand. A unit needs its central and edge terrain cells to be clear, and can stand on a full block or a ledge one layer below.
- Cell contents (`Grid::CellStack::Content`) are Granite, Earth, Grass, Trees, Forest, Crystals, CrystalForest, Rock, Sand, Snow, Foundation, Road, Lava, Technite, Water, Clear and Unknown.
- Terrain changes through terraforming, weapon impacts and liquids: water and lava spread into connected empty space when `World/simulateLiquids` is on.
- Planet types are defined by `.ini` templates (`predefinedPlanets/`). The template sets the noise layers (mountains, extended core, inner core), oceans and vegetation. A random seed creates the actual layout.
- **EMP zones** can leave lasting EMP fields on the map.

Details: [terrain](../documentation/documentation/terrain.txt).

## Resources and compounds

- **Natural resources** (fuel, minerals, uranium) never run out. **Mines** extract whatever lies beneath them every round, as long as nothing lower blocks the path to the core. Undermining an enemy base cuts off its supply.
- **Energy** is produced by generators (from fuel) and reactor cores (from uranium). Most structures need it. **Munitions** are made by munition factories.
- Resource coverage is unknown until a **surveyor** scans it.
- Structures form **compounds** that balance supply and demand automatically. In current versions all of a faction's structures form one implicit compound. Enabling a structure may automatically enable the generators or mines it needs.
- The **advanced resources** work (merged in 2021) added a data-driven resource table, composite materials and the **plantation**, which harvests plants growing on the terrain (bio conversion).

Details: [resources](../documentation/documentation/concepts/resources.txt) and [compound](../documentation/documentation/concepts/compound.txt).

## Units

| Kind | Examples | Notes |
|---|---|---|
| **Vehicles** | a vehicle *base* (hoverbase, heavy base, catamaran base) plus a *turret* (cannon, artillery, rocket/missile launcher, anti-air, EMP, radar, surveyor, terraformer, mechanic, bomb) | Built by vehicle plants. If the turret is too heavy for the base, the vehicle slows down. Catamarans stay on water, and heavy bases prefer flat solid ground. |
| **Planes** | bomber, raptor, sensor drone, cargo plane | Built by plane factories. They fly at sub-orbital height and land in vehicle stores. |
| **Structures** | mine, generator, reactor core, storage, laboratory, vehicle plant, vehicle store, plane factory, munition factory, plantation, towers (cannon, rocket, missile, anti-air, radar, surveyor), Nexus port | Towers behave like immobile turrets that can also fire during the movement phase. |
| **Satellites** | scanning, colonizing, defending, orbital strikes | They orbit high above the terrain, and most need fuel. |

**T2 upgrades** (merged in 2021) let existing units and turrets gain XP and be upgraded to a second tier with better stats.

Unit pages: [`documentation/documentation/units/`](../documentation/documentation/units/).

## Sensors, ghosts and EMP

- *Everything has sensors, and sensors are everything.* You only see what your radar covers, and allied factions share what they see.
- When contact is lost, a unit leaves a **ghost** at its last known position for up to 10 rounds.
- **EMP damage** slows units down, cuts attack and scanning range sharply, and blocks operations. It doesn't stack, and it heals by one level per round.

Details: [scanning range](../documentation/documentation/concepts/scanning%20range.txt), [ghost](../documentation/documentation/concepts/ghost.txt) and [EMP damage](../documentation/documentation/concepts/emp%20damage.txt).

## Research

Each enabled laboratory adds one research point per round. The default script locks advanced units (heavy and catamaran bases, missile launchers, reactor cores …) until they're researched, and lets you improve turret ranges and radar ranges with diminishing returns. The whole research tree and every unit attribute live in the Lua script `lua/attributesAndResearch.lua`, which server hosts can override. See [lua-scripting.md](lua-scripting.md).

## Factions, alliances and voting

- Each connected client controls one **faction**. A secret token lets you resume it later.
- **Alliances** (since v0.7.0) are invite-based. Members share sensors, and their units never auto-target each other.
- Players can **vote** on game decisions, and the server keeps the vote state (`server/components/state/gameState.h`).
- Units can be handed over to another faction, and alliance members can place shared map markers (Shift+click).

## The Nexus

The **Nexus** is a built-in AI faction with bases and **Nexus ports** at up to 12 *poles* around the planet. Ports surrounded by a base give the most resources, and isolated ones give fewer. A game can have the goal `ConquerNexus` (the alternative is `None`). The Nexus's behavior can be replaced with a Lua AI script, and the AI API includes threat and support estimates per pole. See [lua-scripting.md](lua-scripting.md).

## Game rules worth knowing in code

`clientServerShared/runtimeConfig.h` holds the switches that change the rules: `gameGoal`, `doomsdayWeapons` (`Expose`/`Conceal`/`Disallow`), `empZones`, `autoNewRound`, `autoSpawnFromVehiclePlants`, `hibernationSystemEnabled`, `bombsExplodeWhenDestroyed`, `storagesExplodeWhenDestroyed`, `tutorialMode` and the per-type unit limits. All of them can be set from the server configuration (see [running.md](running.md)).
