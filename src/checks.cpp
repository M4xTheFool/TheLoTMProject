#include "checks.hpp"

#include <algorithm>
#include <cctype>

#include "rules.hpp"

namespace lotm {

namespace {

constexpr int kMaxDice = 100;
constexpr int kMaxSides = 1000;

bool allDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

// " + 4 Dexterity", " - 2"
std::string term(int value, const std::string& what) {
    std::string text = value < 0 ? " - " : " + ";
    text += std::to_string(value < 0 ? -value : value);
    if (!what.empty()) text += " " + what;
    return text;
}

}  // namespace

// ---------------------------------------------------------------- any dice

std::string DiceFormula::text() const {
    std::string out;
    for (const DiceGroup& g : groups) {
        if (!out.empty()) out += " + ";
        out += (g.count == 1 ? "" : std::to_string(g.count)) + "d" + std::to_string(g.sides);
    }
    if (modifier != 0) out += term(modifier, "");
    return out;
}

std::optional<DiceFormula> parseDiceFormula(const std::string& text, std::string* problem) {
    auto fail = [&](const std::string& why) -> std::optional<DiceFormula> {
        if (problem) *problem = why;
        return std::nullopt;
    };
    // Parts between + and - signs, without spaces: "2d6+3" -> +2d6, +3.
    std::string compact;
    for (char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) compact += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (compact.empty()) return fail("Type the dice to roll, like d20 or 2d6+3.");
    DiceFormula formula;
    int dice = 0;
    size_t at = 0;
    while (at < compact.size()) {
        int sign = 1;
        if (compact[at] == '+' || compact[at] == '-') {
            sign = compact[at] == '-' ? -1 : 1;
            ++at;
        } else if (at > 0) {
            return fail("Put + or - between the parts, like 2d6+3.");
        }
        size_t end = at;
        while (end < compact.size() && compact[end] != '+' && compact[end] != '-') ++end;
        const std::string part = compact.substr(at, end - at);
        at = end;
        if (part.empty()) return fail("Something is missing next to a + or -.");
        const size_t d = part.find('d');
        if (d == std::string::npos) {
            if (!allDigits(part) || part.size() > 6) return fail("\"" + part + "\" isn't a number or dice.");
            formula.modifier += sign * std::stoi(part);
            continue;
        }
        if (sign < 0) return fail("Dice can only be added, not taken away.");
        const std::string count = part.substr(0, d), sides = part.substr(d + 1);
        if (!count.empty() && (!allDigits(count) || count.size() > 4)) return fail("\"" + part + "\" isn't dice. Write it like 2d6.");
        DiceGroup group;
        group.count = count.empty() ? 1 : std::stoi(count);
        if (sides == "%") {
            group.sides = 100;
        } else if (allDigits(sides) && sides.size() <= 5) {
            group.sides = std::stoi(sides);
        } else {
            return fail("\"" + part + "\" isn't dice. Write it like 2d6.");
        }
        if (group.count < 1) return fail("Roll at least one die.");
        if (group.sides < 2 || group.sides > kMaxSides) return fail("A die has 2 to 1000 sides.");
        dice += group.count;
        if (dice > kMaxDice) return fail("That's more than 100 dice at once.");
        formula.groups.push_back(group);
    }
    if (formula.groups.empty()) return fail("Add at least one die, like d20 or 2d6.");
    if (problem) problem->clear();
    return formula;
}

std::string DiceRoll::describe() const {
    std::string out = formula.text() + ": ";
    if (dice.size() == 1 && formula.modifier == 0) return out + std::to_string(dice[0].value);
    for (size_t i = 0; i < dice.size(); ++i) out += (i > 0 ? " + " : "") + std::to_string(dice[i].value);
    if (formula.modifier != 0) out += term(formula.modifier, "");
    return out + " = " + std::to_string(total);
}

DiceRoll rollDice(Dice& dice, const DiceFormula& formula) {
    DiceRoll roll;
    roll.formula = formula;
    roll.total = formula.modifier;
    for (const DiceGroup& g : formula.groups) {
        for (int i = 0; i < g.count; ++i) {
            const int value = dice.roll(g.sides);
            roll.dice.push_back({g.sides, value});
            roll.total += value;
        }
    }
    return roll;
}

// ---------------------------------------------------------------- checks

std::string checkName(int kind) {
    if (kind >= 0 && kind < kStatCount) return kStatNames[static_cast<size_t>(kind)];
    return "Speed";
}

std::string checkCode(int kind) {
    if (kind >= 0 && kind < kStatCount) return kStatCodes[static_cast<size_t>(kind)];
    return "SPD";
}

std::string difficultyName(int difficulty) {
    std::string name = kDifficulties.front().name;
    for (const DifficultyLevel& level : kDifficulties) {
        if (difficulty >= level.value) name = level.name;
    }
    return name;
}

namespace {

// The score a check uses and the tier, from the character as they are.
void fillFromCharacter(CheckRoll& roll, const Character& c, const Database& db) {
    const Pathway* pathway = db.findPathway(c.pathwayId);
    roll.who = c.name.empty() ? "(unnamed)" : c.name;
    if (roll.setup.kind == kSpeedCheck) {
        roll.score = effectiveSpeed(c, pathway);
    } else {
        const int kind = std::clamp(roll.setup.kind, 0, kStatCount - 1);
        roll.score = statTotals(c, pathway)[static_cast<size_t>(kind)];
    }
    roll.modifier = abilityModifier(roll.score);
    const SpeedTier tier = speedTierOf(c);
    if (roll.setup.addTier && tier.tier > 0) {
        roll.tier = tier.tier;
        roll.tierName = tier.name;
    }
}

}  // namespace

std::string CheckRoll::working() const {
    std::string out;
    if (d20s.size() > 1) {
        out = "rolled " + std::to_string(d20s[0]) + " and " + std::to_string(d20s[1]) +
              (setup.mode == RollMode::Disadvantage ? " with disadvantage" : " with advantage") + ", kept " +
              std::to_string(natural()) + ": ";
    }
    out += std::to_string(natural());
    if (!who.empty()) out += term(modifier, checkName(setup.kind));
    if (tier != 0) out += term(tier, tierName);
    if (setup.extra != 0) out += term(setup.extra, "");
    if (out.find(" + ") != std::string::npos || out.find(" - ") != std::string::npos) out += " = " + std::to_string(total);
    return out;
}

CheckRoll checkWithDice(const Character* c, const Database& db, const CheckSetup& setup, const std::vector<int>& d20s) {
    CheckRoll roll;
    roll.setup = setup;
    if (c) fillFromCharacter(roll, *c, db);
    roll.d20s = d20s.empty() ? std::vector<int>{1} : d20s;
    if (roll.d20s.size() > 1) {
        const auto best = setup.mode == RollMode::Disadvantage ? std::min_element(roll.d20s.begin(), roll.d20s.end())
                                                               : std::max_element(roll.d20s.begin(), roll.d20s.end());
        roll.kept = static_cast<size_t>(best - roll.d20s.begin());
    }
    roll.total = roll.natural() + roll.bonus();
    return roll;
}

CheckRoll rollCheck(Dice& dice, const Character* c, const Database& db, const CheckSetup& setup) {
    std::vector<int> d20s{dice.roll(20)};
    if (setup.mode != RollMode::Normal) d20s.push_back(dice.roll(20));
    return checkWithDice(c, db, setup, d20s);
}

int checkBonus(const Character& c, const Database& db, const CheckSetup& setup) {
    return checkWithDice(&c, db, setup, {1}).bonus();
}

std::string CheckOutcome::headline() const {
    if (roll.natural() == 20 && roll.total < difficulty) return "Natural 20: success";
    if (roll.natural() == 1 && roll.total >= difficulty) return "Natural 1: failure";
    if (roll.natural() == 20) return "Natural 20! Success by " + std::to_string(margin);
    if (roll.natural() == 1) return "Natural 1. Failure by " + std::to_string(-margin);
    if (success) return margin == 0 ? "Success, just" : "Success by " + std::to_string(margin);
    return "Failure by " + std::to_string(-margin);
}

std::string CheckOutcome::describe() const {
    std::string out = roll.who.empty() ? std::string("A") : roll.who + ", a";
    if (roll.who.empty()) out += " roll";
    else out += " " + checkName(roll.setup.kind) + " check";
    out += " against " + std::to_string(difficulty) + " (" + difficultyName(difficulty) + "): " + roll.working() + ". " +
           headline() + ".";
    return out;
}

CheckOutcome checkAgainst(const CheckRoll& roll, int difficulty) {
    CheckOutcome outcome;
    outcome.roll = roll;
    outcome.difficulty = difficulty;
    outcome.margin = roll.total - difficulty;
    outcome.success = roll.natural() == 20 || (roll.natural() != 1 && roll.total >= difficulty);
    return outcome;
}

namespace {

// The chance that the d20 that counts shows `value`, with two dice for advantage or disadvantage.
double keptChance(int value, RollMode mode) {
    const double v = value;
    switch (mode) {
        case RollMode::Advantage: return (v * v - (v - 1) * (v - 1)) / 400.0;              // the higher is v
        case RollMode::Disadvantage: return ((21 - v) * (21 - v) - (20 - v) * (20 - v)) / 400.0;  // the lower is v
        case RollMode::Normal: break;
    }
    return 1.0 / 20.0;
}

}  // namespace

double checkChance(int bonus, int difficulty, RollMode mode) {
    double chance = 0.0;
    for (int v = 1; v <= 20; ++v) {
        if (v == 20 || (v != 1 && v + bonus >= difficulty)) chance += keptChance(v, mode);
    }
    return chance;
}

ContestChances contestChances(int firstBonus, RollMode firstMode, int secondBonus, RollMode secondMode) {
    ContestChances chances;
    for (int a = 1; a <= 20; ++a) {
        for (int b = 1; b <= 20; ++b) {
            const double both = keptChance(a, firstMode) * keptChance(b, secondMode);
            const int first = a + firstBonus, second = b + secondBonus;
            (first > second ? chances.first : first < second ? chances.second : chances.tie) += both;
        }
    }
    return chances;
}

std::string ContestOutcome::headline() const {
    if (winner < 0) return "A tie";
    const CheckRoll& won = winner == 0 ? first : second;
    const int by = std::abs(first.total - second.total);
    return (won.who.empty() ? std::string(winner == 0 ? "The first" : "The second") : won.who) + " wins by " +
           std::to_string(by);
}

std::string ContestOutcome::describe() const {
    auto side = [](const CheckRoll& r) {
        return (r.who.empty() ? std::string("Someone") : r.who) + " (" + checkName(r.setup.kind) + ")";
    };
    return side(first) + " against " + side(second) + ": " + first.working() + " against " + second.working() + ". " +
           headline() + ".";
}

ContestOutcome contest(const CheckRoll& first, const CheckRoll& second) {
    ContestOutcome outcome;
    outcome.first = first;
    outcome.second = second;
    if (first.total != second.total) outcome.winner = first.total > second.total ? 0 : 1;
    return outcome;
}

}  // namespace lotm
