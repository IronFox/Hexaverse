# Bug reporter

`BugReporter/` contains a small Windows Forms program that helps players report a crash. It lists the game's log files, finds the ones related to the crash, and lets the player describe what happened. It then either uploads the report to a server you configure or saves it as a `.zip` file.

The bug reporter is **not part of the default game packages**, and it has **no report address built in**. Out of the box, nothing is ever sent anywhere. Bugs are reported on the project page, which the game's *Help → Report Bug* menu opens: <https://github.com/IronFox/Hexaverse>.

If you make your own release and want players to be able to send reports to you, add the bug reporter as described below, with your own report address.

## How the game uses it

| Situation | Without the bug reporter (default) | With `BugReporter/BugReporter.exe` installed |
|---|---|---|
| *Help → Report Bug* in the Visual Client | Opens the GitHub project page | Opens the GitHub project page (see [Opening it from the menu](#opening-it-from-the-menu) to change this) |
| Fatal error in the client or the server (`MyFatal()`) | Writes `fatal.log` and the log entry, nothing else | Also starts the bug reporter with the active log file and the error message |

The game looks for `../BugReporter/BugReporter.exe`, relative to `client/` and `server/` (`Logging::CallBugReporter()` in `clientServerShared/logEvent.cpp`). If the file isn't there, the call does nothing.

## Building

The project targets .NET 10 (`net10.0-windows`, Windows Forms).

- Install the [.NET 10 SDK](https://dotnet.microsoft.com/download), or Visual Studio 2022/2026 with the *.NET desktop development* workload.
- Open `BugReporter/BugReporter.sln` in Visual Studio, or build from the command line:

```bat
dotnet build BugReporter\BugReporter.csproj -c Release
```

CI compiles the project on every push (`.github/workflows/cmake.yml`, job *BugReporter*), but it isn't packaged.

For distribution, publish it as a single executable into `BugReporter/publish/`:

```bat
rem Needs the .NET 10 Desktop Runtime on the player's PC (small download):
dotnet publish BugReporter\BugReporter.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true -o BugReporter\publish

rem Or self-contained, with no runtime needed (much larger):
dotnet publish BugReporter\BugReporter.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o BugReporter\publish
```

`BugReporter/publish/` then holds `BugReporter.exe` and `BugReporter.json`. The folder is ignored by Git.

## Configuration: `BugReporter.json`

The settings are read from `BugReporter.json` next to `BugReporter.exe`. The file in the repository is the template, and it is copied to the build and publish output.

```json
{
	"reportUrl": "",
	"token": "",
	"privacyNoticeUrl": "",
	"projectUrl": "https://github.com/IronFox/Hexaverse"
}
```

| Setting | Meaning |
|---|---|
| `reportUrl` | Address that receives reports. It must be `https://`; anything else is ignored. **Empty (the default): nothing is uploaded.** The *Send* button becomes *Save report…*, which writes a `.zip` the player can attach to an issue. |
| `token` | Optional. If set, it is sent with every request as the `token` field, so your server can reject unrelated posts. It isn't a secret: anyone with the release can read it. |
| `privacyNoticeUrl` | Optional. Shown as a *Privacy Notice* link below the form. Hidden when empty. If you collect reports (including the optional e-mail address), you should provide one. |
| `projectUrl` | Page offered after saving a report, and in the error dialog. |

If the report server can't be reached (three attempts at startup) or an upload fails, the reporter switches to saving the report to a file.

## Server protocol

The reporter sends `application/x-www-form-urlencoded` POST requests to `reportUrl` over HTTPS. The system's certificate validation applies, including revocation checks. The server answers with the plain text `ok` on success; any other answer is shown to the player.

**Startup check:** `action=respond` (plus `token` if configured). Answer `ok` if you accept reports.

**Report:** `action=store` with these fields:

| Field | Content |
|---|---|
| `token` | The configured token, if any |
| `description` | What happened (required in the form) |
| `reproduction` | What the player did before (optional) |
| `contact` | E-mail address (optional) |
| `gameVersion` | Version from `server/Server.exe --version` or from the log file name |
| `fatalMessage` | The fatal error found in the primary log file, if any |
| `primaryLogFileName`, `primaryLogFileData` | Selected log file. The data is the raw file compressed with Deflate (`System.IO.Compression.DeflateStream`, no zlib header) and then Base64-encoded |
| `logFileName[]`, `logFileData[]` | Related log files the player checked, same encoding, in matching order |

In PHP, for example, `gzinflate(base64_decode($_POST['primaryLogFileData']))` restores a log file. Limit the request size and rate on your server, and store reports outside the web root.

## Adding it to a release

1. Set `reportUrl` (and ideally `privacyNoticeUrl`) in `BugReporter/BugReporter.json`, then publish as shown above.
2. Add this line to `mainBakeRecipe.txt`, and to `serverBakeRecipe.txt` if dedicated servers should start it too:

   ```
   BugReporter/publish/*.* -> BugReporter/*.*
   ```

   Bake copies the published files into `BugReporter/` in the package, next to `client/` and `server/`, where the game looks for them.
3. Package as usual (`repack.bat`, or the Release workflow; see [building.md](building.md#7-packaging)). In the Release workflow, add the `dotnet publish` command before the *Bake* step.

From then on, fatal errors in the client and the server open the bug reporter.

## Opening it from the menu

*Help → Report Bug* always opens the project page. To open the bug reporter from the menu instead, change the command in `CreateHelpMenu()` in `client/components/visual/hud/menu.cpp`:

```cpp
menu->AddCommand("Report Bug","Opens the bug reporter",
	[]()
	{
		SetFullScreen(false);
		Logging::CallBugReporter();
	}
);
```

## Changing the reporter

| File | Content |
|---|---|
| `Program.cs` | Entry point, error dialog, `OpenExternally()` |
| `ReporterSettings.cs` | Loads and validates `BugReporter.json` |
| `MainForm.cs` | Log file discovery, the startup check, upload and saving reports |
| `LogFile.cs` | Parses log files: version, time range, fatal message, entity IDs used to find related logs |
| `whoops.cs` | Dialog shown when something unexpected goes wrong |

Log files are searched in `Documents\My Games\Hexaverse\{client,server,aiClient}\logs`. If you change the log format in `clientServerShared/logEvent.cpp`, update `LogFile.cs` too (a comment in `logEvent.cpp` points this out).
