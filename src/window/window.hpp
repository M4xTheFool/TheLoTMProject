// The app window: the same characters, Sealed Artifacts, pathways and Sefirot as the console program,
// edited with text boxes, dropdowns and sliders. Built with Dear ImGui, which redraws the whole
// window many times a second: every draw* function below runs once per frame.
#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>  // text boxes that edit a std::string
#include <nlohmann/json.hpp>

#include "app.hpp"
#include "exporter.hpp"
#include "list_filter.hpp"
#include "model_json.hpp"
#include "sheet.hpp"

namespace lotm::window {

// The window's accent colours for the theme picked in Settings (see theme.cpp).
struct Palette {
    ImVec4 gold;     // headings, highlights
    ImVec4 muted;    // labels and secondary text
    ImVec4 danger;   // errors, Delete
    ImVec4 warning;  // unsaved changes
    ImVec4 success;  // saved, exported
    std::array<ImVec4, 4> series;  // one colour per character on the Compare tab
};
const Palette& palette();

// Colours, sizes and fonts. applyTheme runs at start-up and whenever the theme is changed in Settings.
void applyTheme(const std::string& theme, float dpiScale);
ImVec4 backgroundColour();
// Every pathway group has its own colour (Fool / Door / Error violet, Red Priest / Demoness crimson ...).
// Your own pathways get one made from their name; mortals are grey.
ImVec4 pathwayColour(const Database& db, const std::string& pathwayId);

struct Fonts {
    ImFont* body = nullptr;
    ImFont* bold = nullptr;
    ImFont* heading = nullptr;  // Cormorant Garamond: names and sheet titles, drawn large
    ImFont* title = nullptr;    // Cinzel: the window title, tabs and section headings
};
// Body text in the computer's own font, headings in the two fonts in assets/fonts.
Fonts loadFonts(const std::filesystem::path& dataDir);

// A copy of one record being edited. Nothing changes on disk until it is saved.
template <typename T>
struct Draft {
    T value;
    bool open = false;   // false: nothing is being edited
    bool isNew = false;  // not saved yet
    bool focusName = false;  // a new record opens on its first tab with the cursor in the Name box
    std::string savedJson;

    void start(const T& from, bool fresh) {
        value = from;
        open = true;
        isNew = fresh;
        focusName = fresh;
        markSaved();
    }
    void close() { open = false; }
    void markSaved() { savedJson = nlohmann::json(value).dump(); }
    bool dirty() const { return open && (isNew || nlohmann::json(value).dump() != savedJson); }

    // Flags for the editor's first tab, and the call to make just before its Name box.
    ImGuiTabItemFlags firstTabFlags() const { return focusName ? ImGuiTabItemFlags_SetSelected : 0; }
    void focusNameOnce() {
        if (!focusName) return;
        ImGui::SetKeyboardFocusHere();
        focusName = false;
    }
};

struct CharacterScreen {
    Draft<Character> draft;
    RecordFilter filter = RecordFilter::inIdOrder();  // the order they were made in until sorted
    std::array<std::string, kStatCount> lastRolls;  // "6, 4, 3, (1) = 13" shown next to each stat
    std::string lastSpeedRoll;
};

struct ArtifactScreen {
    Draft<Artifact> draft;
    RecordFilter filter = RecordFilter::inIdOrder();
};

struct PathwayScreen {
    Draft<Pathway> draft;
    std::string viewingId;  // a built-in pathway shown read-only
    std::string search;
};

struct SefirotScreen {
    std::string selectedId;  // a Sefirah from sefirot.json shown read-only on the right
    Draft<Sefirah> draft;    // one of your own Sefirot being edited
};

struct CompareScreen {
    std::vector<int> ids;  // the characters being compared, in the order they were ticked (up to four)
    std::string search;
};

enum class Tab { Characters, Compare, Artifacts, Pathways, Sefirot, Settings };

struct WindowState {
    App app{Database{}, Storage{std::filesystem::path()}, Dice{}};  // loaded once the window is open
    Fonts fonts;

    Tab tab = Tab::Characters;
    std::optional<Tab> switchTo;  // set to change tabs from code
    CharacterScreen characters;
    CompareScreen compare;
    ArtifactScreen artifacts;
    PathwayScreen pathways;
    SefirotScreen sefirot;

    // Sample sets in data/samples, read the first time the Settings tab shows them.
    std::optional<std::vector<SampleSet>> sampleSets;
    std::string sampleSetsProblem;
    bool showSampleSets = false;  // scroll the Settings tab down to them on the next frame
    // Settings > Share your own: packing your characters and Sealed Artifacts into a sample set.
    struct Share {
        std::string title;
        std::string description;
        std::vector<int> characterIds;
        std::vector<int> artifactIds;
        bool touched = false;  // ticked by hand, so a new title no longer changes the ticks
        std::string pickedFor = "\x01";  // the file the default ticks were made for
    } share;

    // The line at the bottom of the window: the result of the last save, export or error.
    std::string status;
    bool statusIsError = false;

