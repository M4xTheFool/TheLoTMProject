// How each struct maps to JSON. Missing fields in a file fall back to the struct's
// default value, so older save files keep loading after new fields are added.
#pragma once

#include <nlohmann/json.hpp>

#include "model.hpp"

namespace lotm {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SequenceInfo, sequence, name, abilities)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MovementUnlock, sequence, mode)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Pathway, id, name, god, group, primaryStat, secondaryStat,
                                                speedGrade, speedNote, movement, sequences)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Appearance, hair, hairLength, hairStyle, eyes, skin, face, voice,
                                                build, height, clothing, distinguishingMark, description)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Affiliation, organization, rank)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Relationship, characterId, name, type, note)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Dossier, filedBy, recentActions, recentActionsNote, threatLevel, status,
                                                lastSeen, remarks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StatBlock, base, speedBase, hpIncluded, hp, spirituality)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Character, id, name, appearance, age, gender, shortDescription,
                                                backstory, pathwayId, sequence, alignment, titles, aliases,
                                                honorificName, beyonderCharacteristics, hasUniqueness, uniquenessForm,
                                                uniquenessAbilities, stats, artifactIds, affiliation, relationships,
                                                dossier, notes, createdAt, updatedAt)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Artifact, id, name, visualDescription, pathwayId, sequenceLevel,
                                                ability, drawback, notes, createdAt, updatedAt)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Settings, hpMode, exportTheme, backupsToKeep)

}  // namespace lotm
