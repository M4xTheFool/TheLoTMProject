#include "rules.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace lotm {

int abilityModifier(int score) {
    return static_cast<int>(std::floor((score - 10) / 2.0));
}

std::string signedNumber(int value) {
    if (value > 0) return "+" + std::to_string(value);
    return std::to_string(value);
}

int tierForSequence(int sequence) {
    if (sequence >= 8) return 1;
    if (sequence >= 5) return 2;
    if (sequence >= 3) return 3;
    if (sequence >= 1) return 4;
    return 5;
}

SpeedTier speedTier(int tier) {
    switch (tier) {
        case 1: return {1, "Awakened", "Athlete to peak human"};
        case 2: return {2, "Extraordinary", "Clearly superhuman, faster than a carriage"};
        case 3: return {3, "Saint", "Demigod; outruns sound, crosses a city fast"};
        case 4: return {4, "Angel", "Near-instant travel across regions"};
        case 5: return {5, "Divine", "Effectively unbounded within their domain"};
        case kAboveSequenceTier:
            return {kAboveSequenceTier, "Above the Sequence", "Beyond distance: present wherever their Sefirah reaches"};
        default: return {0, "Mortal", "Ordinary human"};
    }
}

SpeedTier speedTierOf(const Character& c) {
    if (c.pathwayId.empty()) return speedTier(0);
    if (c.aboveSequence) return speedTier(kAboveSequenceTier);
    return speedTier(tierForSequence(c.sequence));
}

int primaryBonus(int tier) { return tier; }
int secondaryBonus(int tier) { return tier / 2; }

std::array<int, kStatCount> statBonuses(const Character& c, const Pathway* pathway) {
    std::array<int, kStatCount> bonus{};
    if (!pathway || c.pathwayId.empty()) return bonus;
    const int tier = speedTierOf(c).tier;
    if (int i = statIndex(pathway->primaryStat); i >= 0) bonus[i] += primaryBonus(tier);
    if (int i = statIndex(pathway->secondaryStat); i >= 0) bonus[i] += secondaryBonus(tier);
    return bonus;
}

std::array<int, kStatCount> statTotals(const Character& c, const Pathway* pathway) {
    std::array<int, kStatCount> totals = c.stats.base;
    const auto bonus = statBonuses(c, pathway);
    for (int i = 0; i < kStatCount; ++i) totals[i] += bonus[i];
    return totals;
}

int speedGradeModifier(const std::string& grade) {
    if (grade == "Slow") return -2;
    if (grade == "Fast") return 2;
    if (grade == "Very fast") return 4;
    return 0;
}

int effectiveSpeed(const Character& c, const Pathway* pathway) {
    int speed = c.stats.speedBase;
    if (pathway && !c.pathwayId.empty()) speed += speedGradeModifier(pathway->speedGrade);
    return std::max(1, speed);
}

std::vector<std::string> movementModes(const Character& c, const Pathway* pathway) {
    std::vector<std::string> modes;
    if (!pathway || c.pathwayId.empty()) return modes;
    for (const auto& m : pathway->movement) {
        // Lower sequence numbers are stronger, so Sequence 5 also has everything unlocked at 9-5.
        if (c.sequence <= m.sequence) modes.push_back(m.mode);
    }
    return modes;
}

std::vector<std::string> movementUnlockedAt(const Pathway& pathway, int sequence) {
    std::vector<std::string> modes;
    for (const auto& m : pathway.movement) {
        if (m.sequence == sequence) modes.push_back(m.mode);
    }
    return modes;
}

std::vector<const SequenceInfo*> abilitiesUpTo(const Pathway& pathway, int sequence) {
    std::vector<const SequenceInfo*> result;
    for (const auto& s : pathway.sequences) {
        if (s.sequence >= sequence) result.push_back(&s);
    }
    std::sort(result.begin(), result.end(),
              [](const SequenceInfo* a, const SequenceInfo* b) { return a->sequence > b->sequence; });
    return result;
}

bool honorificEligible(const Character& c) {
    return !c.pathwayId.empty() && c.sequence <= 3;
}

bool sequenceOneChoices(const Character& c) {
    return !c.pathwayId.empty() && c.sequence == 1;
}

bool absorbedUniqueness(const Character& c) {
    return !c.pathwayId.empty() && c.sequence == 0;
}

