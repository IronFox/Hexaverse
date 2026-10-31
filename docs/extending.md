# Extending the game

Adding a unit or structure type touches several layers: the shared object model, the protocol, the server's per-faction views, the client's commands and rendering, and the Lua attribute script. This page walks through the touchpoints, using the real commits that added the **Plantation** (2019–2020) as a worked example. It extends the original notes in [`programmingNotes.md`](../programmingNotes.md).

> In those older commits the shared folder was still called `shared/`. It is now `clientServerShared/`.

## 1. Adding a structure type

### 1.1 Pick a place in the type tree

Structures derive from `Objects::Structure` (`clientServerShared/objects/structure.h`). Most should use one of the CRTP helpers:

| Base | Use for |
|---|---|
| `WorkingStructureT<T>` (derives `WorkingStructure`) | Structures that can be enabled, consume or produce resources, and have jobs (mines, generators, factories, labs …) |
| `OtherStructure<T>` | Passive structures (for example `Connector`) |
| `StorageStructure<T, N>` | Structures that store N resource kinds (`Mine`, `Plantation`) |

You can mix in extra roles: `MobileEntityProducer` (builds units), `Packable` (can be mobilized and settled), `SkyLinkStructure`, `Hangar`.

### 1.2 Declare the type

1. **`clientServerShared/objects/types.h`**: add `DECLARE_POINTER_CLASS(MyStructure)` next to its siblings. This gives you the `PMyStructure`, `WMyStructure` and `PConstMyStructure` pointer typedefs.
2. **`clientServerShared/stringTable.h`**: add a `Type_MyStructure` constant.
3. **New files** `clientServerShared/objects/structure/myStructure.h/.cpp`, containing the class. Look at `mine.*`, `plantation.*` or `laboratory.*` for the virtual methods you'll probably need: round processing, resource supply and demand, `HandleTrigger`, serialization, description.

Plantation example (`plantation.h`):

```cpp
class Plantation : public StorageStructure<Plantation,3>, public Objects::Packable
{
    CONSTRUCT_ENUMERATION2(Triggers, PlantTrees, PlantCrystals);
    virtual void HandleTrigger(UINT16 triggerIndex) override;   // reacts to client buttons
    ...
};
```

### 1.3 Register the prototype

The CRTP bases allocate a static `T::globalPrototype`. Add it to the list in **`Objects::Prototypes::RegisterAll()`** (`clientServerShared/objects/prototypes.cpp`):

```cpp
#include "structure/plantation.h"
...
    << Plantation::globalPrototype
```

### 1.4 Give it attributes in Lua

Every type needs an entry in **`lua/attributesAndResearch.lua`**, or it has no cost or health:

```lua
Plantation = DefaultStructure(40),
```

Use `Structure(cost, scanningRange)` or `ExplicitStructure(cost, health, scanningRange, isUnlocked)` for more control. Use `MakeUnlockResearchable(...)` if it should be locked behind research. See [lua-scripting.md](lua-scripting.md).

### 1.5 Send its state to clients

If the type has state that clients need to show, extend the protocol and the server's view builder:

1. **`clientServerShared/protocol.h`**: declare a state struct with `NETSTRUCTn(...)` and add it to the `Selective…` union of structure sub-states:

   ```cpp
   NETSTRUCT4(PlantationState
       , SerialUnsigned, treeAvailability
       , SerialUnsigned, crystalAvailability
       , bool, plantTrees
       , bool, plantCrystals
   )
   typedef Selective5<MineState,ProducerState,NexusPortState,TowerState,PlantationState> ...;
   ```

   `SelectiveN<...>` (`clientServerShared/protocol/serialBaseTypes.h`) is a serialization helper structure that can dynamically contain any one of the listed types (or none). `SetI()` selects option I and returns it for filling in; `GetI()` returns it, or `nullptr` if another option is active. Only the active option goes over the wire, after a one-byte index. Variants exist up to `Selective6`, so a sixth structure state means switching the typedef to `Selective6<..., NewState>`. Always append new types at the end, because the index is part of the wire and save format.

