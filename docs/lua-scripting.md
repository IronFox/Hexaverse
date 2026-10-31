# Lua scripting

Hexaverse embeds **Lua 5.3.3** (`ThirdParty/lua`). Four kinds of scripts exist. Each one runs in its own environment with its own slice of the `Game` API:

| Script | Runs on | Shipped file | User override | Entry points |
|---|---|---|---|---|
| Research & unit attributes | Server (results sent to clients) | `lua/attributesAndResearch.lua` | `[game folder]\server\config\lua\attributesAndResearch.lua`, then `shared\config\lua\…` | top-level code plus the `ResearchState` class |
| Resource distribution | Server (planet set-up) | `lua/distributions/clustered.lua` | – (chosen with `World/resourceDistribution`) | `Compute()` |
| AI player | Console Client (or Visual Client while debugging) | `lua/AI/ai.lua` (+ `vehicles.lua`, `factories.lua`) | `[game folder]\shared\AIScripts\<name>.lua` | `Init`, `Execute`, `ExportState`, `OnMessage` |
| Tutorial | Visual Client | `lua/tutorial.lua` | – | `OnBegin`, `CheckTaskConditions`, `OnMessageConfirm`, `OnCamera*` |

The full API reference is [`lua/General Lua API Documentation.txt`](../lua/General%20Lua%20API%20Documentation.txt). Each script file also starts with a `--[[ API: … ]]` block that lists the functions specific to it. The C++ bindings live in `clientServerShared/lua/luaGameAPI.cpp`.

Every script can include other files:

```lua
--#require <path relative to the script directory>
--#require "path relative to this file"
```

Helper modules are in `lua/util/` (`util.lua`, `queue.lua`, `resvec.lua`).

---

## 1. Research and attribute script

`lua/attributesAndResearch.lua` defines **every unit attribute** (costs, health, ranges, damage, speed …) and the **research tree** that unlocks or improves them. The server loads it at start-up (`Research::LoadDefaultProgram()` in `clientServerShared/objects/research/globalState.cpp`). The first one found wins:

1. `[game folder]\server\config\lua\attributesAndResearch.lua`
2. `[game folder]\shared\config\lua\attributesAndResearch.lua`
3. `lua/attributesAndResearch.lua` (factory default)

`/IO/reloadResearchProgram` in the control console reloads it while the server runs. Clients get the generated attributes, topics and states. The script itself isn't sent to them any more.

### What the game expects

```lua
ResearchState = {}              -- per-faction research state (arbitrary table, class-like)
function ResearchState.New() ... end
function ResearchState:Export() ... end          -- return serializable content (tables, ints, floats, strings)
function ResearchState:Import(exported) ... end  -- accept known data, ignore the rest (old save games!)
```

Topics are created with:

```lua
Handle Game.CreateResearch(group, name, iconName, advance, getProgress, maxProgress, dependency)
-- group:       parent topic group, or nil for a root group
-- advance:     function(state, points) — called with research points (> 0)
-- getProgress: function(state) -> int
-- maxProgress: > 0 limited, < 0 infinite, 0 = this is a topic group
-- dependency:  topic that must be completed first
```

The shipped script builds two groups, **Unlock** and **Improve**, with helpers such as `MakeUnlockResearchable(...)` and `MakeImproveResearchable(...)`. Improvements use `GetResearchImprovementFactor`, a square-root curve that gives diminishing returns. Turret and vehicle-base configurations are plain tables (`TurretConfig`, `VehicleBaseConfig`), and structures are declared with `Structure(cost, scanningRange)` / `ExplicitStructure(...)`. To rebalance the game, edit these tables.

Other globals available: `Game.Log`, `Game.Grid.lowerResolutionPlanet`, `Game.Grid.bottomHeight`, `Game.Grid.topHeight`.

---

## 2. Resource distribution scripts

When `World/resourceDistribution` is set (the default is `clustered`), the server runs `lua/distributions/<name>.lua` (see `clientServerShared/grid/resources.cpp`) and calls:

