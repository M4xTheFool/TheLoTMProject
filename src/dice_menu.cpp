// The console's Dice and checks menu: a character's check against a difficulty, two characters against
// each other, and any dice written like 2d6+3.
#include <string>
#include <vector>

#include "app.hpp"
#include "checks.hpp"
#include "rules.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

// Someone to roll for, or nobody in particular (0) when there are no characters or 0 is picked.
int chooseRoller(const App& app, const std::string& title) {
    if (app.db.characters.empty()) return 0;
    return chooseCharacter(app, title, "Nobody in particular");
}

// What the check is made with, each stat showing what they would add to the d20.
bool chooseCheckKind(const App& app, const Character* c, CheckSetup& setup) {
    std::vector<std::string> options;
    for (int kind = 0; kind < kCheckKinds; ++kind) {
        std::string line = checkName(kind);
        if (c) {
            CheckSetup s = setup;
            s.kind = kind;
            line += " (" + signedNumber(checkBonus(*c, app.db, s)) + ")";
        }
        options.push_back(line);
    }
    const int pick = ui::choose("Which check?", options, "Cancel");
    if (pick < 0) return false;
    setup.kind = pick;
    return true;
}

// Advantage, their tier and anything else that helps or hinders, asked with the usual answers ready.
void askBonuses(const Character* c, CheckSetup& setup) {
    const int mode = ui::choose("How do they roll? (Enter: normally)",
                                {"Normally", "With advantage (two d20s, the higher counts)",
                                 "With disadvantage (two d20s, the lower counts)"},
                                "", true);
    setup.mode = mode == 1 ? RollMode::Advantage : mode == 2 ? RollMode::Disadvantage : RollMode::Normal;
    if (c && speedTierOf(*c).tier > 0) {
        const SpeedTier tier = speedTierOf(*c);
        setup.addTier = ui::yesNo("Add their tier as a bonus (+" + std::to_string(tier.tier) + " " + tier.name + ")?", true);
    }
    setup.extra = ui::readInt("Anything else to add or take away (a good plan +2, in the dark -5)", -50, 50, 0);
}

int chooseDifficulty() {
    std::vector<std::string> options;
    for (const DifficultyLevel& level : kDifficulties) options.push_back(std::to_string(level.value) + " " + level.name);
    options.push_back("Another number");
    const int pick = ui::choose("How hard is it? (Enter: 15, Medium)", options, "", true);
    if (pick >= 0 && pick < static_cast<int>(kDifficulties.size())) return kDifficulties[static_cast<size_t>(pick)].value;
    if (pick == static_cast<int>(kDifficulties.size())) return ui::readInt("The number to reach", 1, 60, 15);
    return 15;
}

void showRoll(const CheckRoll& roll) {
    if (roll.d20s.size() > 1) {
        ui::info("The d20s: " + std::to_string(roll.d20s[0]) + " and " + std::to_string(roll.d20s[1]) + ", keeping " +
                 std::to_string(roll.natural()) + ".");
    }
}

void makeCheck(App& app) {
    const int id = chooseRoller(app, "Who makes the check?");
    const Character* c = app.db.findCharacter(id);
    CheckSetup setup;
    if (!chooseCheckKind(app, c, setup)) return;
    askBonuses(c, setup);
    const int difficulty = chooseDifficulty();
    while (true) {
        const CheckOutcome outcome = checkAgainst(rollCheck(app.dice, c, app.db, setup), difficulty);
        ui::blank();
        showRoll(outcome.roll);
        ui::info(outcome.describe());
        if (!ui::yesNo("Roll the same again?", false)) return;
    }
}

void makeContest(App& app) {
    if (app.db.characters.empty()) {
        ui::info("A contest needs characters. Make one first, or roll any dice instead.");
        ui::pause();
        return;
    }
    CheckSetup first, second;
    const int a = chooseCharacter(app, "Who is the first?");
    if (!a) return;
    if (!chooseCheckKind(app, app.db.findCharacter(a), first)) return;
    askBonuses(app.db.findCharacter(a), first);
    const int b = chooseCharacter(app, "Against whom?");
    if (!b) return;
    if (!chooseCheckKind(app, app.db.findCharacter(b), second)) return;
    askBonuses(app.db.findCharacter(b), second);
    while (true) {
        // Looked up again each time: nothing is saved in between, but it costs nothing.
        const Character* one = app.db.findCharacter(a);
        const Character* two = app.db.findCharacter(b);
        const ContestOutcome outcome = contest(rollCheck(app.dice, one, app.db, first), rollCheck(app.dice, two, app.db, second));
        ui::blank();
        ui::info(outcome.describe());
        if (!ui::yesNo("Roll the same again?", false)) return;
    }
}

void rollAnyDice(App& app) {
    ui::info("Write the dice the way tabletop games do: d20, 2d6+3, 1d8 + 1d4 - 1, or d% for a d100.");
    std::string last;
    while (true) {
        const std::string text = ui::trim(ui::readLine(last.empty() ? "Dice (Enter to go back): " : "Dice [" + last + "] (Enter rolls them again, 0 goes back): "));
        if (text == "0" || (text.empty() && last.empty())) return;
        const std::string wanted = text.empty() ? last : text;
        std::string problem;
        const auto formula = parseDiceFormula(wanted, &problem);
        if (!formula) {
            ui::info(problem);
            continue;
        }
        last = formula->text();
        ui::info(rollDice(app.dice, *formula).describe());
    }
}

}  // namespace

void runDiceMenu(App& app) {
    while (true) {
        ui::header("Dice and checks");
        ui::info("A check is a d20 plus the stat's modifier (and their tier), against a difficulty or someone else's roll.");
        const int pick = ui::choose("", {"Make a check (one character, against a difficulty)",
                                         "A contest (two characters against each other)", "Roll any dice (like 2d6+3)"});
        switch (pick) {
            case 0: makeCheck(app); break;
            case 1: makeContest(app); break;
            case 2: rollAnyDice(app); break;
            default: return;
        }
    }
}

}  // namespace lotm
