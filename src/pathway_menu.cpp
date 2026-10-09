// The Pathways screen: look through every pathway, and create, edit or delete your own.
// Your own pathways are saved to data/custom_pathways.json; pathways.json is never changed here.
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>
#include <utility>

#include "app.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

const std::vector<std::string> kSpeedGrades = {"Slow", "Average", "Fast", "Very fast"};

bool isNumber(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
}

// "Name: what it does" -> {"Name", "what it does"}. Text without a name comes back as the description.
std::pair<std::string, std::string> splitAbility(const std::string& ability) {
    const size_t colon = ability.find(": ");
    if (colon == std::string::npos || colon == 0 || colon > 48 || ability.substr(0, colon).find(". ") != std::string::npos) {
        return {"", ability};
    }
    return {ability.substr(0, colon), ability.substr(colon + 2)};
}

std::string joinAbility(const std::string& name, const std::string& description) {
    if (name.empty()) return description;
    if (description.empty()) return name;
    return name + ": " + description;
}

// Several typed lines become one ability text.
std::string oneLine(std::string text) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    return ui::trim(text);
}

std::string shorten(const std::string& text, size_t width) {
    if (text.size() <= width) return text;
    size_t cut = width;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;  // don't split a UTF-8 letter
    return text.substr(0, cut) + "...";
}

SequenceInfo& sequenceOf(Pathway& p, int sequence) {
    for (auto& s : p.sequences) {
        if (s.sequence == sequence) return s;
    }
    p.sequences.push_back({sequence, "", {}});
    std::sort(p.sequences.begin(), p.sequences.end(),
              [](const SequenceInfo& a, const SequenceInfo& b) { return a.sequence > b.sequence; });
    for (auto& s : p.sequences) {
        if (s.sequence == sequence) return s;
    }
    return p.sequences.front();  // not reached
}

std::vector<int> customIndexes(const Database& db) {
    std::vector<int> indexes;
    for (size_t i = 0; i < db.pathways.size(); ++i) {
        if (db.pathways[i].custom) indexes.push_back(static_cast<int>(i));
    }
    return indexes;
}

// Picks one of your own pathways. Returns its id, or nothing.
std::optional<std::string> chooseCustomPathway(const App& app, const std::string& title) {
    const auto indexes = customIndexes(app.db);
    if (indexes.empty()) {
        ui::info("You haven't made any pathways yet. Choose \"Create a new pathway\" to start one.");
        return std::nullopt;
    }
    std::vector<std::string> names;
    for (int i : indexes) names.push_back(pathwayLabel(app.db, app.db.pathways[i].id));
    const int pick = ui::choose(title, names, "Cancel");
    if (pick < 0) return std::nullopt;
    return app.db.pathways[indexes[pick]].id;
}

// ---------------------------------------------------------------- editing steps

void stepNames(const Database& db, Pathway& p) {
    ui::header("1. Name, god and neighbours");
    ui::info("Pathways are usually named after their Sequence 9, like Seer or Apprentice.");
    p.name = ui::readRequired("Pathway name", p.name);
    if (p.sequences.empty() || sequenceOf(p, 9).name.empty()) sequenceOf(p, 9).name = p.name;

    ui::blank();
    ui::info("The god at the top of the pathway (Sequence 0), like The Fool or Door.");
    p.god = ui::readText("God", p.god);

    ui::blank();
    std::vector<std::string> groups;
    for (const auto& other : db.pathways) {
        if (!other.group.empty() && std::find(groups.begin(), groups.end(), other.group) == groups.end()) {
            groups.push_back(other.group);
        }
    }
    ui::info("Neighbouring pathways can swap between each other. A pathway with no neighbours can name itself.");
    p.group = ui::choosePreset("Group of neighbouring pathways", groups, p.group);
}

std::string chooseStat(const std::string& label, const std::string& current) {
    std::vector<std::string> options;
    for (int i = 0; i < kStatCount; ++i) options.push_back(std::string(kStatNames[i]) + " (" + kStatCodes[i] + ")");
    const int pick = ui::choose(label + (current.empty() ? "" : " [current: " + current + "]"), options,
                                current.empty() ? "None" : "Clear it", true);
    if (pick == ui::Choice::kKeep) return current;
    if (pick == ui::Choice::kZero) return {};
    return kStatCodes[pick];
}

void stepStats(Pathway& p) {
    ui::header("2. Stats and speed");
    ui::info("The primary stat gets +1 per speed tier, the secondary stat +1 every two tiers.");
    p.primaryStat = chooseStat("Primary stat", p.primaryStat);
    ui::blank();
    p.secondaryStat = chooseStat("Secondary stat", p.secondaryStat);

    ui::blank();
    ui::info("Speed grade moves the Speed score inside its tier: Slow -2, Average 0, Fast +2, Very fast +4.");
    const int pick = ui::choose("Speed grade [current: " + p.speedGrade + "]", kSpeedGrades, "", true);
    if (pick >= 0) p.speedGrade = kSpeedGrades[pick];
    p.speedNote = ui::readText("Speed note (optional, e.g. \"Counts as Fast at night\")", p.speedNote);
}

