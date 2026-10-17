// Settings tab: the same settings as the console program, plus the window's text size.
// Changes are saved straight away.
#include <algorithm>
#include <type_traits>

#include "organizations.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

std::string countOf(size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

// "3 characters, 2 Sealed Artifacts and 4 story events", leaving out the kinds there are none of.
std::string contents(size_t characters, size_t artifacts, size_t events, size_t organizations = 0) {
    std::vector<std::string> parts;
    if (characters > 0) parts.push_back(countOf(characters, "character", "characters"));
    if (organizations > 0) parts.push_back(countOf(organizations, "organization", "organizations"));
    if (artifacts > 0) parts.push_back(countOf(artifacts, "Sealed Artifact", "Sealed Artifacts"));
    if (events > 0) parts.push_back(countOf(events, "story event", "story events"));
    if (parts.empty()) return "nothing";
    std::string text = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) text += (i + 1 == parts.size() ? " and " : ", ") + parts[i];
    return text;
}

std::vector<SampleSet>& sampleSets(WindowState& w) {
    if (!w.sampleSets) {
        w.sampleSetsProblem.clear();
        try {
            w.sampleSets = w.app.storage.loadSampleSets();
        } catch (const StorageError& e) {
            w.sampleSets.emplace();
            w.sampleSetsProblem = e.what();
        }
    }
    return *w.sampleSets;
}

void importAll(WindowState& w) {
    try {
        const SampleImportReport report = importAllSampleSets(w.app, sampleSets(w));
        reloadCharacterDraft(w);
        std::string text = "Added " + contents(report.addedCharacters.size(), report.addedArtifacts.size(),
                                               report.addedEvents.size(), report.addedOrganizations.size()) + ".";
        if (!report.updated.empty()) {
            text += " Updated " + std::to_string(report.updated.size()) + " to a newer version from a sample set.";
        }
        if (!report.skipped.empty()) {
            text += " " + std::to_string(report.skipped.size()) +
                    " were already up to date or edited more recently here, so they were left as they were.";
        }
        setStatus(w, text);
    } catch (const StorageError& e) {
        setStatus(w, std::string("Importing stopped: ") + e.what(), true);
    }
}

// Ready-made characters, Sealed Artifacts, story events and organizations from data/samples: the Tarot Club, Adam and
// Amon, and any set shared through GitHub. One button imports them all.
void sampleSetsSection(WindowState& w) {
    ImGui::Spacing();
    if (w.showSampleSets) {
        ImGui::SetScrollHereY(0.0f);
        w.showSampleSets = false;
    }
    heading(w, "Sample sets");
    const auto& sets = sampleSets(w);
    if (!w.sampleSetsProblem.empty()) ImGui::TextColored(palette().danger, "%s", w.sampleSetsProblem.c_str());
    if (sets.empty() && w.sampleSetsProblem.empty()) {
        ImGui::TextDisabled("No sample sets found in %s.", w.app.storage.samplesDir().string().c_str());
        return;
    }
    ImGui::TextWrapped("Ready-made characters, Sealed Artifacts, story events and organizations in the data/samples folder, including any "
                       "shared through GitHub. Anything already saved under the same name is updated when a set "
                       "has a newer version, and left as it is when you edited it here more recently, so "
                       "importing again only brings in what is new or changed.");
    const bool unsaved = hasUnsavedChanges(w);
    ImGui::BeginDisabled(unsaved || sets.empty());
    if (ImGui::Button("Import all")) importAll(w);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (unsaved) ImGui::TextDisabled("Save or undo the changes you're making first.");
    else ImGui::TextDisabled("%zu sets", sets.size());
    for (const SampleSet& set : sets) {
        ImGui::PushID(set.file.string().c_str());
        const std::string header =
            set.title + "  -  " +
            contents(set.characters.size(), set.artifacts.size(), set.events.size(), set.organizations.size()) + "###set";
        if (ImGui::TreeNode(header.c_str())) {
            if (!set.description.empty()) ImGui::TextWrapped("%s", set.description.c_str());
            for (const Character& c : set.characters) {
                ImGui::BulletText("%s, %s", c.name.c_str(),
                                  c.pathwayId.empty() ? "mortal"
                                  : c.aboveSequence   ? "Above the Sequence"
                                                      : sequenceLabel(w.app.db.findPathway(c.pathwayId), c.sequence).c_str());
            }
            for (const Organization& o : set.organizations) ImGui::BulletText("%s (organization)", o.name.c_str());
            for (const Artifact& a : set.artifacts) ImGui::BulletText("%s (Sealed Artifact)", a.name.c_str());
            for (const StoryEvent& e : set.events) {
                ImGui::BulletText("%s (story event%s)", e.title.c_str(),
                                  e.date.empty() ? "" : (", " + storyDateText(e.date)).c_str());
            }
            ImGui::TextDisabled("data/samples/%s", set.file.filename().string().c_str());
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

bool contains(const std::vector<int>& ids, int id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); }

void toggle(std::vector<int>& ids, int id, bool on) {
    if (on && !contains(ids, id)) ids.push_back(id);
    if (!on) ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}

bool inList(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), ui::toLower(name)) != names.end();
}

