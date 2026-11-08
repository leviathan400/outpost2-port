**English** · [Português (BR)](README.BR.md)

# Outpost 2: Divided Destiny — Modern Port (C++17 + SDL2)

A reimplementation of *Outpost 2: Divided Destiny* (Sierra On-Line / Dynamix,
1997) in **C++17 + SDL2**, rebuilt through reverse engineering from the
disassembly of the original executable.

**The game runs end to end**: both factions' campaigns, the colony games, the
14 tutorials and networked multiplayer matches for up to 6 players. The 94
original mission DLLs are loaded and **actually executed**, by a custom PE
loader with ABI bridges for the 222 symbols they import from `OUTPOST2.EXE`.

> **Project directive:** every mechanism is extracted from the **original
> code** (disassembly and binaries) — nothing is guessed. Where the original
> has an identified defect, the port **fixes it instead of reproducing it**.

See [`PORT_STATUS.EN.md`](PORT_STATUS.EN.md) for the detailed status: what is
done, what is missing, and the original mechanism behind each part.

---

## ⚠️ Nothing from the 1997 game is here

This repository contains **only the port's code**. The game files
(`OUTPOST2.EXE`, the `*.DLL`s, the `*.VOL`s, `OP2_ART.BMP`, the `*.ANI`s,
`OUTPOST2.INI`, `OUTPOST2.HLP`…) and the disassembly are protected commercial
material and are **not** versioned or redistributed.

The port **reads** those files from wherever they already are and modifies
none of them. To play, you need a legitimate installed copy of the game.

---

## Requirements

| | | |
|---|---|---|
| System | **Windows** 10 or 11 | the port uses the Windows API (PE loader, Winsock, GDI) |
| Architecture | **Win32 / x86 — mandatory** | not a preference or a default: see why below |
| Compiler | MSVC with C++17 (Visual Studio **2019 or newer**) | the code uses `__fastcall` and the Microsoft ABI; MinGW/Clang have not been tested |
| CMake | **3.15 or newer** | also tested with CMake 4.x |
| SDL2 | development package **2.30.9 for VC** | the executable's only external dependency |
| Original game | **mandatory**, installed | the 1997 assets are read from disk |
| Python 3 | optional | only for the scripts in `tools/` |

There is no vcpkg, Conan or submodules. `ws2_32` and `ole32` (networking) and
`gdi32` (fonts) are resolved by MSVC itself — nothing to install for them.

### Why the x86 build is a requirement

The port loads the original mission DLLs and **runs their 32-bit code inside
its own process** (`src/mission/pe_loader.cpp`), with API bridges using the x86
`__fastcall` convention (`src/mission/op2_api.cpp`). In a 64-bit process this
is impossible: an x64 build **compiles** and only fails when opening the first
mission. To keep that trap from getting that far, `CMakeLists.txt` stops at
configuration time if the architecture is not 32-bit.

---

## Installing the dependencies

### 1. Compiler (MSVC)

Install **Visual Studio Community** (free, 2022 or newer) —
<https://visualstudio.microsoft.com/downloads/> — and, in the installer, tick
the **"Desktop development with C++"** workload. It already includes
everything the port needs: the MSVC compiler, the Windows SDK and **x86
support**, which is the target used here.

If you don't want the IDE, you can install just the command-line tools
(*Build Tools for Visual Studio*, same page). With winget:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

### 2. CMake

Visual Studio's C++ workload includes a CMake. To have the `cmake` command
available in any terminal, install the standalone version:

```powershell
winget install --id Kitware.CMake
```

Or download it from <https://cmake.org/download/> and tick *"Add CMake to the
system PATH"*. Check with `cmake --version` (must be 3.15+).

### 3. SDL2

Download **`SDL2-devel-2.30.9-VC.zip`** — the *development* package for
Visual C++, not the runtime one — from
<https://github.com/libsdl-org/SDL/releases/tag/release-2.30.9>.

Extract it inside the repository so that the tree looks **exactly** like this:

```
third_party/
  SDL2-2.30.9/
    include/          SDL.h, SDL_render.h, ...
    lib/
      x86/            SDL2.lib, SDL2main.lib, SDL2.dll   <- the ones the build uses
      x64/            (comes in the package; not used)
```

That path is the `SDL2_ROOT` in `CMakeLists.txt`. If you prefer SDL2 somewhere
else, change that variable there. The `third_party/` folder is not versioned —
that's why this step is manual.

