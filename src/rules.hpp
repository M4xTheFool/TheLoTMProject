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

// Speed tiers: 0 Mortal, 1 Awakened (Seq 9-8), 2 Extraordinary (7-5), 3 Saint (4-3), 4 Angel (2-1), 5 Divine (0),
// 6 Above the Sequence (a Great Old One).
inline constexpr int kAboveSequenceTier = 6;
struct SpeedTier {
    int tier = 0;
    std::string name;
    std::string description;
};
int tierForSequence(int sequence);
SpeedTier speedTier(int tier);
SpeedTier speedTierOf(const Character& c);  // tier 0 when the character has no pathway, 6 Above the Sequence

// Pathway stat bonuses: primary stat +1 per tier, secondary stat +1 every two tiers.
int primaryBonus(int tier);
int secondaryBonus(int tier);
std::array<int, kStatCount> statBonuses(const Character& c, const Pathway* pathway);
std::array<int, kStatCount> statTotals(const Character& c, const Pathway* pathway);

// Speed grade shifts the Speed score inside its tier: Slow -2, Average 0, Fast +2, Very fast +4.
int speedGradeModifier(const std::string& grade);
int effectiveSpeed(const Character& c, const Pathway* pathway);  // never below 1
std::vector<std::string> movementModes(const Character& c, const Pathway* pathway);
std::vector<std::string> movementUnlockedAt(const Pathway& pathway, int sequence);  // gained exactly there

// Abilities from Sequence 9 down to the character's sequence, weakest first.
std::vector<const SequenceInfo*> abilitiesUpTo(const Pathway& pathway, int sequence);

bool honorificEligible(const Character& c);   // Sequence 3 or stronger, with a pathway
bool sequenceOneChoices(const Character& c);  // Sequence 1: Beyonder characteristics and a Uniqueness
bool absorbedUniqueness(const Character& c);  // Sequence 0 has absorbed its pathway's Uniqueness
bool aboveTheSequence(const Character& c);    // a Great Old One, with a pathway
// A Sequence 0 god (not Above the Sequence) can take a second pathway and become a dual-pathway god.
bool dualPathwayEligible(const Character& c);
bool isDualPathwayGod(const Character& c);
// "Sequence 7", "Sequence 0" or "Above the Sequence"; "Mortal" without a pathway.
std::string levelName(const Character& c);

// Every power a character has, grouped the way the sheet shows them:
//   - their pathway, Sequence 9 down to theirs;
//   - the Sequence 0 powers a Sequence 1 Uniqueness grants;
//   - a dual-pathway god's second pathway, Sequence 9 down to 1, and the powers its Uniqueness grants;
//   - Above the Sequence, the Sequence 0 powers of every other pathway in their group.
struct AbilityGroup {
    std::string heading;                 // "Sequence 7: Magician", "Apprentice pathway, Sequence 1: Key of Stars"
    std::vector<std::string> abilities;  // "Name: what it does"
    std::vector<std::string> movement;   // movement modes unlocked there
};
std::vector<AbilityGroup> abilityGroups(const Character& c, const Database& db);
int abilityCount(const Character& c, const Database& db);  // abilities only, not movement

// Threat level, added up from three things:
//   Sequence:        mortal 0, Seq 9-8 1, 7-5 3, 4-3 5, 2-1 7, Seq 0 10, Above the Sequence 12
//   recent actions:  peaceful -1, minor incidents 0, violent +1, mass casualties +2 (nothing on file 0)
//   Sealed Artifacts held: one or two +1, three or more +2
// Total: up to 2 Low, 3-4 Moderate, 5-6 High, 7-8 Extreme, 9+ Catastrophic.
inline const std::vector<std::string> kThreatLevels = {"Low", "Moderate", "High", "Extreme", "Catastrophic"};
inline const std::vector<std::string> kRecentActions = {"Peaceful, no known incidents", "Minor incidents",
                                                        "Violent incidents", "Mass casualties or disaster"};
struct ThreatAssessment {
    int total = 0;
    std::string level;        // from the total
    std::string explanation;  // "Sequence 7 (3) + violent incidents (+1) + 2 Sealed Artifacts (+1) = 5"
};
ThreatAssessment assessThreat(const Character& c);
std::string threatLevelOf(const Character& c);  // the hand-set level, or the assessed one

// Suggestions shown in the stat step; the user can always type their own value.
int suggestedHp(const Character& c, const Pathway* pathway);
int suggestedSpirituality(const Character& c, const Pathway* pathway);

// Point buy (D&D 5e): scores 8-15, 27 points.
inline constexpr int kPointBuyBudget = 27;
int pointBuyCost(int score);  // -1 when the score is outside 8-15

// Cleans up fields that no longer apply before saving (honorific name above Sequence 3,
// a Uniqueness away from Sequence 1, a second pathway below Sequence 0 ...).
// Returns notes describing anything that was removed.
std::vector<std::string> normalizeCharacter(Character& c);

}  // namespace lotm
