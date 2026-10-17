// The app window: the same characters, organizations, Sealed Artifacts, pathways and Sefirot as the console program,
// edited with text boxes, dropdowns and sliders. Built with Dear ImGui, which redraws the whole
// window many times a second: every draw* function below runs once per frame.
#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>  // text boxes that edit a std::string
#include <nlohmann/json.hpp>

#include "app.hpp"
#include "checks.hpp"
#include "exporter.hpp"
#include "list_filter.hpp"
#include "model_json.hpp"
#include "records.hpp"
#include "sheet.hpp"
#include "web.hpp"

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
    int sheetStage = -1;         // the Sheet tab shows them as of this stage of theirs ...
    int sheetEvent = 0;          // ... or of their change at this story event; neither: as they are now
    int openStage = -1;          // the stage to unfold on the Stages tab next frame
    bool selectSheet = false;    // switch to the Sheet tab next frame
    bool editLadder = false;     // the Pathway tab shows a pathway box for every Sequence
    std::string switchTo;        // the pathway picked in "Switch to another pathway"
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
    bool organizations = false;  // comparing organizations instead of characters
    std::vector<int> ids;  // the characters being compared, in the order they were ticked (up to four)
    std::vector<int> organizationIds;  // ... or the organizations
    std::string search;
};

// The Organizations tab: your organizations on the left, the one open on the right, its members drawn
// around it as at the Tarot Club's long table.
struct OrganizationScreen {
    Draft<Organization> draft;
    std::string search;
    std::string memberSearch;  // in "Add a member"
    std::string newMemberRank;
    std::string addPathway;    // the pathway picked in "Add a pathway"
    std::string newRank;       // the half-typed rank in "add a rank"
    double openedAt = -100.0;  // when the open organization was picked, for its members to gather
    std::map<int, double> seatedAt;  // when each member took their seat at the table, by character id
    int hovered = 0;           // the member under the pointer at the table
};

// The Web tab: everyone as a circle, every relationship a line, settling into place as you watch.
struct WebScreen {
    WebOptions options;
    bool labels = true;           // relationship words on the lines (when zoomed in far enough)
    Web web;
    WebLayout layout;
    std::string builtFrom = "\x01";  // what the web was built from; when it changes the web is built again
    std::array<int, kBondCount> bondCounts{};  // lines of each kind, shown or not
    std::vector<std::string> organizations;    // to pick from, in the order first met
    std::map<std::string, double> bornAt;  // when each person first appeared, for growing in
    // The view: the point of the web in the middle of the canvas, and pixels per unit of the layout.
    // `shown` glides towards `target`.
    ImVec2 targetCentre{0.0f, 0.0f};
    float targetScale = 0.0f;  // 0: not placed yet
    ImVec2 shownCentre{0.0f, 0.0f};
    float shownScale = 0.0f;
    bool follow = true;           // keep the whole web in view while it settles, until moved by hand
    int dragging = -1;            // the node being dragged
    ImVec2 grabOffset{0.0f, 0.0f};  // from the pointer to the dragged node's centre, in layout units
    std::string selected;         // Web::key of the person picked
    std::string hovered;          // ... and of the one under the pointer, kept while the highlight fades
    float focus = 0.0f;           // 0..1, how far the others have faded while someone is hovered
    int pathFrom = 0;             // "How are they connected?": two saved characters
    int pathTo = 0;
};

// The Timeline tab: story events by date, and the cast as they were on any date.
struct TimelineScreen {
    Draft<StoryEvent> draft;
    bool selectEditor = false;  // show the event being edited on the next frame
    std::string search;
    int characterId = 0;   // only the events with this character
    std::string kind;      // only events of this kind
    // The story strip's "as of" marker: an index into storyDates(), or -1 for now. It glides to its place.
    int asOf = -1;
    float shownAsOf = -1.0f;
    double asOfChangedAt = -10.0;
    bool playing = false;  // stepping through the dates by itself
    double playedAt = 0.0;
    int viewingCharacter = 0;  // a cast card opened as a sheet, as of the marker's date
    int changingPerson = -1;   // the event's person whose change is open (an index into its people)
    std::map<int, double> diedAt;  // when + Dies here was pressed for a person (by index), so the words fade in
    // What each character's card said last frame, and when it changed, so a change lights up.
    std::map<int, std::string> castLines;
    std::map<int, double> castChangedAt;
};

// The Dice tab: checks, contests and any dice, thrown onto a tray.
struct ThrownDie {
    int sides = 20;
    int value = 1;
    int side = 0;        // in a contest, whose die it is (0 or 1)
    bool kept = true;    // false: the d20 that advantage or disadvantage set aside
    float delay = 0.0f;  // seconds after the throw that it sets off
};

