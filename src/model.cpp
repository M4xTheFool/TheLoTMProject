#include "model.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace lotm {

int statIndex(const std::string& code) {
    std::string upper;
    for (char c : code) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (int i = 0; i < kStatCount; ++i) {
        if (upper == kStatCodes[i]) return i;
    }
    return -1;
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

}  // namespace lotm
