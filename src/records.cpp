#include "records.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

#include "model_json.hpp"
#include "organizations.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "story.hpp"

namespace lotm {

namespace {

bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// Trims each line and drops the empty ones.
void tidyLines(std::vector<std::string>& lines) {
    for (auto& line : lines) line = trimmed(line);
    lines.erase(std::remove(lines.begin(), lines.end(), std::string()), lines.end());
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
    // An organization led by someone typed by name gets them as its leader once they are saved, and one
    // they lead follows a new name.
    const auto organizationsBackup = app.db.organizations;
    bool leadersChanged = false;
    for (Organization& o : app.db.organizations) {
        const int leaderId = o.leaderId;
        const std::string leader = o.leader;
        linkLeaderByName(app.db, o);
        leadersChanged = leadersChanged || o.leaderId != leaderId || o.leader != leader;
    }
    try {
        app.storage.saveCharacters(app.db);
        if (leadersChanged) app.storage.saveOrganizations(app.db);
    } catch (const StorageError&) {
        app.db.characters = backup;
        app.db.organizations = organizationsBackup;
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
    const auto eventsBackup = app.db.events;
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
    bool inEvents = false;
    for (auto& e : app.db.events) {
        for (auto& p : e.people) {
            if (p.characterId != id) continue;
            p.characterId = 0;
            if (!name.empty()) p.name = name;
            p.changed = false;  // only a saved character has stages
            p.change = CharacterStage{};
            inEvents = true;
        }
    }
    const auto organizationsBackup = app.db.organizations;
    bool led = false;
    for (auto& o : app.db.organizations) {
        if (o.leaderId != id) continue;
        o.leaderId = 0;
        if (!name.empty()) o.leader = name;
        led = true;
    }
    try {
        app.storage.saveCharacters(app.db);
        if (inEvents) app.storage.saveEvents(app.db);
        if (led) app.storage.saveOrganizations(app.db);
    } catch (const StorageError&) {
        app.db.characters = backup;
        app.db.events = eventsBackup;
        app.db.organizations = organizationsBackup;
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
    const auto eventsBackup = app.db.events;
    auto drop = [id](std::vector<int>& ids) {
        const size_t before = ids.size();
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        return ids.size() != before;
    };
    auto& list = app.db.artifacts;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const Artifact& a) { return a.id == id; }), list.end());
    for (auto& c : app.db.characters) {
        drop(c.artifactIds);
        for (auto& stage : c.stages) drop(stage.artifactIds);
    }
    bool inEvents = false;
    for (auto& e : app.db.events) {
        inEvents = drop(e.artifactIds) || inEvents;
        for (auto& p : e.people) inEvents = drop(p.change.artifactIds) || inEvents;
    }
    try {
        app.storage.saveArtifacts(app.db);
        app.storage.saveCharacters(app.db);
        if (inEvents) app.storage.saveEvents(app.db);
    } catch (const StorageError&) {
        app.db.artifacts = artifactsBackup;
        app.db.characters = charactersBackup;
        app.db.events = eventsBackup;
        throw;
    }
}

void tidyEventChanges(StoryEvent& e) {
    for (EventPerson& p : e.people) {
        if (p.characterId == 0 || !p.changed) {  // only a saved character has stages
            p.changed = false;
            p.change = CharacterStage{};
            continue;
        }
        normalizeStage(p.change);
        p.change.date = e.date;  // always the event's
    }
}

void linkEventPeopleByName(const Database& db, StoryEvent& e) {
    for (EventPerson& p : e.people) {
        if (p.characterId != 0) {
            if (const Character* c = db.findCharacter(p.characterId)) {
                p.name = c->name;
                continue;
            }
            p.characterId = 0;
        }
        const auto matches = db.findCharactersByName(p.name, 0);
        if (!p.name.empty() && matches.size() == 1 && sameName(matches[0]->name, p.name)) {
            p.characterId = matches[0]->id;
            p.name = matches[0]->name;
        }
    }
}

void saveEvent(App& app, StoryEvent& e, bool isNew) {
    const auto backup = app.db.events;
    e.people.erase(std::remove_if(e.people.begin(), e.people.end(),
                                  [](const EventPerson& p) { return p.characterId == 0 && p.name.empty(); }),
                   e.people.end());
    linkEventPeopleByName(app.db, e);
    tidyEventChanges(e);
    e.updatedAt = nowTimestamp();
    if (isNew) {
        e.createdAt = e.updatedAt;
        app.db.events.push_back(e);
    } else if (StoryEvent* existing = app.db.findEvent(e.id)) {
        *existing = e;
    }
    try {
        app.storage.saveEvents(app.db);
    } catch (const StorageError&) {
        app.db.events = backup;
        throw;
    }
}

void deleteEvent(App& app, int id) {
    const auto backup = app.db.events;
    auto& list = app.db.events;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const StoryEvent& e) { return e.id == id; }), list.end());
    try {
        app.storage.saveEvents(app.db);
    } catch (const StorageError&) {
        app.db.events = backup;
        throw;
    }
}

