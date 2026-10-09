#include "records.hpp"

#include <algorithm>
#include <optional>

#include "relations.hpp"
#include "rules.hpp"

namespace lotm {

CharacterSaveReport saveCharacter(App& app, Character& c, bool isNew) {
    CharacterSaveReport report;
    report.cleanups = normalizeCharacter(c);
    const auto backup = app.db.characters;
    std::optional<Character> before;
    if (const Character* saved = isNew ? nullptr : app.db.findCharacter(c.id)) before = *saved;
    report.linked = syncRelationships(app.db, c, before ? &*before : nullptr);
    c.updatedAt = nowTimestamp();
    if (isNew) {
        c.createdAt = c.updatedAt;
        app.db.characters.push_back(c);
    } else if (Character* existing = app.db.findCharacter(c.id)) {
        *existing = c;
    }
    try {
        app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.characters = backup;
        throw;
    }
    return report;
}

int duplicateCharacter(App& app, int id) {
    const Character* original = app.db.findCharacter(id);
    if (!original) throw StorageError("That character no longer exists.");
    Character copy = *original;
    copy.id = app.db.nextCharacterId();
    copy.name += " (copy)";
    copy.createdAt = copy.updatedAt = nowTimestamp();
    app.db.characters.push_back(copy);
    try {
        app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.characters.pop_back();
        throw;
    }
    return copy.id;
}

void deleteCharacter(App& app, int id) {
    const auto backup = app.db.characters;
    const Character* deleted = app.db.findCharacter(id);
    const std::string name = deleted ? deleted->name : "";
    auto& list = app.db.characters;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const Character& c) { return c.id == id; }), list.end());
    for (auto& c : list) {
        for (auto& r : c.relationships) {
            if (r.characterId != id) continue;
            r.characterId = 0;
            if (!name.empty()) r.name = name;
        }
    }
    try {
        app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.characters = backup;
        throw;
    }
}

void saveArtifact(App& app, Artifact& a, bool isNew) {
    const auto backup = app.db.artifacts;
    a.updatedAt = nowTimestamp();
    if (isNew) {
        a.createdAt = a.updatedAt;
        app.db.artifacts.push_back(a);
    } else if (Artifact* existing = app.db.findArtifact(a.id)) {
        *existing = a;
    }
    try {
        app.storage.saveArtifacts(app.db);
    } catch (const StorageError&) {
        app.db.artifacts = backup;
        throw;
    }
}

void deleteArtifact(App& app, int id) {
    const auto artifactsBackup = app.db.artifacts;
    const auto charactersBackup = app.db.characters;
    auto& list = app.db.artifacts;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const Artifact& a) { return a.id == id; }), list.end());
    for (auto& c : app.db.characters) {
        c.artifactIds.erase(std::remove(c.artifactIds.begin(), c.artifactIds.end(), id), c.artifactIds.end());
    }
    try {
        app.storage.saveArtifacts(app.db);
        app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.artifacts = artifactsBackup;
        app.db.characters = charactersBackup;
        throw;
    }
}

void saveCustomPathway(App& app, Pathway& p, bool isNew) {
    const auto backup = app.db.pathways;
    p.custom = true;
    if (isNew) {
        p.id = app.db.newPathwayId(p.name);
        app.db.pathways.push_back(p);
    } else if (Pathway* stored = app.db.findPathway(p.id)) {
        *stored = p;
    }
    try {
        app.storage.saveCustomPathways(app.db);
    } catch (const StorageError&) {
        app.db.pathways = backup;
        throw;
    }
}

void deleteCustomPathway(App& app, const std::string& id) {
    const Pathway* p = app.db.findPathway(id);
    if (!p || !p->custom) throw StorageError("Only your own pathways can be deleted.");
    if (!app.db.pathwayUsers(id).empty()) {
        throw StorageError("Characters or Sealed Artifacts still belong to this pathway.");
    }
    const auto backup = app.db.pathways;
    auto& list = app.db.pathways;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Pathway& x) { return x.id == id; }), list.end());
    try {
        app.storage.saveCustomPathways(app.db);
    } catch (const StorageError&) {
        app.db.pathways = backup;
        throw;
    }
}

}  // namespace lotm