In PowerShell, from the repository directory:

```powershell
mkdir third_party
Expand-Archive -Path "$HOME/Downloads/SDL2-devel-2.30.9-VC.zip" -DestinationPath third_party
```

### 4. Python, only if you'll use `tools/`

The 20 reverse-engineering scripts in `tools/` need **Python 3**. Most use
only the standard library; nine of them need two packages:

```powershell
pip install pefile capstone
```

`pefile` (reading PE resources and exports) is used by `analyze_shell.py`,
`check_dialog_fmt.py`, `disasm_menu.py`, `dump_dialogs.py`, `extract_bmps.py`,
`extract_menu.py`, `inspect_bmps.py`, `inspect_table.py` and `parse_dlg.py`;
`capstone` (disassembler) by the first two. **None of this is needed to
compile or play.**

---

## Build

With the dependencies in place, from the repository directory:

```powershell
cmake -B build -S . -A Win32
cmake --build build --config Release
```

The `-A Win32` is not optional. If it is omitted on a 64-bit machine,
configuration stops with a message saying what to do.

The post-build step copies `outpost2_modern.exe` and `SDL2.dll` to the
project root. **The executable must run from the game's installation
directory** — that's where it reads the `.VOL`s, the mission DLLs and the
original resources from. The simplest way is to clone the repository **inside
the game folder**: the `.gitignore` is a whitelist, made exactly for this, and
ignores all the original material that lives alongside the sources.

### If something goes wrong

| Symptom | Cause |
|---|---|
| `Outpost2Port precisa ser gerado para Win32 (x86)` | the `-A Win32` was left out |
| `SDL.h` not found, or `SDL2.lib` missing | SDL2 is not in `third_party/SDL2-2.30.9/` (check the folder name) |
| `MSB3073` / *"Permission denied"* when copying the exe | the game is running; close it and rebuild |
| compiles, but every mission fails to open | the build came out x64 — delete `build/` and configure again with `-A Win32` |

> Filtering the build output only for `"error C"` **hides** the post-build
> copy failure. Filter for `error|denied`.

---

## Running

```powershell
outpost2_modern.exe                      # main menu
outpost2_modern.exe --game eden01.map    # opens a map directly
```

The game's `OUTPOST2.INI` file is respected. The port adds seven keys the
original doesn't have — AI level, night darkness, volume, demo camera and
fullscreen; they are documented in `PORT_STATUS.EN.md`.

There are **97 command-line modes** for tests and captures, among them a
regression battery (`--savetest` proves save/reload determinism). The list is
in `PORT_STATUS.EN.md`.

---

### Cheats

The original's cheats are not keys: they are sentences typed into the **chat
line** (Enter). On a match the text turns into `Woof Woof!` and the effect
applies to whoever typed it — on every machine, without breaking network
lockstep. **The punctuation is part of the password:**

| Sentence | Effect |
|---|---|
| `Dan's dog could make more ore.` | fills both ore stores to capacity |
| `Dan's car could hold more people.` | workers and scientists +25% |
| `Dan's bike could backfire more often` | weapons reload faster |
| `Dan's gun could die more people.` | weapons with far more penetration |

The first three are from the 1997 game. The fourth is an **addition of this
port**: the effect exists in the original, but its sentence was never
published and the executable only stores the hash — so this sentence was
made to reach it. `PORT_STATUS.BR.md` has the whole recogniser, the hashes,
and what was checked and discarded from the cheat lists that circulate.

---

## Structure

```
src/
  main.cpp        entry point + the 97 test/capture modes
  app/            TApp, UIState, GameState (the game), ShellState, HUD,
                  the frame's 6 tabs, minimap, globe, dialogs
  game/           the simulation: units, structures, movement, pathfinder,
                  combat, economy, research, disasters, triggers, AI
  gfx/            window-sized framebuffer (640x480 base), PBMP, sprites, fonts (bitmap and GDI)
  res/            PE reader (.rsrc + exports), .VOL + LZHUF, ResManager
  core/           OUTPOST2.INI (CConfig)
  audio/          SDL mixer faithful to the original's DirectSound, music (.clm)
  mission/        PE loader for the mission DLLs + API bridge
  net/            lobby (discovery/joining) and lockstep match
tools/            Python reverse-engineering scripts (formats, dumps)
```

