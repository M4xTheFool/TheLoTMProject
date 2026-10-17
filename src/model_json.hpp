// How each struct maps to JSON. Missing fields in a file fall back to the struct's
// default value, so older save files keep loading after new fields are added.
#pragma once

#include <nlohmann/json.hpp>

#include "model.hpp"

namespace lotm {

// Story dates are saved as text, "3 July 1360", so the files stay easy to read and to edit by hand
// (see story.cpp). Text that isn't a date stops the file from loading, with the reason.
void to_json(nlohmann::json& j, const StoryDate& date);
void from_json(const nlohmann::json& j, StoryDate& date);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SequenceInfo, sequence, name, abilities)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MovementUnlock, sequence, mode)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Pathway, id, name, god, group, primaryStat, secondaryStat,
                                                speedGrade, speedNote, description, uniqueness, movement, sequences)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Sefirah, id, name, alsoKnownAs, greatOldOne, symbolism, abilities,
                                                pathwayIds, description, knownHolders, notes)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Appearance, hair, hairLength, hairStyle, eyes, skin, face, voice,
                                                build, height, clothing, distinguishingMark, description)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Affiliation, organization, rank)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Relationship, characterId, name, type, note, organizationId)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Dossier, filedBy, recentActions, recentActionsNote, threatLevel, status,
                                                lastSeen, remarks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StatBlock, base, speedBase, hpIncluded, hp, spirituality)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SecondPathway, pathwayId, beyonderCharacteristics, hasUniqueness,
                                                uniquenessForm, uniquenessAbilities)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SequenceStep, sequence, pathwayId)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CharacterStage, date, label, pathwayId, sequence, aboveSequence,
                                                beyonderCharacteristics, hasUniqueness, uniquenessForm,
                                                uniquenessAbilities, secondPathway, sefirahId, stateOfMind, life,
                                                affiliation, status, titles, aliases, artifactIds, note)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Character, id, name, appearance, age, gender, shortDescription,
                                                backstory, pathwayId, sequence, mixedSequences, stateOfMind,
                                                stateOfMindNote, life, lifeNote, diedOn, alignment, titles, aliases,
                                                honorificName, beyonderCharacteristics, hasUniqueness, uniquenessForm,
                                                uniquenessAbilities, aboveSequence, secondPathway, sefirahId, stats,
                                                artifactIds, affiliation, relationships, dossier, notes, stages,
                                                nowSince, createdAt, updatedAt)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Artifact, id, name, visualDescription, pathwayId, sequenceLevel,
                                                ability, drawback, notes, createdAt, updatedAt)

// "change" is only written for someone who changed at the event, so events.json stays short.
void to_json(nlohmann::json& j, const EventPerson& person);
void from_json(const nlohmann::json& j, EventPerson& person);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StoryEvent, id, title, date, kind, place, chapter, people, artifactIds,
                                                description, notes, createdAt, updatedAt)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Organization, id, name, alsoKnownAs, kind, status, leaderId, leader,
                                                headquarters, founded, symbol, pathwayIds, ranks, description,
                                                otherMembers, notes, createdAt, updatedAt)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Settings, hpMode, exportTheme, backupsToKeep, windowTextSize,
                                                windowTheme, windowAnimations)

}  // namespace lotm
