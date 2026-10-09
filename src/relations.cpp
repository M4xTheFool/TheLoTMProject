#include "relations.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace lotm {

namespace {

std::string lowercase(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool sameType(const std::string& a, const std::string& b) { return lowercase(a) == lowercase(b); }

bool hasLink(const Character& from, int toId, const std::string& type) {
    return std::any_of(from.relationships.begin(), from.relationships.end(), [&](const Relationship& r) {
        return r.characterId == toId && sameType(r.type, type);
    });
}

}  // namespace

std::string reciprocalType(const std::string& type) {
    static const std::pair<const char*, const char*> kPairs[] = {
        {"Mentor", "Student"}, {"Parent", "Child"}, {"Superior", "Subordinate"}};
    for (const auto& [a, b] : kPairs) {
        if (sameType(type, a)) return b;
        if (sameType(type, b)) return a;
    }
    return type;
}

std::vector<std::string> syncRelationships(Database& db, Character& c, const Character* before) {
    std::vector<std::string> changes;

    // Name-only relationships elsewhere that name this character become real links.
    const std::string name = lowercase(c.name);
    for (auto& other : db.characters) {
        if (other.id == c.id || name.empty()) continue;
        for (auto& r : other.relationships) {
            if (r.characterId != 0 || lowercase(r.name) != name) continue;
            r.characterId = c.id;
            changes.push_back(other.name + " already listed " + c.name + " (" + r.type + "); the two are now linked.");
            if (!hasLink(c, other.id, reciprocalType(r.type))) {
                c.relationships.push_back({other.id, other.name, reciprocalType(r.type), ""});
            }
        }
    }

    // Links removed from this character are removed on the other side too.
    if (before) {
        for (const auto& old : before->relationships) {
            if (old.characterId == 0 || hasLink(c, old.characterId, old.type)) continue;
            Character* other = db.findCharacter(old.characterId);
            if (!other) continue;
            auto& list = other->relationships;
            auto it = std::find_if(list.begin(), list.end(), [&](const Relationship& r) {
                return r.characterId == c.id && sameType(r.type, reciprocalType(old.type));
            });
            if (it == list.end()) continue;
            changes.push_back("Removed " + c.name + " (" + it->type + ") from " + other->name + ".");
            list.erase(it);
        }
    }

    // Every link from this character gets the matching relationship back.
    for (const auto& r : c.relationships) {
        if (r.characterId == 0 || r.characterId == c.id) continue;
        Character* other = db.findCharacter(r.characterId);
        if (!other) continue;
        const std::string back = reciprocalType(r.type);
        if (hasLink(*other, c.id, back)) continue;
        other->relationships.push_back({c.id, c.name, back, ""});
        changes.push_back("Added " + c.name + " to " + other->name + " as " + back + ".");
    }
    return changes;
}

void linkRelationshipsByName(const Database& db, Character& c) {
    for (auto& r : c.relationships) {
        if (r.characterId != 0) {
            const Character* linked = db.findCharacter(r.characterId);
            if (linked && lowercase(linked->name) == lowercase(r.name)) continue;
            r.characterId = 0;
        }
        const auto matches = db.findCharactersByName(r.name, c.id);
        if (!r.name.empty() && matches.size() == 1 && lowercase(matches[0]->name) == lowercase(r.name)) {
            r.characterId = matches[0]->id;
            r.name = matches[0]->name;
        }
    }
}

}  // namespace lotm
