# TheLoTMProject

A small program <3

A Lord of the Mysteries creator suite in C++. Make characters and Sealed Artifacts, browse them in a
catalogue, and export them as good-looking sheets.

It comes as two programs that share the same saves:

- **The app window** (`lotm_window`): a normal window with tabs, text boxes, dropdowns and sliders.
- **The console version** (`lotm_creator`): the same features in the terminal, answered with numbers.

- **Character Creator:** name, looks (hair colour, length and style, eyes, skin, face, build, height,
  voice, clothing) and bio, one of the 22 pathways or one of your own, Sequence 9 to 0 with every ability
  described, or Above the Sequence (a Great Old One), alignment, titles, aliases, honorific name (Sequence
  3 and up), Beyonder characteristics and the Uniqueness with its look and the Sequence 0 powers it grants
  (Sequence 1), a second pathway for a dual-pathway god, the Sefirah they hold, affiliation, two-way
  relationships (pick one of your saved characters or type any name), a D&D-style stat block (roll,
  re-roll, type or point-buy), Speed, optional HP, Spirituality, the Sealed Artifacts the character holds,
  and a dossier with an automatic threat level.
- **Sealed Artifact Creator:** name, look, pathway, sequence level, ability, drawback.
- **Pathways:** read any pathway Sequence by Sequence, create and edit your own inside the program, start
  one from a copy of a built-in pathway, and export a pathway as a sheet. The Acrobat pathway from the
  book is already in as one of your own.
- **Sefirot:** the nine Sefirot, the pathways each one gathers, its Great Old One and who has held it.
  Make one of your characters its holder, and export a Sefirah as a sheet.
- **Catalogue:** browse, search, filter by pathway, sort, edit, duplicate, delete.
- **Compare:** two to four characters side by side, with their stats on one chart and the highest value
  in each row marked.
- **Sample sets:** the members of the Tarot Club, the brothers Adam and Amon, and a few canon Sealed
  Artifacts, ready to add to your saves from Settings.
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

## Set up on macOS (one time)

1. Install Apple's compiler: open **Terminal** (press **Cmd+Space**, type `Terminal`, press Enter), type
   `xcode-select --install` and press Enter, then click **Install** in the window that appears. You don't
   need the full Xcode app.
2. Install **Homebrew**, an installer for developer tools: copy the command under "Install Homebrew" on
   <https://brew.sh>, paste it into Terminal and press Enter. It asks for your Mac password (nothing shows
   while you type it). When it finishes it prints **Next steps** with a few commands; paste and run those
   too, so Terminal can find `brew`.
3. Install **CMake**: run `brew install cmake` in Terminal. `cmake --version` should then print a version.
4. Install **VS Code** from <https://code.visualstudio.com/> and drag it into your **Applications** folder.
5. Get the project onto the Mac: install **GitHub Desktop** from <https://desktop.github.com/>, sign in,
   choose **File > Clone Repository**, pick **TheLoTMProject**, then choose
   **Repository > Open in Visual Studio Code**.
6. Accept the pop-up to install the recommended extensions (**C/C++** and **CMake Tools**). If no pop-up
   appears, press **Cmd+Shift+X**, search for each one and click **Install**.
7. If VS Code asks you to pick a "kit", choose the **Clang** one (it mentions `arm64-apple-darwin`, or
   `x86_64-apple-darwin` on an older Intel Mac).

Then build and run as described below. Two Mac differences: VS Code's **Ctrl** shortcuts are **Cmd** on a
Mac (**Cmd+Shift+P**), and the F keys need the **fn** key held down (**fn+F7** builds, **fn+Shift+F5** runs).
Clicking the buttons in the CMake panel does the same without any keys.

Your characters and Sealed Artifacts stay on the computer that saved them. To bring them from another
computer, copy its `data/characters.json` and `data/artifacts.json` into this project's `data` folder.

The first build downloads three libraries (nlohmann/json for the saves, GLFW and Dear ImGui for the app
window), so be online for it.

## Build and run

