// Saving, copying and deleting characters, Sealed Artifacts and your own pathways.
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
    std::vector<std::string> linked;    // relationships added or removed on other characters
};

// Cleans up `c`, keeps relationships two-way, stamps the time and saves. `c` is updated in place.
CharacterSaveReport saveCharacter(App& app, Character& c, bool isNew);
// Saves a copy named "... (copy)" and returns its id.
int duplicateCharacter(App& app, int id);
// Other characters keep their relationship to it, by name only.
void deleteCharacter(App& app, int id);

void saveArtifact(App& app, Artifact& a, bool isNew);
// Also takes it away from every character who held it.
void deleteArtifact(App& app, int id);

struct SampleImportReport {
    std::vector<std::string> addedCharacters;
    std::vector<std::string> addedArtifacts;
    std::vector<std::string> skipped;  // already saved under the same name
};

// Adds a sample set's characters and Sealed Artifacts as new records, keeping who holds what and
// who knows whom. Anything with the same name as a saved record (ignoring case) is skipped and keeps
// its details; the new characters link to it instead, so importing the same set twice adds nothing.
SampleImportReport importSampleSet(App& app, const SampleSet& set);
// Imports every sample set, one after another, and adds up what happened.
SampleImportReport importAllSampleSets(App& app, const std::vector<SampleSet>& sets);

// Packs saved characters and Sealed Artifacts into a sample set, to share them through
// data/samples. The Sealed Artifacts the characters hold go in too. A relationship to someone left
// out keeps their name but no link, so it links up again if they are imported later.
SampleSet makeSampleSet(const Database& db, const std::string& title, const std::string& description,
                        const std::vector<int>& characterIds, const std::vector<int>& artifactIds);
// The file a sample set with this title is saved as: "Max's Creations" -> "maxs_creations.json".
std::string sampleSetFileName(const std::string& title);
// The names of the characters and Sealed Artifacts in these sample sets, in lower case, except the
// one saved in exceptFile (the set being replaced).
std::vector<std::string> namesInSampleSets(const std::vector<SampleSet>& sets, const std::string& exceptFile);

// Saves one of your own pathways. A new one gets an id made from its name.
void saveCustomPathway(App& app, Pathway& p, bool isNew);
// Only your own pathways that no character or Sealed Artifact belongs to (check pathwayUsers first).
void deleteCustomPathway(App& app, const std::string& id);

// Saves one of your own Sefirot. A new one gets an id made from its name.
void saveCustomSefirah(App& app, Sefirah& s, bool isNew);
// Only your own Sefirot. Characters who held it no longer hold any Sefirah.
void deleteCustomSefirah(App& app, const std::string& id);

}  // namespace lotm
