# TheLoTMProject

A small program <3

A Lord of the Mysteries creator suite in C++. Make characters and Sealed Artifacts, browse them in a
catalogue, and export them as good-looking sheets.

- **Character Creator:** name, looks and bio, one of the 22 pathways, Sequence 9 to 0 with its abilities
  filled in, alignment, titles, aliases, honorific name (Sequence 3 and up), Uniqueness, affiliation,
  relationships, a D&D-style stat block (roll, re-roll, type or point-buy), Speed, optional HP,
  Spirituality, and the Sealed Artifacts the character holds.
- **Sealed Artifact Creator:** name, look, pathway, sequence level, ability, drawback.
- **Catalogue:** browse, search, filter by pathway, sort, edit, duplicate, delete.
- **Export:** one entry or the whole catalogue as styled HTML (light and dark themes, prints to PDF),
  Markdown, or plain text.

Everything is saved as JSON in the `data` folder, with automatic backups in `data/backups`.

## Set up on Windows (one time)

1. Install **Visual Studio 2022 Build Tools** from <https://visualstudio.microsoft.com/downloads/>
   (scroll to "Tools for Visual Studio"). In the installer tick **Desktop development with C++**.
   This gives you the compiler and CMake.
2. Install **VS Code** from <https://code.visualstudio.com/>.
3. Open this folder in VS Code (**File > Open Folder**). Accept the pop-up to install the recommended
   extensions (**C/C++** and **CMake Tools**).
4. If VS Code asks you to pick a "kit", choose the one that mentions **Visual Studio Build Tools 2022 - amd64**.

The first build downloads one small library (nlohmann/json), so be online for it.

## Build and run

**In VS Code** (with the CMake Tools extension installed):

1. Click the **CMake** icon (a triangle) in the bar on the far left. The **Project Status** panel opens.
2. Under **Configure**, if no kit is shown, click it and pick **Visual Studio Build Tools 2022 Release - amd64**.
3. Under **Build**, click the build icon that appears when you hover over it (or press **F7**).
4. Under **Launch**, click the play icon (or press **Shift+F5**). The program starts in the **Terminal**
   panel at the bottom; type numbers and press Enter.

You can also press **Ctrl+Shift+P** and run **CMake: Build**, then **CMake: Run Without Debugging**.

**From a terminal:** open **Developer PowerShell for VS 2022** from the Start menu (a normal PowerShell
usually can't find `cmake`), go to this folder with `cd`, then run:

```
cmake -S . -B build
cmake --build build --config Release
.\build\Release\lotm_creator.exe
```

On macOS or Linux the same `cmake` commands work, and the program is `./build/lotm_creator`.

To run the automatic checks: `ctest --test-dir build -C Release`.

## Using it

- Type the number of a menu entry and press Enter. `0` goes back.
- When a value is shown in `[brackets]`, pressing Enter keeps it. Typing `-` clears an optional text.
- Longer texts (descriptions, backstory, abilities) end with an empty line.
- Every creator finishes on a review screen where you can change any step before saving.
- In the stat block, `R` rolls 4d6 and drops the lowest die; roll again as often as you like or type a value.

### Where things are saved

| What | Where |
| --- | --- |
| Pathway database (editable) | `data/pathways.json` |
| Your characters | `data/characters.json` |
| Your Sealed Artifacts | `data/artifacts.json` |
| Settings (HP mode, export theme, backups) | `data/settings.json` |
| Backups (last 5 of each file) | `data/backups/` |
| Exports | `exports/` |

Your own characters, artifacts and exports are **not** uploaded to GitHub (see `.gitignore`); they stay on
your computer. Copy the `data` folder to back them up or move them to another PC.

### Editing or adding pathways

Open `data/pathways.json` in VS Code. Each pathway is one `{ ... }` block with its id, name, god, the two
stats it boosts, its speed grade, the movement it unlocks, and Sequences 9 to 0 with their abilities.
Entries that say **"Edit me"** are waiting for you to fill in. To add a custom pathway, copy a whole block,
give it a new unique `id`, and change the rest. The program picks up changes the next time it starts.

## How the rules work

- **Pathway bonuses:** each pathway boosts a primary and a secondary stat. The bonus grows with the speed
  tier: primary +1 per tier, secondary +1 every two tiers. Stats may go past 20 at high sequences.
- **Speed tiers:** 0 Mortal, 1 Awakened (Seq 9-8), 2 Extraordinary (7-5), 3 Saint (4-3), 4 Angel (2-1),
  5 Divine (0). A higher tier always wins; inside a tier the Speed score decides. The pathway's speed grade
  adjusts the score: Slow -2, Average 0, Fast +2, Very fast +4.
- **HP and Spirituality:** the program suggests a value from the tier and Constitution (HP) or Wisdom
  (Spirituality); you can always type your own. HP can be asked per character, always on, or always off
  in Settings.

## Project layout

```
CMakeLists.txt        build instructions
src/main.cpp          main menu
src/model.*           the data structures (characters, artifacts, pathways)
src/rules.*           modifiers, tiers, bonuses, suggestions
src/storage.*         loading and saving JSON, backups
src/ui.*              console prompts
src/sheet.*           what goes on a character or artifact sheet
src/render.*          drawing a sheet as text, Markdown or HTML
src/*_creator.cpp     the two creators
src/catalogue.cpp     the catalogue
src/export_menu.cpp   exports
tests/tests.cpp       automatic checks
data/pathways.json    the 22 pathways
```

## Troubleshooting

- **No CMake triangle icon in VS Code**: press **Ctrl+Shift+X**, search for **CMake Tools**, and install it
  (and **C/C++** from Microsoft).
- **`cmake` is not recognized**: you are in a normal PowerShell. Use **Developer PowerShell for VS 2022**
  instead, or build from VS Code.
- **CMake complains about the `build` folder** (for example a "generator" mismatch): delete the `build`
  folder and build again.
- **"Could not find the data folder"**: run the program from this project folder (VS Code does this
  automatically), or set the `LOTM_DATA_DIR` environment variable to the `data` folder's path.
- **A save file "is not valid JSON"**: it was edited by hand and has a typo. Fix it, or copy the newest
  matching file from `data/backups` over it.
- **Accented letters look wrong in the console**: use Windows Terminal (the default on Windows 11).