struct DiceThrow {
    std::vector<ThrownDie> dice;
    double at = -100.0;  // ImGui::GetTime() when thrown
    unsigned seed = 1;   // for the faces that flicker past and the way each die tumbles
    // What it came to: one of these.
    std::optional<CheckOutcome> check;
    std::optional<ContestOutcome> contest;
    std::optional<DiceRoll> free;
    double landsAt() const;  // when the last die comes to rest
};

// The colour a roll is listed in, looked up in the theme when drawn (it can change in between).
enum class DiceTone { Gold, Success, Danger, Muted, First, Second };

struct DiceLogEntry {
    std::string headline;  // "Success by 4"
    std::string text;      // the whole roll in words
    DiceTone tone = DiceTone::Gold;
    double showAt = 0.0;   // once its dice have landed
};

struct DiceScreen {
    enum Mode { Check, Contest, AnyDice };
    int mode = Check;
    struct Side {
        int characterId = 0;  // 0: nobody in particular
        CheckSetup setup;
        std::string search;   // in the "Who" list
    };
    std::array<Side, 2> sides;  // a check uses the first; a contest both
    int difficulty = 15;
    std::string formula = "2d6";
    std::string formulaProblem;
    DiceThrow last;
    std::vector<DiceLogEntry> log;  // newest first
    bool rollNow = false;  // throw on the next frame (the Check buttons on a character's Stats tab)
};

enum class Tab { Characters, Compare, Web, Timeline, Dice, Artifacts, Organizations, Pathways, Sefirot, Settings };

struct WindowState {
    App app{Database{}, Storage{std::filesystem::path()}, Dice{}};  // loaded once the window is open
    Fonts fonts;

    Tab tab = Tab::Characters;
    std::optional<Tab> switchTo;  // set to change tabs from code
    CharacterScreen characters;
    CompareScreen compare;
    WebScreen web;
    TimelineScreen timeline;
    DiceScreen dice;
    ArtifactScreen artifacts;
    OrganizationScreen organizations;
    PathwayScreen pathways;
    SefirotScreen sefirot;

    // Sample sets in data/samples, read the first time the Settings tab shows them.
    std::optional<std::vector<SampleSet>> sampleSets;
    std::string sampleSetsProblem;
    bool showSampleSets = false;  // scroll the Settings tab down to them on the next frame
    // Settings > Share your own: packing your characters, organizations, Sealed Artifacts and story
    // events into a sample set.
    struct Share {
        std::string title;
        std::string description;
        SharePicks picks;
        bool touched = false;  // ticked by hand, so a new title no longer changes the ticks
        std::string pickedFor = "\x01";  // the file the default ticks were made for
    } share;

    // The line at the bottom of the window: the result of the last save, export or error.
    std::string status;
    bool statusIsError = false;
    double statusAt = -100.0;  // when it was set, for fading it in
    double tabShownAt = 0.0;  // when the tab on show changed (or the window opened), for fading the screen in

    // "You have unsaved changes" question. `afterwards` runs once the user saves or discards.
    bool askUnsaved = false;
    std::function<bool()> saveCurrent;
    std::function<void()> afterwards;
    bool quitRequested = false;
};

// ---------------------------------------------------------------- screens

void drawCharactersScreen(WindowState& w);
void drawCompareScreen(WindowState& w);
void drawWebScreen(WindowState& w);
void drawTimelineScreen(WindowState& w);
void drawDiceScreen(WindowState& w);
void drawArtifactsScreen(WindowState& w);
void drawOrganizationsScreen(WindowState& w);
void drawPathwaysScreen(WindowState& w);
void drawSefirotScreen(WindowState& w);
void drawSettingsScreen(WindowState& w);

// Save the record open on each screen. On failure they return false and say why on the status line.
bool saveCharacterDraft(WindowState& w);
bool saveArtifactDraft(WindowState& w);
bool savePathwayDraft(WindowState& w);
bool saveSefirahDraft(WindowState& w);
bool saveEventDraft(WindowState& w);
bool saveOrganizationDraft(WindowState& w);