```lua
function Compute()
  local rs = { Minerals = {}, Fuel = {}, Uranium = {} }
  for i = 1, #Game.Grid.unitCells do
    rs.Minerals[i] = 0; rs.Fuel[i] = 0; rs.Uranium[i] = 0
  end
  -- ... fill per-cell yields ...
  return rs
end
```

| API | Meaning |
|---|---|
| `Game.Grid.unitCells` | Every horizontal unit cell, each with `center` (normalized vector), `neighbors` (cells) and `index` |
| `Game.Simplex(x, y, z)` | OpenSimplex noise in [-1, 1] |
| `Game.RandomInt(min, max)` | Seeded random integer |
| `Game.Grid.lowerResolutionPlanet` | true if the planet uses the coarse grid |
| `Game.Log(msg)` | Write to the server log |

Keep yields at **12 or less** (uranium **6 or less**). `clustered.lua` places resource "hotspots", with uranium at the center and minerals and fuel on the rings around it. Set the variable to `''` to use the built-in distribution.

---

## 3. AI scripts

Lua AIs (since v0.9.0) usually control the **Nexus** faction, but any faction can have one, which helps with debugging.

### Runtime model

- The server starts a **Console Client** with an `aiAccessToken` for the AI's faction (`server/components/aiProcess.cpp`).
- At the start of every command phase, the client runs `Execute()` once, asynchronously (`client/components/ai/scriptedAI.cpp`). Callbacks the script registered may run later in the same round. When everything is done, the client requests a new round on its own.
- `ExportState()` is called once per round. Its result is sent to the server and stored in save games (at most **100 KB** serialized, check with `Game.CheckStateExportValidity`). `Init(state)` gets it back after a reload. **Write your AI so it can also start with no state.**
- `OnMessage(sender, message)` (optional) receives messages sent with `Game.SendMessage(target, msg)`.

### Minimal AI

```lua
--#require "vehicles.lua"
--#require "factories.lua"

function Init(state) end            -- state = whatever ExportState returned last time, or nil
function ExportState() return nil end

function Execute()
  Game.Log('Executing round '..Game.GetMainRoundNumber())
  ProcessFactories()                -- from factories.lua
  ProcessVehicles()                 -- from vehicles.lua
end

function OnMessage(sender, message)
  if message == 'AI: Respond' then Game.SendMessage(sender, 'Here') end
end
```

This is the shipped `lua/AI/ai.lua`. It's used when no custom script is set. `factories.lua` decides what the Nexus bases produce from threat and support estimates. `vehicles.lua` drives the units (scouting, attacking, terraforming, self-destruct decisions).

### Deploying

1. Put the script into `[game folder]\shared\AIScripts\<name>.lua`. Names may not contain `\ : ; ..` or `//`, and sub-folders are allowed.
2. Assign it: `Rules/Nexus/aiScript=<name>` in the control console, or in `server.ini`. Reassigning clears the stored AI state.
3. Save games store the script **name** and its last state. The file must still be in `AIScripts` when the save is loaded, or the Nexus falls back to its default behavior.

### Debugging in the visual client

Turn on **Main Menu → Options → Debug Tools for Scripted AI**. The *Auto End Round* hex button then becomes an AI menu with: Auto New Round, **Load AI…**, **Create new AI…** (writes a template), **Reload and run AI** (hold Shift to also flush the state), **Edit Loaded AI** and **Open AI Documentation**. `Game.DebugMarkers.Put/Remove/RemoveAll` draw colored markers on the map for visual debugging. See [`concepts/ai development.txt`](game/concepts/ai-development.md).

### API overview

All functions and tables live under `Game`. Tables that work like arrays are written `X[]`, and maps `{K->V}`.

