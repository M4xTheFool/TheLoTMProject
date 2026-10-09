// The data the program works with: pathways, characters, Sealed Artifacts and settings.
// Every struct here is saved to and loaded from JSON (see model.cpp).
#pragma once

#include <array>
#include <string>
#include <vector>

namespace lotm {

// The six D&D stats, always stored in this order.
inline constexpr int kStatCount = 6;
inline constexpr std::array<const char*, kStatCount> kStatCodes = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
inline constexpr std::array<const char*, kStatCount> kStatNames = {
    "Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"};

// Returns 0-5 for "STR".."CHA" (any letter case), or -1 if the code is unknown.
int statIndex(const std::string& code);

// ---------- Pathway database (data/pathways.json) ----------

struct SequenceInfo {
    int sequence = 9;  // 9 = weakest, 0 = god
    std::string name;
    std::vector<std::string> abilities;
};

struct MovementUnlock {
    int sequence = 9;  // the mode is gained at this sequence and every stronger one
    std::string mode;
};

struct Pathway {
    std::string id;              // short key used in save files, e.g. "seer"
    std::string name;            // "Seer"
    std::string god;             // "The Fool"
    std::string group;           // neighbouring pathways, e.g. "Fool / Door / Error"
    std::string primaryStat;     // "WIS"
    std::string secondaryStat;   // "DEX"
    std::string speedGrade = "Average";  // Slow, Average, Fast or Very fast
    std::string speedNote;
    std::vector<MovementUnlock> movement;
    std::vector<SequenceInfo> sequences;  // Sequence 9 down to 0

    const SequenceInfo* findSequence(int sequence) const;
};

// ---------- Characters (data/characters.json) ----------

struct Appearance {
    std::string hair;
    std::string eyes;
    std::string build;
    std::string height;
    std::string clothing;
    std::string distinguishingMark;
    std::string description;  // free text
};

struct Affiliation {
    std::string organization;
    std::string rank;
};

struct Relationship {
    int characterId = 0;  // 0 = someone who isn't a saved character
    std::string name;     // who they are; for a saved character, their name when the link was made
    std::string type;     // Ally, Rival, Mentor ...
    std::string note;
};

struct StatBlock {
    std::array<int, kStatCount> base = {10, 10, 10, 10, 10, 10};  // before pathway bonuses
    int speedBase = 10;        // before the pathway's speed grade
    bool hpIncluded = false;
    int hp = 0;
    int spirituality = 0;
};

struct Character {
    int id = 0;
    std::string name;
    Appearance appearance;
    std::string age;
    std::string gender;
    std::string shortDescription;
    std::string backstory;
    std::string pathwayId;  // empty = ordinary mortal
    int sequence = 9;
    std::string alignment;
    std::vector<std::string> titles;
    std::vector<std::string> aliases;
    std::vector<std::string> honorificName;  // three lines, Sequence 3 and stronger only
    bool hasUniqueness = false;
    std::string uniquenessForm;
    StatBlock stats;
    std::vector<int> artifactIds;
    Affiliation affiliation;
    std::vector<Relationship> relationships;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};

// ---------- Sealed Artifacts (data/artifacts.json) ----------

inline constexpr int kUnknownSequence = -1;

struct Artifact {
    int id = 0;
    std::string name;
    std::string visualDescription;
    std::string pathwayId;            // empty = unknown pathway
    int sequenceLevel = kUnknownSequence;
    std::string ability;
    std::string drawback;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};

// ---------- Settings (data/settings.json) ----------

struct Settings {
    std::string hpMode = "ask";        // ask, always, never
    std::string exportTheme = "auto";  // auto, light, dark
    int backupsToKeep = 5;
};

// Everything loaded in memory while the program runs.
struct Database {
    std::vector<Pathway> pathways;
    std::vector<Character> characters;
    std::vector<Artifact> artifacts;
    Settings settings;

    const Pathway* findPathway(const std::string& id) const;
    Character* findCharacter(int id);
    const Character* findCharacter(int id) const;
    // Saved characters (other than excludeId) whose name contains the text, ignoring case.
    // An exact name match is returned on its own.
    std::vector<const Character*> findCharactersByName(const std::string& text, int excludeId) const;
    Artifact* findArtifact(int id);
    const Artifact* findArtifact(int id) const;
    int nextCharacterId() const;
    int nextArtifactId() const;
};

// Display helpers shared by every screen and export.
std::string characterCode(int id);  // "C-001"
std::string artifactCode(int id);   // "A-001"
std::string pathwayLabel(const Database& db, const std::string& pathwayId);  // "Seer (The Fool)"
std::string sequenceLabel(const Pathway* pathway, int sequence);             // "Sequence 7: Magician"
std::string relationshipName(const Database& db, const Relationship& r);     // "Fors Wall (C-002)" or "Fors Wall"

}  // namespace lotm
