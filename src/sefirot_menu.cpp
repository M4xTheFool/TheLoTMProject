// Sefirot menu: the nine Sefirot of the story and your own, which pathways each one gathers, who held
// them in the story, and which of your characters hold them now. Your own can be created, edited and
// deleted here (custom_sefirot.json).
#include <algorithm>
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

// Toggles the pathways a Sefirah gathers: a number adds or removes one, Enter finishes.
void choosePathways(const App& app, Sefirah& s) {
    while (true) {
        ui::info("Pathways it gathers (holding the Sefirah and each one's Uniqueness makes a Great Old One):");
        for (size_t i = 0; i < app.db.pathways.size(); ++i) {
            const Pathway& p = app.db.pathways[i];
            const bool on = std::find(s.pathwayIds.begin(), s.pathwayIds.end(), p.id) != s.pathwayIds.end();
            std::string line = std::string(on ? "[x] " : "[ ] ") + std::to_string(i + 1) + ") " + pathwayLabel(app.db, p.id);
            if (const Sefirah* other = on ? app.db.otherSefirahFor(p.id, s.id) : nullptr) {
                line += "  (also gathered by " + other->name + ")";
            }
            ui::info(line);
        }
        const std::string value = ui::readLine("  A number adds or removes a pathway, Enter when done: ");
        if (value.empty()) return;
        int index = 0;
        try {
            index = std::stoi(value);
        } catch (const std::exception&) {
            index = 0;
        }
        if (index < 1 || index > static_cast<int>(app.db.pathways.size())) {
            ui::info("Type a number from the list.");
            continue;
        }
        const std::string& id = app.db.pathways[static_cast<size_t>(index - 1)].id;
        auto it = std::find(s.pathwayIds.begin(), s.pathwayIds.end(), id);
        if (it == s.pathwayIds.end()) s.pathwayIds.push_back(id);
        else s.pathwayIds.erase(it);
    }
}

// Asks for every field of one of your own Sefirot, then saves it.
void editOwnSefirah(App& app, Sefirah s, bool isNew) {
    ui::header(isNew ? "New Sefirah" : "Edit " + s.name);
    s.name = ui::readRequired("Name (like Sefirah Castle)", s.name);
    s.greatOldOne = ui::readText("Its Great Old One (optional)", s.greatOldOne);
    ui::editList("Other names it goes by", s.alsoKnownAs);
    choosePathways(app, s);
    ui::blank();
    ui::info("Above the Sequence: a character past Sequence 0 who holds this Sefirah is its Great Old One.");
    s.symbolism = ui::readText("What the Great Old One symbolizes (optional)", s.symbolism);
    editAbilityList(s.abilities, s.greatOldOne.empty() ? "its Great Old One" : s.greatOldOne);
    s.description = ui::readMultiline("What it is (optional)", s.description);
    ui::editList("Known holders in your story, one line each", s.knownHolders);
    s.notes = ui::readMultiline("Notes (optional)", s.notes);
    try {
        saveCustomSefirah(app, s, isNew);
        ui::info("Saved " + s.name + ". Characters can hold it now.");
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
    }
}

void deleteOwnSefirah(App& app, const Sefirah& s) {
    const size_t held = app.db.sefirahHolders(s.id).size();
    std::string question = "Delete " + s.name + "?";
    if (held > 0) question += " " + std::to_string(held) + " of your characters hold it and would hold none.";
    if (!ui::yesNo(question, false)) return;
    try {
        const std::string name = s.name;
        deleteCustomSefirah(app, s.id);
        ui::info("Deleted " + name + ".");
    } catch (const StorageError& e) {
        ui::info(std::string("Deleting failed: ") + e.what());
    }
}

void viewSefirah(App& app, const std::string& id) {
    while (const Sefirah* s = app.db.findSefirah(id)) {
        std::cout << "\n" << renderText(buildSefirahSheet(*s, app.db));
        std::vector<std::string> options = {"Make one of your characters its holder", "Remove a holder", "Export"};
        if (s->custom) {
            options.push_back("Edit it");
            options.push_back("Delete it");
        } else {
            options.push_back("Copy it into a new Sefirah of your own");
        }
        const int pick = ui::choose("\nWhat now?", options, "Back to the list");
        if (pick == 0) addHolder(app, *s);
        else if (pick == 1) removeHolder(app, *s);
        else if (pick == 2) exportSefirah(app, id);
        else if (pick == 3 && s->custom) editOwnSefirah(app, *s, false);
        else if (pick == 3) {
            Sefirah copy = *s;
            copy.name += " (copy)";
            editOwnSefirah(app, copy, true);
            return;
        } else if (pick == 4) {
            deleteOwnSefirah(app, *s);
            return;
        } else {
            return;
        }
    }
}

}  // namespace

void runSefirotMenu(App& app) {
    while (true) {
        ui::header("Sefirot");
        if (app.db.sefirot.empty()) ui::info("No Sefirot found: sefirot.json is missing from " + app.storage.dataDir().string() + ".");
        ui::info("A Sefirah is a source of power that gathers the Uniquenesses of a group of neighbouring pathways.");
        ui::info("Holding one together with all of those Uniquenesses makes a Great Old One, Above the Sequence.");
        std::vector<std::string> names;
        for (const auto& s : app.db.sefirot) {
            std::string line = sefirahLabel(app.db, s.id);
            const size_t held = app.db.sefirahHolders(s.id).size();
            if (s.custom) line += " (your own)";
            if (held > 0) line += ", held by " + std::to_string(held) + " of your characters";
            names.push_back(line);
        }
        names.push_back("Create a new Sefirah of your own");
        const int pick = ui::choose("", names, "Back to the main menu");
        if (pick < 0) return;
        if (pick == static_cast<int>(app.db.sefirot.size())) {
            Sefirah fresh;
            editOwnSefirah(app, fresh, true);
            continue;
        }
        viewSefirah(app, app.db.sefirot[static_cast<size_t>(pick)].id);
    }
}

}  // namespace lotm
