**English** · [Português (BR)](CHANGELOG.BR.md)

# Changelog

Changes in this fork since it was forked from
[marcelomgoncalves/outpost2](https://github.com/marcelomgoncalves/outpost2)
at commit `1697e7f` (2026-11-06). Newest first.

## 2026-11-08

### Fixed
- **The map view stopped wrapping around every map.** The port let the view
  scroll endlessly left and right on every map, repeating the map. The
  original only does that on "around the world" maps (512 tiles wide); on
  all the others the view stops at the map's left and right edges, like any
  RTS. That is now the default. Units, terrain and the attract-mode camera
  follow the same rule.
- **Dock pads of the mines and Storage Tanks.** The Common and Rare Ore
  Mines, the Magma Well and both Storage Tanks have a truck dock, but the
  pad is not in their building art, so it was never drawn. The original
  draws it in a separate "ground layer" pass under every unit (building
  vtable slot 30), which the port did not have. That pass is now ported:
  - mines and Magma Well: their own blue-grey pad, animation `0x747`
    (`sub_44B280`);
  - Storage Tanks: animation `0x881`, or `0x882`/`0x883` when damaged
    (`sub_409590` / `sub_409840`);
  - Agridome: animation `0x87E`, or `0x87F`/`0x880` when damaged
    (`sub_401970`). This replaces the hand-placed pad of 2026-11-07, which
    looked the same but never showed damage.

  Hidden while the building is being built or destroyed, as in the original.

### Changed
- **Right-clicking a dock always sends the truck there.** In the original,
  right-clicking a building's dock with a vehicle that can't use it -- for
  example a loaded truck on a mine, which only takes empty ones -- was
  ignored (`sub_451280` returns without an order), so the truck didn't move.
  It now drives onto the dock tile with a plain move. A vehicle that can use
  the dock still gets the dock order and loads/unloads as before.

### Added
- **`MapWrap` setting** (`[Game]` section of `OUTPOST2.INI`):
  - `MapWrap=0` (default): as in the original (see above).
  - `MapWrap=1`: every map wraps, the port's old behaviour.
- `--dockclicktest <DLL> [type]`: optional building type (hex, default
  `27` = Agridome), e.g. `15` / `16` / `2A` for the mines, `19` / `1A` for
  the Storage Tanks. Passes for all of them on `t13` and `t14`.

## 2026-11-07

### Fixed
- **Right-click move missed the clicked tile, and never docked.** The
  right-click took its target from window coordinates instead of map-view
  coordinates, so units went to a point 12 px right and 22 px below the
  click — usually the tile underneath. It also bypassed the original's
  move-click logic (`sub_451280`), so right-clicking a building's dock never
  sent the dock order. It now uses the view coordinates and the same path as
  the MOVE button: right-clicking a dock with a truck that can use it docks.
  (Upstream bug, also in `1697e7f`.)
- **Agridome dock pad.** The Agridome has a truck dock (`BUILDING.TXT`:
  Docking Truck = Y, bottom-left tile), but its art has no pad, so trucks
  parked on bare ground. It now gets the same "X" dock pad the other docked
  buildings carry in their art (images 385–388, placed exactly as in all 10
  Structure Factory / Smelter / Spaceport / GORF groups), as OPU shows it.
  Both factions; drawn once the building is finished.
- **Test screenshots after the resolution change.** The `--*shot` /
  `--screenshot` / `--missiontest` modes saved a fixed 640×480 image, which
  came out scrambled for in-game captures now that the game screen is
  window-sized. They now save at the real frame size.

### Added
- `--dockclicktest <DLL>`: right-clicks an Agridome dock with a cargo truck
  through the real input path and checks it docks, then right-clicks a free
  tile and checks it arrives exactly there. Fails on the old code
  (truck stops one tile low), passes on `t13` and `t14`.
- `--missiontest <DLL> <out> [ticks] [camTileX camTileY]`: optional camera
  position for the capture.
- **`MenuSkin` setting** (`[Game]` section of `OUTPOST2.INI`) to choose the
  menu style (`98c305b`):
  - `MenuSkin=0` (default): the port's own look, exactly as upstream draws it:
    dark-blue panels, translucent main menu on the right.
  - `MenuSkin=1`: the original Outpost 2 dialog skin (see below).

  Checked on 19 menu screens: `0` is pixel-identical to the upstream build,
  `1` to the OP2-skin build.
- **Original OP2 dialog skin** for every menu screen (`1038b5a`). All the art
  is read from the game's own `OP2SHRES.DLL`:
  - **Windows:** the navy "blueprint" background (bitmap 225, aligned to the
    screen as in the original), red border, and a navy title bar with a green
    Arial Bold title.
  - **Buttons:** the game's red button art (bitmaps 235/236). The focused
    button gets the bright red outline. Text is yellow-green Arial Bold with
    the shortcut letter underlined.
  - **Text:** green Arial; disabled items are grey.
  - **Lists:** black with a light border, blue selection with white text, a
    standard scroll bar and 14 px rows.
  - **Text boxes and drop-downs:** black, with an arrow button.
  - **Radio buttons and checkboxes:** classic glyphs.
  - **Main menu:** the original's measured size (192×348), position and
    button layout (93% pixel match with a capture of the original).
  - If the bitmaps can't be loaded, every control falls back to the port's
    own style.

### Changed
- **In-game screen follows the window size** (`9480d5d`). It used to be a
  fixed 640×480 image stretched to the window, so a bigger window only made
  everything bigger. Now, as in the original and OPU, the game is drawn at
  the window's real size:
  - the right-hand column (minimap, tabs, panel) keeps its 192 px width,
    against the right edge;
  - the map view fills the rest;
  - resizing the window or toggling fullscreen re-lays it out;
  - the menus stay at 640×480, scaled, as in the original.
  - The Save/Load dialog and the "Success" box are centred in the window.
  - The window border is drawn to the real edge, so the bottom and right
    borders no longer disappear.
  - Difference from the original: the view fills the space exactly instead
    of rounding down to a multiple of 32 px, so no empty strip is left.

## 2026-11-06

### Added
- **English documentation** (`7ead70a`): `README.EN.md` and
  `PORT_STATUS.EN.md`. The Portuguese originals were renamed to
  `README.BR.md` and `PORT_STATUS.BR.md`, and `README.md` is now a short
  landing page linking both languages.
