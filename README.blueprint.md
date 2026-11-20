# OpenTTD with a rail blueprint (copy/paste) tool

This repository is official OpenTTD with one addition: a **rail blueprint tool**. You can copy a piece of railway, save it as a named template, and paste it anywhere, rotated, in any game. Copying also records the land shape, which you can reproduce when pasting.

The copy/paste idea comes from the [CityMania client](https://github.com/citymania-org/cmclient) (GPLv2). The code here is a fresh implementation on top of official OpenTTD, kept small so it can follow every official release.

## How the repository is organised

- **`main`** holds two parts:
  - the commit `chore: import OpenTTD 15.3 source`, an unmodified copy of OpenTTD tag `15.3` (upstream commit `14ec60f248547d4d062a1160f0fc26d742319888`);
  - the blueprint patch as a short series of commits on top. The patch code lives in `src/blueprint/`, with small hooks in the rail toolbar, strings and CMake.
- **`.github/workflows/blueprint-release.yml`** checks every day for a new official OpenTTD release. When it finds one, it:
  1. replays the patch commits onto it;
  2. builds Windows x64;
  3. runs the tests;
  4. publishes a release named `blueprint-<version>`.

  If the patch doesn't apply cleanly, it opens an issue instead.

## Install (Windows)

1. Download `openttd-blueprint-<version>-windows-win64.zip` from the latest release.
2. Extract it to its own folder, for example `C:\Games\OpenTTD-Blueprint\`. **Don't** put it in Steam's OpenTTD folder, because Steam overwrites that folder on every update.
3. Run `openttd.exe`.

The patched game uses the same `Documents\OpenTTD` folder as Steam: same saves, settings, downloaded content and templates.

Saves are compatible both ways: the patch adds no data to save files. The game reports itself as the plain official version, so it can join normal servers of that version.

## Using the tool

- **Opening it:** in the rail construction toolbar, click the blueprint button. The blueprint library window opens.
- **New copy:** drag a rectangle over your railway, then give the template a name. Templates are stored as one file each in `Documents\OpenTTD\blueprints\`.
- **Place:** pick a template from the list.
  - **R** rotates it.
  - Clicking on the map builds it with the rail type selected in the toolbar.
  - With **Include terrain** ticked, the land is first reshaped to match the copied area.
- **What is copied:** track, signals, depots, bridges, tunnels and rail stations owned by your company.
