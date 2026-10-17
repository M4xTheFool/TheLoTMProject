// The running program's state, and the screens reachable from the main menu.
#pragma once

#include <optional>
#include <string>

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
void runPathwayMenu(App& app);
void runSefirotMenu(App& app);
void runTimelineMenu(App& app);
void runWebMenu(App& app);
void runDiceMenu(App& app);
void runOrganizationMenu(App& app);
// The character creator's Stages step: how they were earlier in the story, each from a date on.
void editStages(const App& app, Character& c);
// The creator's questions about Sequence 1 (Beyonder characteristics, the Uniqueness) and Sequence 0 (the
// absorbed Uniqueness), and about a second pathway, for a stage as well.
void editSequenceOne(const App& app, Character& c);
void editSecondPathway(const App& app, Character& c);
// Lists abilities ("Name: what it does") and lets you add, change, move and remove them.
void editAbilityList(std::vector<std::string>& abilities, const std::string& whose);

// Shared helpers used by several screens.
// A saved character from a numbered list, or 0.
int chooseCharacter(const App& app, const std::string& title, const std::string& zeroLabel = "Cancel");
int choosePathway(const App& app, const std::string& title, const std::string& zeroLabel, bool allowKeep);
// Reads a date in the story until it makes sense. Enter keeps the current one, "-" clears it (unless required).
StoryDate readStoryDate(const std::string& prompt, const StoryDate& current, bool required = false);
void exportCharacter(App& app, int id);
void exportArtifact(App& app, int id);
void exportPathway(App& app, const std::string& id);
void exportSefirah(App& app, const std::string& id);
void exportOrganization(App& app, int id);
// Every story event, or one character's (characterId != 0).
void exportTimeline(App& app, int characterId);
void exportWeb(App& app);

}  // namespace lotm
