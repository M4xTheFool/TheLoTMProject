// Sefirot tab: the nine Sefirot of the story and your own on the left; on the right what one is, the
// pathways whose Uniquenesses it gathers, who held it in the story, and which of your characters hold
// it. Your own Sefirot (custom_sefirot.json) are edited here; the story's can be copied.
#include <algorithm>

#include "records.hpp"
#include "rules.hpp"
#include "storage.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

// Saves the character holding the Sefirah (an empty id takes it away). The Sefirah is the only
// change, so the character must not have unsaved changes in the Characters tab.
void setHolder(WindowState& w, int characterId, const std::string& sefirahId) {
    const Character* saved = w.app.db.findCharacter(characterId);
    if (!saved) return;
    Character c = *saved;
    const std::string before = c.sefirahId;
    c.sefirahId = sefirahId;
    try {
        saveCharacter(w.app, c, false);
        reloadCharacterDraft(w);
        std::string text = sefirahId.empty() ? c.name + " no longer holds " + sefirahLabel(w.app.db, before) + "."
                                             : c.name + " holds " + sefirahLabel(w.app.db, sefirahId) + " now.";
        if (!sefirahId.empty() && !before.empty()) text += " They held " + sefirahLabel(w.app.db, before) + " before.";
        setStatus(w, text);
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
    }
}

ImVec4 sefirahColour(const WindowState& w, const Sefirah& s) {
    return pathwayColour(w.app.db, s.pathwayIds.empty() ? std::string() : s.pathwayIds.front());
}

void exportPopup(WindowState& w, const Sefirah& s) {
    if (!ImGui::BeginPopup("export")) return;
    const std::vector<Sheet> sheets = {buildSefirahSheet(s, w.app.db)};
    if (ImGui::Selectable("HTML (opens in your browser, prints to PDF)")) {
        exportAndReport(w, exportName(s), ExportFormat::Html, sheets, s.name);
    }
    if (ImGui::Selectable("Markdown (Discord, Obsidian, wikis)")) {
        exportAndReport(w, exportName(s), ExportFormat::Markdown, sheets, s.name);
    }
    if (ImGui::Selectable("Plain text")) exportAndReport(w, exportName(s), ExportFormat::Text, sheets, s.name);
    ImGui::EndPopup();
}

// The medallion, name and Great Old One at the top of the right side.
void sefirahHeader(WindowState& w, const Sefirah& s) {
    const float font = ImGui::GetFontSize();
    medallion(w, initials(s.name.empty() ? "?" : s.name), sefirahColour(w, s), font * 2.1f);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextUnformatted(s.name.empty() ? "(unnamed Sefirah)" : s.name.c_str());
    ImGui::PopFont();
    std::string subtitle = s.custom ? "Your own Sefirah" : "Sefirah";
    if (!s.greatOldOne.empty()) subtitle += ". Great Old One: " + s.greatOldOne;
    ImGui::TextColored(palette().muted, "%s", subtitle.c_str());
}

void pathwayChips(const Database& db, const Sefirah& s) {
    for (size_t i = 0; i < s.pathwayIds.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        const Pathway* p = db.findPathway(s.pathwayIds[i]);
        chip(p ? p->name + (p->god.empty() ? "" : " (" + p->god + ")") : s.pathwayIds[i] + " (not found)",
             pathwayColour(db, s.pathwayIds[i]));
    }
    if (s.pathwayIds.empty()) ImGui::TextDisabled("No pathways yet.");
}

