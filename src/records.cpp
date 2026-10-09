#include "records.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

#include "relations.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}

}  // namespace

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

SampleImportReport importSampleSet(App& app, const SampleSet& set) {
    SampleImportReport report;
    const auto charactersBackup = app.db.characters;
    const auto artifactsBackup = app.db.artifacts;
    const std::string now = nowTimestamp();

    // The sample's own ids, mapped to the ids they get here.
    std::map<int, int> artifactIds;
    for (Artifact a : set.artifacts) {
        const auto saved = std::find_if(app.db.artifacts.begin(), app.db.artifacts.end(),
                                        [&](const Artifact& x) { return sameName(x.name, a.name); });
        if (saved != app.db.artifacts.end()) {
            artifactIds[a.id] = saved->id;
            report.skipped.push_back(a.name);
            continue;
        }
        const int sampleId = a.id;
        a.id = app.db.nextArtifactId();
        a.createdAt = a.updatedAt = now;
        artifactIds[sampleId] = a.id;
        app.db.artifacts.push_back(a);
        report.addedArtifacts.push_back(a.name);
    }

    std::map<int, int> characterIds;
    std::vector<Character> added;
    int nextId = app.db.nextCharacterId();
    for (const Character& c : set.characters) {
        const auto saved = std::find_if(app.db.characters.begin(), app.db.characters.end(),
                                        [&](const Character& x) { return sameName(x.name, c.name); });
        if (saved != app.db.characters.end()) {
            characterIds[c.id] = saved->id;
            report.skipped.push_back(c.name);
            continue;
        }
        characterIds[c.id] = nextId;
        added.push_back(c);
        added.back().id = nextId++;
    }

    for (Character& c : added) {
        std::vector<int> held;
        for (int id : c.artifactIds) {
            if (auto it = artifactIds.find(id); it != artifactIds.end()) held.push_back(it->second);
        }
        c.artifactIds = held;
        for (Relationship& r : c.relationships) {
            const auto it = characterIds.find(r.characterId);
            r.characterId = it != characterIds.end() ? it->second : 0;
        }
        normalizeCharacter(c);
        c.createdAt = c.updatedAt = now;
        app.db.characters.push_back(c);
        report.addedCharacters.push_back(c.name);
    }
    // Links to characters saved before the import go both ways, like a normal save.
    for (const Character& c : added) {
        Character* stored = app.db.findCharacter(c.id);
        linkRelationshipsByName(app.db, *stored);
        syncRelationships(app.db, *stored, nullptr);
    }

    try {
        if (!report.addedArtifacts.empty()) app.storage.saveArtifacts(app.db);
        if (!report.addedCharacters.empty()) app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.characters = charactersBackup;
        app.db.artifacts = artifactsBackup;
        throw;
    }
    return report;
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
