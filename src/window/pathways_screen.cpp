// Pathways tab: every pathway on the left. Built-in ones can be read and copied; your own
// pathways (custom_pathways.json) can be edited Sequence by Sequence.
#include <algorithm>

#include "presets.hpp"
#include "records.hpp"
#include "storage.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

// An ability with only a name, or only a description, is stored as that text alone. While it is
// being edited, the box it was typed into is remembered so the text doesn't jump to the other box.
enum AbilityShape { kGuess = 0, kNameOnly = 1, kDescriptionOnly = 2 };

std::pair<std::string, std::string> splitForEditing(const std::string& ability, int shape) {
    if (ability.find(": ") == std::string::npos) {
        if (shape == kNameOnly) return {ability, ""};
        if (shape == kDescriptionOnly) return {"", ability};
        // A short phrase with no full stop is most likely a name.
        const bool looksLikeName = !ability.empty() && ability.size() <= 48 &&
                                   ability.find_first_of(".!?") == std::string::npos;
        if (looksLikeName) return {ability, ""};
    }
    return splitAbility(ability);
}

void open(WindowState& w, const Pathway& p, bool isNew) {
    w.pathways.viewingId.clear();
    w.pathways.draft.start(p, isNew);
}

void openNew(WindowState& w) {
    Pathway p;
    p.custom = true;
    for (int seq = 9; seq >= 0; --seq) p.sequences.push_back({seq, "", {}});
    open(w, p, true);
}

void openCopy(WindowState& w, const Pathway& from) {
    Pathway p = from;
    p.id.clear();
    p.name += " (copy)";
    p.custom = true;
    open(w, p, true);
}

bool saveImpl(WindowState& w) {
    auto& draft = w.pathways.draft;
    Pathway& p = draft.value;
    p.name = ui::trim(p.name);
    if (p.name.empty()) {
        setStatus(w, "Give the pathway a name before saving (Overview tab).", true);
        return false;
    }
    for (auto& s : p.sequences) {
        s.name = ui::trim(s.name);
        auto& list = s.abilities;
        list.erase(std::remove_if(list.begin(), list.end(), [](const std::string& a) { return ui::trim(a).empty(); }),
                   list.end());
        if (s.sequence == 9 && s.name.empty()) s.name = p.name;
    }
    try {
        saveCustomPathway(w.app, p, draft.isNew);
        setStatus(w, "Saved the " + p.name + " pathway. It's in every pathway list now.");
        open(w, p, false);
        return true;
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
        return false;
    }
}

