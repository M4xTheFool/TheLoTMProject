// Sefirot tab: the nine Sefirot of the story on the left; on the right what one is, the pathways
// whose Uniquenesses it gathers, who held it in the story, and which of your characters hold it.
#include "records.hpp"
#include "rules.hpp"
#include "storage.hpp"
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

void drawDetails(WindowState& w, const Sefirah& s) {
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    medallion(w, initials(s.name), sefirahColour(w, s), font * 2.1f);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextUnformatted(s.name.c_str());
    ImGui::PopFont();
    ImGui::TextColored(palette().muted, "%s",
                       s.greatOldOne.empty() ? "Sefirah" : ("Sefirah of the " + s.greatOldOne).c_str());
    if (ImGui::Button("Export...")) ImGui::OpenPopup("export");
    if (ImGui::BeginPopup("export")) {
        const std::vector<Sheet> sheets = {buildSefirahSheet(s, db)};
        if (ImGui::Selectable("HTML (opens in your browser, prints to PDF)")) {
            exportAndReport(w, exportName(s), ExportFormat::Html, sheets, s.name);
        }
        if (ImGui::Selectable("Markdown (Discord, Obsidian, wikis)")) {
            exportAndReport(w, exportName(s), ExportFormat::Markdown, sheets, s.name);
        }
        if (ImGui::Selectable("Plain text")) exportAndReport(w, exportName(s), ExportFormat::Text, sheets, s.name);
        ImGui::EndPopup();
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    heading(w, "Pathways");
    ImGui::TextDisabled("Holding this Sefirah and the Uniqueness of every one of these makes a Great Old One.");
    for (size_t i = 0; i < s.pathwayIds.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        const Pathway* p = db.findPathway(s.pathwayIds[i]);
        chip(p ? p->name + (p->god.empty() ? "" : " (" + p->god + ")") : s.pathwayIds[i] + " (not found)",
             pathwayColour(db, s.pathwayIds[i]));
    }

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

    if (!s.knownHolders.empty()) {
        ImGui::Spacing();
        heading(w, "Known holders in the story");
        for (const auto& line : s.knownHolders) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", line.c_str());
        }
    }

    ImGui::Spacing();
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

    if (!s.notes.empty()) {
        ImGui::Spacing();
        heading(w, "Notes");
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("%s", s.notes.c_str());
        ImGui::PopStyleColor();
    }
}

}  // namespace

void drawSefirotScreen(WindowState& w) {
    auto& screen = w.sefirot;
    const Database& db = w.app.db;
    if (!db.findSefirah(screen.selectedId) && !db.sefirot.empty()) screen.selectedId = db.sefirot.front().id;

    // ---- the list
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    ImGui::TextWrapped("The sources of power and authority behind each group of pathways.");
    ImGui::Separator();
    if (db.sefirot.empty()) ImGui::TextDisabled("No Sefirot: sefirot.json is missing from the data folder.");
    for (const auto& s : db.sefirot) {
        const size_t held = db.sefirahHolders(s.id).size();
        std::string subtitle = s.greatOldOne.empty() ? "Sefirah" : s.greatOldOne;
        if (held > 0) subtitle += "  \xC2\xB7  held by " + std::to_string(held);
        if (listCard(w, "s" + s.id, s.id == screen.selectedId, sefirahColour(w, s), initials(s.name), s.name, subtitle)) {
            screen.selectedId = s.id;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the details
    ImGui::BeginChild("details", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (const Sefirah* s = db.findSefirah(screen.selectedId)) drawDetails(w, *s);
    ImGui::EndChild();
}

}  // namespace lotm::window
