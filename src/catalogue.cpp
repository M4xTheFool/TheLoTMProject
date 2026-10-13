#include <algorithm>
#include <cctype>
#include <iterator>
#include <iostream>
#include <optional>

#include "app.hpp"
#include "compare.hpp"
#include "list_filter.hpp"
#include "records.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

std::string pad(std::string text, size_t width) {
    if (text.size() > width) text = text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

// The filter and sort commands both lists share. Returns false when the input was not one of them.
bool handleFilterCommand(const App& app, RecordFilter& filter, const std::string& input,
                         const std::vector<int>& bands, const std::vector<std::string>& names) {
    if (!input.empty() && input[0] == '/') {
        filter.search = input.size() > 1 ? ui::trim(input.substr(1)) : ui::readLine("Search names, aliases or IDs for: ");
        return true;
    }
    const std::string command = ui::toLower(input);
    if (command == "f") {
        int pick = choosePathway(app, "Show only this pathway:", "All pathways", false);
        filter.pathwayId = pick >= 0 ? app.db.pathways[pick].id : "";
        return true;
    }
    if (command == "p") {
        std::vector<std::string> options;
        for (int band : bands) options.push_back(powerBandName(band));
        const int pick = ui::choose("Show only this power:", options, "Any power");
        filter.power = pick >= 0 ? bands[static_cast<size_t>(pick)] : kAnyPower;
        return true;
    }
    if (command == "l" || (command.size() == 2 && command[0] == 'l' && std::isalpha(static_cast<unsigned char>(command[1])))) {
        std::string letter = command.size() == 2 ? command.substr(1) : "";
        if (letter.empty()) {
            std::string used;
            for (char ch : firstLetters(names)) used += std::string(used.empty() ? "" : " ") + ch;
            letter = ui::readLine("First letter (" + (used.empty() ? std::string("none yet") : used) +
                                  "), Enter for any: ");
        }
        filter.letter = letter.empty() ? 0 : firstLetter(letter);
        return true;
    }
    if (command == "s") {
        const auto next = (static_cast<int>(filter.sort) + 1) % static_cast<int>(std::size(kSortOrders));
        filter.sort = kSortOrders[next];
        return true;
    }
    if (command == "c") {
        filter.clear();
        filter.sort = SortOrder::Name;
        return true;
    }
    return false;
}

const char* kListCommands =
    "\nNumber to open, / search (name, alias or ID), F pathway, P power, L first letter (or LA, LB ...), S sort,\n"
    "C clear, Enter back: ";

std::optional<int> parseIndex(const std::string& input, size_t count) {
    try {
        size_t used = 0;
        int number = std::stoi(input, &used);
        if (used == input.size() && number >= 1 && number <= static_cast<int>(count)) return number - 1;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- characters

void removeCharacter(App& app, int id) {
    try {
        deleteCharacter(app, id);
        ui::info("Deleted.");
    } catch (const StorageError& e) {
        ui::info(std::string("Deleting failed: ") + e.what());
    }
}

void copyCharacter(App& app, int id) {
    try {
        const int copyId = duplicateCharacter(app, id);
        ui::info("Saved the copy as " + characterCode(copyId) + ". Open it from the list to edit it.");
    } catch (const StorageError& e) {
        ui::info(std::string("Copying failed: ") + e.what());
    }
}

void viewCharacter(App& app, int id) {
    while (const Character* c = app.db.findCharacter(id)) {
        std::cout << "\n" << renderText(buildCharacterSheet(*c, app.db));
        int pick = ui::choose("\nWhat now?", {"Edit", "Export", "Duplicate", "Delete"}, "Back to the list");
        if (pick == 0) runCharacterCreator(app, id);
        else if (pick == 1) exportCharacter(app, id);
        else if (pick == 2) copyCharacter(app, id);
        else if (pick == 3) {
            if (ui::yesNo("Delete " + c->name + " for good?", false)) {
                removeCharacter(app, id);
                return;
            }
        } else return;
    }
}

void browseCharacters(App& app) {
    RecordFilter filter;
    while (true) {
        const std::vector<const Character*> rows = filterCharacters(app.db, filter);
        std::vector<std::string> names;
        for (const auto& c : app.db.characters) names.push_back(c.name);

        ui::header("Characters (" + std::to_string(rows.size()) + " of " + std::to_string(app.db.characters.size()) + ")");
        ui::info(filter.describe(app.db));
        if (rows.empty()) ui::info("Nothing to show.");
        for (size_t i = 0; i < rows.size(); ++i) {
            const Character& c = *rows[i];
            const Pathway* p = app.db.findPathway(c.pathwayId);
            std::string path = !p ? "Mortal" : c.aboveSequence ? p->name + ", Above" : p->name + ", Seq " + std::to_string(c.sequence);
            if (isMixedPathway(c)) path += " (mixed)";
            std::cout << "  " << pad(std::to_string(i + 1) + ")", 5) << pad(characterCode(c.id), 7) << pad(c.name, 26)
                      << pad(path, 26) << c.alignment << "\n";
        }
        std::string input = ui::readLine(kListCommands);
        if (input.empty()) return;
        if (handleFilterCommand(app, filter, input, characterPowerBands(), names)) continue;
        if (auto index = parseIndex(input, rows.size())) {
            viewCharacter(app, rows[*index]->id);
            continue;
        }
        ui::info("Not a command from the list.");
    }
}

// ---------------------------------------------------------------- artifacts

void removeArtifact(App& app, int id) {
    try {
        deleteArtifact(app, id);
        ui::info("Deleted, and removed from every character who held it.");
    } catch (const StorageError& e) {
        ui::info(std::string("Deleting failed: ") + e.what());
    }
}

void viewArtifact(App& app, int id) {
    while (const Artifact* a = app.db.findArtifact(id)) {
        std::cout << "\n" << renderText(buildArtifactSheet(*a, app.db));
        int pick = ui::choose("\nWhat now?", {"Edit", "Export", "Delete"}, "Back to the list");
        if (pick == 0) runArtifactCreator(app, id);
        else if (pick == 1) exportArtifact(app, id);
        else if (pick == 2) {
            if (ui::yesNo("Delete " + a->name + " for good?", false)) {
                removeArtifact(app, id);
                return;
            }
        } else return;
    }
}

void browseArtifacts(App& app) {
    RecordFilter filter;
    while (true) {
        const std::vector<const Artifact*> rows = filterArtifacts(app.db, filter);
        std::vector<std::string> names;
        for (const auto& a : app.db.artifacts) names.push_back(a.name);

        ui::header("Sealed Artifacts (" + std::to_string(rows.size()) + " of " + std::to_string(app.db.artifacts.size()) + ")");
        ui::info(filter.describe(app.db));
        if (rows.empty()) ui::info("Nothing to show.");
        for (size_t i = 0; i < rows.size(); ++i) {
            const Artifact& a = *rows[i];
            const Pathway* p = app.db.findPathway(a.pathwayId);
            std::string level = a.sequenceLevel == kUnknownSequence ? "Seq ?" : "Seq " + std::to_string(a.sequenceLevel);
            std::cout << "  " << pad(std::to_string(i + 1) + ")", 5) << pad(artifactCode(a.id), 7) << pad(a.name, 30)
                      << pad(p ? p->name : "Unknown", 22) << level << "\n";
        }
        std::string input = ui::readLine(kListCommands);
        if (input.empty()) return;
        if (handleFilterCommand(app, filter, input, artifactPowerBands(), names)) continue;
        if (auto index = parseIndex(input, rows.size())) {
            viewArtifact(app, rows[*index]->id);
            continue;
        }
        ui::info("Not a command from the list.");
    }
}

// Pick two to four characters and show them side by side.
void compareCharacters(App& app) {
    if (app.db.characters.size() < 2) {
        ui::info("Save at least two characters to compare them.");
        ui::pause();
        return;
    }
    std::vector<const Character*> picked;
    while (picked.size() < 4) {
        std::vector<std::string> options;
        std::vector<const Character*> offered;
        for (const auto& c : app.db.characters) {
            if (std::find(picked.begin(), picked.end(), &c) != picked.end()) continue;
            options.push_back(c.name + " (" + characterCode(c.id) + ")");
            offered.push_back(&c);
        }
        if (offered.empty()) break;
        const std::string title = picked.empty() ? "Compare who? Pick the first character:"
                                                 : "Add character " + std::to_string(picked.size() + 1) + ":";
        const std::string zero = picked.size() < 2 ? "Cancel" : "That's everyone, compare them";
        const int pick = ui::choose(title, options, zero);
        if (pick < 0) {
            if (picked.size() < 2) return;
            break;
        }
        picked.push_back(offered[static_cast<size_t>(pick)]);
    }
    ui::blank();
    std::cout << renderComparisonText(buildComparison(picked, app.db));
    ui::pause();
}

}  // namespace

void runCatalogue(App& app) {
    while (true) {
        ui::header("Catalogue");
        int pick = ui::choose("", {"Characters (" + std::to_string(app.db.characters.size()) + ")",
                                   "Sealed Artifacts (" + std::to_string(app.db.artifacts.size()) + ")",
                                   "Compare characters side by side"},
                              "Back to the main menu");
        if (pick == 0) browseCharacters(app);
        else if (pick == 1) browseArtifacts(app);
        else if (pick == 2) compareCharacters(app);
        else return;
    }
}

}  // namespace lotm
