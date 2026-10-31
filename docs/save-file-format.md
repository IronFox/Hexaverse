# Save file format (`.hsf`) and RIFF

A Hexaverse save file (`.hsf`, "Hexaverse Save File") is a RIFF file with the form type `HSF `, written and read by DeltaWorks' `Riff::Chunk` container (`Core/io/riff_handler.*` in [DeltaWorks](https://github.com/IronFox/DeltaWorks)). Generic RIFF tools (for example `riffdump`, or the RIFF readers in media libraries) can walk its chunk tree. The chunk contents are private to Hexaverse.

Save files up to file version 1050 use DeltaWorks' older layout, which has no form type and no list types. The server still loads them; see [Older save files](#older-save-files).

The container, including how it tells the two layouts apart, is documented in DeltaWorks' [`docs/riff.md`](https://github.com/IronFox/DeltaWorks/blob/master/docs/riff.md). This page covers what Hexaverse puts into it. The player's view of save games (where they are stored, how to load them) is in [game/config/save-games.md](game/config/save-games.md).

Code: `server/components/state/fileIO.cpp` (`FileIO::Save`, `FileIO::Load`, chunk ids in `namespace RID`, list names in `enum ChunkName`, list types in `ListTypeOf`), `clientServerShared/fileVersion.h` (version numbers), `clientServerShared/objects/statistics.cpp` (faction statistics). The self-test `SaveFileFormats` in `server/components/tests.cpp` (run by `Server.exe --test`) checks both layouts.

## Comparison with RIFF

