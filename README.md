# TomeTik

**TomeTik** is *ToME — Troubles of Middle Earth*, the classic Tolkien-themed
roguelike, played with **graphical tiles** instead of plain text — including an
**isometric view**.

![TomeTik in isometric mode](screenshots/shot1.png)

Despite the "Tik" in the name, it has nothing to do with Tcl/Tk.

## What it's based on

This project takes **TomeTik 0.3** (a tiles overlay on top of ToME 2.2.2) and
brings it to modern systems: it now compiles with current toolchains and runs on
Linux and Windows, with several graphical frontends and a finished isometric
renderer.

## Borrowed from OmnibandTk

OmnibandTk is a sister port that shares the same ToME 2.2.2 base, so a few data
pieces are reused between them:

- The **David Gervais isometric tileset** (`dg_iso32.gif`) used by the iso view.
- The **`graf-*.prf` tile mapping files** that tell the game which tile to draw
  for each monster, object and terrain.

## Frontends

- **GTK2** — native multi-window UI with 32×32 tiles and the isometric mode. The
  main way to play on Linux.
- **GDI (Windows)** — the classic Windows build, producing a standalone
  `tometik.exe` (cross-compiled with mingw-w64).
- **Docker** — scripts to build any of the above and to *play in your browser*
  via a VNC / noVNC container, no local install needed.

## Some of what's been added

- A finished, portable **isometric renderer** (the original one was a stub).
- **Click-to-move** with A\* pathfinding and an **autoexplore** option.
- **Context action on click** (e.g. click a monster to attack it).
- **Health bars** over the player and visible monsters.
- **Sound and music** through SDL2_mixer.
- Arrow keys for movement, wide ("big") tiles on by default, and a modern build.

## Credits

ToME by Eric "DarkGod" Stevens and the ToME team; built on the Angband /
PernAngband lineage. Isometric tiles by David Gervais. See `credits.txt` for the
full history.
