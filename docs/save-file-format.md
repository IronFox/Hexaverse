# Save file format (`.hsf`) and RIFF

A Hexaverse save file (`.hsf`, "Hexaverse Save File") is a chunk tree written by DeltaWorks' `Riff::Chunk` container (`Core/io/riff_handler.*` in [DeltaWorks](https://github.com/IronFox/DeltaWorks)). It starts with `RIFF` and uses RIFF's chunk header, size and padding rules, but it is **not** a standard RIFF file: it has no form type, its `LIST` chunks have no list type, and lists are named by a preceding `NAME` chunk instead. Generic RIFF tools (for example `riffdump`, or the RIFF readers in media libraries) misread it from byte 8 onwards.

The container itself, and how it differs from the Microsoft RIFF specification in general, is documented in DeltaWorks' [`docs/riff.md`](https://github.com/IronFox/DeltaWorks/blob/master/docs/riff.md). This page covers what Hexaverse puts into it. Players' view of save games (where they are stored, how to load them) is in [game/config/save-games.md](game/config/save-games.md).

Code: `server/components/state/fileIO.cpp` (`State::Save`, `State::Load`, chunk ids in `namespace RID`, list names in `enum ChunkName`), `clientServerShared/fileVersion.h` (version numbers), `clientServerShared/objects/statistics.cpp` (faction statistics).

## Comparison with RIFF

| RIFF rule | `.hsf` | |
|---|---|---|
| File starts with the 4 bytes `RIFF` | Yes | Conforms |
| Followed by a 32-bit little-endian size counting everything after the size field | Yes. The size covers the children including their pad bytes | Conforms on little-endian machines. Sizes are written in host byte order, and every supported build target (Windows x86/x64) is little-endian |
| Size field is followed by a 4-character **form type** (`WAVE`, `AVI ` …) that identifies the format | **No form type.** The first child chunk (`VERS`) starts directly at offset 8 | Diverges. A RIFF reader takes `VERS` as the form type and then misreads everything after it |
| Chunk = 4-byte id, 32-bit little-endian data size, data | Yes | Conforms |
| Size field excludes the pad byte; data of odd size is followed by one pad byte | Yes | Conforms |
| Pad byte is zero | Pad bytes are **not initialised**. Files written by a debug build contain `0xCD` (the MSVC debug-heap fill), release builds leave whatever was in memory | Diverges. Readers skip pad bytes, so this does not break loading |
| `LIST` chunk data starts with a 4-character **list type** | **No list type.** A `LIST`'s data is just its child chunks | Diverges |
| Lists are identified by their list type | Lists are identified by a `NAME` chunk placed **immediately before** the `LIST`, as its sibling. In `.hsf` the `NAME` holds a 2-byte `ChunkName` key (or, for faction marker lists, the marker type name as text) | Diverges (DeltaWorks convention) |
| Ids are ASCII alphanumerics, padded with trailing spaces | Mostly uppercase letters and digits. Some ids are padded with **leading** spaces (`" WTC"`, `" WCC"`, `" MFA"`, `"  T2"`) | Diverges slightly. Readers compare ids as raw 4-byte values, so it has no practical effect |
| Registered ids have fixed meanings (`LIST`, `INFO`, `JUNK` …) | `LIST` and `RIFF` are the only reserved ids. Hexaverse also uses `NAME` as a plain data chunk (the faction name) inside faction lists, where it is never followed by a `LIST`, so it is not mistaken for a list name | Private ids, no conflict in practice |
| Maximum size 4 GiB | Same (32-bit sizes; larger chunks abort the save) | Conforms |
| Chunk data | Raw C++ PODs in host byte order, and DeltaWorks compact serialization (`SerializeToCompactMemory`) | Opaque to anything but Hexaverse |

In short, the chunk framing is RIFF and a RIFF walker can follow it if it is told that there is no form type and no list types. Treat `.hsf` as a private format that borrows RIFF's chunk layout.

## Byte layout

```
offset  bytes
0       "RIFF"
4       u32  size of everything that follows (file size - 8)
8       "VERS"  u32 4   u32 file version          <- first child, no form type
20      "TREX"  u32 4   u32 terrain grid exponent
32      "NAME"  u32 2   u16 ChunkName             <- names the LIST that follows
42      "LIST"  u32 n   child chunks...           <- no list type
...
```

To walk the file: read the 8-byte root header, then read chunks from offset 8 until offset `8 + size`. For each chunk read the id and size; if the id is `LIST`, its data are child chunks to walk the same way; otherwise skip `size + (size & 1)` bytes.

## Top-level structure

Written by `State::Save`, in this order. `NAME = k` + `LIST` means a `NAME` chunk with the 2-byte key `k`, followed by a `LIST`.

| Chunk | Contents |
|---|---|
| `VERS` | `u32` file version. Current is **1050** (`SupportedFileVersion`). Files below 1035 or above the supported version are rejected. Loaders branch on `FileVersions` to read older layouts |
| `TREX` | `u32` terrain grid exponent. Loading fails if it does not match the build's `Grid::HighExponent` (absent means 6) |
| `NAME = 1` (`WorldInfoGroup`) + `LIST` | `WDID` world id, `RNDN` round number, `WCFG` planet config, `SNWH` snow height, ` MFA` most factions alive, `ROAA` alliances active at round one, `WINA` winning alliance (only if the game has ended), `GAMP` game phase (text), `TTMP` terrain type map, `VARC` save-game variables (`path<TAB>value` lines), `RSTB` resource table, `RESS` compressed research script |
| `NAME = 2049` (`WorldGroup`) + `LIST` | One ` WTC` chunk per terrain family: the current world, delta-compressed |
| `NAME = 3073` (`InitialWorldGroup`) + `LIST` | Same, for the world as it was when the game started |
| `NAME = 1025` (`ObjectGroup`) + `LIST` | One **unnamed** `LIST` per vehicle (including stored ones) and per structure, holding `ENTI`, `LOCA`, `VEHC`/`STYP`, task and job chunks and so on |
| `NAME = 4097` (`ResourceSection`) + `LIST`, once per resource | `RSNM` name, `RSCV` coverage |
| `NAME = 6145` (`AllianceSection`) + `LIST`, once per alliance | `ALID` id, `ALFL` flags |
| `NAME = 5121` (`FactionSection`) + `LIST`, once per faction | `NEXU` (Nexus only), `ID  ` faction id, `ALID`, `NAME` faction name, `COLR` colour, `ACTK` access token, `REST`/`ARET` research state, `AISN`/`AISS` AI script, `NAME = 5123` + `LIST` markers (one text-named `LIST` per marker type, `MKST` per marker), `FFLG` flags, `WTCK` known world per terrain family, `RSDC` or `RSDF` discovered resources |
| `NAME = 5122` (`FactionStatisticsSection`) + `LIST`, once per faction | `ID  ` faction id, then the statistics chunks |

The full list of chunk ids, including deprecated ones still accepted on load (`RSRC`, `RSRM`, ` MAA`), is `namespace RID` in `fileIO.cpp`. Older files contain ids the current writer no longer emits; `server/config/tutorial.hsf` is version 1047, for example.