void stepLore(Pathway& p) {
    ui::header("3. Overview and Uniqueness");
    p.description = ui::readMultiline("Overview (optional): themes, Sefirah, Divine Kingdom, anything worth knowing",
                                      p.description);
    ui::blank();
    ui::info("The Uniqueness: its name and what it looks like. Sequence 1 characters who hold it start from this.");
    p.uniqueness = ui::readMultiline("Uniqueness (optional)", p.uniqueness);
}

void editAbility(std::vector<std::string>& abilities, size_t index) {
    auto [name, description] = splitAbility(abilities[index]);
    std::cout << "\n" << abilities[index] << "\n";
    const int pick = ui::choose("", {"Change the name", "Change what it does", "Move it up", "Remove it"}, "Back");
    if (pick == 0) {
        name = ui::readText("Name", name);
        abilities[index] = joinAbility(name, description);
    } else if (pick == 1) {
        description = oneLine(ui::readMultiline("What it does", description));
        abilities[index] = joinAbility(name, description);
    } else if (pick == 2 && index > 0) {
        std::swap(abilities[index], abilities[index - 1]);
    } else if (pick == 3) {
        ui::info("Removed " + (name.empty() ? std::string("the ability") : name) + ".");
        abilities.erase(abilities.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void editAbilities(SequenceInfo& s) {
    while (true) {
        std::cout << "Abilities of " << sequenceLabel(nullptr, s.sequence) << (s.name.empty() ? "" : ": " + s.name)
                  << ":\n";
        if (s.abilities.empty()) std::cout << "  (none yet)\n";
        for (size_t i = 0; i < s.abilities.size(); ++i) {
            std::cout << "  " << (i + 1) << ") " << shorten(s.abilities[i], 90) << "\n";
        }
        const std::string value =
            ui::readLine("  Type a new ability's name to add it, a number to change one, or Enter when done: ");
        if (value.empty()) return;
        if (isNumber(value)) {
            const int index = std::stoi(value);
            if (index >= 1 && index <= static_cast<int>(s.abilities.size())) editAbility(s.abilities, index - 1);
            else ui::info("There is no ability " + value + ".");
            continue;
        }
        // "Name: what it does" typed in one go is taken as it is.
        auto [name, description] = splitAbility(value);
        if (name.empty()) {
            name = value;
            description = oneLine(ui::readMultiline("What does " + name + " do?", ""));
        }
        s.abilities.push_back(joinAbility(name, description));
    }
}

void stepSequence(Pathway& p, int sequence) {
    SequenceInfo& s = sequenceOf(p, sequence);
    ui::header("Sequence " + std::to_string(sequence));
    s.name = ui::readText("Name of Sequence " + std::to_string(sequence), s.name);
    editAbilities(s);
}

void printSummary(const Pathway& p) {
    std::cout << "  " << p.name << (p.god.empty() ? "" : " (" + p.god + ")") << "\n";
    ui::info("Group: " + (p.group.empty() ? std::string("(none)") : p.group));
    ui::info("Stats: primary " + (p.primaryStat.empty() ? "-" : p.primaryStat) + ", secondary " +
             (p.secondaryStat.empty() ? "-" : p.secondaryStat) + ". Speed grade: " + p.speedGrade + ".");
    ui::info(std::string("Overview: ") + (p.description.empty() ? "(none)" : "written") +
             ". Uniqueness: " + (p.uniqueness.empty() ? "(none)" : shorten(p.uniqueness, 60)));
}

std::string sequenceMenuLine(const Pathway& p, int sequence) {
    const SequenceInfo* s = p.findSequence(sequence);
    std::string line = "Sequence " + std::to_string(sequence) + ": " +
                       (s && !s->name.empty() ? s->name : std::string("(no name yet)"));
    const size_t count = s ? s->abilities.size() : 0;
    line += ", " + std::to_string(count) + (count == 1 ? " ability" : " abilities");
    return line;
}

bool savePathway(App& app, Pathway& p, bool isNew) {
    const auto backup = app.db.pathways;
    p.custom = true;
    if (isNew) {
        p.id = app.db.newPathwayId(p.name);
        app.db.pathways.push_back(p);
    } else if (Pathway* stored = app.db.findPathway(p.id)) {
        *stored = p;
    }
    try {
        app.storage.saveCustomPathways(app.db);
        ui::info("Saved the " + p.name + " pathway (id \"" + p.id + "\"). It's in the pathway list everywhere now.");
        return true;
    } catch (const StorageError& e) {
        app.db.pathways = backup;
        ui::info(std::string("Saving failed: ") + e.what());
        return false;
    }
}

// Shows the review menu, after walking through every step first when walkThrough is set.
// Returns the saved pathway's id.
std::optional<std::string> runPathwayEditor(App& app, Pathway p, bool isNew, bool walkThrough) {
    if (walkThrough) {
        stepNames(app.db, p);
        stepStats(p);
        stepLore(p);
        ui::blank();
        if (ui::yesNo("Fill in the Sequences now, from 9 down to 0? (Enter skips any of them)", true)) {
            for (int seq = 9; seq >= 0; --seq) stepSequence(p, seq);
        }
    }
    while (true) {
        ui::header(isNew ? "Review the new pathway" : "Edit pathway");
        printSummary(p);
        std::vector<std::string> options = {"Name, god and group", "Stats and speed", "Overview and Uniqueness"};
        for (int seq = 9; seq >= 0; --seq) options.push_back(sequenceMenuLine(p, seq));
        options.push_back("Look at the whole pathway");
        options.push_back("Save");
        const int pick = ui::choose("\nChange something, or save:", options, "Cancel without saving");
        if (pick == 0) stepNames(app.db, p);
        else if (pick == 1) stepStats(p);
        else if (pick == 2) stepLore(p);
        else if (pick >= 3 && pick <= 12) stepSequence(p, 12 - pick);
        else if (pick == 13) std::cout << "\n" << renderText(buildPathwaySheet(p));
        else if (pick == 14) {
            if (savePathway(app, p, isNew)) return p.id;
        } else if (ui::yesNo("Discard these changes?", false)) {
            return std::nullopt;
        }
    }
}

std::optional<std::string> createPathway(App& app, const Pathway* copyFrom) {
    Pathway p;
    if (copyFrom) {
        p = *copyFrom;
        p.id.clear();
        p.name += " (copy)";
    } else {
        for (int seq = 9; seq >= 0; --seq) p.sequences.push_back({seq, "", {}});
    }
    p.custom = true;
    return runPathwayEditor(app, p, true, copyFrom == nullptr);
}

void deletePathway(App& app, const std::string& id) {
    const Pathway* p = app.db.findPathway(id);
    if (!p || !p->custom) return;
    const auto users = app.db.pathwayUsers(id);
    if (!users.empty()) {
        ui::info("These still belong to the " + p->name + " pathway, so it can't be deleted:");
        for (const auto& name : users) ui::info("  " + name);
        ui::info("Give them another pathway first (edit them from the Catalogue).");
        return;
    }
    if (!ui::yesNo("Delete the " + p->name + " pathway for good?", false)) return;
    const auto backup = app.db.pathways;
    auto& list = app.db.pathways;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Pathway& x) { return x.id == id; }), list.end());
    try {
        app.storage.saveCustomPathways(app.db);
        ui::info("Deleted. A copy is in the backups folder in case you change your mind.");
    } catch (const StorageError& e) {
        app.db.pathways = backup;
        ui::info(std::string("Deleting failed: ") + e.what());
    }
}

