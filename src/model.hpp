// The data the program works with: pathways, characters, Sealed Artifacts and settings.
// Every struct here is saved to and loaded from JSON (see model.cpp).
#pragma once

#include <array>
#include <string>
#include <utility>
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
    std::vector<std::string> abilities;  // "Name: what it does"
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
    std::string description;  // optional overview: themes, Sefirah, Divine Kingdom ...
    std::string uniqueness;   // optional: the pathway's Uniqueness, its name and look
    std::vector<MovementUnlock> movement;
    std::vector<SequenceInfo> sequences;  // Sequence 9 down to 0

    // Not stored in the file: true for pathways from custom_pathways.json, which the
    // program's Pathways menu can edit. The built-in ones in pathways.json are edited by hand.
    bool custom = false;

    const SequenceInfo* findSequence(int sequence) const;
};

// ---------- Characters (data/characters.json) ----------

struct Appearance {
    std::string hair;  // colour
    std::string hairLength;
    std::string hairStyle;
    std::string eyes;
    std::string skin;
    std::string face;
    std::string voice;
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

// An official file on the character, as a church or agency would keep it.
struct Dossier {
    std::string filedBy;        // "Nighthawks (Church of the Evernight Goddess)"
    std::string recentActions;  // one of kRecentActions in rules.hpp, or empty
    std::string recentActionsNote;
    std::string threatLevel;    // set by hand; empty = worked out automatically (see assessThreat)
    std::string status;         // Active, At large, In custody ...
    std::string lastSeen;
    std::string remarks;

    bool empty() const;
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
    int beyonderCharacteristics = 1;  // Sequence 1 only: 1 or 2
    bool hasUniqueness = false;       // Sequence 1 only
    std::string uniquenessForm;
    std::vector<std::string> uniquenessAbilities;  // Sequence 0 abilities the Uniqueness grants
    StatBlock stats;
    std::vector<int> artifactIds;
    Affiliation affiliation;
    std::vector<Relationship> relationships;
    Dossier dossier;
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
    int windowTextSize = 100;  // app window only: text size in percent
    std::string windowTheme = "night";  // app window only: night (dark) or parchment (light)
};

// Everything loaded in memory while the program runs.
struct Database {
    std::vector<Pathway> pathways;
    std::vector<Character> characters;
    std::vector<Artifact> artifacts;
    Settings settings;

    const Pathway* findPathway(const std::string& id) const;
    Pathway* findPathway(const std::string& id);
    Character* findCharacter(int id);
    const Character* findCharacter(int id) const;
    // Saved characters (other than excludeId) whose name contains the text, ignoring case.
    // An exact name match is returned on its own.
    std::vector<const Character*> findCharactersByName(const std::string& text, int excludeId) const;
    Artifact* findArtifact(int id);
    const Artifact* findArtifact(int id) const;
    int nextCharacterId() const;
    int nextArtifactId() const;
    // A free id for a new pathway, made from its name: "Mystery Pryer" -> "mystery_pryer" (or "mystery_pryer_2").
    std::string newPathwayId(const std::string& name) const;
    // Names of the characters and Sealed Artifacts that belong to this pathway.
    std::vector<std::string> pathwayUsers(const std::string& id) const;
};

// Display helpers shared by every screen and export.
std::string characterCode(int id);  // "C-001"
std::string artifactCode(int id);   // "A-001"
std::string pathwayLabel(const Database& db, const std::string& pathwayId);  // "Seer (The Fool)"
std::string sequenceLabel(const Pathway* pathway, int sequence);             // "Sequence 7: Magician"
std::string relationshipName(const Database& db, const Relationship& r);     // "Fors Wall (C-002)" or "Fors Wall"

// Abilities are stored as "Name: what it does". splitAbility gives {"Name", "what it does"}; only a
// short name before the first ": " counts, so text without one comes back whole as the description.
std::pair<std::string, std::string> splitAbility(const std::string& ability);
std::string joinAbility(const std::string& name, const std::string& description);

}  // namespace lotm