// Grey text that wraps at the edge of its column.
void note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

const std::string& recordName(const Character& c) { return c.name; }
const std::string& recordName(const Artifact& a) { return a.name; }
const std::string& recordName(const StoryEvent& e) { return e.title; }
const std::string& recordName(const Organization& o) { return o.name; }

// Packs your own characters, Sealed Artifacts, story events and organizations into a file in
// data/samples, which GitHub Desktop can then commit and share. Whoever pulls it gets them with Import all.
void shareSection(WindowState& w) {
    auto& share = w.share;
    const Database& db = w.app.db;
    ImGui::Spacing();
    heading(w, "Share your own");
    ImGui::TextWrapped("Turn your characters, organizations, Sealed Artifacts and story events into a sample set. It is saved in "
                       "the data/samples folder; commit it in GitHub Desktop and push, and anyone who pulls gets "
                       "them with Import all. Your story events and organizations are only on this computer until you "
                       "share them here. Your own pathways and Sefirot are shared already: they are in custom_pathways.json "
                       "and custom_sefirot.json, which GitHub Desktop commits the same way.");
    textField("Title", share.title, "e.g. Max's characters");
    multilineField("Description (optional)", share.description, 2);

    const std::string fileName = sampleSetFileFor(sampleSets(w), share.title.empty() ? "My sample set" : ui::trim(share.title));
    const auto taken = namesInSampleSets(sampleSets(w), fileName);
    // Until you tick something by hand: the title of a set that's already there ticks what it has, so
    // you can save your changed versions over it; a new title ticks everything not in another set.
    if (!share.touched && share.pickedFor != fileName) {
        share.pickedFor = fileName;
        share.picks = defaultSharePicks(db, sampleSets(w), fileName);
    }

    // records: the records themselves or pointers to them; about(record): the grey text after the name.
    auto pickList = [&](const char* id, const char* title, const auto& records, std::vector<int>& picked, auto about) {
        auto record = [](const auto& r) -> const auto& {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(r)>>) return *r;
            else return r;
        };
        ImGui::PushID(id);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);
        ImGui::SameLine();
        if (ImGui::SmallButton("All")) {
            picked.clear();
            for (const auto& r : records) picked.push_back(record(r).id);
            share.touched = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("None")) {
            picked.clear();
            share.touched = true;
        }
        if (records.empty()) ImGui::TextDisabled("None saved yet.");
        for (const auto& r : records) {
            bool on = contains(picked, record(r).id);
            if (ImGui::Checkbox((recordName(record(r)) + "##" + std::to_string(record(r).id)).c_str(), &on)) {
                toggle(picked, record(r).id, on);
                share.touched = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", about(record(r)).c_str());
        }
        ImGui::PopID();
    };
    // Short, for four lists side by side.
    auto inASet = [&](const std::string& name) { return inList(taken, name) ? ", in another set" : ""; };
    const std::vector<const StoryEvent*> events = eventsInOrder(db);
    const std::vector<const Organization*> organizations = organizationsByName(db);
    SharePicks& picks = share.picks;
    if (ImGui::BeginTable("pick", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        pickList("characters", "Characters", db.characters, picks.characterIds,
                 [&](const Character& c) { return characterCode(c.id) + inASet(c.name); });
        ImGui::TableNextColumn();
        pickList("organizations", "Organizations", organizations, picks.organizationIds,
                 [&](const Organization& o) { return organizationCode(o.id) + inASet(o.name); });
        note("The ones your ticked characters belong to go in too.");
        ImGui::TableNextColumn();
        pickList("artifacts", "Sealed Artifacts", db.artifacts, picks.artifactIds,
                 [&](const Artifact& a) { return artifactCode(a.id) + inASet(a.name); });
        note("The ones your ticked characters hold go in too.");
        ImGui::TableNextColumn();
        pickList("events", "Story events", events, picks.eventIds, [&](const StoryEvent& e) {
            return (e.date.empty() ? eventCode(e.id) : storyDateText(e.date)) +
                   (eventInSampleSets(sampleSets(w), fileName, e) ? ", in another set" : "");
        });
        if (!events.empty() && ImGui::SmallButton("Tick the ones with your ticked characters")) {
            for (int id : eventsWithCharacters(db, picks.characterIds)) toggle(picks.eventIds, id, true);
            share.touched = true;
        }
        note("Someone at an event who isn't ticked goes as a name, and links up again when they are imported "
             "from another set, with what changed for them there.");
        ImGui::EndTable();
    }

    const SampleSet set = makeSampleSet(db, ui::trim(share.title), ui::trim(share.description), picks);
    const bool replacing = std::filesystem::exists(w.app.storage.samplesDir() / fileName);
    const bool ready = !ui::trim(share.title).empty() &&
                       (!set.characters.empty() || !set.artifacts.empty() || !set.events.empty() ||
                        !set.organizations.empty());
    ImGui::Spacing();
    ImGui::BeginDisabled(!ready);
    const std::string button = (replacing ? "Replace data/samples/" : "Save as data/samples/") + fileName;
    if (ImGui::Button(button.c_str())) {
        try {
            w.app.storage.saveSampleSet(set, fileName);
            w.sampleSets.reset();  // read the folder again, with the new set in it
            share.touched = false;
            share.pickedFor = fileName;
            setStatus(w, "Saved " + contents(set.characters.size(), set.artifacts.size(), set.events.size(), set.organizations.size()) +
                             " to data/samples/" + fileName + ". Commit it in GitHub Desktop and push to share it.");
        } catch (const StorageError& e) {
            setStatus(w, std::string("Saving failed: ") + e.what(), true);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::trim(share.title).empty()) ImGui::TextDisabled("Give it a title first.");
    else if (!ready) ImGui::TextDisabled("Tick at least one character, organization, Sealed Artifact or story event.");
    else if (replacing) ImGui::TextColored(palette().warning, "A sample set with this title is already there; this replaces it with the versions ticked here.");
    else ImGui::TextDisabled("%s", contents(set.characters.size(), set.artifacts.size(), set.events.size(), set.organizations.size()).c_str());
}

}  // namespace

