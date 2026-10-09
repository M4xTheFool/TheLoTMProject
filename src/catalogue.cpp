#include <algorithm>
#include <iostream>
#include <optional>

#include "app.hpp"
#include "records.hpp"
#include "render.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

std::string pad(std::string text, size_t width) {
    if (text.size() > width) text = text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    return ui::toLower(haystack).find(ui::toLower(needle)) != std::string::npos;
}

// Shared list state: search text, pathway filter, sort order.
struct ListFilter {
    std::string search;
    std::string pathwayId;
    int sort = 0;  // 0 name, 1 pathway, 2 sequence (strongest first)
    static constexpr const char* kSortNames[3] = {"name", "pathway", "sequence"};

    std::string describe(const App& app) const {
        std::string text = "Sorted by " + std::string(kSortNames[sort]);
        if (!search.empty()) text += ", name contains \"" + search + "\"";
        if (!pathwayId.empty()) text += ", pathway " + pathwayLabel(app.db, pathwayId);
        return text;
    }

    // Handles the shared commands. Returns false when the input was not one of them.
    bool handle(const App& app, const std::string& input) {
        if (input == "/" || (!input.empty() && input[0] == '/')) {
            search = input.size() > 1 ? ui::trim(input.substr(1)) : ui::readLine("Search names for: ");
            return true;
        }
        if (input == "f" || input == "F") {
            int pick = choosePathway(app, "Show only this pathway:", "All pathways", false);
            pathwayId = pick >= 0 ? app.db.pathways[pick].id : "";
            return true;
        }
        if (input == "s" || input == "S") {
            sort = (sort + 1) % 3;
            return true;
        }
        if (input == "c" || input == "C") {
            search.clear();
            pathwayId.clear();
            sort = 0;
            return true;
        }
        return false;
    }
};

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
    ListFilter filter;
    while (true) {
        std::vector<const Character*> rows;
        for (const auto& c : app.db.characters) {
            if (!filter.search.empty() && !containsIgnoreCase(c.name, filter.search)) continue;
            if (!filter.pathwayId.empty() && c.pathwayId != filter.pathwayId) continue;
            rows.push_back(&c);
        }
        std::stable_sort(rows.begin(), rows.end(), [&](const Character* a, const Character* b) {
            if (filter.sort == 1 && a->pathwayId != b->pathwayId) {
                return pathwayLabel(app.db, a->pathwayId) < pathwayLabel(app.db, b->pathwayId);
            }
            if (filter.sort == 2) {
                const int sa = a->pathwayId.empty() ? 10 : a->sequence;
                const int sb = b->pathwayId.empty() ? 10 : b->sequence;
                if (sa != sb) return sa < sb;
            }
            return ui::toLower(a->name) < ui::toLower(b->name);
        });

        ui::header("Characters (" + std::to_string(rows.size()) + " of " + std::to_string(app.db.characters.size()) + ")");
        ui::info(filter.describe(app));
        if (rows.empty()) ui::info("Nothing to show.");
        for (size_t i = 0; i < rows.size(); ++i) {
            const Character& c = *rows[i];
            const Pathway* p = app.db.findPathway(c.pathwayId);
            std::string path = p ? p->name + ", Seq " + std::to_string(c.sequence) : "Mortal";
            std::cout << "  " << pad(std::to_string(i + 1) + ")", 5) << pad(characterCode(c.id), 7) << pad(c.name, 26)
                      << pad(path, 26) << c.alignment << "\n";
        }
        std::string input = ui::readLine("\nNumber to open, / search, F filter by pathway, S sort, C clear, Enter back: ");
        if (input.empty()) return;
        if (filter.handle(app, input)) continue;
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
    ListFilter filter;
    while (true) {
        std::vector<const Artifact*> rows;
        for (const auto& a : app.db.artifacts) {
            if (!filter.search.empty() && !containsIgnoreCase(a.name, filter.search)) continue;
            if (!filter.pathwayId.empty() && a.pathwayId != filter.pathwayId) continue;
            rows.push_back(&a);
        }
        std::stable_sort(rows.begin(), rows.end(), [&](const Artifact* a, const Artifact* b) {
            if (filter.sort == 1 && a->pathwayId != b->pathwayId) {
                return pathwayLabel(app.db, a->pathwayId) < pathwayLabel(app.db, b->pathwayId);
            }
            if (filter.sort == 2) {
                const int sa = a->sequenceLevel == kUnknownSequence ? 10 : a->sequenceLevel;
                const int sb = b->sequenceLevel == kUnknownSequence ? 10 : b->sequenceLevel;
                if (sa != sb) return sa < sb;
            }
            return ui::toLower(a->name) < ui::toLower(b->name);
        });

        ui::header("Sealed Artifacts (" + std::to_string(rows.size()) + " of " + std::to_string(app.db.artifacts.size()) + ")");
        ui::info(filter.describe(app));
        if (rows.empty()) ui::info("Nothing to show.");
        for (size_t i = 0; i < rows.size(); ++i) {
            const Artifact& a = *rows[i];
            const Pathway* p = app.db.findPathway(a.pathwayId);
            std::string level = a.sequenceLevel == kUnknownSequence ? "Seq ?" : "Seq " + std::to_string(a.sequenceLevel);
            std::cout << "  " << pad(std::to_string(i + 1) + ")", 5) << pad(artifactCode(a.id), 7) << pad(a.name, 30)
                      << pad(p ? p->name : "Unknown", 22) << level << "\n";
        }
        std::string input = ui::readLine("\nNumber to open, / search, F filter by pathway, S sort, C clear, Enter back: ");
        if (input.empty()) return;
        if (filter.handle(app, input)) continue;
        if (auto index = parseIndex(input, rows.size())) {
            viewArtifact(app, rows[*index]->id);
            continue;
        }
        ui::info("Not a command from the list.");
    }
}

}  // namespace

void runCatalogue(App& app) {
    while (true) {
        ui::header("Catalogue");
        int pick = ui::choose("", {"Characters (" + std::to_string(app.db.characters.size()) + ")",
                                   "Sealed Artifacts (" + std::to_string(app.db.artifacts.size()) + ")"},
                              "Back to the main menu");
        if (pick == 0) browseCharacters(app);
        else if (pick == 1) browseArtifacts(app);
        else return;
    }
}

}  // namespace lotm
