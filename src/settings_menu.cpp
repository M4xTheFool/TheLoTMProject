#include "app.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
    return text;
}

// Ready-made characters and Sealed Artifacts from data/samples, such as the Tarot Club.
void addSampleSet(App& app) {
    std::vector<SampleSet> sets;
    try {
        sets = app.storage.loadSampleSets();
    } catch (const StorageError& e) {
        ui::info(e.what());
        ui::pause();
        return;
    }
    if (sets.empty()) {
        ui::info("There are no sample sets in " + app.storage.samplesDir().string() + ".");
        ui::pause();
        return;
    }
    std::vector<std::string> titles;
    for (const auto& set : sets) {
        titles.push_back(set.title + " (" + std::to_string(set.characters.size()) + " characters, " +
                         std::to_string(set.artifacts.size()) + " Sealed Artifacts)");
    }
    const int pick = ui::choose("Add which sample set to your saves?", titles, "Cancel");
    if (pick < 0) return;
    const SampleSet& set = sets[static_cast<size_t>(pick)];
    ui::blank();
    if (!set.description.empty()) ui::info(set.description);
    ui::info("Anything already saved under the same name is left as it is.");
    if (!ui::yesNo("Add them?", true)) return;
    try {
        const SampleImportReport report = importSampleSet(app, set);
        ui::blank();
        if (!report.addedCharacters.empty()) ui::info("Characters added: " + joined(report.addedCharacters));
        if (!report.addedArtifacts.empty()) ui::info("Sealed Artifacts added: " + joined(report.addedArtifacts));
        if (!report.skipped.empty()) ui::info("Already saved, so skipped: " + joined(report.skipped));
    } catch (const StorageError& e) {
        ui::info(std::string("Could not add the sample set: ") + e.what());
    }
    ui::pause();
}

}  // namespace

void runSettingsMenu(App& app) {
    Settings& s = app.db.settings;
    while (true) {
        ui::header("Settings");
        int pick = ui::choose("",
                              {"HP: " + s.hpMode + " (ask per character, always include, or never include)",
                               "Export theme: " + s.exportTheme + " (auto follows your system, or light / dark)",
                               "Backups kept per file: " + std::to_string(s.backupsToKeep),
                               "Add a sample set (the Tarot Club and canon Sealed Artifacts)"},
                              "Back to the main menu");
        if (pick == 0) {
            int m = ui::choose("HP mode:", kHpModes, "Cancel");
            if (m >= 0) s.hpMode = kHpModes[m];
        } else if (pick == 1) {
            int t = ui::choose("Export theme:", kExportThemes, "Cancel");
            if (t >= 0) s.exportTheme = kExportThemes[t];
        } else if (pick == 2) {
            s.backupsToKeep = ui::readInt("Backups to keep (0 turns backups off)", 0, 50, s.backupsToKeep);
        } else if (pick == 3) {
            addSampleSet(app);
            continue;
        } else {
            return;
        }
        try {
            app.storage.saveSettings(app.db);
        } catch (const StorageError& e) {
            ui::info(std::string("Could not save settings: ") + e.what());
        }
    }
}

}  // namespace lotm
