#include <iostream>

#include "app.hpp"
#include "ui.hpp"

namespace lotm {

// Lists pathways under their group headings (neighbouring pathways sit together).
int choosePathway(const App& app, const std::string& title, const std::string& zeroLabel, bool allowKeep) {
    const auto& pathways = app.db.pathways;
    std::cout << title << "\n";
    std::string group = "\x01";  // never matches a real group name
    for (size_t i = 0; i < pathways.size(); ++i) {
        const std::string thisGroup = pathways[i].group.empty() ? "Custom pathways" : pathways[i].group;
        if (thisGroup != group) {
            group = thisGroup;
            std::cout << "  " << group << "\n";
        }
        std::cout << "    " << (i + 1 < 10 ? " " : "") << (i + 1) << ") " << pathways[i].name;
        if (!pathways[i].god.empty()) std::cout << " (" << pathways[i].god << ")";
        std::cout << "\n";
    }
    if (!zeroLabel.empty()) std::cout << "     0) " << zeroLabel << "\n";

    while (true) {
        std::string value = ui::readLine(allowKeep ? "> (Enter keeps current) " : "> ");
        if (value.empty() && allowKeep) return ui::Choice::kKeep;
        try {
            size_t used = 0;
            int number = std::stoi(value, &used);
            if (used == value.size()) {
                if (number == 0 && !zeroLabel.empty()) return ui::Choice::kZero;
                if (number >= 1 && number <= static_cast<int>(pathways.size())) return number - 1;
            }
        } catch (const std::exception&) {
        }
        ui::info("Please type a number from the list.");
    }
}

}  // namespace lotm