| Area | Main members |
|---|---|
| General | `Log`, `GetMainRoundNumber`, `GetRound`, `GetPhase`, `CanIssueUnitCommands`, `Random`, `RandomIndex`, `MeasureSecondsFromNow`, `SendMessage`, `CheckStateExportValidity` |
| Scene | `units`, `factions`, `alliances`, `GetMobileAt(Loc)`, `GetStructureAt(Loc)`. Client only: `myUnits`, `alliedUnits`, `hostileUnits`, `selectedUnit`, `myFaction` |
| Types | `Game.Type.ByName('turret:base')`, `Game.Type.Vehicle(turret, base)`, `allTypes`, `units`, `turrets`, `vehicleBases` |
| Grid & terrain | `Game.Grid.*` (`GetDistance`, `IsClear`, `FindVehicleGround`, `FindHighestVehicleGround`, `GetNaturalResourcesAt`, `FindHighestAvailableResourceLocation[Ext]`, `AffectedByEMP`, `HeightToRadius`/`RadiusToHeight`, `ToLocation`, `suborbitalHeight`, `bottomHeight`, `topHeight`), `Game.Terrain.*` (`GetContent`, `cells`, `Pack`/`Unpack*`, `GetLowestSensorStrength`) |
| Research | `Game.Research.topics`, `myQueue`, `maxQueueLength` |
| Markers | `CreateMarker`, `myMarkers`, `maxMarkerCount`, `Game.DebugMarkers.*` |
| AI only | `Game.AI.IsBase(pole)`, `GetThreatLevel(pole)`, `GetSupportLevel(pole)`, `Estimate(...)`, `EstimateUnit(...)`, `ShouldBuildPlanes()`, `ShouldBuildHeavyVehicleBases(pole)`, `GetBaseVehiclePlants`, `GetBasePlaneFactories`, `GetOwnMobiles`, `GetNexusPort`, `IsPlaneFactoryHere` |

Table types: `Unit`, `UnitType`, `TurretType`, `VBaseType`, `Faction`, `Alliance`, `ResBalance`, `ResVec`, `Loc`, `Cell`, `Vector`, `Reachable`, `Turret`, `Hit`, `Job`, `Task`, `Prod`, `Topic`, `AiEst`, `AiThreat`, `AiSupport` and `ExportResult`.

Unit commands (methods on `Unit`) include movement (`DirectTo(Reachable)`, `GetReach(...)`, `GetReachFrom(...)`), combat (`Attack(Loc[])`, `SelfDetonate`, `Obliterate`), maintenance (`Repair`, `Upgrade`, `GetUpgradeCost`), terraforming (`DigDown`, `DigHorizontally`, `ClearAbove`, `BuildWallUp`, `BuildHorizontalWall`, `BuildRoad[To]`, `BuildLow/HighWallTo`, `BuildBridge`, `BuildBlocks`, `RemoveBlocks`), construction (`BuildStructure(UnitType)`, `PredictRoundsToCompletion`) and queue control (`ClearTasks`, `ClearSafeTasks`, `DropLastTask`, `SetAutomation`, `Sleep`, `Destroy`).

The Nexus estimates (`AiEst`) sort hostile units into proximity zones: `Irrelevant`, `Noticed`, `Yellow` (encroaching), `Red` (near base) and `Black` (near port). They separate air and ground activity, and sensor and attack impact. `AiThreat` and `AiSupport` add those up per pole, scaled by server difficulty.

---

## 4. Tutorial script

`lua/tutorial.lua` drives the tutorial in the visual client (`client/components/visual/tutorial.cpp`). The game calls these global functions:

| Function | When |
|---|---|
| `OnBegin(atTask)` | Tutorial started or resumed |
| `CheckTaskConditions()` | The situation changed. Check whether the current task is done |
| `OnMessageConfirm()` | The player confirmed the last `Game.DisplayMessage()` |
| `OnCameraDrag()`, `OnCameraRotate()`, `OnCameraZoom()` | Camera input |

It can also use `Game.CallDelayed(seconds, fn)`, `Game.ClearAllDelayed()`, `Game.FocusCameraAt(stackID)` and `Game.SignalTaskStart(index)`. The tutorial map is `server/config/tutorial.hsf`. `tutorial maps.txt` lists the settings a tutorial map must have (no EMP zones, no goal, AI off, no round timer, one terraformer).

---

## 5. Modding (work in progress)

The `master-resource_bridge` branch extends Lua to **define new structure types** (`lua/objects/ScriptedType.lua`, `structure/StructureType.lua`, `WorkingStructureType.lua`) with command callbacks. Its first example is a *Resource Relay* that moves resources between relays through orbit. This isn't merged into `master` yet.
