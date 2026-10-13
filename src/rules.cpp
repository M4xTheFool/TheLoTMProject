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

std::string pathwayIdAt(const Character& c, int sequence) {
    if (sequence > c.sequence) {  // weaker than their Sequence now
        for (const SequenceStep& step : c.mixedSequences) {
            if (step.sequence == sequence && !step.pathwayId.empty()) return step.pathwayId;
        }
    }
    return c.pathwayId;
}

bool isMixedPathway(const Character& c) { return !earlierPathwayIds(c).empty(); }

std::vector<std::string> earlierPathwayIds(const Character& c) {
    std::vector<std::string> ids;
    if (c.pathwayId.empty()) return ids;
    for (int s = 9; s > c.sequence; --s) {
        const std::string id = pathwayIdAt(c, s);
        if (id != c.pathwayId && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    return ids;
}

bool neighbouringPathways(const Database& db, const std::string& a, const std::string& b) {
    if (a == b) return true;
    const auto group = db.pathwayGroup(a);
    return std::any_of(group.begin(), group.end(), [&](const Pathway* p) { return p->id == b; });
}

std::vector<PathwaySwitch> pathwaySwitches(const Character& c, const Database& db) {
    std::vector<PathwaySwitch> switches;
    if (c.pathwayId.empty()) return switches;
    for (int s = 8; s >= c.sequence; --s) {
        const std::string from = pathwayIdAt(c, s + 1);
        const std::string to = pathwayIdAt(c, s);
        if (from != to) switches.push_back({s, from, to, neighbouringPathways(db, from, to)});
    }
    return switches;
}

bool riskySwitch(const Character& c, const Database& db) {
    const auto switches = pathwaySwitches(c, db);
    return std::any_of(switches.begin(), switches.end(), [](const PathwaySwitch& s) { return !s.neighbouring; });
}

bool switchPathway(Character& c, const std::string& newPathwayId) {
    if (c.pathwayId.empty() || c.aboveSequence || c.sequence <= 0 || newPathwayId.empty() ||
        newPathwayId == c.pathwayId) {
        return false;
    }
    std::vector<SequenceStep> held;
    for (int s = 9; s >= c.sequence; --s) {
        const std::string id = pathwayIdAt(c, s);
        if (id != newPathwayId) held.push_back({s, id});  // a Sequence from the new pathway needs no entry
    }
    c.mixedSequences = held;
    c.pathwayId = newPathwayId;
    c.sequence -= 1;
    return true;
}

int stateOfMindLevel(const std::string& state) {
    const auto it = std::find(kStatesOfMind.begin(), kStatesOfMind.end(), state);
    return it == kStatesOfMind.end() ? -1 : static_cast<int>(it - kStatesOfMind.begin());
}

std::vector<AbilityGroup> abilityGroups(const Character& c, const Database& db) {
    std::vector<AbilityGroup> groups;
    const Pathway* pathway = db.findPathway(c.pathwayId);
    if (!pathway) return groups;
    // Sequence 9 up to theirs, each from the pathway it came from.
    const bool mixed = isMixedPathway(c);
    for (int sequence = 9; sequence >= c.sequence; --sequence) {
        const Pathway* from = db.findPathway(pathwayIdAt(c, sequence));
        const SequenceInfo* s = from ? from->findSequence(sequence) : nullptr;
        if (!s) continue;
        AbilityGroup group;
        group.heading = sequenceLabel(from, sequence) + (mixed ? " (" + from->name + " pathway)" : "");
        group.abilities = s->abilities;
        group.movement = movementUnlockedAt(*from, sequence);
        group.pathwayId = from->id;
        groups.push_back(group);
    }
    if (c.hasUniqueness && !c.uniquenessAbilities.empty()) {
        AbilityGroup group;
        group.heading = "From the Uniqueness: " + sequenceLabel(pathway, 0);
        group.abilities = c.uniquenessAbilities;
        group.pathwayId = pathway->id;
        groups.push_back(group);
    }
    // Above the Sequence is one more step past Sequence 0, written on the Sefirah.
    if (const Sefirah* s = aboveTheSequence(c) ? greatOldOneSefirah(c, db) : nullptr) {
        if (!s->abilities.empty() || !s->symbolism.empty()) {
            AbilityGroup group;
            group.heading = aboveSequenceLabel(s);
            group.abilities = s->abilities;
            group.symbolism = s->symbolism;
            group.pathwayId = pathway->id;
            groups.push_back(group);
        }
    }
    // A dual-pathway god and a Great Old One have every ability of their extra pathways, Sequence 9 to 0.
    auto wholePathway = [&](const Pathway& other) {
        AbilityGroup group;
        group.pathwayId = other.id;
        group.heading = pathwayLabel(db, other.id);
        group.summary = "All abilities of the " + other.name + " pathway" +
                        (other.god.empty() ? "" : " (" + other.god + ")") + ", Sequence 9 to 0";
        for (const auto& s : other.sequences) group.wholePathwayCount += static_cast<int>(s.abilities.size());
        for (const auto& m : other.movement) group.movement.push_back(m.mode);
        groups.push_back(group);
    };
    if (const Pathway* second = isDualPathwayGod(c) ? db.findPathway(c.secondPathway.pathwayId) : nullptr) {
        wholePathway(*second);
    }
    if (aboveTheSequence(c)) {
        for (const Pathway* other : db.pathwayGroup(c.pathwayId)) {
            if (other != pathway) wholePathway(*other);
        }
    }
    return groups;
}

const Sefirah* greatOldOneSefirah(const Character& c, const Database& db) {
    if (const Sefirah* held = db.findSefirah(c.sefirahId)) {
        const auto& ids = held->pathwayIds;
        if (std::find(ids.begin(), ids.end(), c.pathwayId) != ids.end()) return held;
    }
    return db.sefirahForPathway(c.pathwayId);
}

std::string aboveSequenceLabel(const Sefirah* sefirah) {
    if (!sefirah || sefirah->greatOldOne.empty()) return "Above the Sequence";
    return "Above the Sequence: " + sefirah->greatOldOne;
}

int abilityCount(const Character& c, const Database& db) {
    int count = 0;
    for (const AbilityGroup& group : abilityGroups(c, db)) {
        count += group.summary.empty() ? static_cast<int>(group.abilities.size()) : group.wholePathwayCount;
    }
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
    // Blank honorific lines (the window shows empty boxes to fill in) are not part of the name.
    auto& honorific = c.honorificName;
    honorific.erase(std::remove_if(honorific.begin(), honorific.end(),
                                   [](const std::string& line) {
                                       return line.find_first_not_of(" \t") == std::string::npos;
                                   }),
                    honorific.end());
    if (!honorificEligible(c) && !c.honorificName.empty()) {
        c.honorificName.clear();
        notes.push_back("Honorific name removed: it needs Sequence 3 or stronger.");
    }

    // Mixed Sequences: only Sequences weaker than the one they are on now can come from another
    // pathway, each listed once (the last entry wins), weakest first.
    std::vector<SequenceStep> steps;
    bool droppedHeld = false;
    for (const SequenceStep& step : c.mixedSequences) {
        if (c.pathwayId.empty() || step.pathwayId.empty() || step.pathwayId == c.pathwayId) continue;
        if (step.sequence < 0 || step.sequence > 9) continue;
        if (step.sequence <= c.sequence) {
            droppedHeld = true;
            continue;
        }
        steps.erase(std::remove_if(steps.begin(), steps.end(),
                                   [&](const SequenceStep& s) { return s.sequence == step.sequence; }),
                    steps.end());
        steps.push_back(step);
    }
    std::sort(steps.begin(), steps.end(), [](const SequenceStep& a, const SequenceStep& b) { return a.sequence > b.sequence; });
    if (droppedHeld) {
        notes.push_back("Mixed Sequences removed from Sequence " + std::to_string(c.sequence) +
                        " up: the Sequence they are on comes from their pathway now, and they don't hold the "
                        "stronger ones.");
    }
    c.mixedSequences = steps;
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
        // The whole second pathway is theirs, so picked Uniqueness powers (older saves) are not needed.
        second.uniquenessAbilities.clear();
        if (!second.hasUniqueness) second.uniquenessForm.clear();
    }

    if (!c.stats.hpIncluded) c.stats.hp = 0;
    return notes;
}

}  // namespace lotm
