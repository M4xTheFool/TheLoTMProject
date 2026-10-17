#include <iostream>
#include <optional>

#include "app.hpp"
#include "exporter.hpp"
#include "organizations.hpp"
#include "sheet.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "web.hpp"

namespace lotm {

namespace {

std::optional<ExportFormat> chooseFormat() {
    int pick = ui::choose("Which format?",
                          {"HTML (styled sheet, open in a browser, print to PDF)", "Markdown (Discord, Obsidian, wikis)",
                           "Plain text"},
                          "Cancel");
    if (pick == 0) return ExportFormat::Html;
    if (pick == 1) return ExportFormat::Markdown;
    if (pick == 2) return ExportFormat::Text;
    return std::nullopt;
}

void exportSheets(App& app, const std::string& baseName, ExportFormat format, const std::vector<Sheet>& sheets,
                  const std::string& pageTitle) {
    try {
        const auto file = writeExport(app, baseName, format, sheets, pageTitle);
        ui::info("Exported to " + file.string());
        if (ui::yesNo("Open it now?", format == ExportFormat::Html) && !openWithDefaultApp(file)) {
            ui::info("Could not open it automatically; open the file yourself.");
        }
    } catch (const StorageError& e) {
        ui::info(e.what());
    }
}

}  // namespace

void exportCharacter(App& app, int id) {
    const Character* c = app.db.findCharacter(id);
    if (!c) return;
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, exportName(*c), *format, {buildCharacterSheet(*c, app.db)}, c->name);
}

void exportArtifact(App& app, int id) {
    const Artifact* a = app.db.findArtifact(id);
    if (!a) return;
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, exportName(*a), *format, {buildArtifactSheet(*a, app.db)}, a->name);
}

void exportPathway(App& app, const std::string& id) {
    const Pathway* p = app.db.findPathway(id);
    if (!p) return;
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, exportName(*p), *format, {buildPathwaySheet(*p, app.db)}, p->name + " pathway");
}

void exportSefirah(App& app, const std::string& id) {
    const Sefirah* s = app.db.findSefirah(id);
    if (!s) return;
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, exportName(*s), *format, {buildSefirahSheet(*s, app.db)}, s->name);
}

void exportOrganization(App& app, int id) {
    const Organization* o = app.db.findOrganization(id);
    if (!o) return;
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, exportName(*o), *format, {buildOrganizationSheet(*o, app.db)}, o->name);
}

void exportTimeline(App& app, int characterId) {
    const Character* c = app.db.findCharacter(characterId);
    if (app.db.events.empty()) {
        ui::info("No story events yet.");
        return;
    }
    auto format = chooseFormat();
    if (!format) return;
    const Sheet sheet = buildTimelineSheet(app.db, c ? characterId : 0);
    exportSheets(app, c ? "timeline-" + exportName(*c) : "timeline", *format, {sheet}, sheet.title);
}

void exportWeb(App& app) {
    if (app.db.characters.empty()) {
        ui::info("No characters yet.");
        return;
    }
    ui::info("HTML draws the web as a picture; Markdown and plain text list who knows whom.");
    auto format = chooseFormat();
    if (!format) return;
    exportSheets(app, "relationship-web", *format, {buildWebSheet(app.db, false)}, "Who Knows Whom");
}

void runExportMenu(App& app) {
    while (true) {
        ui::header("Export");
        ui::info("Files go to: " + app.storage.exportsDir().string());
        int pick = ui::choose("",
                              {"One character", "One organization", "One Sealed Artifact", "One pathway", "One Sefirah",
                               "Everything (characters, organizations, Sealed Artifacts, your own pathways and the "
                               "Sefirot your characters hold, in one file)",
                               "The timeline (story events by date)", "The relationship web (who knows whom)"},
                              "Back to the main menu");
        if (pick == 0) {
            if (app.db.characters.empty()) { ui::info("No characters yet."); continue; }
            std::vector<std::string> names;
            for (const auto& c : app.db.characters) names.push_back(c.name + " (" + characterCode(c.id) + ")");
            int which = ui::choose("Which character?", names, "Cancel");
            if (which >= 0) exportCharacter(app, app.db.characters[which].id);
        } else if (pick == 1) {
            if (app.db.organizations.empty()) { ui::info("No organizations yet."); continue; }
            const std::vector<const Organization*> list = organizationsByName(app.db);
            std::vector<std::string> names;
            for (const Organization* o : list) names.push_back(o->name + " (" + organizationCode(o->id) + ")");
            int which = ui::choose("Which organization?", names, "Cancel");
            if (which >= 0) exportOrganization(app, list[static_cast<size_t>(which)]->id);
        } else if (pick == 2) {
            if (app.db.artifacts.empty()) { ui::info("No Sealed Artifacts yet."); continue; }
            std::vector<std::string> names;
            for (const auto& a : app.db.artifacts) names.push_back(a.name + " (" + artifactCode(a.id) + ")");
            int which = ui::choose("Which Sealed Artifact?", names, "Cancel");
            if (which >= 0) exportArtifact(app, app.db.artifacts[which].id);
        } else if (pick == 3) {
            int which = choosePathway(app, "Which pathway?", "Cancel", false);
            if (which >= 0) exportPathway(app, app.db.pathways[which].id);
        } else if (pick == 4) {
            if (app.db.sefirot.empty()) { ui::info("No Sefirot found (sefirot.json is missing)."); continue; }
            std::vector<std::string> names;
            for (const auto& s : app.db.sefirot) names.push_back(sefirahLabel(app.db, s.id));
            int which = ui::choose("Which Sefirah?", names, "Cancel");
            if (which >= 0) exportSefirah(app, app.db.sefirot[which].id);
        } else if (pick == 5) {
            if (app.db.characters.empty() && app.db.artifacts.empty() && app.db.organizations.empty()) {
                ui::info("The catalogue is empty.");
                continue;
            }
            auto format = chooseFormat();
            if (format) exportSheets(app, catalogueExportName(), *format, catalogueSheets(app.db), "The Catalogue");
        } else if (pick == 6) {
            exportTimeline(app, 0);
        } else if (pick == 7) {
            exportWeb(app);
        } else {
            return;
        }
    }
}

}  // namespace lotm
