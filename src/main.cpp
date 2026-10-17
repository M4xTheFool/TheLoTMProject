// LoTM Creator Suite: create Lord of the Mysteries characters, Sealed Artifacts and organizations,
// browse them in the Catalogue, and export them as readable sheets.
#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "app.hpp"
#include "ui.hpp"

namespace {

void mainMenu(lotm::App& app) {
    while (true) {
        lotm::ui::header("LoTM Creator Suite");
        lotm::ui::info(std::to_string(app.db.characters.size()) + " characters, " +
                       std::to_string(app.db.artifacts.size()) + " Sealed Artifacts, " +
                       std::to_string(app.db.events.size()) + " story events, " +
                       std::to_string(app.db.organizations.size()) + " organizations, " +
                       std::to_string(app.db.pathways.size()) + " pathways and " +
                       std::to_string(app.db.sefirot.size()) + " Sefirot loaded.");
        int pick = lotm::ui::choose("", {"Create a character", "Create a Sealed Artifact", "Open the Catalogue",
                                         "Organizations (the groups your characters belong to)",
                                         "Timeline (story events, and everyone as they were on a date)",
                                         "Relationship web (who knows whom)",
                                         "Dice and checks (roll for your characters, or any dice)",
                                         "Pathways (look through them, or add your own)",
                                         "Sefirot (the sources of power, and who holds them)", "Export", "Settings"},
                                    "Quit");
        switch (pick) {
            case 0: lotm::runCharacterCreator(app); break;
            case 1: lotm::runArtifactCreator(app); break;
            case 2: lotm::runCatalogue(app); break;
            case 3: lotm::runOrganizationMenu(app); break;
            case 4: lotm::runTimelineMenu(app); break;
            case 5: lotm::runWebMenu(app); break;
            case 6: lotm::runDiceMenu(app); break;
            case 7: lotm::runPathwayMenu(app); break;
            case 8: lotm::runSefirotMenu(app); break;
            case 9: lotm::runExportMenu(app); break;
            case 10: lotm::runSettingsMenu(app); break;
            default: return;
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Lets the console show accented letters and other non-English text correctly.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    (void)argc;
    try {
        const auto dataDir = lotm::findDataDir(argv[0]);
        lotm::App app{lotm::Database{}, lotm::Storage{dataDir}, lotm::Dice{}};
        app.storage.loadAll(app.db);
        std::cout << "Data folder: " << dataDir.string() << "\n";
        mainMenu(app);
        std::cout << "Goodbye.\n";
        return 0;
    } catch (const lotm::ui::InputClosed&) {
        std::cout << "\nGoodbye.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
#ifdef _WIN32
        // Keep the window open when the program was started by double-clicking it.
        std::cerr << "Press Enter to close.";
        std::cin.get();
#endif
        return 1;
    }
}