    // "You have unsaved changes" question. `afterwards` runs once the user saves or discards.
    bool askUnsaved = false;
    std::function<bool()> saveCurrent;
    std::function<void()> afterwards;
    bool quitRequested = false;
};

// ---------------------------------------------------------------- screens

void drawCharactersScreen(WindowState& w);
void drawCompareScreen(WindowState& w);
void drawArtifactsScreen(WindowState& w);
void drawPathwaysScreen(WindowState& w);
void drawSefirotScreen(WindowState& w);
void drawSettingsScreen(WindowState& w);

// Save the record open on each screen. On failure they return false and say why on the status line.
bool saveCharacterDraft(WindowState& w);
bool saveArtifactDraft(WindowState& w);
bool savePathwayDraft(WindowState& w);
bool saveSefirahDraft(WindowState& w);

// Opens a saved character on the Characters tab (asking first about unsaved changes there).
void openCharacter(WindowState& w, int id);
// Shows a Sefirah on the Sefirot tab, in the editor when it is one of your own.
void openSefirah(WindowState& w, const std::string& id);
// Starts a new Sefirah of your own on the Sefirot tab, gathering this pathway.
void openNewSefirah(WindowState& w, const std::string& pathwayId);

// Shows the saved version of the open character again, after something else changed it on disk
// (adding a sample set can add relationships to it). Does nothing while it has unsaved changes.
void reloadCharacterDraft(WindowState& w);

// True when any screen has changes that are not saved yet.
bool hasUnsavedChanges(const WindowState& w);
// Runs `action` now, or first asks to save or discard the open screen's unsaved changes.
void whenSaved(WindowState& w, bool dirty, std::function<bool()> save, std::function<void()> action);
void drawUnsavedQuestion(WindowState& w);

// ---------------------------------------------------------------- shared controls (widgets.cpp)

void setStatus(WindowState& w, const std::string& text, bool error = false);
void helpMarker(const char* text);
// A label in the form's left column; the next control fills the rest of the row unless given a width.
void fieldLabel(const char* text);

bool textField(const char* label, std::string& value, const char* hint = nullptr);
bool multilineField(const char* label, std::string& value, float lines = 4.0f);
// A text box with a dropdown of suggestions next to it. Typing your own value always works.
bool presetField(const char* label, std::string& value, const std::vector<std::string>& presets);
// Short entries (titles, aliases): one text box each, with a remove button, plus an "Add" box.
bool listEditor(const char* id, std::vector<std::string>& items, const char* addHint);
// Pathways grouped under their neighbouring pathways. noneLabel names the empty choice.
// Abilities as rows of a name box and a description box, with buttons to move and remove each one,
// and "+ Add an ability". Stored as "Name: what it does". Returns true when the list changed.
bool abilityListEditor(std::vector<std::string>& abilities);
bool pathwayCombo(const char* label, const Database& db, std::string& pathwayId, const char* noneLabel);
// A slider from Sequence 9 (left) to Sequence 0 (right), labelled with the Sequence's name. Given
// `above`, it has one more step past Sequence 0: Above the Sequence.
bool sequenceSlider(const char* id, const Pathway* pathway, int& sequence, bool* above = nullptr);
// Sefirot from sefirot.json. noneLabel names the empty choice.
bool sefirahCombo(const char* label, const Database& db, std::string& sefirahId, const char* noneLabel);
// The saved characters, to pick one without typing its name: a list with each one's
// medallion, under a search box. Returns the picked character's id, or 0.
int savedCharacterList(const WindowState& w, std::string& search, int excludeId);

// The search box over a character or Sealed Artifact list, and a "Filter and sort" button for power,
// first letter, pathway and sort order. hint is the search box's grey text, bands are the power bands offered, names the list's names
// (for the letters in use). Under it, "Showing 3 of 12" while a filter is on.
void listFilterControls(const WindowState& w, RecordFilter& filter, const char* hint, const std::vector<int>& bands,
                        const std::vector<std::string>& names, size_t shown, size_t total);

// A section heading in the title font, with a gold rule running to the right.
void heading(const WindowState& w, const std::string& text);
void drawSheet(const WindowState& w, const Sheet& sheet);

// ---- decoration (widgets.cpp)

// "Klein Moretti" -> "KM"
std::string initials(const std::string& name);
// A round badge with a coloured ring and a short text inside (initials, a Sequence number).
void medallion(const WindowState& w, const std::string& text, const ImVec4& colour, float radius);
// A small rounded label in a colour. Call ImGui::SameLine() between chips.
void chip(const std::string& text, const ImVec4& colour);
// A coloured dot in front of the next text on the line.
void statusDot(const ImVec4& colour);
// One row of a list on the left: a medallion with `badge`, the title, and a muted line below.
// Returns true when clicked. `id` must be unique in the list.
bool listCard(const WindowState& w, const std::string& id, bool selected, const ImVec4& colour,
              const std::string& badge, const std::string& title, const std::string& subtitle);
// The six stats as boxes: the total large, the modifier below, base and bonus small.
void statBoxes(const WindowState& w, const std::vector<StatRow>& stats);
// The six stats on one chart with six spokes, one outline per set of totals, in the Compare colours.
void statChart(const std::vector<std::array<int, kStatCount>>& totals, float size);

// Save / Export / Delete row shared by the editors. Returns which button was pressed.
enum class EditorAction { None, Save, Revert, ExportHtml, ExportMarkdown, ExportText, Duplicate, Delete, Copy };
struct EditorButtons {
    bool dirty = false;
    bool isNew = false;
    bool canDuplicate = false;
    bool canDelete = true;
    std::string deleteBlockedReason;  // shown instead of deleting when not empty
};
EditorAction drawEditorButtons(const EditorButtons& options);

// Exports sheets, opens HTML exports in the browser, and reports the result on the status line.
void exportAndReport(WindowState& w, const std::string& baseName, ExportFormat format,
                     const std::vector<Sheet>& sheets, const std::string& title);

}  // namespace lotm::window
