<!-- Generated from documentation/documentation/concepts/ai development.txt (DokuWiki syntax). -->

# AI Development

Starting with version v0.9.0, custom [Lua scripted](https://www.lua.org/) AIs can be developed and deployed.

## Runtime Concept

When loaded, the AI script is executed asynchronously exactly once during round start.
However, certain callback operations may trigger execution of registered Lua functions at a later state during the same round.
Once execution of the script and all remaining callback functions has completed, a new round is automatically requested by the console client.

During execution, the script has access to the known scene via predefined global variables and functions.
The system aims to provide almost all player options to the AI, but this is a continuous process.
To get an overview of the current functionality, check the included API documentation file in `[install]/lua/General Lua API Documentation.txt`.

## Persistent State

Every faction can have an active AI script assigned, however under most circumstances only the Nexus faction makes sense.
For debugging it can be useful to associate an AI script with a visual client controlled faction.

During savegame storage, the name of the assigned AI and its last serialized state is written to the save file.
The Lua function ExportState() is invoked once per round by the client to send a state to the server in order to preserve it.
Currently the total serial size of the exported data must not exceed 100kb.

The AI script is referenced via name by the running session, and restored as such when loading a savegame.
Any restored AI script must be located in *[game folder](../config/game-folder.md)/shared/AIScripts*, or restoration will fail, and the Nexus AI will return to its default behavior.

You should design your AI such that it is capable of resuming without a state, possibly at the expense of some performance degradation.

## Deployment

Final deployment is [configured](../config/server.md) via the [control console](../config/control-console.md) variable `Rules/Nexus/aiScript`.
You must have a working Lua AI file located in *[game folder](../config/game-folder.md)/shared/AIScripts* to assign it.
Re-assignment clears out any existing AI state, forcing the AI to start from scratch.

## Debugging

Development should be done in an interactive client session.
The new visual client option (Main Menu->Options) entry `Debug Tools for Scripted AI` toggles extended AI control buttons which affect the currently loaded faction.

Once the checkbox has been flipped, the `Auto End Round` [hex button](commands.md) is replaced by a new AI button.
It toggles the opening of a sub row of commands that control AI execution and (re)loading.

![ai debug](../../../documentation/images/ai%20debug.png)

Left to right, the top button functionality is:

- **Auto New Round**: Same like regular auto new round
- **Load AI...**: Lists all .lua files in *[game folder](../config/game-folder.md)/shared/AIScripts* for quick loading. Loading a script this way flushes any persistent state, even when reloading the same script again.
- **Create new AI...**: After entering a name, a new template AI script will be created using the given name. It will work but do nothing.
- **Reload and run AI**: Reloads your AI from file, and immediately executes it. Its state will be preserved unless you hold the **Shift**-Key while pressing this button (in which case the state will be flushed).
- **Edit Loaded AI**: Opens the currently loaded AI file with any installed editor associated with .lua files. You should be able to edit them using Notepad, if necessary.
- **Open AI Documentation**: Opens the API documentation file with any installed editor associated with .txt files.

---

[Game documentation index](../README.md)