bool statCombo(const char* label, std::string& code) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const int current = statIndex(code);
    const std::string preview = current < 0 ? "None" : std::string(kStatNames[current]) + " (" + kStatCodes[current] + ")";
    bool changed = false;
    if (ImGui::BeginCombo((std::string("##") + label).c_str(), preview.c_str())) {
        if (ImGui::Selectable("None", current < 0)) {
            code.clear();
            changed = true;
        }
        for (int i = 0; i < kStatCount; ++i) {
            const std::string name = std::string(kStatNames[i]) + " (" + kStatCodes[i] + ")";
            if (ImGui::Selectable(name.c_str(), i == current)) {
                code = kStatCodes[i];
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void overviewTab(WindowState& w, Pathway& p) {
    w.pathways.draft.focusNameOnce();
    textField("Name", p.name, "usually the Sequence 9 name, like Seer");
    textField("God", p.god, "the one at Sequence 0, like The Fool");
    std::vector<std::string> groups;
    for (const auto& other : w.app.db.pathways) {
        if (!other.group.empty() && std::find(groups.begin(), groups.end(), other.group) == groups.end()) {
            groups.push_back(other.group);
        }
    }
    presetField("Neighbours", p.group, groups);
    helpMarker("The group of neighbouring pathways it belongs to. A pathway with no neighbours can name itself.");

    ImGui::SeparatorText("Stats and speed");
    statCombo("Primary stat", p.primaryStat);
    statCombo("Secondary stat", p.secondaryStat);
    ImGui::TextDisabled("The primary stat gets +1 per speed tier, the secondary +1 every two tiers.");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Speed grade");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    for (const auto& grade : kSpeedGrades) {
        if (ImGui::RadioButton(grade.c_str(), p.speedGrade == grade)) p.speedGrade = grade;
        ImGui::SameLine();
    }
    ImGui::NewLine();
    textField("Speed note", p.speedNote, "optional, e.g. counts as Fast at night");

    ImGui::SeparatorText("About it");
    multilineField("Overview: themes, Sefirah, Divine Kingdom, anything worth knowing", p.description, 5);
    multilineField("The Uniqueness: its name and what it looks like", p.uniqueness, 4);
    ImGui::TextDisabled("A Sequence 1 character who holds the Uniqueness can start from this description.");
}

void sequencesTab(Pathway& p) {
    ImGui::TextDisabled("Open a Sequence to name it and write its abilities.");
    ImGuiStorage* storage = ImGui::GetStateStorage();
    for (int seq = 9; seq >= 0; --seq) {
        SequenceInfo* s = nullptr;
        for (auto& candidate : p.sequences) {
            if (candidate.sequence == seq) s = &candidate;
        }
        if (!s) {
            p.sequences.push_back({seq, "", {}});
            std::sort(p.sequences.begin(), p.sequences.end(),
                      [](const SequenceInfo& a, const SequenceInfo& b) { return a.sequence > b.sequence; });
            return;  // drawn next frame
        }
        const size_t count = s->abilities.size();
        const std::string header = "Sequence " + std::to_string(seq) + ": " +
                                   (s->name.empty() ? std::string("(no name yet)") : s->name) + "  -  " +
                                   std::to_string(count) + (count == 1 ? " ability" : " abilities") + "###seq" +
                                   std::to_string(seq);
        ImGui::PushID(seq);
        if (ImGui::CollapsingHeader(header.c_str())) {
            ImGui::Indent();
            textField("Sequence name", s->name);
            int moveUp = -1, moveDown = -1, remove = -1;
            const ImGuiID focusRow = ImGui::GetID("focusRow");
            for (size_t i = 0; i < s->abilities.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                const ImGuiID shapeId = ImGui::GetID("shape");
                auto [name, description] = splitForEditing(s->abilities[i], storage->GetInt(shapeId, kGuess));
                const float buttons = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemInnerSpacing.x * 3.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons);
                if (storage->GetInt(focusRow, -1) == static_cast<int>(i)) {
                    ImGui::SetKeyboardFocusHere();
                    storage->SetInt(focusRow, -1);
                }
                const bool nameChanged = ImGui::InputTextWithHint("##name", "ability name", &name);
                const ImVec2 square(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("^", square)) moveUp = static_cast<int>(i);
                ImGui::SetItemTooltip("Move up");
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("v", square)) moveDown = static_cast<int>(i);
                ImGui::SetItemTooltip("Move down");
                ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("x", square)) remove = static_cast<int>(i);
                ImGui::SetItemTooltip("Remove");
                const float lines = description.size() > 300 ? 6.0f : 3.0f;
                const bool descriptionChanged = ImGui::InputTextMultiline(
                    "##what", &description,
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * lines + ImGui::GetStyle().FramePadding.y * 2.0f),
                    ImGuiInputTextFlags_WordWrap);
                if (nameChanged || descriptionChanged) {
                    s->abilities[i] = joinAbility(name, description);
                    const int shape = !description.empty() && name.empty() ? kDescriptionOnly
                                      : !name.empty() && description.empty() ? kNameOnly
                                                                              : kGuess;
                    storage->SetInt(shapeId, shape);
                }
                ImGui::Spacing();
                ImGui::PopID();
            }
            auto& list = s->abilities;
            const size_t before = list.size();
            if (moveUp > 0) std::swap(list[moveUp], list[moveUp - 1]);
            if (moveDown >= 0 && moveDown + 1 < static_cast<int>(list.size())) std::swap(list[moveDown], list[moveDown + 1]);
            if (remove >= 0) list.erase(list.begin() + remove);
            if (ImGui::Button("+ Add an ability")) {
                list.push_back("");
                storage->SetInt(focusRow, static_cast<int>(list.size()) - 1);
            }
            if (moveUp > 0 || moveDown >= 0 || list.size() != before) {
                // Rows changed places, so the remembered boxes no longer match them.
                for (size_t i = 0; i < std::max(before, list.size()); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    storage->SetInt(ImGui::GetID("shape"), kGuess);
                    ImGui::PopID();
                }
            }
            ImGui::Unindent();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }
}