bool aboveTheSequence(const Character& c) {
    return !c.pathwayId.empty() && c.aboveSequence;
}

bool dualPathwayEligible(const Character& c) {
    return !c.pathwayId.empty() && c.sequence == 0 && !c.aboveSequence;
}

bool isDualPathwayGod(const Character& c) {
    return dualPathwayEligible(c) && !c.secondPathway.empty();
}

std::string levelName(const Character& c) {
    if (c.pathwayId.empty()) return "Mortal";
    if (c.aboveSequence) return "Above the Sequence";
    return "Sequence " + std::to_string(c.sequence);
}

std::vector<AbilityGroup> abilityGroups(const Character& c, const Database& db) {
    std::vector<AbilityGroup> groups;
    const Pathway* pathway = db.findPathway(c.pathwayId);
    if (!pathway) return groups;
    for (const SequenceInfo* s : abilitiesUpTo(*pathway, c.sequence)) {
        groups.push_back({sequenceLabel(pathway, s->sequence), s->abilities, movementUnlockedAt(*pathway, s->sequence)});
    }
    if (c.hasUniqueness && !c.uniquenessAbilities.empty()) {
        groups.push_back({"From the Uniqueness: " + sequenceLabel(pathway, 0), c.uniquenessAbilities, {}});
    }
    // A Sequence 1 characteristic carries the powers of that pathway's Sequences 9 to 1.
    if (const Pathway* second = isDualPathwayGod(c) ? db.findPathway(c.secondPathway.pathwayId) : nullptr) {
        for (const SequenceInfo* s : abilitiesUpTo(*second, 1)) {
            groups.push_back({second->name + " pathway, " + sequenceLabel(second, s->sequence), s->abilities,
                              movementUnlockedAt(*second, s->sequence)});
        }
        const SecondPathway& held = c.secondPathway;
        if (held.hasUniqueness && !held.uniquenessAbilities.empty()) {
            groups.push_back({"From the " + second->name + " Uniqueness: " + sequenceLabel(second, 0),
                              held.uniquenessAbilities, {}});
        }
    }
    // A Great Old One holds every Uniqueness of its group, and with them every Sequence 0 power.
    if (aboveTheSequence(c)) {
        for (const Pathway* other : db.pathwayGroup(c.pathwayId)) {
            const SequenceInfo* zero = other != pathway ? other->findSequence(0) : nullptr;
            if (!zero) continue;
            groups.push_back({"From the " + other->name + " Uniqueness: " + sequenceLabel(other, 0), zero->abilities,
                              movementUnlockedAt(*other, 0)});
        }
    }
    return groups;
}

int abilityCount(const Character& c, const Database& db) {
    int count = 0;
    for (const AbilityGroup& group : abilityGroups(c, db)) count += static_cast<int>(group.abilities.size());
    return count;
}

ThreatAssessment assessThreat(const Character& c) {
    static constexpr std::array<int, 7> kSequencePoints = {0, 1, 3, 5, 7, 10, 12};  // by speed tier
    const int tier = speedTierOf(c).tier;
    const int sequencePoints = kSequencePoints[static_cast<size_t>(tier)];

    int actionPoints = 0;
    const auto action = std::find(kRecentActions.begin(), kRecentActions.end(), c.dossier.recentActions);
    if (action != kRecentActions.end()) actionPoints = static_cast<int>(action - kRecentActions.begin()) - 1;

    const int artifacts = static_cast<int>(c.artifactIds.size());
    const int artifactPoints = artifacts >= 3 ? 2 : (artifacts >= 1 ? 1 : 0);

    ThreatAssessment result;
    result.total = sequencePoints + actionPoints + artifactPoints;
    const int t = result.total;
    result.level = kThreatLevels[t <= 2 ? 0 : t <= 4 ? 1 : t <= 6 ? 2 : t <= 8 ? 3 : 4];

    result.explanation = tier == 0 ? "Mortal (0)" : levelName(c) + " (" + std::to_string(sequencePoints) + ")";
    if (action != kRecentActions.end() && actionPoints != 0) {
        std::string label = *action;
        label[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(label[0])));
        result.explanation += " + " + label.substr(0, label.find(',')) + " (" + signedNumber(actionPoints) + ")";
    }
    if (artifactPoints > 0) {
        result.explanation += " + " + std::to_string(artifacts) + " Sealed Artifact" + (artifacts == 1 ? "" : "s") +
                              " (+" + std::to_string(artifactPoints) + ")";
    }
    result.explanation += " = " + std::to_string(result.total);
    return result;
}