int saveOrganization(App& app, Organization& o, bool isNew) {
    const auto organizationsBackup = app.db.organizations;
    const auto charactersBackup = app.db.characters;
    const auto eventsBackup = app.db.events;
    o.name = trimmed(o.name);
    tidyLines(o.alsoKnownAs);
    tidyLines(o.ranks);
    tidyLines(o.otherMembers);
    std::vector<std::string> pathways;
    for (const auto& id : o.pathwayIds) {
        if (!id.empty() && std::find(pathways.begin(), pathways.end(), id) == pathways.end()) pathways.push_back(id);
    }
    o.pathwayIds = pathways;
    linkLeaderByName(app.db, o);
    const std::string now = nowTimestamp();
    o.updatedAt = now;

    // A new name goes to every affiliation that named it by the old one, or that the new name wouldn't
    // match any more. One that still names it (another name it is known as) stays as typed.
    int renamed = 0;
    bool charactersChanged = false, eventsChanged = false;
    const Organization* saved = isNew ? nullptr : app.db.findOrganization(o.id);
    if (saved && saved->name != o.name && !o.name.empty()) {
        auto rename = [&](std::string& text) {
            if (organizationNamed(app.db, text) != saved) return false;
            if (!sameName(trimmed(text), saved->name) && namesOrganization(o, text)) return false;
            text = o.name;
            return true;
        };
        for (Character& c : app.db.characters) {
            bool changed = rename(c.affiliation.organization);
            for (CharacterStage& stage : c.stages) changed = rename(stage.affiliation.organization) || changed;
            changed = rename(c.dossier.filedBy) || changed;
            if (!changed) continue;
            c.updatedAt = now;
            ++renamed;
            charactersChanged = true;
        }
        for (StoryEvent& e : app.db.events) {
            bool changed = false;
            for (EventPerson& p : e.people) {
                if (p.changed) changed = rename(p.change.affiliation.organization) || changed;
            }
            if (!changed) continue;
            e.updatedAt = now;
            eventsChanged = true;
        }
    }

    if (isNew) {
        if (o.id <= 0 || app.db.findOrganization(o.id)) o.id = app.db.nextOrganizationId();
        o.createdAt = now;
        app.db.organizations.push_back(o);
    } else if (Organization* existing = app.db.findOrganization(o.id)) {
        *existing = o;
    }
    try {
        app.storage.saveOrganizations(app.db);
        if (charactersChanged) app.storage.saveCharacters(app.db);
        if (eventsChanged) app.storage.saveEvents(app.db);
    } catch (const StorageError&) {
        app.db.organizations = organizationsBackup;
        app.db.characters = charactersBackup;
        app.db.events = eventsBackup;
        throw;
    }
    return renamed;
}

void deleteOrganization(App& app, int id) {
    const auto backup = app.db.organizations;
    auto& list = app.db.organizations;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const Organization& o) { return o.id == id; }), list.end());
    try {
        app.storage.saveOrganizations(app.db);
    } catch (const StorageError&) {
        app.db.organizations = backup;
        throw;
    }
}

