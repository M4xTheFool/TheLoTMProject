// Sefirot menu: the nine Sefirot of the story, which pathways each one gathers, who held them in
// the story, and which of your characters hold them now.
#include <iostream>

#include "app.hpp"
#include "records.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

// Saves the character with a new Sefirah (empty takes it away).
void setHolder(App& app, int characterId, const std::string& sefirahId) {
    const Character* saved = app.db.findCharacter(characterId);
    if (!saved) return;
    Character c = *saved;
    c.sefirahId = sefirahId;
    try {
        saveCharacter(app, c, false);
        ui::info(sefirahId.empty() ? c.name + " no longer holds a Sefirah."
                                   : c.name + " holds " + sefirahLabel(app.db, sefirahId) + " now.");
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
    }
}

void addHolder(App& app, const Sefirah& s) {
    std::vector<const Character*> offered;
    std::vector<std::string> names;
    for (const auto& c : app.db.characters) {
        if (c.sefirahId == s.id) continue;
        offered.push_back(&c);
        std::string line = c.name + " (" + characterCode(c.id) + ", " + levelName(c);
        if (!c.sefirahId.empty()) line += ", holds " + sefirahLabel(app.db, c.sefirahId);
        names.push_back(line + ")");
    }
    if (offered.empty()) {
        ui::info("Every saved character holds it already, or there are none yet.");
        return;
    }
    const int pick = ui::choose("Who holds " + s.name + "?", names, "Cancel");
    if (pick < 0) return;
    const Character& c = *offered[static_cast<size_t>(pick)];
    if (!c.sefirahId.empty() &&
        !ui::yesNo(c.name + " holds " + sefirahLabel(app.db, c.sefirahId) + ". Give them " + s.name + " instead?", true)) {
        return;
    }
    setHolder(app, c.id, s.id);
}

void removeHolder(App& app, const Sefirah& s) {
    const auto holders = app.db.sefirahHolders(s.id);
    if (holders.empty()) {
        ui::info("None of your characters holds it.");
        return;
    }
    std::vector<std::string> names;
    for (const Character* c : holders) names.push_back(c->name + " (" + characterCode(c->id) + ")");
    const int pick = ui::choose("Take " + s.name + " away from whom?", names, "Cancel");
    if (pick >= 0) setHolder(app, holders[static_cast<size_t>(pick)]->id, "");
}

void viewSefirah(App& app, const std::string& id) {
    while (const Sefirah* s = app.db.findSefirah(id)) {
        std::cout << "\n" << renderText(buildSefirahSheet(*s, app.db));
        const int pick = ui::choose("\nWhat now?", {"Make one of your characters its holder", "Remove a holder", "Export"},
                                    "Back to the list");
        if (pick == 0) addHolder(app, *s);
        else if (pick == 1) removeHolder(app, *s);
        else if (pick == 2) exportSefirah(app, id);
        else return;
    }
}

}  // namespace

void runSefirotMenu(App& app) {
    while (true) {
        ui::header("Sefirot");
        if (app.db.sefirot.empty()) {
            ui::info("No Sefirot found: sefirot.json is missing from " + app.storage.dataDir().string() + ".");
            ui::pause();
            return;
        }
        ui::info("A Sefirah is a source of power that gathers the Uniquenesses of a group of neighbouring pathways.");
        ui::info("Holding one together with all of those Uniquenesses makes a Great Old One, Above the Sequence.");
        std::vector<std::string> names;
        for (const auto& s : app.db.sefirot) {
            std::string line = sefirahLabel(app.db, s.id);
            const size_t held = app.db.sefirahHolders(s.id).size();
            if (held > 0) line += ", held by " + std::to_string(held) + " of your characters";
            names.push_back(line);
        }
        const int pick = ui::choose("", names, "Back to the main menu");
        if (pick < 0) return;
        viewSefirah(app, app.db.sefirot[static_cast<size_t>(pick)].id);
    }
}

}  // namespace lotm
