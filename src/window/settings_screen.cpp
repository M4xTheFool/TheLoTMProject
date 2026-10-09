// Settings tab: the same settings as the console program, plus the window's text size.
// Changes are saved straight away.
#include <algorithm>

#include "presets.hpp"
#include "records.hpp"
#include "storage.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
    return text;
}

void addSampleSet(WindowState& w, const SampleSet& set) {
    try {
        const SampleImportReport report = importSampleSet(w.app, set);
        reloadCharacterDraft(w);
        std::string text = "Added " + std::to_string(report.addedCharacters.size()) + " characters and " +
                           std::to_string(report.addedArtifacts.size()) + " Sealed Artifacts from " + set.title + ".";
        if (!report.skipped.empty()) text += " Already saved, so left as they were: " + joined(report.skipped) + ".";
        setStatus(w, text);
    } catch (const StorageError& e) {
        setStatus(w, std::string("Could not add the sample set: ") + e.what(), true);
    }
}

// Ready-made characters and Sealed Artifacts from data/samples, such as the Tarot Club.
void sampleSetsSection(WindowState& w) {
    ImGui::SeparatorText("Sample sets");
    if (w.showSampleSets) {
        ImGui::SetScrollHereY(0.0f);
        w.showSampleSets = false;
    }
    if (!w.sampleSets) {
        try {
            w.sampleSets = w.app.storage.loadSampleSets();
        } catch (const StorageError& e) {
            w.sampleSets.emplace();
            w.sampleSetsProblem = e.what();
        }
    }
    if (!w.sampleSetsProblem.empty()) ImGui::TextColored(palette().danger, "%s", w.sampleSetsProblem.c_str());
    if (w.sampleSets->empty() && w.sampleSetsProblem.empty()) {
        ImGui::TextDisabled("No sample sets found in %s.", w.app.storage.samplesDir().string().c_str());
    }
    const bool unsaved = hasUnsavedChanges(w);
    for (const SampleSet& set : *w.sampleSets) {
        ImGui::PushID(set.file.string().c_str());
        ImGui::PushFont(w.fonts.bold, 0.0f);
        ImGui::TextUnformatted(set.title.c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu characters, %zu Sealed Artifacts", set.characters.size(), set.artifacts.size());
        if (!set.description.empty()) ImGui::TextWrapped("%s", set.description.c_str());
        if (ImGui::TreeNode("What's in it")) {
            for (const Character& c : set.characters) {
                ImGui::BulletText("%s, %s", c.name.c_str(),
                                  c.pathwayId.empty()
                                      ? "mortal"
                                      : sequenceLabel(w.app.db.findPathway(c.pathwayId), c.sequence).c_str());
            }
            for (const Artifact& a : set.artifacts) ImGui::BulletText("%s (Sealed Artifact)", a.name.c_str());
            ImGui::TreePop();
        }
        ImGui::BeginDisabled(unsaved);
        if (ImGui::Button("Add to my saves")) addSampleSet(w, set);
        ImGui::EndDisabled();
        if (unsaved) {
            ImGui::SameLine();
            ImGui::TextDisabled("Save or undo the changes you're making first.");
        } else {
            ImGui::SetItemTooltip("Anything already saved under the same name is left as it is.");
        }
        ImGui::Spacing();
        ImGui::PopID();
    }
}

}  // namespace

void drawSettingsScreen(WindowState& w) {
    Settings& s = w.app.db.settings;
    bool changed = false;

    ImGui::BeginChild("settings", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    heading(w, "Settings");
    ImGui::TextDisabled("Changes are saved straight away.");

    ImGui::SeparatorText("HP");
    const char* hpLabels[] = {"Ask for each character", "Always include HP", "Never include HP"};
    for (size_t i = 0; i < kHpModes.size(); ++i) {
        if (ImGui::RadioButton(hpLabels[i], s.hpMode == kHpModes[i])) {
            s.hpMode = kHpModes[i];
            changed = true;
        }
    }

    ImGui::SeparatorText("Exported HTML sheets");
    const char* themeLabels[] = {"Follow the computer's light or dark mode", "Always light", "Always dark"};
    for (size_t i = 0; i < kExportThemes.size(); ++i) {
        if (ImGui::RadioButton(themeLabels[i], s.exportTheme == kExportThemes[i])) {
            s.exportTheme = kExportThemes[i];
            changed = true;
        }
    }

    ImGui::SeparatorText("Backups");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
    ImGui::SliderInt("Backups kept per file", &s.backupsToKeep, 0, 50, s.backupsToKeep == 0 ? "off" : "%d");
    changed |= ImGui::IsItemDeactivatedAfterEdit();  // save once the slider is let go
    ImGui::TextDisabled("Every save keeps a copy of the previous file in data/backups.");

    ImGui::SeparatorText("This window");
    const char* windowThemeLabels[] = {"Night (dark ink, gold accents)", "Parchment (light paper, brown ink)"};
    for (size_t i = 0; i < kWindowThemes.size(); ++i) {
        if (ImGui::RadioButton(windowThemeLabels[i], s.windowTheme == kWindowThemes[i])) {
            s.windowTheme = kWindowThemes[i];  // the window switches before drawing the next frame
            changed = true;
        }
    }
    // The new size is applied when the slider is let go, so the slider doesn't move under the mouse.
    static int textSize = 100;
    static bool dragging = false;
    if (!dragging) textSize = s.windowTextSize;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
    ImGui::SliderInt("Text size", &textSize, 70, 160, "%d%%");
    dragging = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        s.windowTextSize = std::clamp(textSize, 70, 160);
        changed = true;
    }

    sampleSetsSection(w);

    ImGui::SeparatorText("Files");
    ImGui::TextWrapped("Data folder: %s", w.app.storage.dataDir().string().c_str());
    if (ImGui::Button("Open the data folder")) openWithDefaultApp(w.app.storage.dataDir());
    ImGui::SameLine();
    if (ImGui::Button("Open the exports folder")) {
        std::error_code ec;
        std::filesystem::create_directories(w.app.storage.exportsDir(), ec);
        openWithDefaultApp(w.app.storage.exportsDir());
    }
    ImGui::SameLine();
    if (ImGui::Button("Export everything as HTML")) {
        exportAndReport(w, catalogueExportName(), ExportFormat::Html, catalogueSheets(w.app.db), "The Catalogue");
    }
    ImGui::SetItemTooltip("Every character, Sealed Artifact and pathway of your own, in one page.");
    ImGui::EndChild();

    if (changed) {
        try {
            w.app.storage.saveSettings(w.app.db);
        } catch (const StorageError& e) {
            setStatus(w, std::string("Could not save settings: ") + e.what(), true);
        }
    }
}

}  // namespace lotm::window