void joinOrganization(App& app, int characterId, const Organization& o, const std::string& rank) {
    const Character* saved = app.db.findCharacter(characterId);
    if (!saved) throw StorageError("That character no longer exists.");
    Character c = *saved;
    c.affiliation.organization = o.name;
    c.affiliation.rank = trimmed(rank);
    saveCharacter(app, c, false);
}

void leaveOrganization(App& app, int characterId) {
    const Character* saved = app.db.findCharacter(characterId);
    if (!saved) throw StorageError("That character no longer exists.");
    Character c = *saved;
    c.affiliation = Affiliation{};
    saveCharacter(app, c, false);
}

// The same record apart from when it was created and edited.
template <typename Record>
bool sameDetails(Record a, Record b) {
    a.createdAt = a.updatedAt = b.createdAt = b.updatedAt = "";
    return nlohmann::json(a) == nlohmann::json(b);
}

namespace {

// Imports a sample set's characters and Sealed Artifacts (records), its story events (events), or
// both. Events alone still find the set's characters and Sealed Artifacts, saved already, by name.
SampleImportReport importParts(App& app, const SampleSet& set, bool records, bool events) {
    SampleImportReport report;
    const auto charactersBackup = app.db.characters;
    const auto artifactsBackup = app.db.artifacts;
    const auto eventsBackup = app.db.events;
    const auto organizationsBackup = app.db.organizations;
    const std::string now = nowTimestamp();
    // A set's own edit time is kept, so a newer version shared later can be told apart. A record
    // with no edit time (written by hand) never counts as newer than a saved one.
    auto editedAt = [&](const std::string& stamp) { return stamp.empty() ? now : stamp; };
    auto newer = [](const std::string& stamp, const std::string& saved) { return stamp >= saved; };
    bool artifactsChanged = false;
    bool charactersChanged = false;

    // The sample's own ids, mapped to the ids they get here.
    std::map<int, int> artifactIds;
    for (Artifact a : set.artifacts) {
        const int sampleId = a.id;
        const auto saved = std::find_if(app.db.artifacts.begin(), app.db.artifacts.end(),
                                        [&](const Artifact& x) { return sameName(x.name, a.name); });
        if (saved != app.db.artifacts.end()) {
            artifactIds[sampleId] = saved->id;
            if (!records) continue;
            a.id = saved->id;
            a.createdAt = saved->createdAt;
            const bool isNewer = newer(a.updatedAt, saved->updatedAt);
            a.updatedAt = editedAt(a.updatedAt);
            if (isNewer && !sameDetails(a, *saved)) {
                *saved = a;
                artifactsChanged = true;
                report.updated.push_back(a.name);
            } else {
                report.skipped.push_back(a.name);
            }
            continue;
        }
        if (!records) continue;
        a.id = app.db.nextArtifactId();
        a.createdAt = a.updatedAt = editedAt(a.updatedAt);
        artifactIds[sampleId] = a.id;
        app.db.artifacts.push_back(a);
        artifactsChanged = true;
        report.addedArtifacts.push_back(a.name);
    }

    std::map<int, int> characterIds;
    std::vector<bool> alreadySaved;  // whether each sample character matches a saved one
    int nextId = app.db.nextCharacterId();
    for (const Character& c : set.characters) {
        const auto saved = std::find_if(app.db.characters.begin(), app.db.characters.end(),
                                        [&](const Character& x) { return sameName(x.name, c.name); });
        const bool exists = saved != app.db.characters.end();
        if (exists || records) characterIds[c.id] = exists ? saved->id : nextId++;
        alreadySaved.push_back(exists);
    }

    // Each sample character as it would be saved here: ids mapped, links found by name.
    std::vector<Character> incoming;
    std::vector<bool> isNewer;
    for (size_t i = 0; records && i < set.characters.size(); ++i) {
        Character c = set.characters[i];
        c.id = characterIds[c.id];
        auto mapArtifacts = [&](std::vector<int>& ids) {
            std::vector<int> mapped;
            for (int id : ids) {
                if (auto it = artifactIds.find(id); it != artifactIds.end()) mapped.push_back(it->second);
            }
            ids = mapped;
        };
        mapArtifacts(c.artifactIds);
        for (CharacterStage& stage : c.stages) mapArtifacts(stage.artifactIds);
        for (Relationship& r : c.relationships) {
            const auto it = characterIds.find(r.characterId);
            r.characterId = it != characterIds.end() ? it->second : 0;
        }
        linkRelationshipsByName(app.db, c);
        normalizeCharacter(c);
        if (const Character* saved = alreadySaved[i] ? app.db.findCharacter(c.id) : nullptr) {
            // What the saved version has outside this set (people and Sealed Artifacts it doesn't
            // know about) stays.
            auto inSet = [&](int id) {
                return std::any_of(characterIds.begin(), characterIds.end(), [&](const auto& m) { return m.second == id; });
            };
            for (const Relationship& r : saved->relationships) {
                if (r.characterId == 0 || inSet(r.characterId)) continue;
                const bool has = std::any_of(c.relationships.begin(), c.relationships.end(), [&](const Relationship& x) {
                    return x.characterId == r.characterId && sameName(x.type, r.type);
                });
                if (!has) c.relationships.push_back(r);
            }
            for (int id : saved->artifactIds) {
                const bool fromSet = std::any_of(artifactIds.begin(), artifactIds.end(), [&](const auto& m) { return m.second == id; });
                if (!fromSet && std::find(c.artifactIds.begin(), c.artifactIds.end(), id) == c.artifactIds.end()) {
                    c.artifactIds.push_back(id);
                }
            }
            c.createdAt = saved->createdAt;
            isNewer.push_back(newer(c.updatedAt, saved->updatedAt));
            c.updatedAt = editedAt(c.updatedAt);
        } else {
            isNewer.push_back(true);
            c.createdAt = c.updatedAt = editedAt(c.updatedAt);
        }
        incoming.push_back(std::move(c));
    }

    std::vector<std::pair<int, std::optional<Character>>> changed;  // id, and the version it replaced
    for (size_t i = 0; i < incoming.size(); ++i) {
        Character& c = incoming[i];
        if (!alreadySaved[i]) {
            app.db.characters.push_back(c);
            report.addedCharacters.push_back(c.name);
            changed.emplace_back(c.id, std::nullopt);
            continue;
        }
        Character* saved = app.db.findCharacter(c.id);
        if (isNewer[i] && !sameDetails(c, *saved)) {
            changed.emplace_back(c.id, *saved);
            *saved = c;
            report.updated.push_back(c.name);
        } else {
            report.skipped.push_back(c.name);
        }
    }
    // Links go both ways, like a normal save, and links the new version dropped go on both sides.
    for (const auto& [id, before] : changed) {
        Character* stored = app.db.findCharacter(id);
        linkRelationshipsByName(app.db, *stored);
        syncRelationships(app.db, *stored, before ? &*before : nullptr);
        charactersChanged = true;
    }

    // Organizations: the same name is the same organization. A leader is linked to the set's characters
    // as they are saved here, or by name.
    bool organizationsChanged = false;
    for (Organization o : records ? set.organizations : std::vector<Organization>{}) {
        const auto leader = characterIds.find(o.leaderId);
        o.leaderId = o.leaderId != 0 && leader != characterIds.end() ? leader->second : 0;
        linkLeaderByName(app.db, o);
        const auto saved = std::find_if(app.db.organizations.begin(), app.db.organizations.end(),
                                        [&](const Organization& x) { return sameName(x.name, o.name); });
        if (saved != app.db.organizations.end()) {
            o.id = saved->id;
            o.createdAt = saved->createdAt;
            const bool isNewer = newer(o.updatedAt, saved->updatedAt);
            o.updatedAt = editedAt(o.updatedAt);
            if (isNewer && !sameDetails(o, *saved)) {
                *saved = o;
                organizationsChanged = true;
                report.updated.push_back(o.name);
            } else {
                report.skipped.push_back(o.name);
            }
            continue;
        }
        o.id = app.db.nextOrganizationId();
        o.createdAt = o.updatedAt = editedAt(o.updatedAt);
        app.db.organizations.push_back(o);
        organizationsChanged = true;
        report.addedOrganizations.push_back(o.name);
    }

    // Story events: the same title and date is the same event. People are linked to the set's
    // characters as they are saved here, or by name.
    bool eventsChanged = false;
    for (StoryEvent e : events ? set.events : std::vector<StoryEvent>{}) {
        auto mapArtifacts = [&](std::vector<int>& ids) {
            std::vector<int> mapped;
            for (int id : ids) {
                if (auto it = artifactIds.find(id); it != artifactIds.end()) mapped.push_back(it->second);
            }
            ids = mapped;
        };
        for (EventPerson& p : e.people) {
            const auto it = characterIds.find(p.characterId);
            p.characterId = it != characterIds.end() ? it->second : 0;
            mapArtifacts(p.change.artifactIds);
        }
        mapArtifacts(e.artifactIds);
        linkEventPeopleByName(app.db, e);
        tidyEventChanges(e);
        const auto saved = std::find_if(app.db.events.begin(), app.db.events.end(), [&](const StoryEvent& x) {
            return sameName(x.title, e.title) && sameStoryDate(x.date, e.date);
        });
        if (saved != app.db.events.end()) {
            e.id = saved->id;
            e.createdAt = saved->createdAt;
            const bool eventIsNewer = newer(e.updatedAt, saved->updatedAt);
            e.updatedAt = editedAt(e.updatedAt);
            if (eventIsNewer && !sameDetails(e, *saved)) {
                *saved = e;
                eventsChanged = true;
                report.updated.push_back(e.title);
            } else {
                report.skipped.push_back(e.title);
            }
            continue;
        }
        e.id = app.db.nextEventId();
        e.createdAt = e.updatedAt = editedAt(e.updatedAt);
        app.db.events.push_back(e);
        eventsChanged = true;
        report.addedEvents.push_back(e.title);
    }

    try {
        if (artifactsChanged) app.storage.saveArtifacts(app.db);
        if (charactersChanged) app.storage.saveCharacters(app.db);
        if (eventsChanged) app.storage.saveEvents(app.db);
        if (organizationsChanged) app.storage.saveOrganizations(app.db);
    } catch (const StorageError&) {
        app.db.characters = charactersBackup;
        app.db.artifacts = artifactsBackup;
        app.db.events = eventsBackup;
        app.db.organizations = organizationsBackup;
        throw;
    }
    return report;
}

}  // namespace

