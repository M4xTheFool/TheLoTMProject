// Loading and saving the JSON files in the data folder, with automatic backups.
#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "model.hpp"

namespace lotm {

// Thrown with a message written for the person using the program.
class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Finds the folder that holds pathways.json. Looks at LOTM_DATA_DIR, then upward from the
// current folder and from the program's own folder, then the project folder it was built from.
std::filesystem::path findDataDir(const char* argv0);

std::string nowTimestamp();  // "2026-10-09 14:05:33", local time

class Storage {
public:
    explicit Storage(std::filesystem::path dataDir);

    const std::filesystem::path& dataDir() const { return dataDir_; }
    std::filesystem::path exportsDir() const;  // sits next to the data folder
    std::filesystem::path backupsDir() const;
    std::filesystem::path customPathwaysFile() const;  // data/custom_pathways.json

    void loadAll(Database& db) const;
    void saveCharacters(const Database& db) const;
    void saveArtifacts(const Database& db) const;
    void saveSettings(const Database& db) const;
    // Writes the pathways marked custom to custom_pathways.json. pathways.json is never written.
    void saveCustomPathways(const Database& db) const;

private:
    std::filesystem::path dataDir_;
};

}  // namespace lotm