2. **`server/components/state/gameState.cpp`**: in `Game::Network::Describe(Protocol::StructureState&, …)`, fill the sub-state, but only when the faction may see it (`expose & Expose::…`) and the matching change flag is set:

   ```cpp
   if (const Objects::Plantation* p = dynamic_cast<const Objects::Plantation*>(&s))
   {
       Protocol::PlantationState& sub = state.selective.Set4();
       sub.treeAvailability = p->treePresence; ...
   }
   ```

3. On the client, apply the received sub-state to the local object (`client/components/objectNetworkHelper.cpp` / `objectStorage.cpp`).

Bump the protocol and file versions (`clientServerShared/fileVersion.h`) when the wire or save format changes, and keep loaders backward compatible.

### 1.6 Commands (buttons)

Unit commands are defined on the client in **`client/components/visual/commandDefinitions.cpp`** (`Objects::Commands::DefineAll()`). Write a `DefineMyStructure(CommandContainer&)` that usually starts with `DefineWorkingStructure(commands)` and then adds `Command::IconCommand`s:

```cpp
commands.Add(new Command::IconCommand("Plant Trees",
    []() {                                   // on click
        if (PPlantation p = ToPlantation(selectedEntity))
            Network::Channels::EntityTrigger.SendTo(Network::client,
                Protocol::EntityTrigger(p->id, Plantation::Triggers::PlantTrees));
    },
    [](bool controlEnabled) {                // state: enabled / perma-active / hidden
        ...
    },
    NoCosts));
```

The command name doubles as the icon name. `client/resources/icons/Plant-Trees.png` matches "Plant Trees" (see `hud/iconNameToFileName.cpp`).

For simple per-entity actions, reuse the generic **`EntityTrigger`** channel with a trigger index. The server turns it into a journal entry (`Journal::Round::EntityTrigger`) that calls `entity->HandleTrigger(index)`. Only add a new channel (§3) if the command needs extra data.

### 1.7 Rendering

- **Geometry:** put `client/objects/<typeName>.obj` (+ `.mtl`) there. The name is the type name with the first letter lower-cased and spaces removed. The source `.blend`/`.max` files are committed alongside.
- **Register the hull:** in `client/components/visual/scene/objectComponent.cpp`, include the header and call `LoadFor<Objects::MyStructure>();`. This loads the `.obj` and sets up the standard and T2 shaders.
- **Icon:** add a matching icon to `client/resources/icons/`. Floating icons go in `client/resources/floaters/`.
- If needed, add custom visuals in `scene/objectRendering.cpp`.

### 1.8 Documentation and packaging

- Add a unit page to `documentation/documentation/units/structures/` (`.prefix` for the generated header, `.txt` for the hand-written text). The `DocumentationEditor` tool helps with this.
- New file types must be added to `mainBakeRecipe.txt`, or they won't ship.
- Add a changelog line to `changelog.txt`.

### Checklist

| Done | Step | File(s) |
|---|---|---|
| ☐ | Pointer class declaration | `clientServerShared/objects/types.h` |
| ☐ | Type string constant | `clientServerShared/stringTable.h` |
| ☐ | Class | `clientServerShared/objects/structure/*.h/.cpp` |
| ☐ | Prototype registration | `clientServerShared/objects/prototypes.cpp` |
| ☐ | Attributes / research | `lua/attributesAndResearch.lua` |
| ☐ | Network sub-state (optional) | `clientServerShared/protocol.h`, `server/components/state/gameState.cpp`, client apply |
| ☐ | Commands | `client/components/visual/commandDefinitions.cpp` (+ `HandleTrigger`) |
| ☐ | Geometry + hull registration | `client/objects/*.obj`, `client/components/visual/scene/objectComponent.cpp` |
| ☐ | Icons | `client/resources/icons/` |
| ☐ | Save-game IO | the class's serialization, `fileVersion.h` |
| ☐ | Docs, recipe, changelog | `documentation/…`, `mainBakeRecipe.txt`, `changelog.txt` |

