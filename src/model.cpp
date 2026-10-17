#include "model.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace lotm {

namespace {

std::string lowercase(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

}  // namespace

int statIndex(const std::string& code) {
    std::string upper;
    for (char c : code) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (int i = 0; i < kStatCount; ++i) {
        if (upper == kStatCodes[i]) return i;
    }
    return -1;
}

bool Dossier::empty() const {
    return filedBy.empty() && recentActions.empty() && recentActionsNote.empty() && threatLevel.empty() &&
           status.empty() && lastSeen.empty() && remarks.empty();
}

const SequenceInfo* Pathway::findSequence(int sequence) const {
    for (const auto& s : sequences) {
        if (s.sequence == sequence) return &s;
    }
    return nullptr;
}

const Pathway* Database::findPathway(const std::string& id) const {
    if (id.empty()) return nullptr;
    for (const auto& p : pathways) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

Pathway* Database::findPathway(const std::string& id) {
    return const_cast<Pathway*>(static_cast<const Database&>(*this).findPathway(id));
}

const Sefirah* Database::findSefirah(const std::string& id) const {
    if (id.empty()) return nullptr;
    for (const auto& s : sefirot) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const Sefirah* Database::sefirahForPathway(const std::string& pathwayId) const {
    if (pathwayId.empty()) return nullptr;
    for (const auto& s : sefirot) {
        if (std::find(s.pathwayIds.begin(), s.pathwayIds.end(), pathwayId) != s.pathwayIds.end()) return &s;
    }
    return nullptr;
}

std::vector<const Pathway*> Database::pathwayGroup(const std::string& pathwayId) const {
    std::vector<const Pathway*> group;
    const Pathway* own = findPathway(pathwayId);
    if (!own) return group;
    group.push_back(own);
    if (const Sefirah* s = sefirahForPathway(pathwayId)) {
        for (const auto& id : s->pathwayIds) {
            if (const Pathway* p = findPathway(id); p && p != own) group.push_back(p);
        }
        return group;
    }
    if (own->group.empty()) return group;
    for (const auto& p : pathways) {
        if (&p != own && p.group == own->group) group.push_back(&p);
    }
    return group;
}

std::vector<const Character*> Database::sefirahHolders(const std::string& sefirahId) const {
    std::vector<const Character*> holders;
    for (const auto& c : characters) {
        if (!sefirahId.empty() && c.sefirahId == sefirahId) holders.push_back(&c);
    }
    return holders;
}

Character* Database::findCharacter(int id) {
    for (auto& c : characters) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const Character* Database::findCharacter(int id) const {
    for (const auto& c : characters) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

std::vector<const Character*> Database::findCharactersByName(const std::string& text, int excludeId) const {
    const std::string wanted = lowercase(text);
    std::vector<const Character*> matches;
    if (wanted.empty()) return matches;
    for (const auto& c : characters) {
        if (c.id == excludeId) continue;
        const std::string name = lowercase(c.name);
        if (name == wanted) return {&c};
        if (name.find(wanted) != std::string::npos) matches.push_back(&c);
    }
    return matches;
}

Artifact* Database::findArtifact(int id) {
    for (auto& a : artifacts) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

const Artifact* Database::findArtifact(int id) const {
    for (const auto& a : artifacts) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

StoryEvent* Database::findEvent(int id) {
    for (auto& e : events) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const StoryEvent* Database::findEvent(int id) const {
    for (const auto& e : events) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

int Database::nextCharacterId() const {
    int highest = 0;
    for (const auto& c : characters) highest = std::max(highest, c.id);
    return highest + 1;
}

int Database::nextArtifactId() const {
    int highest = 0;
    for (const auto& a : artifacts) highest = std::max(highest, a.id);
    return highest + 1;
}

int Database::nextEventId() const {
    int highest = 0;
    for (const auto& e : events) highest = std::max(highest, e.id);
    return highest + 1;
}

namespace {

// "Mystery Pryer" -> "mystery_pryer"; fallback when nothing usable is left.
std::string idFromName(const std::string& name, const char* fallback) {
    std::string id;
    for (unsigned char ch : name) {
        if (std::isalnum(ch) && ch < 128) id += static_cast<char>(std::tolower(ch));
        else if (!id.empty() && id.back() != '_') id += '_';
    }
    while (!id.empty() && id.back() == '_') id.pop_back();
    return id.empty() ? fallback : id;
}

}  // namespace

std::string Database::newPathwayId(const std::string& name) const {
    const std::string id = idFromName(name, "pathway");
    std::string unique = id;
    for (int n = 2; findPathway(unique); ++n) unique = id + "_" + std::to_string(n);
    return unique;
}

std::string Database::newSefirahId(const std::string& name) const {
    const std::string id = idFromName(name, "sefirah");
    std::string unique = id;
    for (int n = 2; findSefirah(unique); ++n) unique = id + "_" + std::to_string(n);
    return unique;
}

const Sefirah* Database::otherSefirahFor(const std::string& pathwayId, const std::string& sefirahId) const {
    for (const auto& s : sefirot) {
        if (s.id == sefirahId) continue;
        if (std::find(s.pathwayIds.begin(), s.pathwayIds.end(), pathwayId) != s.pathwayIds.end()) return &s;
    }
    return nullptr;
}

std::vector<std::string> Database::pathwayUsers(const std::string& id) const {
    std::vector<std::string> users;
    for (const auto& c : characters) {
        const bool mixedIn = std::any_of(c.mixedSequences.begin(), c.mixedSequences.end(),
                                         [&](const SequenceStep& step) { return step.pathwayId == id; });
        if (c.pathwayId == id || mixedIn) users.push_back(c.name + " (" + characterCode(c.id) + ")");
    }
    for (const auto& a : artifacts) {
        if (a.pathwayId == id) users.push_back(a.name + " (" + artifactCode(a.id) + ")");
    }
    return users;
}

static std::string code(char prefix, int id) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%c-%03d", prefix, id);
    return buffer;
}

std::string characterCode(int id) { return code('C', id); }
std::string artifactCode(int id) { return code('A', id); }
std::string eventCode(int id) { return code('E', id); }

std::string pathwayLabel(const Database& db, const std::string& pathwayId) {
    if (pathwayId.empty()) return "None";
    const Pathway* p = db.findPathway(pathwayId);
    if (!p) return pathwayId + " (pathway not found)";
    if (p->god.empty()) return p->name;
    return p->name + " (" + p->god + ")";
}

std::string sequenceLabel(const Pathway* pathway, int sequence) {
    std::string label = "Sequence " + std::to_string(sequence);
    if (pathway) {
        if (const SequenceInfo* s = pathway->findSequence(sequence); s && !s->name.empty()) {
            label += ": " + s->name;
        }
    }
    return label;
}

std::string sefirahLabel(const Database& db, const std::string& sefirahId) {
    if (sefirahId.empty()) return "None";
    const Sefirah* s = db.findSefirah(sefirahId);
    if (!s) return sefirahId + " (Sefirah not found)";
    if (s->greatOldOne.empty() || s->greatOldOne == s->name) return s->name;
    return s->name + " (" + s->greatOldOne + ")";
}

std::pair<std::string, std::string> splitAbility(const std::string& ability) {
    const size_t colon = ability.find(": ");
    if (colon == std::string::npos || colon == 0 || colon > 48 || ability.substr(0, colon).find(". ") != std::string::npos) {
        return {"", ability};
    }
    return {ability.substr(0, colon), ability.substr(colon + 2)};
}

std::string joinAbility(const std::string& name, const std::string& description) {
    if (name.empty()) return description;
    if (description.empty()) return name;
    return name + ": " + description;
}

std::string nameInitials(const std::string& name) {
    // The first letter of each word (stopping at a bracket), then the first and last of those.
    std::vector<std::string> letters;
    bool wordStart = true;
    for (size_t i = 0; i < name.size();) {
        size_t end = i + 1;
        while (end < name.size() && (static_cast<unsigned char>(name[end]) & 0xC0) == 0x80) ++end;  // one UTF-8 letter
        const unsigned char ch = static_cast<unsigned char>(name[i]);
        if (ch == '(') break;
        if (ch == ' ' || ch == '-') {
            wordStart = true;
        } else {
            if (wordStart && (std::isalpha(ch) || ch >= 0x80)) {
                std::string letter = name.substr(i, end - i);
                if (letter.size() == 1) letter[0] = static_cast<char>(std::toupper(ch));
                letters.push_back(letter);
            }
            wordStart = false;
        }
        i = end;
    }
    if (letters.empty()) return "?";
    return letters.size() == 1 ? letters[0] : letters.front() + letters.back();
}

std::string relationshipName(const Database& db, const Relationship& r) {
    if (const Character* other = r.characterId != 0 ? db.findCharacter(r.characterId) : nullptr) {
        return other->name + " (" + characterCode(other->id) + ")";
    }
    return r.name.empty() ? "(unknown)" : r.name;
}

}  // namespace lotm
