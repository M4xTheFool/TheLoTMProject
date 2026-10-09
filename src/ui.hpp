// Console input and output helpers. Every prompt follows the same rules:
//   - type a number to pick from a list
//   - press Enter to keep the current value
//   - type "-" to clear an optional value
#pragma once

#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace lotm::ui {

// Thrown when input ends (for example the window is closed or Ctrl+Z / Ctrl+D is pressed).
class InputClosed : public std::exception {
public:
    const char* what() const noexcept override { return "input closed"; }
};

std::string trim(const std::string& text);
std::string toLower(std::string text);

void header(const std::string& title);
void info(const std::string& text);  // indented line
void blank();

std::string readLine(const std::string& prompt);
std::string readRequired(const std::string& prompt, const std::string& current = "");

// Shows the current value; Enter keeps it, "-" clears it.
std::string readText(const std::string& prompt, const std::string& current);

// Several lines, finished with an empty line. Enter on the first line keeps the current text.
std::string readMultiline(const std::string& prompt, const std::string& current);

int readInt(const std::string& prompt, int min, int max, std::optional<int> current = std::nullopt);
bool yesNo(const std::string& prompt, std::optional<bool> current = std::nullopt);

struct Choice {
    // Index into the options, kZero for the "0" entry, kKeep when Enter was pressed.
    static constexpr int kZero = -1;
    static constexpr int kKeep = -2;
};

// Numbered menu. zeroLabel (if not empty) adds a "0) ..." entry. allowKeep lets Enter return kKeep.
int choose(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel = "Back",
           bool allowKeep = false);

// Edits a list of short entries: type text to add, a number to remove, Enter to finish.
void editList(const std::string& label, std::vector<std::string>& items);

// Pick one of the presets, type your own, or keep / clear the current value.
std::string choosePreset(const std::string& label, const std::vector<std::string>& presets,
                         const std::string& current);

void pause();

}  // namespace lotm::ui
