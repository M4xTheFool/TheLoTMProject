#include <iostream>
#include <optional>

#include "app.hpp"
#include "exporter.hpp"
#include "sheet.hpp"
#include "storage.hpp"
#include "ui.hpp"

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
    exportSheets(app, exportName(*p), *format, {buildPathwaySheet(*p)}, p->name + " pathway");
}

void runExportMenu(App& app) {
    while (true) {
        ui::header("Export");
        ui::info("Files go to: " + app.storage.exportsDir().string());
        int pick = ui::choose("",
                              {"One character", "One Sealed Artifact", "One pathway",
                               "Everything (characters, Sealed Artifacts and your own pathways in one file)"},
                              "Back to the main menu");
        if (pick == 0) {
            if (app.db.characters.empty()) { ui::info("No characters yet."); continue; }
            std::vector<std::string> names;
            for (const auto& c : app.db.characters) names.push_back(c.name + " (" + characterCode(c.id) + ")");
            int which = ui::choose("Which character?", names, "Cancel");
            if (which >= 0) exportCharacter(app, app.db.characters[which].id);
        } else if (pick == 1) {
            if (app.db.artifacts.empty()) { ui::info("No Sealed Artifacts yet."); continue; }
            std::vector<std::string> names;
            for (const auto& a : app.db.artifacts) names.push_back(a.name + " (" + artifactCode(a.id) + ")");
            int which = ui::choose("Which Sealed Artifact?", names, "Cancel");
            if (which >= 0) exportArtifact(app, app.db.artifacts[which].id);
        } else if (pick == 2) {
            int which = choosePathway(app, "Which pathway?", "Cancel", false);
            if (which >= 0) exportPathway(app, app.db.pathways[which].id);
        } else if (pick == 3) {
            if (app.db.characters.empty() && app.db.artifacts.empty()) { ui::info("The catalogue is empty."); continue; }
            auto format = chooseFormat();
            if (format) exportSheets(app, catalogueExportName(), *format, catalogueSheets(app.db), "The Catalogue");
        } else {
            return;
        }
    }
}

}  // namespace lotm
