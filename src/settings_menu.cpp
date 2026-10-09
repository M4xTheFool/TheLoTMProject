#include "app.hpp"
#include "presets.hpp"
#include "ui.hpp"

namespace lotm {

void runSettingsMenu(App& app) {
    Settings& s = app.db.settings;
    while (true) {
        ui::header("Settings");
        int pick = ui::choose("",
                              {"HP: " + s.hpMode + " (ask per character, always include, or never include)",
                               "Export theme: " + s.exportTheme + " (auto follows your system, or light / dark)",
                               "Backups kept per file: " + std::to_string(s.backupsToKeep)},
                              "Back to the main menu");
        if (pick == 0) {
            int m = ui::choose("HP mode:", kHpModes, "Cancel");
            if (m >= 0) s.hpMode = kHpModes[m];
        } else if (pick == 1) {
            int t = ui::choose("Export theme:", kExportThemes, "Cancel");
            if (t >= 0) s.exportTheme = kExportThemes[t];
        } else if (pick == 2) {
            s.backupsToKeep = ui::readInt("Backups to keep (0 turns backups off)", 0, 50, s.backupsToKeep);
        } else {
            return;
        }
        try {
            app.storage.saveSettings(app.db);
        } catch (const StorageError& e) {
            ui::info(std::string("Could not save settings: ") + e.what());
        }
    }
}

}  // namespace lotm