**In VS Code** (with the CMake Tools extension installed):

1. Click the **CMake** icon (a triangle) in the bar on the far left. The **Project Status** panel opens.
2. Under **Configure**, if no kit is shown, click it and pick **Visual Studio Build Tools 2022 Release - amd64**
   (on a Mac, the **Clang** kit).
3. Under **Build**, click the build icon that appears when you hover over it (or press **F7**).
4. Under **Launch**, pick which program to start: hover over it, click the pencil icon, and choose
   **lotm_window** (the app window) or **lotm_creator** (the console version). VS Code remembers the choice.
5. Click the play icon next to **Launch** (or press **Shift+F5**). The app window opens on its own; the
   console version starts in the **Terminal** panel at the bottom, where you type numbers and press Enter.

You can also press **Ctrl+Shift+P** and run **CMake: Build**, then **CMake: Run Without Debugging**.

**From a terminal:** open **Developer PowerShell for VS 2022** from the Start menu (a normal PowerShell
usually can't find `cmake`), go to this folder with `cd`, then run:

```
cmake -S . -B build
cmake --build build --config Release
.\build\Release\lotm_window.exe
```

The console version is `.\build\Release\lotm_creator.exe`.

**On a Mac**, use the normal **Terminal** app. Go to the project folder with `cd` (type `cd `, drag the
folder from Finder into the Terminal window, press Enter), then run:

```
cmake -S . -B build
cmake --build build
./build/lotm_window
```

The console version is `./build/lotm_creator`. Linux uses the same commands but needs a few packages for
the window first: `sudo apt install libgl-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`.
To build only the console version, configure with `cmake -S . -B build -DLOTM_BUILD_WINDOW=OFF`.

To run the automatic checks: `ctest --test-dir build -C Release`.

## Using the app window

- The tabs at the top are **Characters**, **Compare**, **Sealed Artifacts**, **Pathways**, **Sefirot**
  and **Settings**. Each one lists everything on the left (with a search box) and shows the one you picked on
  the right. Drag the list's right edge to make it wider.
- A character has its own tabs: Identity, Looks, Pathway, Customization, Stats, Sealed Artifacts,
  Relationships, Dossier, Notes, and Sheet, which shows the finished sheet as it will be exported.
- The Sequence, the six stats, Speed and a Sealed Artifact's level are sliders. Next to each stat,
  **Roll** rolls 4d6 and drops the lowest die; hover over the slider to see the last roll.
- Boxes with an arrow on the right take anything you type, or pick a suggestion from the arrow.
- **Above the Sequence:** in a character's Pathway tab, drag the Sequence slider all the way to the right,
  one step past Sequence 0. The character becomes a Great Old One: the tab offers to give them their
  group's Sefirah, and they have every ability of every pathway in the group. Their own pathway is written
  out in full; each other pathway shows as one line, "All abilities of the ... pathway".
- **Dual-pathway gods:** at Sequence 0, the Pathway tab has a **Second pathway** box. Pick the other
  pathway, then choose one or two of its Sequence 1 Beyonder characteristics and whether they hold its
  Uniqueness. They have every ability of the second pathway, Sequence 9 to 0, shown as one line, "All
  abilities of the ... pathway". The sheet, the exports and Compare show both pathways.
- **Relationships:** the arrow next to a name lists your saved characters, and **+ Add a saved character**
  opens a searchable list of them, so you don't have to type names. **+ Add someone by name** still takes
  anyone, saved or not.
- **Sefirot** lists the nine Sefirot. Pick one to read what it is, which pathways it gathers, its Great Old
  One and who has held it. **+ Make one of your characters its holder** links a saved character to it. A
  character holds one Sefirah at a time; you can also pick it in their Customization tab.
- Nothing is saved until you click **Save** (or press **Ctrl+S**, **Cmd+S** on a Mac). The Save button
  turns gold when there are changes. If you open something else first, the window asks whether to save
  them, throw them away, or keep editing. Closing the window asks the same.
- **Compare** puts two to four characters side by side: tick them in the list. It shows their pathway,
  Sequence, tier and threat level, every stat with a bar (and the six stats on one chart), Speed, HP,
  Spirituality, abilities, Sealed Artifacts and how they're related to each other. The highest value in
  each row is in bold, in that character's colour. Rows none of them has filled in are left out.
  **Open** jumps to that character's editor.
- **Export...** saves the open sheet as HTML, Markdown or plain text in the `exports` folder; HTML opens in
  your browser. Settings has a button that exports everything at once.
- In Settings, **Text size** makes everything in the window bigger or smaller, and the window comes in
  two themes: **Night** (dark ink with gold) and **Parchment** (light paper with brown ink).
- Every pathway group has its own colour. A character's medallion in the list shows their Sequence in
  that colour; the top of the editor shows their initials, tier, threat level and affiliation.
- **Sample sets** in Settings adds ready-made characters and Sealed Artifacts to your saves (see below).

## Using the console version

- Type the number of a menu entry and press Enter. `0` goes back.
- When a value is shown in `[brackets]`, pressing Enter keeps it. Typing `-` clears an optional text.
- Longer texts (descriptions, backstory, abilities) end with an empty line.
- Relationships take any name, like `Fors Wall`, or type `P` to pick one of your saved characters from a
  numbered list. If the name matches a character you've already saved,
  the two are linked and the other character gets the relationship back when you save (Mentor and
  Student, Parent and Child, Superior and Subordinate flip; the rest stay the same). Removing it removes
  both sides. If you list someone by name first and create them later, they link up when you save them.
- Every creator finishes on a review screen where you can change any step before saving.
- In the stat block, `R` rolls 4d6 and drops the lowest die; roll again as often as you like or type a value.
- The Sequence question takes 9 to 0, and a Sequence 0 is then asked whether they are Above the Sequence
  and whether they are a dual-pathway god with a second pathway.
- **Sefirot** in the main menu lists the nine Sefirot; open one to read it, make a saved character its
  holder or take it away, or export it.
- **Open the Catalogue**, then **Compare characters side by side**, picks two to four characters and
  prints them as a table, with the highest value in each row marked `*`.

### Where things are saved

| What | Where |
| --- | --- |
| The 22 built-in pathways | `data/pathways.json` |
| Your own pathways (the Acrobat and any you create) | `data/custom_pathways.json` |
| The nine Sefirot | `data/sefirot.json` |
| Your characters | `data/characters.json` |
| Your Sealed Artifacts | `data/artifacts.json` |
| Settings (HP mode, export theme, backups, window text size and theme) | `data/settings.json` |
| Backups (last 5 of each file) | `data/backups/` |
| Exports | `exports/` |

Your own characters, artifacts and exports are **not** uploaded to GitHub (see `.gitignore`); they stay on
your computer. Copy the `data` folder to back them up or move them to another PC. Your own pathways are
different: GitHub Desktop shows `custom_pathways.json` as changed after you edit a pathway, and committing
it shares your pathways with anyone else who uses the repository.

### Sample sets: the Tarot Club, Adam and Amon

`data/samples` holds ready-made characters and Sealed Artifacts. There are two sets:

- **The Tarot Club:** every member as of the end of the first novel, with their pathways, Sequences, looks,
  backstories and relationships to each other, plus canon Sealed Artifacts linked to whoever holds them.
- **Adam and Amon:** the two sons of the Ancient Sun God. Adam is the Sequence 0 Visionary and Amon the
  Sequence 0 Error, a dual-pathway god who also holds the Door pathway's Uniqueness. Their relationships
  link to Klein and the rest of the Tarot Club if you've added them.

Stats are an interpretation (a 27-point buy before pathway bonuses), not something the novel gives. Each
character's Notes say what is canon, with chapter numbers where they help, and what is a guess.

To add them, open **Settings** in the app window and click **Add to my saves** under **Sample sets**, or in
the console choose **Settings**, then **Add a sample set**. They become ordinary characters and Sealed
Artifacts that you can edit or delete. Anything you've already saved under the same name (your own Klein
Moretti, say) is left as it is, and the new characters link to it instead; adding the same set twice adds
nothing.