SampleImportReport importSampleSet(App& app, const SampleSet& set) { return importParts(app, set, true, true); }

SampleImportReport importAllSampleSets(App& app, const std::vector<SampleSet>& sets) {
    SampleImportReport total;
    auto add = [&](SampleImportReport report) {
        for (auto& name : report.addedCharacters) total.addedCharacters.push_back(std::move(name));
        for (auto& name : report.addedArtifacts) total.addedArtifacts.push_back(std::move(name));
        for (auto& name : report.addedEvents) total.addedEvents.push_back(std::move(name));
        for (auto& name : report.addedOrganizations) total.addedOrganizations.push_back(std::move(name));
        for (auto& name : report.updated) total.updated.push_back(std::move(name));
        for (auto& name : report.skipped) total.skipped.push_back(std::move(name));
    };
    // Everyone first, then the events, so an event finds the people at it whichever set they're in.
    for (const SampleSet& set : sets) add(importParts(app, set, true, false));
    for (const SampleSet& set : sets) add(importParts(app, set, false, true));
    return total;
}

SampleSet makeSampleSet(const Database& db, const std::string& title, const std::string& description,
                        const SharePicks& picks) {
    SampleSet set;
    set.title = title;
    set.description = description;
    auto picked = [](const std::vector<int>& ids, int id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    const std::vector<int>& characterIds = picks.characterIds;
    const std::vector<int>& eventIds = picks.eventIds;
    std::vector<int> artifacts = picks.artifactIds;
    std::vector<int> organizations = picks.organizationIds;
    auto belongsTo = [&](const std::string& affiliation) {
        if (const Organization* o = organizationNamed(db, affiliation); o && !picked(organizations, o->id)) {
            organizations.push_back(o->id);
        }
    };
    for (const auto& c : db.characters) {
        if (!picked(characterIds, c.id)) continue;
        Character copy = c;
        for (Relationship& r : copy.relationships) {
            if (r.characterId != 0 && !picked(characterIds, r.characterId)) {
                if (const Character* other = db.findCharacter(r.characterId)) r.name = other->name;
                r.characterId = 0;
            }
        }
        for (int id : copy.artifactIds) {
            if (!picked(artifacts, id)) artifacts.push_back(id);
        }
        for (const CharacterStage& stage : copy.stages) {
            for (int id : stage.artifactIds) {
                if (!picked(artifacts, id)) artifacts.push_back(id);
            }
            belongsTo(stage.affiliation.organization);
        }
        belongsTo(copy.affiliation.organization);
        set.characters.push_back(std::move(copy));
    }
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (!picked(eventIds, e->id)) continue;
        StoryEvent copy = *e;
        for (EventPerson& p : copy.people) {
            // Someone left out keeps their name, and what changed for them, for when they are
            // imported from another set.
            if (p.characterId != 0 && !picked(characterIds, p.characterId)) {
                p.name = eventPersonName(db, p);
                p.characterId = 0;
            }
            for (int id : p.change.artifactIds) {
                if (!picked(artifacts, id)) artifacts.push_back(id);
            }
        }
        for (int id : copy.artifactIds) {
            if (!picked(artifacts, id)) artifacts.push_back(id);
        }
        set.events.push_back(std::move(copy));
    }
    for (const auto& a : db.artifacts) {
        if (picked(artifacts, a.id)) set.artifacts.push_back(a);
    }
    for (const auto& o : db.organizations) {
        if (!picked(organizations, o.id)) continue;
        Organization copy = o;
        if (copy.leaderId != 0 && !picked(characterIds, copy.leaderId)) {  // left out: by name only
            copy.leader = leaderName(o, db);
            copy.leaderId = 0;
        }
        set.organizations.push_back(std::move(copy));
    }
    return set;
}