// Opens a saved character on the Characters tab (asking first about unsaved changes there).
void openCharacter(WindowState& w, int id);
// A stage's fields (all but its date), for the Stages tab and an event's People tab. `key` tells
// stages apart (each list's half-typed entry is kept by it).
void stageFields(WindowState& w, const Character& c, CharacterStage& stage, const std::string& key);
// Opens a saved organization on the Organizations tab (asking first about unsaved changes there).
void openOrganization(WindowState& w, int id);
// Starts a new organization on the Organizations tab, filled in from the characters who already name it
// (their ranks), or empty when nobody does.
void openNewOrganization(WindowState& w, const std::string& name = "");
// An organization's colour: its first pathway's, its leader's, or one made from its name.
ImVec4 organizationColour(const Database& db, const Organization& o);
// Opens a story event on the Timeline tab, or a new one with these people in it.
void openEvent(WindowState& w, int id);
void openNewEvent(WindowState& w, const std::vector<int>& characterIds = {});
// Opens the Dice tab on a check for this character (kind: a stat, or kSpeedCheck) and rolls it. The roll
// uses the character as they are on the Characters tab, unsaved changes included.
void openCheck(WindowState& w, int characterId, int kind);
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
// A date in the story, typed as text ("3 July 1360"). Says below how it reads, or what is wrong with it;
// the date only changes while the text reads as one.
bool storyDateField(const char* label, StoryDate& date, const char* hint = "e.g. 3 July 1360, or just 1360");
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
// A colour written as hex ("#d4577b"), darkened on the parchment theme so it reads.
ImVec4 colourFromHex(const std::string& hex);
// A medallion drawn at a point (initials, a Sequence number), for screens that place their own.
// colour.w fades the whole thing.
void medallionAt(const WindowState& w, ImVec2 centre, float radius, const ImVec4& colour, const std::string& text);

// ---- movement (widgets.cpp)

// Settings > This window > Animations. While off, glide and popIn arrive at once.
void setAnimations(bool on);
bool animations();
// An ImU32 colour with its alpha multiplied (and the window's fade, like every ImGui colour).
inline ImU32 colourU32(const ImVec4& c, float alpha = 1.0f) {
    return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha));
}

// A number that glides towards `target` instead of jumping, remembered under `id` between frames.
// `speed`: about how many times a second it closes the gap.
float glide(ImGuiID id, float target, float speed = 10.0f);
// The same, starting from `start` the first time it is asked for (a chart growing out of its middle).
float glideFrom(ImGuiID id, float start, float target, float speed = 10.0f);
// From 0 to 1 over `seconds` after `since`, easing out without overshooting: fades and slides.
float easeIn(double since, float seconds = 0.3f);
// From 0 to 1 over `seconds` after `since` (ImGui::GetTime()), easing out with a small overshoot.
float popIn(double since, float seconds = 0.45f);
// Seconds since the last frame, never more than a tenth of a second (after a pause, nothing jumps).
float frameSeconds();
// A round badge with a coloured ring and a short text inside (initials, a Sequence number).
void medallion(const WindowState& w, const std::string& text, const ImVec4& colour, float radius);
// A small rounded label in a colour. Call ImGui::SameLine() between chips.
void chip(const std::string& text, const ImVec4& colour);
// A coloured dot in front of the next text on the line.
void statusDot(const ImVec4& colour);
// Extra marks on a list card's medallion.
struct CardMarks {
    std::vector<ImVec4> ring;  // the ring in these colours, in equal parts: a mixed-pathway Beyonder's Sequences
    bool dot = false;          // a small dot at the top right in dotColour: their state of mind
    ImVec4 dotColour;
    bool dead = false;         // greyed, with a mourning band across the corner (see mourningBand)
};
// Someone who has died: the colour of a Death on the Timeline, a grey violet.
ImVec4 deathColour();
// A colour faded towards grey, by `amount` from 0 (as it is) to 1 (grey), keeping its alpha.
ImVec4 mourned(const ImVec4& colour, float amount);
// A dark band across the top right of a medallion, as on a portrait in mourning. `amount` (0 to 1) grows
// it out from its middle.
void mourningBand(ImDrawList* draw, ImVec2 centre, float radius, float amount, float alpha = 1.0f);
// One row of a list on the left: a medallion with `badge`, the title, and a muted line below.
// Returns true when clicked. `id` must be unique in the list.
bool listCard(const WindowState& w, const std::string& id, bool selected, const ImVec4& colour,
              const std::string& badge, const std::string& title, const std::string& subtitle,
              const CardMarks& marks = {});
// Chips on as many lines as they need.
void chipRow(const std::vector<std::pair<std::string, ImVec4>>& chips);
// A row of buttons of equal width across the space left, the selected one lit in gold. Returns the one
// pressed, or -1. `tips` are their tooltips, in order.
int segmented(const char* id, const std::vector<std::string>& labels, int selected, const std::vector<std::string>& tips = {});
// The colour a sheet's badge is drawn in, and a row of them.
ImVec4 badgeColour(const WindowState& w, const Badge& badge);
void badgeRow(const WindowState& w, const std::vector<Badge>& badges);
// A state of mind's colour: green for Sane through red for Mad and Lost control; muted for your own words.
ImVec4 stateOfMindColour(const std::string& state);
// The Sequence ladder: a medallion for each Sequence from 9 up to theirs in the colour of the pathway it
// came from, joined by a line that turns dashed at a switch of pathway (red when not a neighbour), with
// the pathways named below.
void sequenceLadder(const WindowState& w, const std::vector<LadderRung>& rungs);
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