A sample set is a JSON file with a `title`, a `description`, and `characters` and `artifacts` lists in the
same layout as `characters.json` and `artifacts.json`. The ids inside it only need to match each other.

### Adding your own pathways

In the app window, open the **Pathways** tab and click **+ New pathway**. Fill in the Overview tab, then
open each Sequence in the Sequences tab to name it and add its abilities, and click **Save**. To start from
a built-in pathway, pick it in the list and click **Copy into a new pathway of your own**.

In the console version, choose **Pathways** in the main menu, then **Create a new pathway**. The program asks for the name (usually
the Sequence 9 name, like Seer), the god at the top, its group of neighbouring pathways, the two stats it
boosts, its speed grade, an optional overview and Uniqueness, and then each Sequence from 9 down to 0 with
its abilities. Press Enter to skip anything and come back to it later from **Edit one of your pathways**.
When you add an ability, type its name and then what it does, or type both at once as
`Name: what it does`. To start from an existing pathway, open it with **Look at a pathway** and choose
**Copy it into a new pathway of your own**.

Your pathways show up in every pathway list (character creator, Sealed Artifacts, filters). A pathway that
a character or Sealed Artifact still belongs to can't be deleted until you move them to another pathway.
If a pathway describes its Uniqueness, a Sequence 1 character who holds it starts from that description.

