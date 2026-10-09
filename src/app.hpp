// The running program's state, and the screens reachable from the main menu.
#pragma once

#include <optional>

#include "dice.hpp"
#include "model.hpp"
#include "storage.hpp"

namespace lotm {

struct App {
    Database db;
    Storage storage;
    Dice dice;
};

// Each returns the saved record's id, or nothing if the user cancelled.
std::optional<int> runArtifactCreator(App& app, std::optional<int> editId = std::nullopt);
std::optional<int> runCharacterCreator(App& app, std::optional<int> editId = std::nullopt);

void runCatalogue(App& app);
void runExportMenu(App& app);
void runSettingsMenu(App& app);

// Shared helpers used by several screens.
int choosePathway(const App& app, const std::string& title, const std::string& zeroLabel, bool allowKeep);
void exportCharacter(App& app, int id);
void exportArtifact(App& app, int id);

}  // namespace lotm