void viewPathway(App& app, std::string id) {
    while (const Pathway* p = app.db.findPathway(id)) {
        std::cout << "\n" << renderText(buildPathwaySheet(*p));
        std::vector<std::string> options = {"Export"};
        if (p->custom) {
            options.push_back("Edit");
            options.push_back("Delete");
        } else {
            options.push_back("Copy it into a new pathway of your own");
        }
        const int pick = ui::choose("\nWhat now?", options, "Back");
        if (pick == 0) {
            exportPathway(app, id);
        } else if (pick == 1 && p->custom) {
            runPathwayEditor(app, *p, false, false);
        } else if (pick == 1) {
            if (auto newId = createPathway(app, p)) id = *newId;
        } else if (pick == 2) {
            deletePathway(app, id);
        } else {
            return;
        }
    }
}

}  // namespace

void runPathwayMenu(App& app) {
    while (true) {
        ui::header("Pathways");
        const size_t custom = customIndexes(app.db).size();
        ui::info(std::to_string(app.db.pathways.size() - custom) + " built-in pathways and " + std::to_string(custom) +
                 " of your own.");
        const int pick = ui::choose("", {"Look at a pathway", "Create a new pathway", "Edit one of your pathways",
                                         "Delete one of your pathways"},
                                    "Back to the main menu");
        if (pick == 0) {
            const int which = choosePathway(app, "Which pathway?", "Cancel", false);
            if (which >= 0) viewPathway(app, app.db.pathways[which].id);
        } else if (pick == 1) {
            createPathway(app, nullptr);
        } else if (pick == 2) {
            if (auto id = chooseCustomPathway(app, "Which of your pathways?")) {
                if (const Pathway* p = app.db.findPathway(*id)) runPathwayEditor(app, *p, false, false);
            }
        } else if (pick == 3) {
            if (auto id = chooseCustomPathway(app, "Delete which of your pathways?")) deletePathway(app, *id);
        } else {
            return;
        }
    }
}

}  // namespace lotm
