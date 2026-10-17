// Saving, copying and deleting characters, Sealed Artifacts, story events, organizations and your own
// pathways and Sefirot.
// Shared by the console screens and the app window, so both follow exactly the same rules.
// Every function writes the file straight away. If writing fails it throws StorageError and
// leaves the database in memory as it was.
#pragma once

#include <string>
#include <vector>

#include "app.hpp"

namespace lotm {

struct CharacterSaveReport {
    std::vector<std::string> cleanups;  // fields removed because they no longer apply (see normalizeCharacter)
    std::vector<std::string> linked;    // relationships, events and organizations that found them, one line each
};

// Cleans up `c`, keeps relationships two-way, stamps the time and saves. `c` is updated in place.
// Names typed by hand elsewhere find them too: people at story events and an organization's leader
// with their name are linked to them, and an organization that lists them by name among the members
// who aren't saved characters makes them a member (when they have no affiliation yet).
CharacterSaveReport saveCharacter(App& app, Character& c, bool isNew);
// Saves a copy named "... (copy)" and returns its id.
int duplicateCharacter(App& app, int id);
// Other characters keep their relationship to it, events the people in them and organizations their
// leader, by name only.
void deleteCharacter(App& app, int id);

void saveArtifact(App& app, Artifact& a, bool isNew);
// Also takes it away from every character who held it (now and in their stages) and every event.
void deleteArtifact(App& app, int id);

// Saves a story event (data/events.json). People typed by name who match one saved character
// exactly (ignoring case) are linked to them.
void saveEvent(App& app, StoryEvent& e, bool isNew);
void deleteEvent(App& app, int id);
// Links people in an event to saved characters by name, and unlinks ones whose character is gone.
void linkEventPeopleByName(const Database& db, StoryEvent& e);
// Drops what changed for people who aren't saved characters, and makes each change make sense (see
// normalizeStage), dated on the event.
void tidyEventChanges(StoryEvent& e);

// Saves an organization (data/organizations.json), linking a leader typed by name to the saved character.
// Renaming one renames it for its members too: every affiliation that named it by its old name (now, in
// a stage, at a story event, or as who filed a dossier) gets the new one. Relationships that name it by
// hand are linked to it. Returns how many characters' affiliations were renamed.
int saveOrganization(App& app, Organization& o, bool isNew);
// Its members keep it as their affiliation, and relationships with it keep its name, as typed text.
void deleteOrganization(App& app, int id);
// Makes a saved character a member: their affiliation becomes the organization's name, with this rank.
void joinOrganization(App& app, int characterId, const Organization& o, const std::string& rank);
// Takes a saved character out of the organization they belong to: their affiliation is cleared.
void leaveOrganization(App& app, int characterId);

struct SampleImportReport {
    std::vector<std::string> addedCharacters;
    std::vector<std::string> addedArtifacts;
    std::vector<std::string> addedEvents;
    std::vector<std::string> addedOrganizations;
    std::vector<std::string> updated;  // saved already, and the sample set had a newer version
    std::vector<std::string> skipped;  // saved already, and the same or edited more recently here
};

// Adds a sample set's characters, Sealed Artifacts, organizations and story events, keeping who holds
// what, who knows whom, who leads what and who was where. A record with the same name as a saved one
// (ignoring case; for an event the same title and date) is the same record: when the set's
// version was edited at the same time or later ("last edited") and differs, it replaces the saved
// one, which keeps its id. A saved record edited here more recently is left as it is. Updated
// characters keep their links to characters and Sealed Artifacts the set doesn't have. Importing
// the same set twice changes nothing.
SampleImportReport importSampleSet(App& app, const SampleSet& set);
// Imports every sample set and adds up what happened: first every set's characters and Sealed
// Artifacts, then every set's story events, so the people at an event are found whichever set they
// are in. When two sets have the same record, the most recently edited version ends up saved.
SampleImportReport importAllSampleSets(App& app, const std::vector<SampleSet>& sets);

// What goes in a sample set: ids of saved records.
struct SharePicks {
    std::vector<int> characterIds;
    std::vector<int> artifactIds;
    std::vector<int> eventIds;
    std::vector<int> organizationIds;
};
// Packs saved characters, Sealed Artifacts, story events and organizations into a sample set, to share
// them through data/samples. The Sealed Artifacts the characters hold (now, in a stage or at an event)
// and the ones the events name go in too, and so do the organizations the characters belong to (now
// or in a stage). A relationship to someone left out, someone left out who was at an event (with what
// changed for them there), or a leader left out, keeps their name but no link, so it links up again
// when they are imported too, from another set.
SampleSet makeSampleSet(const Database& db, const std::string& title, const std::string& description,
                        const SharePicks& picks);
// The story events with any of these characters in them, in story order.
std::vector<int> eventsWithCharacters(const Database& db, const std::vector<int>& characterIds);
// The file a sample set with this title is saved as: "Max's Creations" -> "maxs_creations.json".
std::string sampleSetFileName(const std::string& title);
// The names of the characters, Sealed Artifacts and organizations in these sample sets, in lower case,
// except the one saved in exceptFile (the set being replaced).
std::vector<std::string> namesInSampleSets(const std::vector<SampleSet>& sets, const std::string& exceptFile);
// Whether a sample set other than exceptFile has this story event (the same title and date).
bool eventInSampleSets(const std::vector<SampleSet>& sets, const std::string& exceptFile, const StoryEvent& e);
// The file to save a sample set with this title in: the file of a set that already has this title
// (any capitals), so saving "The Tarot Club" again replaces tarot_club.json, or else sampleSetFileName.
std::string sampleSetFileFor(const std::vector<SampleSet>& sets, const std::string& title);
// What sharing ticks before you change anything. Replacing the set saved in fileName ticks the
// records that set already has, and the events since with its characters in them that no other set
// has; a new set ticks everything not already in another sample set.
SharePicks defaultSharePicks(const Database& db, const std::vector<SampleSet>& sets, const std::string& fileName);

// Saves one of your own pathways. A new one gets an id made from its name.
void saveCustomPathway(App& app, Pathway& p, bool isNew);
// Only your own pathways that no character or Sealed Artifact belongs to (check pathwayUsers first).
void deleteCustomPathway(App& app, const std::string& id);

// Saves one of your own Sefirot. A new one gets an id made from its name.
void saveCustomSefirah(App& app, Sefirah& s, bool isNew);
// Only your own Sefirot. Characters who held it no longer hold any Sefirah.
void deleteCustomSefirah(App& app, const std::string& id);

}  // namespace lotm