That's 45,200 lines of C++17. The 1997 formats are all decoded and documented
in `PORT_STATUS.EN.md`: the `.VOL` container (with LZHUF), the `.map`, the
PBMP tilesets, the `op2_art` sprites, the data sheets and the `.ANI` cursors.

---

## Recent changes

### 2026-11-08

- **The map view stops at the edges**, as in the original: only "around the
  world" maps (512 tiles wide) wrap. Units, terrain and the demo camera follow
  the same rule. `MapWrap=1` brings back wrapping on every map, the old
  behaviour.
- **A `Screen Mode` radio pair** — full screen or window — on **both**
  preferences screens: `PREFERENCES` in the main menu, and the `PREFERENCES`
  list in the in-game Options tab. It only takes effect on `ACCEPT`/`&APPLY`,
  and writes `[Game] Fullscreen` to the INI.
- **The in-game `PREFERENCES` list was hiding its own scroll arrows** behind
  the buttons: nothing past the first page could be reached, and the pane was
  even stealing the `L&ESS` button's click.
- **The `L&ESS`/`&MORE` tooltips** are now the original's, one per item
  ("Turn music volume down (L&ess)"), instead of one generic sentence.

### 2026-11-07

- **Right-click move hit the wrong tile and never docked.** The target came
  from window coordinates instead of map-view coordinates, so units went to a
  point 12 px right and 22 px below the click — usually one tile low. The path
  also bypassed the original's move logic (`sub_451280`), so right-clicking a
  building's dock never sent the dock order. It now uses the view coordinates
  and the same path as the MOVE button.
- **Agridome dock pad.** The Agridome has a truck dock (`BUILDING.TXT`:
  Docking Truck = Y) but its art has no pad, so trucks parked on bare ground.
  It now gets the same pad every other docked building carries in its art
  (images 385-388, placed as in all 10 Structure Factory / Smelter / Spaceport
  / GORF groups), for both colonies and only once the building is finished.
- **The game screen follows the window size.** It used to be a fixed 640x480
  image stretched to the window, so a bigger window only made everything
  bigger. The game is now drawn at the real window size: the right column
  (minimap, tabs, panel) keeps its 192 px pinned to the right edge, the map
  view takes the rest, and resizing the window or toggling fullscreen re-lays
  it out. The menus stay at 640x480 scaled, as in the original. The frame
  reaches the real edge, and the save/load dialog and the "Success" box are
  centred.
- **A `MenuSkin` key** to choose the menu style. `0` (default) is the port's
  own look — dark-blue panels, translucent main menu on the right. `1` is the
  **original Outpost 2 dialog skin**, with all art read from the game's own
  `OP2SHRES.DLL`: the navy "blueprint" background (bitmap 225), red border and
  title bar with green bold Arial; buttons from the game's red button art
  (bitmaps 235/236) with a bright outline on focus; black lists with a blue
  selection; the main menu measured from the original (192x348). Without the
  bitmaps every control falls back to the port's style.
- New test modes: `--dockclicktest <DLL>`, which right-clicks an Agridome dock
  with a cargo truck through the real input path and checks that it docks, then
  that a plain move lands exactly on the clicked tile; and an optional camera
  position for `--missiontest`. Screenshots now save at the real frame size.

### 2026-11-06

- **The `END MISSION` items did nothing** — two defects plus three divergences
  from the original. See section 5.7 of `PORT_STATUS.BR.md`.
- **Cheats typed into the chat line**, as in the original, plus one addition of
  this port for the fourth effect. See the cheats section above.
- **English documentation**: `README.EN.md` and `PORT_STATUS.EN.md`. The
  Portuguese originals became `README.BR.md` and `PORT_STATUS.BR.md`, and
  `README.md` is a landing page for both languages.

---

## License

This port's code is under the **MIT** license (see [`LICENSE`](LICENSE)).

The license covers **only the port's code**. It does not extend to anything
from *Outpost 2: Divided Destiny* — not the executable, the DLLs, the `.VOL`s,
the art, the sounds, the maps, or the disassembly used as reference. None of
that is in this repository, and a legitimate copy of the game remains a
requirement to run it.

## Credits

*Outpost 2: Divided Destiny* is by Sierra On-Line / Dynamix. This is an
independent preservation and reimplementation work, unaffiliated with the
rights holders.

The community projects **OPUPatch** and **TethysAPI** were used as
documentation references — to check class names, offsets and the list of the
original's known defects. **No code from them was copied here**, as they are
under a different license.

Reverse Engineering done by Claude Code!