The 22 built-in pathways in `data/pathways.json` can only be changed by hand: open the file in VS Code,
find the pathway's `{ ... }` block and edit it. Write each ability as `"Name: what it does"`; the exports
show the name in bold. Movement powers are ordinary abilities of the Sequence that unlocks them. The
program picks up changes the next time it starts.

## How the rules work

- **Pathway bonuses:** each pathway boosts a primary and a secondary stat. The bonus grows with the speed
  tier: primary +1 per tier, secondary +1 every two tiers. Stats may go past 20 at high sequences.
- **Speed tiers:** 0 Mortal, 1 Awakened (Seq 9-8), 2 Extraordinary (7-5), 3 Saint (4-3), 4 Angel (2-1),
  5 Divine (0), 6 Above the Sequence. A higher tier always wins; inside a tier the Speed score decides. The pathway's speed grade
  adjusts the score: Slow -2, Average 0, Fast +2, Very fast +4.
- **HP and Spirituality:** the program suggests a value from the tier and Constitution (HP) or Wisdom
  (Spirituality); you can always type your own. HP can be asked per character, always on, or always off
  in Settings.
- **Sequence 1 and the Uniqueness:** only a Sequence 1 chooses whether they hold 1 or 2 Beyonder
  characteristics and whether they hold the pathway's Uniqueness. If they hold it, Customization asks you
  to describe what it looks like and to pick the Sequence 0 abilities it grants. A Sequence 0 has already
  absorbed it and has every Sequence 0 ability, but you can still describe it. Other Sequences can't hold
  one, so the question doesn't appear for them.
- **Above the Sequence:** a Great Old One holds a Sefirah and every Uniqueness of their pathway group. They
  count as Sequence 0 for everything else, get tier 6 bonuses (primary +6, secondary +3) and every
  ability of every pathway in their group. They can't also take a second pathway.
- **Dual-pathway gods:** only a Sequence 0 who isn't Above the Sequence can take a second pathway. It
  adds every ability of that pathway, Sequence 9 to 0, but no extra stat bonus. A pathway outside their own group is allowed, with a warning, since every canon
  example combines neighbouring pathways.
- **Sefirah:** any character can hold one Sefirah. Several of your characters can hold the same one (canon
  has its rival claimants and successors), and the Sefirot tab lists them all.
