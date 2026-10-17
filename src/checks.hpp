// Dice and checks. Any roll written the way tabletop games write it ("2d6+3"), and checks: a
// character rolls a d20 and adds a stat's modifier, to reach a difficulty or to beat someone else's
// roll. Shared by the console, the app window (Dice tab) and the tests.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "dice.hpp"
#include "model.hpp"

namespace lotm {

// ---- Any dice

struct DiceGroup {
    int count = 1;
    int sides = 20;
};

// "2d6 + 1d4 + 3": groups of dice, and a number added at the end (it may be negative).
struct DiceFormula {
    std::vector<DiceGroup> groups;
    int modifier = 0;
    std::string text() const;  // "2d6 + 3", "d20", "d100 - 2"
};

// Reads "d20", "2d6+3", "3d8 - 2", "1d6 + 1d4 + 2" and "d%" (a d100). Up to 100 dice with 2 to 1000 sides
// each. On text it can't read it returns nothing and says why in `problem`.
std::optional<DiceFormula> parseDiceFormula(const std::string& text, std::string* problem = nullptr);

struct DieRoll {
    int sides = 20;
    int value = 1;
};

struct DiceRoll {
    DiceFormula formula;
    std::vector<DieRoll> dice;
    int total = 0;
    std::string describe() const;  // "2d6 + 3: 4 + 6 + 3 = 13", "d20: 17"
};
DiceRoll rollDice(Dice& dice, const DiceFormula& formula);

// ---- Checks

// What a check is made with: one of the six stats, or Speed (for chases and races).
inline constexpr int kSpeedCheck = kStatCount;
inline constexpr int kCheckKinds = kStatCount + 1;
std::string checkName(int kind);  // "Dexterity", "Speed"
std::string checkCode(int kind);  // "DEX", "SPD"

// Advantage rolls two d20s and keeps the higher, disadvantage the lower.
enum class RollMode { Normal, Advantage, Disadvantage };

// How hard something is: the number a check has to reach, as tabletop games name them.
struct DifficultyLevel {
    int value;
    const char* name;
};
inline constexpr std::array<DifficultyLevel, 6> kDifficulties = {{{5, "Very easy"},
                                                                  {10, "Easy"},
                                                                  {15, "Medium"},
                                                                  {20, "Hard"},
                                                                  {25, "Very hard"},
                                                                  {30, "Nearly impossible"}}};
// The hardest level at or below the number: 17 is "Medium", 3 is "Very easy".
std::string difficultyName(int difficulty);

struct CheckSetup {
    int kind = 0;                     // a stat (0-5) or kSpeedCheck
    RollMode mode = RollMode::Normal;
    bool addTier = true;              // their tier as a bonus: +1 for Sequence 9-8 up to +6 Above the Sequence
    int extra = 0;                    // anything else: +2 for a good plan, -5 in the dark
};

// What someone adds to the d20, and the roll itself.
struct CheckRoll {
    std::string who;             // empty: nobody in particular, only the extra bonus counts
    CheckSetup setup;
    int score = 0;               // the stat's total with pathway bonuses (or Speed); 0 without anyone
    int modifier = 0;            // from the score
    int tier = 0;                // added when setup.addTier
    std::string tierName;        // "Saint"
    std::vector<int> d20s;       // one, or two with advantage or disadvantage
    size_t kept = 0;             // the die that counts
    int total = 0;

    int natural() const { return d20s.empty() ? 0 : d20s[kept]; }
    int bonus() const { return modifier + tier + setup.extra; }
    // "12 + 4 Dexterity + 3 Saint - 2 = 17", or "rolled 12 and 7, kept 12; ..." with two dice.
    std::string working() const;
};

// The roll for a character (or nobody, when `c` is null) with the d20s already rolled. rollCheck
// rolls them; this is for showing a roll that was made elsewhere, and for tests.
CheckRoll checkWithDice(const Character* c, const Database& db, const CheckSetup& setup, const std::vector<int>& d20s);
CheckRoll rollCheck(Dice& dice, const Character* c, const Database& db, const CheckSetup& setup);
// What they would add to a d20 for this check, without rolling.
int checkBonus(const Character& c, const Database& db, const CheckSetup& setup);

// A check against a difficulty. A natural 20 always succeeds and a natural 1 always fails.
struct CheckOutcome {
    CheckRoll roll;
    int difficulty = 15;
    bool success = false;
    int margin = 0;  // how far above (or below, negative) the difficulty the total came
    std::string headline() const;  // "Success by 4", "Natural 20: success", "Failure by 3"
    // "Fors Wall, Dexterity check against 15 (Medium): 12 + 4 Dexterity + 3 Saint = 19. Success by 4."
    std::string describe() const;
};
CheckOutcome checkAgainst(const CheckRoll& roll, int difficulty);

// The chance (0 to 1) that a check with this bonus reaches the difficulty, natural 20s and 1s included.
double checkChance(int bonus, int difficulty, RollMode mode = RollMode::Normal);

// Two rolls against each other: the higher total wins; the same total is a tie.
struct ContestOutcome {
    CheckRoll first;
    CheckRoll second;
    int winner = -1;  // 0 the first, 1 the second, -1 a tie
    std::string headline() const;  // "Klein Moretti wins by 7", "A tie"
    // "Klein Moretti (Charisma) against Audrey Hall (Wisdom): 15 + 5 Charisma = 20 against 11 + 4 Wisdom = 15.
    //  Klein Moretti wins by 5."
    std::string describe() const;
};
ContestOutcome contest(const CheckRoll& first, const CheckRoll& second);

// The chances that the first wins, that they tie and that the second wins; together they make 1.
struct ContestChances {
    double first = 0.0;
    double tie = 0.0;
    double second = 0.0;
};
ContestChances contestChances(int firstBonus, RollMode firstMode, int secondBonus, RollMode secondMode);

}  // namespace lotm