std::vector<int> eventsWithCharacters(const Database& db, const std::vector<int>& characterIds) {
    std::vector<int> ids;
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (std::any_of(e->people.begin(), e->people.end(), [&](const EventPerson& p) {
                return p.characterId != 0 &&
                       std::find(characterIds.begin(), characterIds.end(), p.characterId) != characterIds.end();
            })) {
            ids.push_back(e->id);
        }
    }
    return ids;
}

std::string sampleSetFileName(const std::string& title) {
    std::string name;
    for (unsigned char ch : title) {
        if (ch == '\'') continue;  // "Max's" -> "maxs"
        if (std::isalnum(ch) && ch < 128) name += static_cast<char>(std::tolower(ch));
        else if (!name.empty() && name.back() != '_') name += '_';
    }
    while (!name.empty() && name.back() == '_') name.pop_back();
    return (name.empty() ? "my_sample_set" : name) + ".json";
}

std::vector<std::string> namesInSampleSets(const std::vector<SampleSet>& sets, const std::string& exceptFile) {
    std::vector<std::string> names;
    auto lower = [](std::string text) {
        for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return text;
    };
    for (const SampleSet& set : sets) {
        if (set.file.filename().string() == exceptFile) continue;
        for (const auto& c : set.characters) names.push_back(lower(c.name));
        for (const auto& a : set.artifacts) names.push_back(lower(a.name));
        for (const auto& o : set.organizations) names.push_back(lower(o.name));
    }
    return names;
}