void drawSettingsScreen(WindowState& w) {
    Settings& s = w.app.db.settings;
    bool changed = false;

    ImGui::BeginChild("settings", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextDisabled("Changes are saved straight away.");

    ImGui::Spacing();
    heading(w, "HP");
    const char* hpLabels[] = {"Ask for each character", "Always include HP", "Never include HP"};
    for (size_t i = 0; i < kHpModes.size(); ++i) {
        if (ImGui::RadioButton(hpLabels[i], s.hpMode == kHpModes[i])) {
            s.hpMode = kHpModes[i];
            changed = true;
        }
    }

    ImGui::Spacing();
    heading(w, "Exported HTML sheets");
    const char* themeLabels[] = {"Follow the computer's light or dark mode", "Always light", "Always dark"};
    for (size_t i = 0; i < kExportThemes.size(); ++i) {
        if (ImGui::RadioButton(themeLabels[i], s.exportTheme == kExportThemes[i])) {
            s.exportTheme = kExportThemes[i];
            changed = true;
        }
    }

    ImGui::Spacing();
    heading(w, "Backups");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
    ImGui::SliderInt("Backups kept per file", &s.backupsToKeep, 0, 50, s.backupsToKeep == 0 ? "off" : "%d");
    changed |= ImGui::IsItemDeactivatedAfterEdit();  // save once the slider is let go
    ImGui::TextDisabled("Every save keeps a copy of the previous file in data/backups.");

    ImGui::Spacing();
    heading(w, "This window");
    const char* windowThemeLabels[] = {"Night (dark ink, gold accents)", "Parchment (light paper, brown ink)"};
    for (size_t i = 0; i < kWindowThemes.size(); ++i) {
        if (ImGui::RadioButton(windowThemeLabels[i], s.windowTheme == kWindowThemes[i])) {
            s.windowTheme = kWindowThemes[i];  // the window switches before drawing the next frame
            changed = true;
        }
    }
    if (ImGui::Checkbox("Animations", &s.windowAnimations)) changed = true;
    ImGui::SameLine();
    ImGui::TextDisabled("dice that tumble, screens that fade in, numbers that count up");
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
    shareSection(w);

    ImGui::Spacing();
    heading(w, "Files");
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
    ImGui::SetItemTooltip("Every character, organization, Sealed Artifact and pathway of your own, in one page.");
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
