// Organizations menu: the groups your characters belong to. Each one's members are the characters whose
// affiliation names it, so adding someone here changes their affiliation. Organizations your characters
// name that aren't written up yet are offered too, ready to fill in.
#include <algorithm>
#include <iostream>

#include "app.hpp"
#include "compare.hpp"
#include "organizations.hpp"
#include "records.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "story.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

std::string countOf(size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

// Toggles the pathways it is known for: a number adds or removes one, Enter finishes.
void choosePathways(const App& app, Organization& o) {
    while (true) {
        ui::info("Pathways it is known for (the first gives it its colour):");
        for (size_t i = 0; i < app.db.pathways.size(); ++i) {
            const Pathway& p = app.db.pathways[i];
            const bool on = std::find(o.pathwayIds.begin(), o.pathwayIds.end(), p.id) != o.pathwayIds.end();
            ui::info(std::string(on ? "[x] " : "[ ] ") + std::to_string(i + 1) + ") " + pathwayLabel(app.db, p.id));
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
        auto it = std::find(o.pathwayIds.begin(), o.pathwayIds.end(), id);
        if (it == o.pathwayIds.end()) o.pathwayIds.push_back(id);
        else o.pathwayIds.erase(it);
    }
}

// Its members first, then any other saved character, or someone typed by name.
void chooseLeader(const App& app, Organization& o) {
    const std::string current = leaderName(o, app.db);
    std::vector<const Character*> offered;
    for (const Member& m : membersOf(o, app.db)) offered.push_back(m.character);
    for (const Character& c : app.db.characters) {
        if (std::find(offered.begin(), offered.end(), &c) == offered.end()) offered.push_back(&c);
    }
    std::vector<std::string> options;
    for (const Character* c : offered) options.push_back(c->name + " (" + characterCode(c->id) + ")");
    options.push_back("Someone who isn't a saved character (type their name)");
    const int pick = ui::choose("Who leads it?" + (current.empty() ? std::string() : " [current: " + current + "]"), options,
                                "Nobody, or not known", true);
    if (pick == ui::Choice::kKeep) return;
    if (pick == ui::Choice::kZero) {
        o.leaderId = 0;
        o.leader.clear();
    } else if (pick == static_cast<int>(offered.size())) {
        const std::string typed = o.leaderId == 0 ? o.leader : "";
        o.leaderId = 0;
        o.leader = ui::readRequired("Their name", typed);
    } else {
        o.leaderId = offered[static_cast<size_t>(pick)]->id;
        o.leader = offered[static_cast<size_t>(pick)]->name;
    }
}

// Asks for every field, then saves it.
void editOrganization(App& app, Organization o, bool isNew) {
    ui::header(isNew ? "New organization" : "Edit " + o.name);
    ui::info("Enter keeps each value, \"-\" clears it.");
    o.name = ui::readRequired("Name (like the Tarot Club)", o.name);
    ui::editList("Other names it goes by (an affiliation can use these too)", o.alsoKnownAs);
    o.kind = ui::choosePreset("Kind", kOrganizationKinds, o.kind);
    o.status = ui::choosePreset("Status", kOrganizationStatuses, o.status);
    chooseLeader(app, o);
    o.headquarters = ui::readText("Headquarters (optional)", o.headquarters);
    o.founded = readStoryDate("Founded (e.g. 1349; Enter for not known)", o.founded);
    o.symbol = ui::readText("Its symbol, or how members know each other (optional)", o.symbol);
    choosePathways(app, o);
    ui::editList("Ranks, highest first (members pick from these)", o.ranks);
    o.description = ui::readMultiline("What it is and what it wants (optional)", o.description);
    ui::editList("Members who aren't saved characters, one line each", o.otherMembers);
    o.notes = ui::readMultiline("Notes (optional)", o.notes);
    try {
        const std::string oldName = isNew ? "" : app.db.findOrganization(o.id) ? app.db.findOrganization(o.id)->name : "";
        const int renamed = saveOrganization(app, o, isNew);
        ui::info("Saved " + o.name + " as " + organizationCode(o.id) + ".");
        if (renamed > 0) {
            ui::info("Changed " + oldName + " to the new name on " +
                     (renamed == 1 ? std::string("1 character's page") : std::to_string(renamed) + " characters' pages") + " too.");
        }
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
    }
}

void addMember(App& app, const Organization& o) {
    std::vector<const Character*> offered;
    std::vector<std::string> names;
    for (const Character& c : app.db.characters) {
        if (organizationNamed(app.db, c.affiliation.organization) == &o) continue;
        offered.push_back(&c);
        std::string line = c.name + " (" + characterCode(c.id) + ")";
        if (!isIndependent(c.affiliation.organization)) line += ", in " + c.affiliation.organization;
        names.push_back(line);
    }
    if (offered.empty()) {
        ui::info("Every saved character belongs to it already, or there are none yet.");
        return;
    }
    const int pick = ui::choose("Who joins " + o.name + "?", names, "Cancel");
    if (pick < 0) return;
    const Character& c = *offered[static_cast<size_t>(pick)];
    if (!isIndependent(c.affiliation.organization) &&
        !ui::yesNo(c.name + " belongs to " + c.affiliation.organization + ". A character belongs to one organization; move them?", true)) {
        return;
    }
    const std::string rank = ui::choosePreset("Their rank or role", rankSuggestions(app.db, o.name), "");
    try {
        const std::string name = c.name;
        joinOrganization(app, c.id, o, rank);
        ui::info(name + " belongs to " + o.name + " now.");
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
    }
}

void removeMember(App& app, const Organization& o) {
    const std::vector<Member> members = membersOf(o, app.db);
    if (members.empty()) {
        ui::info("None of your characters belongs to it.");
        return;
    }
    std::vector<std::string> names;
    for (const Member& m : members) names.push_back(m.character->name + (m.rank.empty() ? "" : ", " + m.rank));
    const int pick = ui::choose("Who leaves " + o.name + "? (their affiliation is cleared)", names, "Cancel");
    if (pick < 0) return;
    try {
        const std::string name = members[static_cast<size_t>(pick)].character->name;
        leaveOrganization(app, members[static_cast<size_t>(pick)].character->id);
        ui::info(name + " no longer belongs to " + o.name + ".");
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
    }
}

void viewOrganization(App& app, int id) {
    while (const Organization* o = app.db.findOrganization(id)) {
        std::cout << "\n" << renderText(buildOrganizationSheet(*o, app.db));
        const int pick = ui::choose("\nWhat now?", {"Edit", "Add a member", "Take a member out", "Export", "Delete"},
                                    "Back to the list");
        if (pick == 0) editOrganization(app, *o, false);
        else if (pick == 1) addMember(app, *o);
        else if (pick == 2) removeMember(app, *o);
        else if (pick == 3) exportOrganization(app, id);
        else if (pick == 4) {
            if (!ui::yesNo("Delete " + o->name + "? Its members keep it as their affiliation, as typed text.", false)) continue;
            try {
                const std::string name = o->name;
                deleteOrganization(app, id);
                ui::info("Deleted " + name + ".");
            } catch (const StorageError& e) {
                ui::info(std::string("Deleting failed: ") + e.what());
            }
            return;
        } else {
            return;
        }
    }
}

// Pick two to four organizations and show them side by side.
void compareOrganizations(App& app) {
    const std::vector<const Organization*> all = organizationsByName(app.db);
    std::vector<const Organization*> picked;
    while (picked.size() < 4) {
        std::vector<std::string> options;
        std::vector<const Organization*> offered;
        for (const Organization* o : all) {
            if (std::find(picked.begin(), picked.end(), o) != picked.end()) continue;
            options.push_back(o->name + " (" + organizationCode(o->id) + ")");
            offered.push_back(o);
        }
        if (offered.empty()) break;
        const std::string title = picked.empty() ? "Compare which? Pick the first organization:"
                                                 : "Add organization " + std::to_string(picked.size() + 1) + ":";
        const int pick = ui::choose(title, options, picked.size() < 2 ? "Cancel" : "That's all, compare them");
        if (pick < 0) {
            if (picked.size() < 2) return;
            break;
        }
        picked.push_back(offered[static_cast<size_t>(pick)]);
    }
    ui::blank();
    std::cout << renderComparisonText(buildOrganizationComparison(picked, app.db));
    ui::pause();
}

}  // namespace

void runOrganizationMenu(App& app) {
    while (true) {
        ui::header("Organizations");
        ui::info("The groups your characters belong to. A character is a member when their affiliation names it.");
        const std::vector<const Organization*> saved = organizationsByName(app.db);
        const std::vector<UnsavedOrganization> unsaved = organizationsNotSaved(app.db);
        std::vector<std::string> options;
        for (const Organization* o : saved) {
            const size_t members = membersOf(*o, app.db).size() + static_cast<size_t>(std::count_if(
                                       o->otherMembers.begin(), o->otherMembers.end(), [](const std::string& line) { return !ui::trim(line).empty(); }));
            std::string line = o->name + " (" + organizationCode(o->id);
            if (!o->kind.empty()) line += ", " + o->kind;
            options.push_back(line + ", " + countOf(members, "member", "members") + ")");
        }
        for (const UnsavedOrganization& u : unsaved) {
            options.push_back("Write up " + u.name + " (named by " + countOf(static_cast<size_t>(u.members), "character", "characters") +
                              (u.members == 0 ? " now, only in earlier stages" : "") + ")");
        }
        options.push_back("Create a new organization");
        if (saved.size() >= 2) options.push_back("Compare organizations side by side");
        const int pick = ui::choose("", options, "Back to the main menu");
        if (pick < 0) return;
        const size_t index = static_cast<size_t>(pick);
        if (index < saved.size()) {
            viewOrganization(app, saved[index]->id);
        } else if (index < saved.size() + unsaved.size()) {
            Organization fresh = organizationFromMembers(app.db, unsaved[index - saved.size()].name);
            fresh.id = app.db.nextOrganizationId();
            editOrganization(app, fresh, true);
        } else if (index == saved.size() + unsaved.size()) {
            Organization fresh;
            fresh.id = app.db.nextOrganizationId();
            editOrganization(app, fresh, true);
        } else {
            compareOrganizations(app);
        }
    }
}

}  // namespace lotm
