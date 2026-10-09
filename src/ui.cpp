#include "ui.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace lotm::ui {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string toLower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

void header(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

void info(const std::string& text) { std::cout << "  " << text << "\n"; }

void blank() { std::cout << "\n"; }

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed{};
    return trim(line);
}

std::string readRequired(const std::string& prompt, const std::string& current) {
    while (true) {
        std::string shown = prompt;
        if (!current.empty()) shown += " [" + current + "]";
        std::string value = readLine(shown + ": ");
        if (value.empty() && !current.empty()) return current;
        if (!value.empty() && value != "-") return value;
        info("This one is required.");
    }
}

std::string readText(const std::string& prompt, const std::string& current) {
    std::string shown = prompt;
    if (!current.empty()) shown += " [" + current + "]";
    std::string value = readLine(shown + ": ");
    if (value.empty()) return current;
    if (value == "-") return {};
    return value;
}

std::string readMultiline(const std::string& prompt, const std::string& current) {
    std::cout << prompt << "\n";
    if (!current.empty()) {
        std::string indented = current;
        for (size_t pos = 0; (pos = indented.find('\n', pos)) != std::string::npos; pos += 5) {
            indented.replace(pos, 1, "\n    ");
        }
        std::cout << "  Current text:\n";
        std::cout << "    " << indented << "\n";
        std::cout << "  Press Enter to keep it, type \"-\" to clear it, or type new text.\n";
    }
    std::cout << "  (Finish with an empty line.)\n";
    std::string text;
    bool first = true;
    while (true) {
        std::string line = readLine("  > ");
        if (first) {
            if (line.empty()) return current;
            if (line == "-") return {};
        }
        if (line.empty()) break;
        if (!first) text += "\n";
        text += line;
        first = false;
    }
    return text;
}

int readInt(const std::string& prompt, int min, int max, std::optional<int> current) {
    while (true) {
        std::string shown = prompt + " (" + std::to_string(min) + "-" + std::to_string(max) + ")";
        if (current) shown += " [" + std::to_string(*current) + "]";
        std::string value = readLine(shown + ": ");
        if (value.empty() && current) return *current;
        try {
            size_t used = 0;
            int number = std::stoi(value, &used);
            if (used == value.size() && number >= min && number <= max) return number;
        } catch (const std::exception&) {
        }
        info("Please type a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

bool yesNo(const std::string& prompt, std::optional<bool> current) {
    while (true) {
        std::string hint = current ? (*current ? " [Y/n]" : " [y/N]") : " [y/n]";
        std::string value = toLower(readLine(prompt + hint + ": "));
        if (value.empty() && current) return *current;
        if (value == "y" || value == "yes") return true;
        if (value == "n" || value == "no") return false;
        info("Please type y or n.");
    }
}

int choose(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel,
           bool allowKeep) {
    if (!title.empty()) std::cout << title << "\n";
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << "  " << (i + 1 < 10 && options.size() >= 10 ? " " : "") << (i + 1) << ") " << options[i] << "\n";
    }
    if (!zeroLabel.empty()) std::cout << "  " << (options.size() >= 10 ? " " : "") << "0) " << zeroLabel << "\n";
    while (true) {
        std::string value = readLine(allowKeep ? "> (Enter keeps current) " : "> ");
        if (value.empty() && allowKeep) return Choice::kKeep;
        try {
            size_t used = 0;
            int number = std::stoi(value, &used);
            if (used == value.size()) {
                if (number == 0 && !zeroLabel.empty()) return Choice::kZero;
                if (number >= 1 && number <= static_cast<int>(options.size())) return number - 1;
            }
        } catch (const std::exception&) {
        }
        info("Please type a number from the list.");
    }
}

void editList(const std::string& label, std::vector<std::string>& items) {
    while (true) {
        std::cout << label << ":\n";
        if (items.empty()) std::cout << "  (none yet)\n";
        for (size_t i = 0; i < items.size(); ++i) std::cout << "  " << (i + 1) << ") " << items[i] << "\n";
        std::string value = readLine("  Type a new entry to add it, a number to remove one, or Enter when done: ");
        if (value.empty()) return;
        bool isNumber = std::all_of(value.begin(), value.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
        if (isNumber) {
            int index = std::stoi(value);
            if (index >= 1 && index <= static_cast<int>(items.size())) {
                info("Removed \"" + items[index - 1] + "\".");
                items.erase(items.begin() + (index - 1));
                continue;
            }
            info("There is no entry " + value + ".");
            continue;
        }
        items.push_back(value);
    }
}

std::string choosePreset(const std::string& label, const std::vector<std::string>& presets,
                         const std::string& current) {
    std::vector<std::string> options = presets;
    options.push_back("Type my own");
    std::string title = label + (current.empty() ? "" : " [current: " + current + "]");
    int pick = choose(title, options, current.empty() ? "Skip" : "Clear it", true);
    if (pick == Choice::kKeep) return current;
    if (pick == Choice::kZero) return {};
    if (pick == static_cast<int>(presets.size())) return readText("  " + label, current);
    return presets[pick];
}

void pause() { readLine("Press Enter to continue..."); }

}  // namespace lotm::ui
