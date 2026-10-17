// Organizations: the groups your characters belong to, such as the Tarot Club or the Nighthawks.
// A character's affiliation is typed text, so an organization's members are the saved characters whose
// affiliation names it; nothing else has to be kept up to date. Shared by the console, the app window
// and the exports.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "model.hpp"
#include "sheet.hpp"

namespace lotm {

// Suggestions for an organization's kind and status; any other word works too.
inline const std::vector<std::string> kOrganizationKinds = {
    "Church", "Secret organization", "Order", "Club", "Family", "Royal family", "Nation", "Government agency",
    "School of thought", "Company"};
inline const std::vector<std::string> kOrganizationStatuses = {"Active",    "Secret",    "In hiding", "Scattered",
                                                               "Disbanded", "Destroyed", "Unknown"};

// "Independent" (any capitals) or nothing at all: no organization.
bool isIndependent(const std::string& affiliation);
// Whether an affiliation, as typed on a character, names this organization: its name or a name it is
// also known as, ignoring capitals and spaces at the ends. Without a bracket, the part before a bracket
// counts too: "Nighthawks" names "Nighthawks (Church of the Evernight Goddess)", and the other way round.
bool namesOrganization(const Organization& o, const std::string& affiliation);
// The saved organization an affiliation names, or nullptr. When several do, its exact name wins, then a
// name it is also known as, then the part before a bracket; on a tie the oldest (lowest id).
const Organization* organizationNamed(const Database& db, const std::string& affiliation);
// Whether an affiliation makes someone a member of this organization: it names it, and no other saved
// organization takes it first (as organizationNamed decides). Works for one not saved yet too.
bool belongsTo(const Database& db, const Organization& o, const std::string& affiliation);

// A saved character who belongs to an organization now.
struct Member {
    const Character* character = nullptr;
    std::string rank;    // as typed on them
    int rankIndex = -1;  // its place in the organization's ranks, highest first; -1 for a rank it doesn't list
    bool leader = false;
};
// Its members: the leader first, then by rank as the organization lists them, then the rest by name.
std::vector<Member> membersOf(const Organization& o, const Database& db);
// Their rank as shown: "The Fool, leader", "Leader" for a leader with no rank (or one that says so already),
// the rank alone for anyone else, or empty.
std::string memberRole(const Member& m);

// A saved character who belonged to it in one of their stages (or a change at a story event), but not now.
struct FormerMember {
    const Character* character = nullptr;
    std::string rank;  // the rank they had last
    StoryDate from;    // the first stage it was named in
    StoryDate until;   // the stage after the last one it was named in; empty when not known
};
std::vector<FormerMember> formerMembersOf(const Organization& o, const Database& db);
// "1349 to 1351", "from 1349", or empty.
std::string formerMemberDates(const FormerMember& m);

// The leader's name: a saved character's name now, or the name typed. Empty when nobody leads it.
std::string leaderName(const Organization& o, const Database& db);
// Links a leader typed by name to the one saved character with that name (ignoring capitals), keeps a
// linked leader's name up to date, and unlinks one whose character is gone (keeping their name).
void linkLeaderByName(const Database& db, Organization& o);

// The number a character's power is ranked by: a mortal 0, Sequence 9 is 1 ... Sequence 0 is 10,
// Above the Sequence 11.
int powerScore(const Character& c);

// How strong an organization is, from its saved members who are alive (see deadNow in story.hpp).
struct OrganizationStrength {
    int members = 0;  // saved characters, alive
    int dead = 0;     // saved characters who died, not counted in anything below
    int others = 0;   // members who aren't saved characters
    const Character* strongest = nullptr;      // the highest powerScore (a King of Angels or dual-pathway god first on a tie)
    const Character* mostDangerous = nullptr;  // the highest threat level, then the highest threat score
    std::string threatLevel;                   // the most dangerous member's; empty without members
    int threatScore = 0;                       // every member's threat score added up (see assessThreat)
    std::array<int, 7> tiers{};                // members in each power tier, Mortal up to Above the Sequence
    std::array<int, 5> threats{};              // members at each threat level, Low up to Catastrophic
    int artifacts = 0;                         // Sealed Artifacts its members hold
};
OrganizationStrength organizationStrength(const Organization& o, const Database& db);
// "2 Above the Sequence, 1 Divine and 4 Angel": the power tiers, strongest first, leaving out empty ones.
std::string tierCounts(const OrganizationStrength& strength);

// The names to offer for an affiliation: your own organizations, then other names your characters use,
// then the ready-made ones (kOrganizations), each once.
std::vector<std::string> organizationSuggestions(const Database& db);
// The same without "Independent", for who filed a dossier.
std::vector<std::string> keeperSuggestions(const Database& db);
// The ranks to offer for an affiliation: the organization's own, then other ranks its members use.
std::vector<std::string> rankSuggestions(const Database& db, const std::string& affiliation);

// An organization your characters name (now or in a stage) that isn't saved yet.
struct UnsavedOrganization {
    std::string name;  // as most of them write it
    int members = 0;   // how many name it now
};
// Most members first, then by name.
std::vector<UnsavedOrganization> organizationsNotSaved(const Database& db);
// A new organization (id 0) made from what its members say: the name, and the ranks they use, most
// used first.
Organization organizationFromMembers(const Database& db, const std::string& name);

// The saved organizations, by name.
std::vector<const Organization*> organizationsByName(const Database& db);

Sheet buildOrganizationSheet(const Organization& o, const Database& db);

}  // namespace lotm