| RIFF rule | `.hsf` (version 1051 and later) | |
|---|---|---|
| File starts with `RIFF` and a 32-bit little-endian size of everything after the size field | Yes | Conforms. Sizes are written in host byte order, and every supported build target (Windows x86/x64) is little-endian |
| Size field is followed by a 4-character **form type** | `HSF ` | Conforms |
| Chunk = 4-byte id, 32-bit little-endian data size, data | Yes | Conforms |
| Size field excludes the pad byte; data of odd size is followed by one pad byte | Yes | Conforms |
| Pad byte is zero | Yes | Conforms. Files written before version 1051 have uninitialised pad bytes (`0xCD` from debug builds); readers ignore them |
| `LIST` chunk data starts with a 4-character **list type** | Yes, see [List types](#list-types) | Conforms |
| Private chunks may carry any data | Most lists are named by a `NAME` chunk placed **immediately before** the `LIST`, as its sibling. The `NAME` holds a 2-byte `ChunkName` key (or, for faction marker lists, the marker type name as text) | Conforms. To a RIFF reader these are two ordinary sibling chunks; only Hexaverse knows that one names the other. Loading identifies lists by their `NAME`, not by their list type |
| Ids are ASCII alphanumerics, padded with trailing spaces | Mostly uppercase letters and digits. Some ids are padded with **leading** spaces (`" WTC"`, `" WCC"`, `" MFA"`, `"  T2"`) | Diverges slightly. Readers compare ids as raw 4-byte values, so it has no practical effect |
| Registered ids have fixed meanings (`LIST`, `INFO`, `JUNK` …) | Hexaverse uses only private ids, including `NAME` as a plain data chunk (the faction name) inside faction lists, where it is never followed by a `LIST` | No conflict |
| Maximum size 4 GiB | Same (32-bit sizes; larger chunks abort the save) | Conforms |
| Chunk data | Raw C++ PODs in host byte order, and DeltaWorks compact serialization (`SerializeToCompactMemory`) | Opaque to anything but Hexaverse |

## Byte layout

```
offset  bytes
0       "RIFF"
4       u32  size of everything that follows (file size - 8)
8       "HSF "                                    <- form type
12      "VERS"  u32 4   u32 file version          <- first chunk
24      "TREX"  u32 4   u32 terrain grid exponent
36      "NAME"  u32 2   u16 ChunkName             <- names the LIST that follows
46      "LIST"  u32 n   "WINF"  child chunks...   <- list type, then children
...
```

## List types

| List | List type |
|---|---|
| `NAME = 1` world info (`WorldInfoGroup`) | `WINF` |
| `NAME = 1025` objects (`ObjectGroup`) | `OBJS` |
| one per vehicle inside `OBJS` (unnamed) | `MENT` |
| one per structure inside `OBJS` (unnamed) | `STRC` |
| `NAME = 1026` vehicle task (`VehicleTaskSection`) | `TASK` |
| production queue of a vehicle plant or other producer (unnamed) | `PQUE` |
| attack queue of a tower (unnamed) | `AQUE` |
| `NAME = 2049` world (`WorldGroup`) | `WRLD` |
| `NAME = 3073` initial world (`InitialWorldGroup`) | `IWLD` |
| `NAME = 4097` resource (`ResourceSection`) | `RESO` |
| `NAME = 5121` faction (`FactionSection`) | `FACT` |
| `NAME = 5122` faction statistics (`FactionStatisticsSection`) | `FSTA` |
| `NAME = 5123` faction markers (`FactionMarkers`) | `MRKS` |
| one per marker type inside `MRKS`, named by the type name | `MRKT` |
| `NAME = 6145` alliance (`AllianceSection`) | `ALLI` |

## Top-level structure

Written by `FileIO::Save`, in this order. `NAME = k` + `LIST` means a `NAME` chunk with the 2-byte key `k`, followed by a `LIST`.

| Chunk | Contents |
|---|---|
| `VERS` | `u32` file version. Current is **1051** (`SupportedFileVersion`). Files below 1035 or above the supported version are rejected. Loaders branch on `FileVersions` to read older contents |
| `TREX` | `u32` terrain grid exponent. Loading fails if it does not match the build's `Grid::HighExponent` (absent means 6) |
| `NAME = 1` + `LIST WINF` | `WDID` world id, `RNDN` round number, `WCFG` planet config, `SNWH` snow height, ` MFA` most factions alive, `ROAA` alliances active at round one, `WINA` winning alliance (only if the game has ended), `GAMP` game phase (text), `TTMP` terrain type map, `VARC` save-game variables (`path<TAB>value` lines), `RSTB` resource table, `RESS` compressed research script |
| `NAME = 2049` + `LIST WRLD` | One ` WTC` chunk per terrain family: the current world, delta-compressed |
| `NAME = 3073` + `LIST IWLD` | Same, for the world as it was when the game started |
| `NAME = 1025` + `LIST OBJS` | One `LIST MENT` per vehicle (including stored ones) and one `LIST STRC` per structure, holding `ENTI`, `LOCA`, `VEHC`/`STYP`, task and job chunks and so on |
| `NAME = 4097` + `LIST RESO`, once per resource | `RSNM` name, `RSCV` coverage |
| `NAME = 6145` + `LIST ALLI`, once per alliance | `ALID` id, `ALFL` flags |
| `NAME = 5121` + `LIST FACT`, once per faction | `NEXU` (Nexus only), `ID  ` faction id, `ALID`, `NAME` faction name, `COLR` colour, `ACTK` access token, `REST`/`ARET` research state, `AISN`/`AISS` AI script, `NAME = 5123` + `LIST MRKS` markers (one text-named `LIST MRKT` per marker type, `MKST` per marker), `FFLG` flags, `WTCK` known world per terrain family, `RSDC` or `RSDF` discovered resources |
| `NAME = 5122` + `LIST FSTA`, once per faction | `ID  ` faction id, then the statistics chunks |

The full list of chunk ids, including deprecated ones still accepted on load (`RSRC`, `RSRM`, ` MAA`), is `namespace RID` in `fileIO.cpp`.

## Older save files

Files up to version 1050 (including `server/config/tutorial.hsf`, version 1047) have the same chunks, but:

- no form type: `VERS` starts directly at offset 8,
- no list types: a `LIST`'s data is just its children,
- uninitialised pad bytes.

The container recognises them by bytes 12–15. In an old file these are the size of `VERS` (`04 00 00 00`), which never looks like a chunk id; in a new file they are `VERS`. Both layouts load into the same in-memory tree, so `FileIO::Load` does not distinguish them. `FileIO::Config::writeRiffFormType = false` still writes the old layout (used by the self-test).

Builds before version 1051 **cannot** read new files. Their container skips the form type but then reads each list type as a chunk header, and fails with "RIFF size violation" while parsing, before the version check runs.
