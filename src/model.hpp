// The data the program works with: pathways, Sefirot, characters, Sealed Artifacts, story events,
// organizations and settings.
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

// ---------- Sefirot (data/sefirot.json, and your own in data/custom_sefirot.json) ----------

// A Sefirah: a source of power and authority that gathers the Uniquenesses of a group of
// neighbouring pathways. Holding it and every one of those Uniquenesses makes a Great Old One.
struct Sefirah {
    std::string id;           // short key used in save files, e.g. "sefirah_castle"
    std::string name;         // "Sefirah Castle"
    std::vector<std::string> alsoKnownAs;
    std::string greatOldOne;  // the Great Old One it belongs to, e.g. "Lord of the Mysteries"
    // Above the Sequence, the step past Sequence 0 that this Sefirah's Great Old One stands on:
    // what it symbolizes, and the abilities a Great Old One of this Sefirah has ("Name: what it does").
    std::string symbolism;
    std::vector<std::string> abilities;
    std::vector<std::string> pathwayIds;  // the pathway group, e.g. seer, apprentice, marauder
    std::string description;
    std::vector<std::string> knownHolders;  // who held it in the story, one line each
    std::string notes;                      // sources, and what is a guess

    // Not stored in the file: true for your own Sefirot from custom_sefirot.json, which the program
    // creates, edits and deletes. The ones in sefirot.json are edited by hand.
    bool custom = false;
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
    // A relationship with a whole saved organization instead of one person (characterId is then 0). It
    // reaches every member, unless a member has a relationship of their own with this character; see
    // tiesThroughOrganizations in relations.hpp.
    int organizationId = 0;
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

// A dual-pathway god's second pathway: the Sequence 1 Beyonder characteristic(s) of another pathway
// and, usually, its Uniqueness. Amon, Sequence 0 Error, took the Door Uniqueness and a Key of Stars
// characteristic this way. Only a Sequence 0 god can have one (see dualPathwayEligible in rules.hpp).
struct SecondPathway {
    std::string pathwayId;            // empty = a single pathway
    int beyonderCharacteristics = 1;  // its Sequence 1 characteristics: 1 or 2
    bool hasUniqueness = true;        // holds that pathway's Uniqueness as well
    std::string uniquenessForm;
    // No longer used: a dual-pathway god has every ability of the second pathway. Older saves may
    // still have picked powers here; normalizeCharacter clears them.
    std::vector<std::string> uniquenessAbilities;

    bool empty() const { return pathwayId.empty(); }
};

// A Sequence a mixed-pathway Beyonder took from another pathway than the one they are on now. A
// Sequence 4 Witch (Assassin pathway, Demoness) who climbed the Spectator pathway up to Sequence 5
// has Sequences 9 to 5 listed here with "spectator".
struct SequenceStep {
    int sequence = 9;
    std::string pathwayId;
};

// A date in the story's world, written the way the books write them: "3 July 1360". The month and day
// are optional ("July 1360", "1360"). Dates are in the Fifth Epoch, the one the books are set in,
// unless another epoch is named ("1200, Fourth Epoch"). See story.hpp for reading and writing them.
struct StoryDate {
    int epoch = 5;
    int year = 0;   // 0 = no date
    int month = 0;  // 1-12, 0 = not known
    int day = 0;    // 1-31, 0 = not known

