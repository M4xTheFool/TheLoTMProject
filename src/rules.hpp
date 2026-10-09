// Game rules: modifiers, speed tiers, pathway bonuses, suggested HP and Spirituality.
// Everything is a plain function of the character and its pathway, so it is easy to test.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "model.hpp"

namespace lotm {

// D&D ability modifier: 10-11 = +0, 12-13 = +1, 8-9 = -1 ...
int abilityModifier(int score);
std::string signedNumber(int value);  // "+2", "0", "-1"

// Speed tiers: 0 Mortal, 1 Awakened (Seq 9-8), 2 Extraordinary (7-5), 3 Saint (4-3), 4 Angel (2-1), 5 Divine (0).
struct SpeedTier {
    int tier = 0;
    std::string name;
    std::string description;
};
int tierForSequence(int sequence);
SpeedTier speedTier(int tier);
SpeedTier speedTierOf(const Character& c);  // tier 0 when the character has no pathway

// Pathway stat bonuses: primary stat +1 per tier, secondary stat +1 every two tiers.
int primaryBonus(int tier);
int secondaryBonus(int tier);
std::array<int, kStatCount> statBonuses(const Character& c, const Pathway* pathway);
std::array<int, kStatCount> statTotals(const Character& c, const Pathway* pathway);

// Speed grade shifts the Speed score inside its tier: Slow -2, Average 0, Fast +2, Very fast +4.
int speedGradeModifier(const std::string& grade);
int effectiveSpeed(const Character& c, const Pathway* pathway);  // never below 1
std::vector<std::string> movementModes(const Character& c, const Pathway* pathway);

// Abilities from Sequence 9 down to the character's sequence, weakest first.
std::vector<const SequenceInfo*> abilitiesUpTo(const Pathway& pathway, int sequence);

bool honorificEligible(const Character& c);  // Sequence 3 or stronger, with a pathway

// Suggestions shown in the stat step; the user can always type their own value.
int suggestedHp(const Character& c, const Pathway* pathway);
int suggestedSpirituality(const Character& c, const Pathway* pathway);

// Point buy (D&D 5e): scores 8-15, 27 points.
inline constexpr int kPointBuyBudget = 27;
int pointBuyCost(int score);  // -1 when the score is outside 8-15

// Cleans up fields that no longer apply before saving (honorific name above Sequence 3 ...).
// Returns notes describing anything that was removed.
std::vector<std::string> normalizeCharacter(Character& c);

}  // namespace lotm
