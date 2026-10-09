// The app window: the same characters, Sealed Artifacts and pathways as the console program,
// edited with text boxes, dropdowns and sliders. Built with Dear ImGui, which redraws the whole
// window many times a second: every draw* function below runs once per frame.
#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_stdlib.h>  // text boxes that edit a std::string
#include <nlohmann/json.hpp>

#include "app.hpp"
#include "exporter.hpp"
#include "model_json.hpp"
#include "sheet.hpp"

namespace lotm::window {

// The window's colours, close to the HTML exports: parchment text, gold accents.
inline const ImVec4 kGold{0.85f, 0.68f, 0.33f, 1.0f};
inline const ImVec4 kMuted{0.62f, 0.59f, 0.53f, 1.0f};
inline const ImVec4 kDanger{0.86f, 0.38f, 0.33f, 1.0f};
inline const ImVec4 kWarning{0.93f, 0.72f, 0.36f, 1.0f};
inline const ImVec4 kSuccess{0.53f, 0.76f, 0.52f, 1.0f};

struct Fonts {
    ImFont* body = nullptr;
    ImFont* bold = nullptr;
    ImFont* heading = nullptr;  // serif, for titles and section headings
};

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
    std::string search;
    std::array<std::string, kStatCount> lastRolls;  // "6, 4, 3, (1) = 13" shown next to each stat
    std::string lastSpeedRoll;
};

struct ArtifactScreen {
    Draft<Artifact> draft;
    std::string search;
};

struct PathwayScreen {
    Draft<Pathway> draft;
    std::string viewingId;  // a built-in pathway shown read-only
    std::string search;
};

enum class Tab { Characters, Artifacts, Pathways, Settings };

struct WindowState {
    App app{Database{}, Storage{std::filesystem::path()}, Dice{}};  // loaded once the window is open
    Fonts fonts;

    Tab tab = Tab::Characters;
    std::optional<Tab> switchTo;  // set to change tabs from code
    CharacterScreen characters;
    ArtifactScreen artifacts;
    PathwayScreen pathways;

    // Sample sets in data/samples, read the first time the Settings tab shows them.
    std::optional<std::vector<SampleSet>> sampleSets;
    std::string sampleSetsProblem;
    bool showSampleSets = false;  // scroll the Settings tab down to them on the next frame

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
void drawArtifactsScreen(WindowState& w);
void drawPathwaysScreen(WindowState& w);
void drawSettingsScreen(WindowState& w);

// Save the record open on each screen. On failure they return false and say why on the status line.
bool saveCharacterDraft(WindowState& w);
bool saveArtifactDraft(WindowState& w);
bool savePathwayDraft(WindowState& w);

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
bool pathwayCombo(const char* label, const Database& db, std::string& pathwayId, const char* noneLabel);
// A slider from Sequence 9 (left) to Sequence 0 (right), labelled with the Sequence's name.
bool sequenceSlider(const char* id, const Pathway* pathway, int& sequence);

void heading(const WindowState& w, const std::string& text);
void drawSheet(const WindowState& w, const Sheet& sheet);

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