    bool empty() const { return year == 0; }
};

// A character earlier in your story: what they were like from a date on, until their next stage. The
// character's own fields are how they are now; a stage only holds what tends to change as a story goes
// on, and everything else (name, looks, stats ...) comes from now. See characterAsOf in story.hpp.
struct CharacterStage {
    StoryDate date;
    std::string label;  // what changed then, "Becomes the Magician"
    std::string pathwayId;
    int sequence = 9;
    bool aboveSequence = false;
    // At Sequence 1: one or two Beyonder characteristics and the Uniqueness (a King of Angels has two
    // or the Uniqueness). At Sequence 0: a second pathway (a dual-pathway god). As on the character.
    int beyonderCharacteristics = 1;
    bool hasUniqueness = false;
    std::string uniquenessForm;
    std::vector<std::string> uniquenessAbilities;
    SecondPathway secondPathway;
    std::string sefirahId;
    std::string stateOfMind;
    std::string life;  // alive or dead then: empty = alive, "Dead", or your own words
    Affiliation affiliation;
    std::string status;  // as in the dossier: Active, Missing, Deceased ...
    std::vector<std::string> titles;
    std::vector<std::string> aliases;
    std::vector<int> artifactIds;  // the Sealed Artifacts they held then
    std::string note;
};

struct Character {
    int id = 0;
    std::string name;
    Appearance appearance;
    std::string age;
    std::string gender;
    std::string shortDescription;
    std::string backstory;
    std::string pathwayId;  // empty = ordinary mortal; a mixed-pathway Beyonder's pathway now
    int sequence = 9;
    // A mixed-pathway Beyonder: the Sequences weaker than their current one that came from another
    // pathway, weakest first. Every Sequence not listed came from pathwayId. Empty = one pathway.
    std::vector<SequenceStep> mixedSequences;
    // State of mind, often shaken by switching to a pathway that isn't a neighbour: one of
    // kStatesOfMind in rules.hpp, or your own words. Empty = not recorded.
    std::string stateOfMind;
    std::string stateOfMindNote;
    // Alive or dead now: "Dead", "Alive", or your own words ("Revived", "A wandering spirit"). Empty is
    // alive, unless their latest stage or a change at a story event has them dead (see deathOf in story.hpp).
    std::string life;
    std::string lifeNote;  // how they died, or came back
    StoryDate diedOn;      // when they died, for one who is dead now
    std::string alignment;
    std::vector<std::string> titles;
    std::vector<std::string> aliases;
    std::vector<std::string> honorificName;  // usually three lines, sometimes more; Sequence 3 and stronger only
    int beyonderCharacteristics = 1;  // Sequence 1 only: 1 or 2
    bool hasUniqueness = false;       // Sequence 1 only
    std::string uniquenessForm;
    std::vector<std::string> uniquenessAbilities;  // Sequence 0 abilities the Uniqueness grants
    // Above the Sequence: a Great Old One, past Sequence 0 (which `sequence` stays at). It holds its
    // group's Sefirah and every Uniqueness of the neighbouring pathways.
    bool aboveSequence = false;
    SecondPathway secondPathway;  // a dual-pathway god's second pathway (Sequence 0 only)
    std::string sefirahId;        // the Sefirah they hold, if any
    StatBlock stats;
    std::vector<int> artifactIds;
    Affiliation affiliation;
    std::vector<Relationship> relationships;
    Dossier dossier;
    std::string notes;
    // The character earlier in your story, oldest first. What they are now starts on nowSince, when
    // it's given; otherwise "now" is the end of the story.
    std::vector<CharacterStage> stages;
    StoryDate nowSince;
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

// ---------- Story events (data/events.json) ----------

// Someone who takes part in an event: one of your saved characters, or anyone by name.
struct EventPerson {
    int characterId = 0;  // 0 = someone who isn't a saved character
    std::string name;     // for a saved character, their name when they were added
    std::string role;     // optional: "attacker", "witness", "host" ...
    // What changed for a saved character here (an advancement, joining an organization ...): from the
    // event's date on they are as this stage says, until their next stage. Its date is the event's.
    // See stagesOf in story.hpp.
    bool changed = false;
    CharacterStage change;
};

// Something that happens in your story, on the timeline.
struct StoryEvent {
    int id = 0;
    std::string title;
    StoryDate date;
    std::string kind;     // one of kEventKinds in story.hpp, or your own word
    std::string place;
    std::string chapter;  // where it is in your book: "Chapter 12", "Book 2, chapter 3"
    std::vector<EventPerson> people;
    std::vector<int> artifactIds;
    std::string description;
    std::string notes;
    std::string createdAt;
    std::string updatedAt;
};

// ---------- Organizations (data/organizations.json) ----------

// A group your characters belong to: the Tarot Club, the Nighthawks, a church, a family. A character's
// affiliation stays typed text; the members of an organization are the characters whose affiliation
// names it (its name or one it is also known as, ignoring capitals). See organizations.hpp.
struct Organization {
    int id = 0;
    std::string name;
    std::vector<std::string> alsoKnownAs;  // other names for it, which an affiliation can use too
    std::string kind;                      // one of kOrganizationKinds in organizations.hpp, or your own word
    std::string status;                    // Active, In hiding, Disbanded ...
    int leaderId = 0;                      // a saved character, or 0 for someone who isn't one
    std::string leader;                    // who leads it; for a saved character, their name when picked
    std::string headquarters;
    StoryDate founded;
    std::string symbol;                    // its emblem, or how members know each other
    std::vector<std::string> pathwayIds;   // the pathways it is known for; the first gives its colour
    std::vector<std::string> ranks;        // highest first: "Deacon", "Captain", "Nighthawk"
    std::string description;
    std::vector<std::string> otherMembers;  // members who aren't saved characters, one line each
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
    bool windowAnimations = true;       // app window only: things move smoothly (dice tumble, tabs fade in)
};

// Everything loaded in memory while the program runs.
struct Database {
    std::vector<Pathway> pathways;
    std::vector<Sefirah> sefirot;
    std::vector<Character> characters;
    std::vector<Artifact> artifacts;
    std::vector<StoryEvent> events;
    std::vector<Organization> organizations;
    Settings settings;

