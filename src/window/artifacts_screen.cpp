// Sealed Artifacts tab: the list on the left, the artifact being edited on the right.
#include "records.hpp"
#include "storage.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

void open(WindowState& w, const Artifact& a, bool isNew) { w.artifacts.draft.start(a, isNew); }

void openNew(WindowState& w) {
    Artifact a;
    a.id = w.app.db.nextArtifactId();
    open(w, a, true);
}

bool saveImpl(WindowState& w) {
    auto& draft = w.artifacts.draft;
    Artifact& a = draft.value;
    a.name = ui::trim(a.name);
    if (a.name.empty()) {
        setStatus(w, "Give the Sealed Artifact a name before saving.", true);
        return false;
    }
    try {
        saveArtifact(w.app, a, draft.isNew);
        setStatus(w, "Saved " + a.name + " as " + artifactCode(a.id) + ".");
        open(w, a, false);
        return true;
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
        return false;
    }
}

void form(WindowState& w, Artifact& a) {
    w.artifacts.draft.focusNameOnce();
    textField("Name", a.name, "required");
    pathwayCombo("Pathway", w.app.db, a.pathwayId, "Unknown pathway");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Sequence level");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    bool known = a.sequenceLevel != kUnknownSequence;
    if (ImGui::Checkbox("Known", &known)) a.sequenceLevel = known ? 9 : kUnknownSequence;
    if (known) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        int climbed = 9 - a.sequenceLevel;
        const std::string shown = "Sequence " + std::to_string(a.sequenceLevel) + " level";
        if (ImGui::SliderInt("##level", &climbed, 0, 9, shown.c_str(), ImGuiSliderFlags_NoInput)) {
            a.sequenceLevel = 9 - climbed;
        }
    }

    ImGui::Spacing();
    multilineField("What it looks like", a.visualDescription, 3);
    multilineField("Its ability", a.ability, 5);
    multilineField("Its drawback", a.drawback, 4);
    multilineField("Notes (containment, history ...)", a.notes, 3);

    ImGui::SeparatorText("Held by");
    bool any = false;
    for (const auto& c : w.app.db.characters) {
        for (int id : c.artifactIds) {
            if (id != a.id) continue;
            ImGui::BulletText("%s (%s)", c.name.c_str(), characterCode(c.id).c_str());
            any = true;
        }
    }
    if (!any) ImGui::TextDisabled("Nobody yet. Give it to a character in their Sealed Artifacts tab.");
}

}  // namespace

bool saveArtifactDraft(WindowState& w) { return saveImpl(w); }

void drawArtifactsScreen(WindowState& w) {
    auto& s = w.artifacts;
    auto& draft = s.draft;
    auto saveThis = [&w] { return saveImpl(w); };

    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (ImGui::Button("+ New Sealed Artifact", ImVec2(-FLT_MIN, 0))) {
        whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w); });
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search names", &s.search);
    ImGui::Separator();
    if (draft.open && draft.isNew) ImGui::Selectable("(new Sealed Artifact)", true);
    if (w.app.db.artifacts.empty()) ImGui::TextDisabled("No Sealed Artifacts yet.");
    for (const auto& a : w.app.db.artifacts) {
        if (!s.search.empty() && ui::toLower(a.name).find(ui::toLower(s.search)) == std::string::npos) continue;
        const bool selected = draft.open && !draft.isNew && draft.value.id == a.id;
        if (ImGui::Selectable((a.name + "##" + std::to_string(a.id)).c_str(), selected) && !selected) {
            const int id = a.id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                if (const Artifact* found = w.app.db.findArtifact(id)) open(w, *found, false);
            });
        }
        ImGui::Indent();
        ImGui::TextDisabled("%s, %s", artifactCode(a.id).c_str(),
                            a.pathwayId.empty() ? "unknown pathway" : pathwayLabel(w.app.db, a.pathwayId).c_str());
        ImGui::Unindent();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("editor", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!draft.open) {
        ImGui::Spacing();
        ImGui::TextDisabled("Pick a Sealed Artifact on the left, or create a new one.");
        ImGui::EndChild();
        return;
    }
    Artifact& a = draft.value;
    const Sheet sheet = buildArtifactSheet(a, w.app.db);
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.6f);
    ImGui::TextUnformatted(a.name.empty() ? "(unnamed)" : a.name.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", draft.isNew ? "new" : artifactCode(a.id).c_str());

    EditorButtons buttons;
    buttons.dirty = draft.dirty();
    buttons.isNew = draft.isNew;
    switch (drawEditorButtons(buttons)) {
        case EditorAction::Save: saveImpl(w); break;
        case EditorAction::Revert:
            if (draft.isNew) draft.close();
            else if (const Artifact* saved = w.app.db.findArtifact(a.id)) open(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, exportName(a), ExportFormat::Html, {sheet}, a.name); break;
        case EditorAction::ExportMarkdown:
            exportAndReport(w, exportName(a), ExportFormat::Markdown, {sheet}, a.name);
            break;
        case EditorAction::ExportText: exportAndReport(w, exportName(a), ExportFormat::Text, {sheet}, a.name); break;
        case EditorAction::Delete:
            try {
                const std::string name = a.name;
                deleteArtifact(w.app, a.id);
                draft.close();
                setStatus(w, "Deleted " + name + ", and took it away from everyone who held it.");
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
    if (ImGui::BeginTabBar("artifactTabs")) {
        if (ImGui::BeginTabItem("Details", nullptr, draft.firstTabFlags())) {
            ImGui::BeginChild("tab");
            form(w, a);
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