bool eventInSampleSets(const std::vector<SampleSet>& sets, const std::string& exceptFile, const StoryEvent& e) {
    return std::any_of(sets.begin(), sets.end(), [&](const SampleSet& set) {
        return set.file.filename().string() != exceptFile &&
               std::any_of(set.events.begin(), set.events.end(), [&](const StoryEvent& x) {
                   return sameName(x.title, e.title) && sameStoryDate(x.date, e.date);
               });
    });
}

std::string sampleSetFileFor(const std::vector<SampleSet>& sets, const std::string& title) {
    for (const SampleSet& set : sets) {
        if (!set.file.empty() && sameName(set.title, title)) return set.file.filename().string();
    }
    return sampleSetFileName(title);
}

SharePicks defaultSharePicks(const Database& db, const std::vector<SampleSet>& sets, const std::string& fileName) {
    SharePicks picks;
    const auto existing = std::find_if(sets.begin(), sets.end(),
                                       [&](const SampleSet& set) { return set.file.filename().string() == fileName; });
    if (existing != sets.end()) {
        for (const auto& c : db.characters) {
            if (std::any_of(existing->characters.begin(), existing->characters.end(),
                            [&](const Character& x) { return sameName(x.name, c.name); })) {
                picks.characterIds.push_back(c.id);
            }
        }
        for (const auto& a : db.artifacts) {
            if (std::any_of(existing->artifacts.begin(), existing->artifacts.end(),
                            [&](const Artifact& x) { return sameName(x.name, a.name); })) {
                picks.artifactIds.push_back(a.id);
            }
        }
        for (const auto& o : db.organizations) {
            if (std::any_of(existing->organizations.begin(), existing->organizations.end(),
                            [&](const Organization& x) { return sameName(x.name, o.name); })) {
                picks.organizationIds.push_back(o.id);
            }
        }
        // Its events, and the ones written since with its characters in them, so they aren't left behind.
        const std::vector<int> withThem = eventsWithCharacters(db, picks.characterIds);
        for (const StoryEvent* e : eventsInOrder(db)) {
            const bool inThisSet = std::any_of(existing->events.begin(), existing->events.end(), [&](const StoryEvent& x) {
                return sameName(x.title, e->title) && sameStoryDate(x.date, e->date);
            });
            const bool newWithThem = std::find(withThem.begin(), withThem.end(), e->id) != withThem.end() &&
                                     !eventInSampleSets(sets, fileName, *e);
            if (inThisSet || newWithThem) picks.eventIds.push_back(e->id);
        }
        return picks;
    }
    const auto taken = namesInSampleSets(sets, fileName);
    auto isTaken = [&](const std::string& name) {
        return std::any_of(taken.begin(), taken.end(), [&](const std::string& t) { return sameName(t, name); });
    };
    for (const auto& c : db.characters) {
        if (!isTaken(c.name)) picks.characterIds.push_back(c.id);
    }
    for (const auto& a : db.artifacts) {
        if (!isTaken(a.name)) picks.artifactIds.push_back(a.id);
    }
    for (const auto& o : db.organizations) {
        if (!isTaken(o.name)) picks.organizationIds.push_back(o.id);
    }
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (!eventInSampleSets(sets, fileName, *e)) picks.eventIds.push_back(e->id);
    }
    return picks;
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