std::string threatLevelOf(const Character& c) {
    return c.dossier.threatLevel.empty() ? assessThreat(c).level : c.dossier.threatLevel;
}

static int sequencesAdvanced(const Character& c) {
    if (c.pathwayId.empty()) return 1;
    if (c.aboveSequence) return 11;
    return 10 - c.sequence;  // Sequence 9 = 1, Sequence 0 = 10
}

int suggestedHp(const Character& c, const Pathway* pathway) {
    static constexpr std::array<int, 7> kBaseByTier = {10, 12, 20, 40, 80, 160, 320};
    const int tier = speedTierOf(c).tier;
    const int con = statTotals(c, pathway)[2];
    return std::max(1, kBaseByTier[tier] + abilityModifier(con) * sequencesAdvanced(c));
}

int suggestedSpirituality(const Character& c, const Pathway* pathway) {
    static constexpr std::array<int, 7> kBaseByTier = {5, 10, 20, 40, 80, 160, 320};
    const int tier = speedTierOf(c).tier;
    const int wis = statTotals(c, pathway)[4];
    return std::max(1, kBaseByTier[tier] + abilityModifier(wis) * sequencesAdvanced(c));
}

int pointBuyCost(int score) {
    static constexpr std::array<int, 8> kCost = {0, 1, 2, 3, 4, 5, 7, 9};  // scores 8..15
    if (score < 8 || score > 15) return -1;
    return kCost[score - 8];
}

std::vector<std::string> normalizeCharacter(Character& c) {
    std::vector<std::string> notes;
    if (c.pathwayId.empty()) {
        c.sequence = 9;
        c.aboveSequence = false;
    }
    if (c.aboveSequence) c.sequence = 0;  // a Great Old One is past Sequence 0 of its pathway
    c.sequence = std::clamp(c.sequence, 0, 9);
    if (!honorificEligible(c) && !c.honorificName.empty()) {
        c.honorificName.clear();
        notes.push_back("Honorific name removed: it needs Sequence 3 or stronger.");
    }
    if (!sequenceOneChoices(c)) {
        if (c.hasUniqueness && !absorbedUniqueness(c)) {
            notes.push_back("Uniqueness removed: only Sequence 1 can hold one.");
        }
        if (c.beyonderCharacteristics != 1) {
            notes.push_back("Beyonder characteristics set back to 1: only Sequence 1 chooses between 1 and 2.");
        }
        c.hasUniqueness = false;
        c.beyonderCharacteristics = 1;
    }
    c.beyonderCharacteristics = std::clamp(c.beyonderCharacteristics, 1, 2);
    if (!c.hasUniqueness) {
        // A Sequence 0 god keeps the description of the Uniqueness it absorbed, but has every
        // Sequence 0 power anyway, so the picked ones go.
        const bool keepForm = absorbedUniqueness(c);
        const bool hadDetails = (!keepForm && !c.uniquenessForm.empty()) || !c.uniquenessAbilities.empty();
        if (!keepForm) c.uniquenessForm.clear();
        c.uniquenessAbilities.clear();
        if (hadDetails && sequenceOneChoices(c)) {
            notes.push_back("Uniqueness details removed: the character no longer holds one.");
        }
    }

    SecondPathway& second = c.secondPathway;
    if (!second.empty()) {
        if (aboveTheSequence(c)) {
            notes.push_back("Second pathway removed: Above the Sequence they hold every Uniqueness of their group.");
            second = SecondPathway{};
        } else if (!dualPathwayEligible(c)) {
            notes.push_back("Second pathway removed: only a Sequence 0 god can be a dual-pathway god.");
            second = SecondPathway{};
        } else if (second.pathwayId == c.pathwayId) {
            notes.push_back("Second pathway removed: it was their own pathway.");
            second = SecondPathway{};
        }
    }
    if (second.empty()) {
        second = SecondPathway{};
    } else {
        second.beyonderCharacteristics = std::clamp(second.beyonderCharacteristics, 1, 2);
        if (!second.hasUniqueness) {
            second.uniquenessForm.clear();
            second.uniquenessAbilities.clear();
        }
    }

    if (!c.stats.hpIncluded) c.stats.hp = 0;
    return notes;
}

}  // namespace lotm
