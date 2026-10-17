// Keeps relationships two-way: when one character names another saved character, the other
// character gets the matching relationship back (Mentor <-> Student, Friend <-> Friend ...). A
// relationship with a whole organization reaches its members instead (see Tie below).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "model.hpp"

namespace lotm {

// The relationship seen from the other side: Mentor <-> Student, Parent <-> Child,
// Superior <-> Subordinate; every other type stays the same.
std::string reciprocalType(const std::string& type);

// Call just before saving `c`. `before` is the saved version of `c` (nullptr for a new character),
// and `c` itself may not be in db.characters yet. Updates the other characters in db and `c`:
//   - a saved character `c` links to gets the matching relationship back, if it hasn't one;
//   - a link removed from `c` is removed from the other character too;
//   - relationships elsewhere that only name `c` (by name, ignoring case) become links to `c`.
// Returns one line per change, for showing to the user.
std::vector<std::string> syncRelationships(Database& db, Character& c, const Character* before);

// For screens where names are typed freely (the app window): a relationship whose name matches one
// saved character exactly (ignoring case) is linked to it, and a linked one whose name was changed to
// someone else is unlinked. A name that isn't a saved character's but names a saved organization (the
// way an affiliation does) is linked to the organization. Call before syncRelationships.
void linkRelationshipsByName(const Database& db, Character& c);

// ---- Relationships with organizations

// A relationship that reaches someone through an organization: one with the whole Aurora Order reaches
// each of its members. Seen from one character, either way round:
//   - outgoing: they have a relationship with the organization, and `other` is one of its members;
//   - incoming: `other` has a relationship with an organization they belong to.
// When the two have a relationship of their own (either of them lists the other), that one counts
// instead, and `overridden` is set.
struct Tie {
    const Character* other = nullptr;
    const Organization* organization = nullptr;
    std::string type;  // what `other` is to them: Enemy, Mentor, or Student for the other side of Mentor
    std::string note;
    bool outgoing = false;
    bool overridden = false;
};
// Every tie that reaches `c` through an organization: the outgoing ones first, then the incoming, each by
// organization and then by name. `c` may be a draft that isn't saved yet; members are the saved characters.
std::vector<Tie> tiesThroughOrganizations(const Database& db, const Character& c);
// What `b` is to `a` through organizations, when the two have no relationship of their own: one word per
// organization that ties them ("Enemy"), with the organization's name. Empty when nothing ties them.
std::vector<std::pair<std::string, std::string>> tiesBetween(const Database& db, const Character& a, const Character& b);

// Someone's relationship with a whole organization.
struct OrganizationRelationship {
    const Character* character = nullptr;
    const Relationship* relationship = nullptr;
};
// The saved characters who have a relationship with this organization, by name.
std::vector<OrganizationRelationship> relationshipsWithOrganization(const Database& db, int organizationId);

}  // namespace lotm
