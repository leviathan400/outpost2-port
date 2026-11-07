**English** · [Português (BR)](CHANGELOG.BR.md)

# Changelog

Changes in this fork since it was forked from
[marcelomgoncalves/outpost2](https://github.com/marcelomgoncalves/outpost2)
at commit `1697e7f` (2026-11-06). Newest first.

## 2026-11-07

### Added
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