    const Pathway* findPathway(const std::string& id) const;
    Pathway* findPathway(const std::string& id);
    const Sefirah* findSefirah(const std::string& id) const;
    // The Sefirah that gathers this pathway's Uniqueness, or nullptr (your own pathways have none).
    const Sefirah* sefirahForPathway(const std::string& pathwayId) const;
    // The pathway and its neighbours, whose Uniquenesses a Great Old One holds: the pathways of its
    // Sefirah, or else the pathways with the same group name. The pathway itself comes first.
    std::vector<const Pathway*> pathwayGroup(const std::string& pathwayId) const;
    // Saved characters who hold this Sefirah.
    std::vector<const Character*> sefirahHolders(const std::string& sefirahId) const;
    Character* findCharacter(int id);
    const Character* findCharacter(int id) const;
    // Saved characters (other than excludeId) whose name contains the text, ignoring case.
    // An exact name match is returned on its own.
    std::vector<const Character*> findCharactersByName(const std::string& text, int excludeId) const;
    Artifact* findArtifact(int id);
    const Artifact* findArtifact(int id) const;
    StoryEvent* findEvent(int id);
    const StoryEvent* findEvent(int id) const;
    Organization* findOrganization(int id);
    const Organization* findOrganization(int id) const;
    int nextCharacterId() const;
    int nextArtifactId() const;
    int nextEventId() const;
    int nextOrganizationId() const;
    // A free id for a new pathway, made from its name: "Mystery Pryer" -> "mystery_pryer" (or "mystery_pryer_2").
    std::string newPathwayId(const std::string& name) const;
    // The same for a new Sefirah: "Sefirah of Masks" -> "sefirah_of_masks".
    std::string newSefirahId(const std::string& name) const;
    // Another Sefirah that already gathers this pathway, or nullptr.
    const Sefirah* otherSefirahFor(const std::string& pathwayId, const std::string& sefirahId) const;
    // Names of the characters and Sealed Artifacts that belong to this pathway (a mixed-pathway
    // Beyonder with a Sequence from it too).
    std::vector<std::string> pathwayUsers(const std::string& id) const;
};

// Display helpers shared by every screen and export.
std::string characterCode(int id);  // "C-001"
std::string artifactCode(int id);   // "A-001"
std::string eventCode(int id);      // "E-001"
std::string organizationCode(int id);  // "O-001"
std::string pathwayLabel(const Database& db, const std::string& pathwayId);  // "Seer (The Fool)"
std::string sequenceLabel(const Pathway* pathway, int sequence);             // "Sequence 7: Magician"
std::string sefirahLabel(const Database& db, const std::string& sefirahId);   // "Sefirah Castle (Lord of the Mysteries)"
// "Fors Wall (C-002)", "Aurora Order (O-003)" for an organization, or "Fors Wall"
std::string relationshipName(const Database& db, const Relationship& r);
// "Klein Moretti" -> "KM": the first letter of the first and last word, stopping at a bracket.
std::string nameInitials(const std::string& name);

// Abilities are stored as "Name: what it does". splitAbility gives {"Name", "what it does"}; only a
// short name before the first ": " counts, so text without one comes back whole as the description.
std::pair<std::string, std::string> splitAbility(const std::string& ability);
std::string joinAbility(const std::string& name, const std::string& description);

}  // namespace lotm
