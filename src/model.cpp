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

static std::string code(char prefix, int id) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%c-%03d", prefix, id);
    return buffer;
}

std::string characterCode(int id) { return code('C', id); }
std::string artifactCode(int id) { return code('A', id); }

std::string pathwayLabel(const Database& db, const std::string& pathwayId) {
    if (pathwayId.empty()) return "None";
    const Pathway* p = db.findPathway(pathwayId);
    if (!p) return pathwayId + " (missing from pathways.json)";
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

std::string relationshipName(const Database& db, const Relationship& r) {
    if (const Character* other = r.characterId != 0 ? db.findCharacter(r.characterId) : nullptr) {
        return other->name + " (" + characterCode(other->id) + ")";
    }
    return r.name.empty() ? "(unknown)" : r.name;
}

}  // namespace lotm