// Which of your saved characters hold it, with buttons to give it to one or take it away.
void yourHolders(WindowState& w, const Sefirah& s) {
    const Database& db = w.app.db;
    heading(w, "Your characters who hold it");
    const bool unsaved = hasUnsavedChanges(w);
    const auto holders = db.sefirahHolders(s.id);
    if (holders.empty()) ImGui::TextDisabled("None yet.");
    int takeAway = 0;
    for (const Character* c : holders) {
        ImGui::PushID(c->id);
        medallion(w, initials(c->name), pathwayColour(db, c->pathwayId), ImGui::GetFrameHeight() * 0.5f);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(c->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s, %s", characterCode(c->id).c_str(), levelName(*c).c_str());
        ImGui::SameLine();
        if (ImGui::TextLink("Open")) openCharacter(w, c->id);
        ImGui::SameLine();
        ImGui::BeginDisabled(unsaved);
        if (ImGui::SmallButton("Take it away")) takeAway = c->id;
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if (takeAway != 0) setHolder(w, takeAway, "");

    ImGui::BeginDisabled(unsaved || db.characters.empty());
    if (ImGui::Button("+ Make one of your characters its holder")) ImGui::OpenPopup("pickHolder");
    ImGui::EndDisabled();
    if (unsaved) {
        ImGui::SameLine();
        ImGui::TextDisabled("Save or undo the changes you're making first.");
    }
    if (ImGui::BeginPopup("pickHolder")) {
        static std::string search;
        ImGui::TextDisabled("A character holds one Sefirah at a time.");
        if (const int id = savedCharacterList(w, search, 0)) {
            setHolder(w, id, s.id);
            search.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// The Great Old One's step past Sequence 0, read-only: what it symbolizes and its abilities.
void aboveTheSequence(WindowState& w, const Sefirah& s) {
    heading(w, "Above the Sequence");
    if (!s.greatOldOne.empty()) {
        ImGui::TextColored(palette().gold, "%s", s.greatOldOne.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("its Great Old One");
    }
    if (!s.symbolism.empty()) {
        ImGui::TextColored(palette().muted, "Symbolism");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", s.symbolism.c_str());
    }
    if (s.abilities.empty()) ImGui::TextDisabled("No Great Old One abilities written yet.");
    for (const auto& a : s.abilities) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", a.c_str());
    }
}

void openDraft(WindowState& w, const Sefirah& s, bool isNew) {
    w.sefirot.selectedId.clear();
    w.sefirot.draft.start(s, isNew);
}

void openNew(WindowState& w) {
    Sefirah s;
    s.custom = true;
    openDraft(w, s, true);
}

void openCopy(WindowState& w, const Sefirah& from) {
    Sefirah s = from;
    s.id.clear();
    s.name += " (copy)";
    s.custom = true;
    openDraft(w, s, true);
}

bool saveImpl(WindowState& w) {
    auto& draft = w.sefirot.draft;
    Sefirah& s = draft.value;
    s.name = ui::trim(s.name);
    if (s.name.empty()) {
        setStatus(w, "Give the Sefirah a name before saving.", true);
        return false;
    }
    s.symbolism = ui::trim(s.symbolism);
    for (auto* list : {&s.alsoKnownAs, &s.knownHolders, &s.abilities}) {
        list->erase(std::remove_if(list->begin(), list->end(), [](const std::string& x) { return ui::trim(x).empty(); }),
                    list->end());
    }
    try {
        saveCustomSefirah(w.app, s, draft.isNew);
        setStatus(w, "Saved " + s.name + ". Characters can hold it now.");
        openDraft(w, s, false);
        return true;
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
        return false;
    }
}

// Tick the pathways it gathers, listed under their groups.
void pathwayPicker(const Database& db, Sefirah& s) {
    std::string group = "\x01";
    for (const auto& p : db.pathways) {
        const std::string thisGroup = p.custom ? "Your own pathways" : p.group;
        if (thisGroup != group) {
            group = thisGroup;
            ImGui::TextDisabled("%s", group.c_str());
        }
        auto& ids = s.pathwayIds;
        bool on = std::find(ids.begin(), ids.end(), p.id) != ids.end();
        statusDot(pathwayColour(db, p.id));
        if (ImGui::Checkbox((pathwayLabel(db, p.id) + "##" + p.id).c_str(), &on)) {
            if (on) ids.push_back(p.id);
            else ids.erase(std::remove(ids.begin(), ids.end(), p.id), ids.end());
        }
        if (const Sefirah* other = on ? db.otherSefirahFor(p.id, s.id) : nullptr) {
            ImGui::SameLine();
            ImGui::TextColored(palette().warning, "also gathered by %s", other->name.c_str());
        }
    }
}

void drawEditor(WindowState& w) {
    auto& draft = w.sefirot.draft;
    Sefirah& s = draft.value;
    sefirahHeader(w, s);
    EditorButtons buttons;
    buttons.dirty = draft.dirty();
    buttons.isNew = draft.isNew;
    const Sheet sheet = buildSefirahSheet(s, w.app.db);
    switch (drawEditorButtons(buttons)) {
        case EditorAction::Save: saveImpl(w); break;
        case EditorAction::Revert:
            if (draft.isNew) draft.close();
            else if (const Sefirah* saved = w.app.db.findSefirah(s.id)) openDraft(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, exportName(s), ExportFormat::Html, {sheet}, s.name); break;
        case EditorAction::ExportMarkdown:
            exportAndReport(w, exportName(s), ExportFormat::Markdown, {sheet}, s.name);
            break;
        case EditorAction::ExportText: exportAndReport(w, exportName(s), ExportFormat::Text, {sheet}, s.name); break;
        case EditorAction::Delete:
            try {
                const std::string name = s.name;
                const size_t held = w.app.db.sefirahHolders(s.id).size();
                deleteCustomSefirah(w.app, s.id);
                draft.close();
                reloadCharacterDraft(w);
                std::string text = "Deleted " + name + ".";
                if (held > 0) text += " The characters who held it no longer hold a Sefirah.";
                if (w.app.db.settings.backupsToKeep > 0) text += " A copy is in the backups folder.";
                setStatus(w, text);
            } catch (const StorageError& e) {
                setStatus(w, std::string("Deleting failed: ") + e.what(), true);
            }
            break;
        default: break;
    }
    ImGui::EndGroup();
    if (!draft.open) return;

    ImGui::Spacing();
    if (ImGui::BeginTabBar("sefirahTabs")) {
        if (ImGui::BeginTabItem("Details", nullptr, draft.firstTabFlags())) {
            ImGui::BeginChild("tab");
            draft.focusNameOnce();
            textField("Name", s.name, "e.g. Sefirah of Masks");
            textField("Great Old One", s.greatOldOne, "who holds it with every Uniqueness of its pathways");
            ImGui::SeparatorText("Also known as");
            listEditor("aka", s.alsoKnownAs, "add another name");
            ImGui::SeparatorText("Pathways it gathers");
            ImGui::TextDisabled("Holding the Sefirah and the Uniqueness of each of these makes a Great Old One.");
            pathwayPicker(w.app.db, s);
            ImGui::SeparatorText("Above the Sequence");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("The step past Sequence 0. A character Above the Sequence who holds this Sefirah "
                                "is its Great Old One and has these abilities.");
            ImGui::PopTextWrapPos();
            multilineField("Symbolism: what the Great Old One stands for", s.symbolism, 2);
            ImGui::TextUnformatted("Great Old One abilities");
            ImGui::PushID("gooAbilities");
            abilityListEditor(s.abilities);
            ImGui::PopID();
            ImGui::SeparatorText("What it is");
            multilineField("Description", s.description, 5);
            ImGui::SeparatorText("Known holders in the story");
            listEditor("holders", s.knownHolders, "add a holder, one line each");
            ImGui::SeparatorText("Notes");
            multilineField("Guesses, sources, ideas ...", s.notes, 3);
            if (!draft.isNew) {
                ImGui::Spacing();
                yourHolders(w, s);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sheet")) {
            ImGui::BeginChild("tab");
            drawSheet(w, sheet);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// A Sefirah from sefirot.json, read-only.
void drawDetails(WindowState& w, const Sefirah& s) {
    const Database& db = w.app.db;
    sefirahHeader(w, s);
    if (ImGui::Button("Copy into a new Sefirah of your own")) openCopy(w, s);
    ImGui::SetItemTooltip("The Sefirot of the story are read-only here. A copy can be changed freely.");
    ImGui::SameLine();
    if (ImGui::Button("Export...")) ImGui::OpenPopup("export");
    exportPopup(w, s);
    ImGui::EndGroup();

    ImGui::Spacing();
    heading(w, "Pathways");
    ImGui::TextDisabled("Holding this Sefirah and the Uniqueness of every one of these makes a Great Old One.");
    pathwayChips(db, s);

    ImGui::Spacing();
    heading(w, "What it is");
    if (!s.alsoKnownAs.empty()) {
        std::string names;
        for (const auto& name : s.alsoKnownAs) names += (names.empty() ? "" : ", ") + name;
        ImGui::TextColored(palette().muted, "Also known as");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", names.c_str());
    }
    ImGui::TextWrapped("%s", s.description.c_str());

    ImGui::Spacing();
    aboveTheSequence(w, s);

    if (!s.knownHolders.empty()) {
        ImGui::Spacing();
        heading(w, "Known holders in the story");
        for (const auto& line : s.knownHolders) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", line.c_str());
        }
    }

    ImGui::Spacing();
    yourHolders(w, s);

    if (!s.notes.empty()) {
        ImGui::Spacing();
        heading(w, "Notes");
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("%s", s.notes.c_str());
        ImGui::PopStyleColor();
    }
}

}  // namespace

bool saveSefirahDraft(WindowState& w) { return saveImpl(w); }

void openNewSefirah(WindowState& w, const std::string& pathwayId) {
    whenSaved(w, w.sefirot.draft.dirty(), [&w] { return saveImpl(w); }, [&w, pathwayId] {
        Sefirah s;
        s.custom = true;
        s.pathwayIds = {pathwayId};
        openDraft(w, s, true);
        w.switchTo = Tab::Sefirot;
    });
}

void openSefirah(WindowState& w, const std::string& id) {
    whenSaved(w, w.sefirot.draft.dirty(), [&w] { return saveImpl(w); }, [&w, id] {
        const Sefirah* found = w.app.db.findSefirah(id);
        if (!found) return;
        if (found->custom) {
            openDraft(w, *found, false);
        } else {
            w.sefirot.draft.close();
            w.sefirot.selectedId = id;
        }
        w.switchTo = Tab::Sefirot;
    });
}

void drawSefirotScreen(WindowState& w) {
    auto& screen = w.sefirot;
    auto& draft = screen.draft;
    const Database& db = w.app.db;
    auto saveThis = [&w] { return saveImpl(w); };
    if (!draft.open && !db.findSefirah(screen.selectedId) && !db.sefirot.empty()) {
        screen.selectedId = db.sefirot.front().id;
    }

    // ---- the list: the story's Sefirot, then your own
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (ImGui::Button("+ New Sefirah", ImVec2(-FLT_MIN, 0))) {
        whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w); });
    }
    ImGui::TextWrapped("The sources of power and authority behind each group of pathways.");
    ImGui::Separator();
    if (db.sefirot.empty()) ImGui::TextDisabled("No Sefirot: sefirot.json is missing from the data folder.");
    bool ownHeading = false;
    for (const auto& s : db.sefirot) {
        if (s.custom && !ownHeading) {
            ImGui::SeparatorText("Your own Sefirot");
            ownHeading = true;
        }
        const size_t held = db.sefirahHolders(s.id).size();
        std::string subtitle = s.greatOldOne.empty() ? "Sefirah" : s.greatOldOne;
        if (held > 0) subtitle += "  \xC2\xB7  held by " + std::to_string(held);
        const bool selected = s.custom ? (draft.open && !draft.isNew && draft.value.id == s.id)
                                       : (!draft.open && s.id == screen.selectedId);
        if (listCard(w, "s" + s.id, selected, sefirahColour(w, s), initials(s.name), s.name, subtitle) && !selected) {
            const std::string id = s.id;
            const bool custom = s.custom;
            whenSaved(w, draft.dirty(), saveThis, [&w, id, custom] {
                const Sefirah* found = w.app.db.findSefirah(id);
                if (!found) return;
                if (custom) {
                    openDraft(w, *found, false);
                } else {
                    w.sefirot.draft.close();
                    w.sefirot.selectedId = id;
                }
            });
        }
    }
    if (draft.open && draft.isNew) {
        if (!ownHeading) ImGui::SeparatorText("Your own Sefirot");
        ImGui::Selectable("(new Sefirah)", true);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the details, or the editor for one of your own
    ImGui::BeginChild("details", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (draft.open) drawEditor(w);
    else if (const Sefirah* s = db.findSefirah(screen.selectedId)) drawDetails(w, *s);
    ImGui::EndChild();
}

}  // namespace lotm::window