// The top of one of your own pathways: a medallion in its group's colour, the name and god, and
// chips for its stats, speed and group.
void pathwayHeader(const WindowState& w, const Pathway& p) {
    const float font = ImGui::GetFontSize();
    const ImVec4 colour = pathwayColour(w.app.db, p.id);
    medallion(w, initials(p.name.empty() ? "?" : p.name), colour, font * 2.1f);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextUnformatted(p.name.empty() ? "(unnamed pathway)" : (p.name + " pathway").c_str());
    ImGui::PopFont();
    ImGui::TextColored(palette().muted, "%s", p.god.empty() ? "No god named yet" : ("Leads to " + p.god).c_str());
    if (!p.primaryStat.empty()) {
        chip("Primary " + p.primaryStat, colour);
        ImGui::SameLine();
    }
    if (!p.secondaryStat.empty()) {
        chip("Secondary " + p.secondaryStat, colour);
        ImGui::SameLine();
    }
    chip("Speed: " + p.speedGrade, colour);
    if (!p.group.empty()) {
        ImGui::SameLine();
        chip(p.group, palette().muted);
    }
    ImGui::EndGroup();
    ImGui::Spacing();
}

void drawBuiltIn(WindowState& w, const Pathway& p) {
    const Sheet sheet = buildPathwaySheet(p);
    if (ImGui::Button("Copy into a new pathway of your own")) openCopy(w, p);
    ImGui::SetItemTooltip("Built-in pathways are read-only here. A copy can be changed freely.");
    ImGui::SameLine();
    if (ImGui::Button("Export...")) ImGui::OpenPopup("export");
    if (ImGui::BeginPopup("export")) {
        const std::string title = p.name + " pathway";
        if (ImGui::Selectable("HTML (opens in your browser, prints to PDF)")) {
            exportAndReport(w, exportName(p), ExportFormat::Html, {sheet}, title);
        }
        if (ImGui::Selectable("Markdown (Discord, Obsidian, wikis)")) {
            exportAndReport(w, exportName(p), ExportFormat::Markdown, {sheet}, title);
        }
        if (ImGui::Selectable("Plain text")) exportAndReport(w, exportName(p), ExportFormat::Text, {sheet}, title);
        ImGui::EndPopup();
    }
    ImGui::Spacing();
    ImGui::BeginChild("sheet");
    drawSheet(w, sheet);
    ImGui::EndChild();
}

}  // namespace

bool savePathwayDraft(WindowState& w) { return saveImpl(w); }