void saveCustomSefirah(App& app, Sefirah& s, bool isNew) {
    const auto backup = app.db.sefirot;
    s.custom = true;
    if (isNew) {
        s.id = app.db.newSefirahId(s.name);
        app.db.sefirot.push_back(s);
    } else {
        for (auto& stored : app.db.sefirot) {
            if (stored.id == s.id) stored = s;
        }
    }
    try {
        app.storage.saveCustomSefirot(app.db);
    } catch (const StorageError&) {
        app.db.sefirot = backup;
        throw;
    }
}

void deleteCustomSefirah(App& app, const std::string& id) {
    const Sefirah* s = app.db.findSefirah(id);
    if (!s || !s->custom) throw StorageError("Only your own Sefirot can be deleted.");
    const auto sefirotBackup = app.db.sefirot;
    const auto charactersBackup = app.db.characters;
    bool held = false;
    for (auto& c : app.db.characters) {
        if (c.sefirahId == id) {
            c.sefirahId.clear();
            held = true;
        }
    }
    auto& list = app.db.sefirot;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Sefirah& x) { return x.id == id; }), list.end());
    try {
        app.storage.saveCustomSefirot(app.db);
        if (held) app.storage.saveCharacters(app.db);
    } catch (const StorageError&) {
        app.db.sefirot = sefirotBackup;
        app.db.characters = charactersBackup;
        throw;
    }
}

}  // namespace lotm