## 2. Adding a turret or vehicle base

Turrets and vehicle bases are **components** (`clientServerShared/objects/components.*`), not entity classes. Vehicles combine one base and one turret, and their type name is `turret:base` (see `Game.Type.ByName`).

1. Declare the component and its attributes in `components.*` / `componentAttribute.*`.
2. Add its configuration to `TurretConfig` or `VehicleBaseConfig` in `lua/attributesAndResearch.lua` (range, damage, weight, speed, trajectory …). Make it researchable if you want.
3. Add commands it needs (for example terraforming operations) in `commandDefinitions.cpp`. Server-side operations are implemented as jobs (`objects/jobs.*`) and executed by the evolver in the movement phase.
4. Add geometry and icons as above.
5. Expose the new operations to Lua AI scripts in `clientServerShared/lua/luaGameAPI.cpp`, and document them in `lua/General Lua API Documentation.txt`.

## 3. Adding a protocol channel

1. Add the ID to `Protocol::ChannelID` in `clientServerShared/protocol.h`, with a comment giving the direction (`//c2s:` or `//s2c:`). Append it to the right group, and remember that IDs are part of the wire format.
2. Declare the message struct (`NETSTRUCTn` or a hand-written serializable struct).
3. Server: declare the channel or sender in `server/components/networkChannels.h/.cpp` and install it in `server/components/state/networkState.cpp` (`server.InstallChannel(...)`). Commands from players should become **journal entries** (`server/components/journalSaveGame.*`) instead of changing the state directly.
4. Client: declare the matching channel in `client/components/network.h/.cpp` and handle incoming messages there, or schedule an animation event (`client/components/animation/*`).
5. Bump the protocol version.

## 4. Adding a server setting

1. Declare it with help text and default value in **`server/config/declaration.xml`**, inside the right `<context>`.
2. Bind it in **`server/components/reflection.cpp`** with `Variable::Set(...)`, `VAR(...)` or `Variable::SetStringReference(...)`, inside the matching `parser->EnterNewFolder("…")` block. Choose a `Variable::Persistence` (`PlanetConfig`, `RestoreFromSavegame` …) so the setting is saved where it should be.
3. If clients need it, add it to `Game::Network::AssembleGameConfig` / `AssembleGameInfo` (`gameState.cpp`) and the matching protocol struct.
4. The setting then works in `server.ini`, the control console, the Server Manager and the interactive configuration mode (if you add UI there: `client/components/configurationMode.cpp`).

## 5. Adding a planet type

Copy `predefinedPlanets/Template.ini` (or an existing planet), adjust it, and save it under a new name. Alternatively, design one in the client's interactive configuration mode and press **Save Planet as…**, which writes to `[game folder]\shared\config\myPlanets\`. Add shipped planets to `predefinedPlanets/`. The recipe includes `predefinedPlanets/*.ini` automatically.

## 6. Code conventions

- C++ with `/std:c++latest`. DeltaWorks types (`String`, `PathString`, `Array`, `HashTable`, `count_t`, `index_t`) are used everywhere instead of the STL equivalents.
- Shared pointers are declared through `DECLARE_POINTER_CLASS(X)` (`clientServerShared/typeDecl.h`), which gives `PX` (shared), `WX` (weak) and `PConstX`.
- Enumerations are declared with `CONSTRUCT_ENUMERATIONn(Name, …)`, which also gives string conversion.
- `elif` is a DeltaWorks macro for `else if`.
- Log with `LogEvent(EventClass::Preset::…, …)` or `LogEvent(EventSignificance::…, …)`.
- Commit messages are prefixed with the area: `Objects:`, `GUI:`, `Server:`, `Client:`, `LuaAPI:`, `LuaAI:`, `Rendering:`, `Protocol:`, `IO:`, `General:` …
- When DeltaWorks changes, adapt and reference the DeltaWorks commit in the message (*"General: Adapt to changes in base library (<sha>)"*).
