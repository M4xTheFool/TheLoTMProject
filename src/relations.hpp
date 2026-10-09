// Keeps relationships two-way: when one character names another saved character, the other
// character gets the matching relationship back (Mentor <-> Student, Friend <-> Friend ...).
#pragma once

#include <string>
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
// someone else is unlinked. Call before syncRelationships.
void linkRelationshipsByName(const Database& db, Character& c);

}  // namespace lotm