- **Threat level:** three numbers added up. Sequence: mortal 0, Sequence 9-8 1, 7-5 3, 4-3 5, 2-1 7,
  Sequence 0 10, Above the Sequence 12. Recent actions: peaceful -1, minor incidents 0, violent +1, mass casualties +2. Sealed
  Artifacts held: one or two +1, three or more +2. A total up to 2 is Low, 3-4 Moderate, 5-6 High, 7-8
  Extreme, 9 or more Catastrophic. It updates by itself unless you set a level by hand in the dossier.

## Project layout

```
CMakeLists.txt        build instructions
src/window/           the app window: one file per tab, plus widgets.cpp, theme.cpp and window_main.cpp
assets/fonts/         the window's heading fonts (Cinzel, Cormorant Garamond) and their licences
src/main.cpp          the console version's main menu
src/model.*           the data structures (characters, artifacts, pathways)
src/rules.*           modifiers, tiers, bonuses, suggestions
src/storage.*         loading and saving JSON, backups
src/ui.*              console prompts
src/records.*         saving, deleting and adding sample sets, shared by both programs
src/sheet.*           what goes on a character or artifact sheet
src/render.*          drawing a sheet as text, Markdown or HTML
src/exporter.*        writing exports to the exports folder
src/presets.hpp       the suggestion lists (hair colours, organizations ...)
src/*_creator.cpp     the two creators
src/pathway_menu.cpp  the Pathways menu (your own pathways)
src/catalogue.cpp     the catalogue
src/compare.*         comparing characters side by side, shared by both programs
src/sefirot_menu.cpp  the console's Sefirot menu
src/export_menu.cpp   exports
tests/tests.cpp       automatic checks
data/pathways.json    the 22 built-in pathways
data/custom_pathways.json  your own pathways
data/sefirot.json     the nine Sefirot
data/samples/         sample sets (the Tarot Club, Adam and Amon)
```

## Troubleshooting

- **F5 says "program 'enter program name ...' does not exist"**: VS Code made a blank `launch.json` for you.
  Press **Cancel** and use **Shift+F5**, or delete `.vscode/launch.json` and pull the latest version from
  GitHub, which includes a working one. After that, F5 builds and runs the program picked under **Launch** too.
- **No CMake triangle icon in VS Code**: press **Ctrl+Shift+X**, search for **CMake Tools**, and install it
  (and **C/C++** from Microsoft).
- **`cmake` is not recognized**: you are in a normal PowerShell. Use **Developer PowerShell for VS 2022**
  instead, or build from VS Code.
- **Mac: F5 says debugger type `cppvsdbg` is not available**: VS Code picked the Windows entry. Open
  **Run and Debug** (**Cmd+Shift+D**) and choose **Run LoTM Creator (macOS / Linux)** in the list at the
  top; VS Code remembers it. Or run without the debugger: **fn+Shift+F5**, or the play icon in the CMake panel.
- **Mac: "No kits found", or "No CMAKE_CXX_COMPILER could be found"**: Apple's compiler isn't installed
  yet. Run `xcode-select --install` in Terminal, then in VS Code press **Cmd+Shift+P** and run
  **CMake: Scan for Kits**.
- **Mac: `cmake: command not found`, or VS Code says "Bad CMake executable"**: install CMake with
  `brew install cmake`. If `brew` isn't found either, run the **Next steps** commands Homebrew printed when
  it was installed. Then quit Terminal and VS Code completely (**Cmd+Q**) and open them again.
- **CMake complains about the `build` folder** (for example a "generator" mismatch): delete the `build`
  folder and build again.
- **"Could not find the data folder"**: run the program from this project folder (VS Code does this
  automatically), or set the `LOTM_DATA_DIR` environment variable to the `data` folder's path.
- **A save file "is not valid JSON"**: it was edited by hand and has a typo. Fix it, or copy the newest
  matching file from `data/backups` over it.
- **The app window doesn't open, or opens and closes at once**: the window needs OpenGL 3, which every
  graphics driver from the last ten years has. Update the graphics driver, or use the console version.
- **Accented letters look wrong in the console**: use Windows Terminal (the default on Windows 11).
