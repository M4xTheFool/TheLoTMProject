// Settings tab: the same settings as the console program, plus the window's text size.
// Changes are saved straight away.
#include <algorithm>

#include "presets.hpp"
#include "storage.hpp"
#include "window.hpp"

namespace lotm::window {

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
