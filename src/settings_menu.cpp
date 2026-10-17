#include <algorithm>
#include <filesystem>

#include "app.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "story.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
    return text;
}

std::vector<SampleSet> readSampleSets(const App& app) {
    try {
        return app.storage.loadSampleSets();
    } catch (const StorageError& e) {
        ui::info(e.what());
        return {};
    }
}

// Every ready-made set in data/samples (the Tarot Club, Adam and Amon, and any shared through GitHub).
void importAll(App& app) {
    const std::vector<SampleSet> sets = readSampleSets(app);
    if (sets.empty()) {
        ui::info("There are no sample sets in " + app.storage.samplesDir().string() + ".");
        ui::pause();
        return;
    }
    ui::blank();
    for (const auto& set : sets) {
        ui::info(set.title + ": " + std::to_string(set.characters.size()) + " characters, " +
                 std::to_string(set.artifacts.size()) + " Sealed Artifacts");
    }
    ui::info("Anything already saved under the same name is updated when the set has a newer version,");
    ui::info("and left as it is when you edited it here more recently.");
    if (!ui::yesNo("Import them all?", true)) return;
    try {
        const SampleImportReport report = importAllSampleSets(app, sets);
        ui::blank();
        if (!report.addedCharacters.empty()) ui::info("Characters added: " + joined(report.addedCharacters));
        if (!report.addedArtifacts.empty()) ui::info("Sealed Artifacts added: " + joined(report.addedArtifacts));
        if (!report.updated.empty()) ui::info("Updated to a newer version: " + joined(report.updated));
        if (report.addedCharacters.empty() && report.addedArtifacts.empty() && report.updated.empty()) {
            ui::info("Nothing new to add.");
        }
        if (!report.skipped.empty()) ui::info("Up to date or edited here more recently, so left as they are: " + joined(report.skipped));
    } catch (const StorageError& e) {
        ui::info(std::string("Importing stopped: ") + e.what());
    }
    ui::pause();
}

// Tick records by number; Enter when done. describe(record) is the line after the number.
template <typename Record, typename Describe>
void pickRecords(const std::string& title, const std::vector<Record>& records, std::vector<int>& picked,
                 Describe describe) {
    if (records.empty()) return;
    while (true) {
        ui::info(title);
        for (size_t i = 0; i < records.size(); ++i) {
            const Record& r = records[i];
            const bool on = std::find(picked.begin(), picked.end(), r.id) != picked.end();
            ui::info(std::string(on ? "[x] " : "[ ] ") + std::to_string(i + 1) + ") " + describe(r));
        }
        const std::string value = ui::readLine("  A number ticks or unticks one, A ticks all, N none, Enter when done: ");
        if (value.empty()) return;
        if (ui::toLower(value) == "a") {
            picked.clear();
            for (const auto& r : records) picked.push_back(r.id);
            continue;
        }
        if (ui::toLower(value) == "n") {
            picked.clear();
            continue;
        }
        int index = 0;
        try {
            index = std::stoi(value);
        } catch (const std::exception&) {
            index = 0;
        }
        if (index < 1 || index > static_cast<int>(records.size())) {
            ui::info("Type a number from the list.");
            continue;
        }
        const int id = records[static_cast<size_t>(index - 1)].id;
        auto it = std::find(picked.begin(), picked.end(), id);
        if (it == picked.end()) picked.push_back(id);
        else picked.erase(it);
    }
}

// Packs your own characters, Sealed Artifacts and story events into a sample set in data/samples, to
// share them.
void shareOwn(App& app) {
    ui::header("Share your own");
    ui::info("Your characters, Sealed Artifacts and story events become a sample set in the data/samples");
    ui::info("folder. Commit it in GitHub Desktop and push, and anyone who pulls gets them with Import all.");
    ui::info("Your own pathways and Sefirot are shared already, in custom_pathways.json and custom_sefirot.json.");
    if (app.db.characters.empty() && app.db.artifacts.empty() && app.db.events.empty()) {
        ui::info("There is nothing to share yet: save a character, a Sealed Artifact or a story event first.");
        ui::pause();
        return;
    }
    const std::string title = ui::readRequired("Title (like Max's characters)", "");
    const std::string description = ui::readMultiline("Description (optional)", "");
    const auto sets = readSampleSets(app);
    const std::string fileName = sampleSetFileFor(sets, title);
    const auto taken = namesInSampleSets(sets, fileName);
    const auto inSet = [&](const std::string& name) {
        return std::find(taken.begin(), taken.end(), ui::toLower(name)) != taken.end() ? ", already in a sample set" : "";
    };
    std::vector<int> characters, artifacts, events;
    defaultSharePicks(app.db, sets, fileName, characters, artifacts, events);
    pickRecords("Characters to share:", app.db.characters, characters,
                [&](const Character& c) { return c.name + " (" + characterCode(c.id) + ")" + inSet(c.name); });
    pickRecords("Sealed Artifacts to share (the ones your characters hold go in too):", app.db.artifacts, artifacts,
                [&](const Artifact& a) { return a.name + " (" + artifactCode(a.id) + ")" + inSet(a.name); });
    std::vector<StoryEvent> ordered;
    for (const StoryEvent* e : eventsInOrder(app.db)) ordered.push_back(*e);
    pickRecords("Story events to share (people not ticked above go as names, and link up again when imported):",
                ordered, events,
                [&](const StoryEvent& e) {
                    return e.title + " (" + eventCode(e.id) + (e.date.empty() ? "" : ", " + storyDateText(e.date)) +
                           ")" + (eventInSampleSets(sets, fileName, e) ? ", already in a sample set" : "");
                });
    const SampleSet set = makeSampleSet(app.db, title, description, characters, artifacts, events);
    if (set.characters.empty() && set.artifacts.empty() && set.events.empty()) {
        ui::info("Nothing is ticked, so nothing was saved.");
        ui::pause();
        return;
    }
    const bool replacing = std::filesystem::exists(app.storage.samplesDir() / fileName);
    const std::string question = std::string(replacing ? "Replace" : "Save") + " data/samples/" + fileName + " with " +
                                 std::to_string(set.characters.size()) + " characters, " +
                                 std::to_string(set.artifacts.size()) + " Sealed Artifacts and " +
                                 std::to_string(set.events.size()) + " story events?";
    if (!ui::yesNo(question, true)) return;
    try {
        app.storage.saveSampleSet(set, fileName);
        ui::info("Saved. Commit data/samples/" + fileName + " in GitHub Desktop and push to share it.");
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
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
                               "Import all sample sets (the Tarot Club, Adam and Amon, and any shared ones)",
                               "Share your own characters, Sealed Artifacts and story events as a sample set"},
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
            importAll(app);
            continue;
        } else if (pick == 4) {
            shareOwn(app);
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