void drawPathwaysScreen(WindowState& w) {
    auto& s = w.pathways;
    auto& draft = s.draft;
    auto saveThis = [&w] { return saveImpl(w); };

    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (ImGui::Button("+ New pathway", ImVec2(-FLT_MIN, 0))) {
        whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w); });
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search", &s.search);
    if (draft.open && draft.isNew) {
        ImGui::SeparatorText("Your own pathways");
        ImGui::Selectable("(new pathway)", true);
    }
    // Your own pathways first, then the built-in ones under their groups.
    for (const bool custom : {true, false}) {
        std::string group = "\x01";
        for (const auto& p : w.app.db.pathways) {
            if (p.custom != custom) continue;
            const std::string label = pathwayLabel(w.app.db, p.id);
            if (!s.search.empty() && ui::toLower(label).find(ui::toLower(s.search)) == std::string::npos) continue;
            const std::string thisGroup = custom ? "Your own pathways" : p.group;
            if (thisGroup != group) {
                group = thisGroup;
                if (!(draft.open && draft.isNew && custom)) ImGui::SeparatorText(group.c_str());
            }
            const bool selected = custom ? (draft.open && !draft.isNew && draft.value.id == p.id) : s.viewingId == p.id;
            statusDot(pathwayColour(w.app.db, p.id));
            if (ImGui::Selectable((label + "##" + p.id).c_str(), selected) && !selected) {
                const std::string id = p.id;
                whenSaved(w, draft.dirty(), saveThis, [&w, id, custom] {
                    const Pathway* found = w.app.db.findPathway(id);
                    if (!found) return;
                    if (custom) {
                        open(w, *found, false);
                    } else {
                        w.pathways.draft.close();
                        w.pathways.viewingId = id;
                    }
                });
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("editor", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!draft.open) {
        if (const Pathway* p = w.app.db.findPathway(s.viewingId)) {
            drawBuiltIn(w, *p);
        } else {
            ImGui::Spacing();
            ImGui::TextWrapped("Pick a pathway on the left to read it, or create a new one. Your own pathways can be "
                               "edited here; the built-in ones can be copied into a pathway of your own.");
        }
        ImGui::EndChild();
        return;
    }

    Pathway& p = draft.value;
    pathwayHeader(w, p);

    EditorButtons buttons;
    buttons.dirty = draft.dirty();
    buttons.isNew = draft.isNew;
    const auto users = draft.isNew ? std::vector<std::string>{} : w.app.db.pathwayUsers(p.id);
    if (!users.empty()) {
        buttons.deleteBlockedReason = "These still belong to the " + p.name + " pathway, so it can't be deleted:";
        for (const auto& name : users) buttons.deleteBlockedReason += "\n  " + name;
        buttons.deleteBlockedReason += "\nGive them another pathway first.";
    }
    const Sheet sheet = buildPathwaySheet(p);
    const std::string title = p.name + " pathway";
    switch (drawEditorButtons(buttons)) {
        case EditorAction::Save: saveImpl(w); break;
        case EditorAction::Revert:
            if (draft.isNew) draft.close();
            else if (const Pathway* saved = w.app.db.findPathway(p.id)) open(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, exportName(p), ExportFormat::Html, {sheet}, title); break;
        case EditorAction::ExportMarkdown:
            exportAndReport(w, exportName(p), ExportFormat::Markdown, {sheet}, title);
            break;
        case EditorAction::ExportText: exportAndReport(w, exportName(p), ExportFormat::Text, {sheet}, title); break;
        case EditorAction::Delete:
            try {
                const std::string name = p.name;
                deleteCustomPathway(w.app, p.id);
                draft.close();
                setStatus(w, "Deleted the " + name + " pathway." +
                                 (w.app.db.settings.backupsToKeep > 0 ? " A copy is in the backups folder." : ""));
            } catch (const StorageError& e) {
                setStatus(w, std::string("Deleting failed: ") + e.what(), true);
            }
            break;
        default: break;
    }
    if (!draft.open) {
        ImGui::EndChild();
        return;
    }

    ImGui::Spacing();
    if (ImGui::BeginTabBar("pathwayTabs")) {
        if (ImGui::BeginTabItem("Overview", nullptr, draft.firstTabFlags())) {
            ImGui::BeginChild("tab");
            overviewTab(w, p);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sequences")) {
            ImGui::BeginChild("tab");
            sequencesTab(p);
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
    ImGui::EndChild();
}

}  // namespace lotm::window
