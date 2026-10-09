#include "storage.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

#include "model_json.hpp"

namespace fs = std::filesystem;
using nlohmann::json;
using nlohmann::ordered_json;

namespace lotm {

namespace {

std::string getEnv(const char* name) {
#ifdef _MSC_VER
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

std::tm localTime(std::time_t t) {
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

bool hasPathways(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "pathways.json", ec);
}

// Checks <start>/data, <start>/../data ... a few levels up.
std::optional<fs::path> searchUpward(fs::path start) {
    std::error_code ec;
    start = fs::absolute(start, ec);
    if (ec) return std::nullopt;
    for (int level = 0; level < 6 && !start.empty(); ++level) {
        if (hasPathways(start / "data")) return start / "data";
        if (!start.has_parent_path() || start.parent_path() == start) break;
        start = start.parent_path();
    }
    return std::nullopt;
}

json readJsonFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw StorageError("Could not open " + file.string());
    try {
        return json::parse(in);
    } catch (const json::exception& e) {
        throw StorageError(file.filename().string() + " could not be read, it is not valid JSON.\n" +
                           "Details: " + e.what() + "\n" +
                           "Fix the file in a text editor, or restore a copy from the backups folder.");
    }
}

// Saves a copy of the current file into backups/, keeping only the newest `keep` copies.
void backUp(const fs::path& file, const fs::path& backupsDir, int keep) {
    std::error_code ec;
    if (keep <= 0 || !fs::exists(file, ec)) return;
    fs::create_directories(backupsDir, ec);

    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const std::tm tm = localTime(std::chrono::system_clock::to_time_t(now));
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    char millis[8];
    std::snprintf(millis, sizeof millis, "%03d", static_cast<int>(ms));

    // A counter keeps names unique (and in order) when two saves land in the same millisecond.
    const std::string stem = file.stem().string();
    for (int n = 0; n < 100; ++n) {
        char counter[8];
        std::snprintf(counter, sizeof counter, "%02d", n);
        const fs::path target = backupsDir / (stem + "-" + stamp + "-" + millis + "-" + counter + ".json");
        if (fs::exists(target, ec)) continue;
        fs::copy_file(file, target, ec);
        break;
    }

    // Timestamps sort by name, so the oldest backups come first.
    std::set<fs::path> backups;
    for (const auto& entry : fs::directory_iterator(backupsDir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind(stem + "-", 0) == 0 && entry.path().extension() == ".json") backups.insert(entry.path());
    }
    while (static_cast<int>(backups.size()) > keep) {
        fs::remove(*backups.begin(), ec);
        backups.erase(backups.begin());
    }
}

// Writes to a temporary file first so a crash never leaves a half-written save.
template <typename Json>
void writeJsonFile(const fs::path& file, const Json& data, const fs::path& backupsDir, int keep) {
    backUp(file, backupsDir, keep);
    const fs::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw StorageError("Could not write " + temp.string());
        out << data.dump(2, ' ', false, Json::error_handler_t::replace) << '\n';
        if (!out) throw StorageError("Could not finish writing " + temp.string());
    }
    std::error_code ec;
    fs::rename(temp, file, ec);
    if (ec) throw StorageError("Could not replace " + file.string() + ": " + ec.message());
}

std::vector<Pathway> readPathways(const fs::path& file) {
    const json data = readJsonFile(file);
    try {
        return data.value("pathways", json::array()).get<std::vector<Pathway>>();
    } catch (const json::exception& e) {
        throw StorageError(file.filename().string() + " has a pathway in the wrong shape.\nDetails: " + e.what());
    }
}

// The same fields as model_json.hpp, in the order people expect when they open the file,
// leaving out optional fields that are empty.
ordered_json pathwayToJson(const Pathway& p) {
    ordered_json j;
    j["id"] = p.id;
    j["name"] = p.name;
    j["god"] = p.god;
    j["group"] = p.group;
    j["primaryStat"] = p.primaryStat;
    j["secondaryStat"] = p.secondaryStat;
    j["speedGrade"] = p.speedGrade;
    j["speedNote"] = p.speedNote;
    if (!p.description.empty()) j["description"] = p.description;
    if (!p.uniqueness.empty()) j["uniqueness"] = p.uniqueness;
    if (!p.movement.empty()) {
        ordered_json movement = ordered_json::array();
        for (const auto& m : p.movement) movement.push_back(ordered_json{{"sequence", m.sequence}, {"mode", m.mode}});
        j["movement"] = movement;
    }
    ordered_json sequences = ordered_json::array();
    for (const auto& s : p.sequences) {
        ordered_json entry;
        entry["sequence"] = s.sequence;
        entry["name"] = s.name;
        entry["abilities"] = s.abilities;
        sequences.push_back(entry);
    }
    j["sequences"] = sequences;
    return j;
}

const std::vector<std::string> kCustomPathwaysAbout = {
    "Your own pathways. The Pathways menu in the program creates, edits and deletes them,",
    "and you can also edit this file by hand. It uses the same layout as pathways.json,",
    "and every id must be different from the ids in both files.",
    "Write each ability as \"Name: what it does\"."};

template <typename T>
std::vector<T> readList(const fs::path& file, const char* key) {
    std::error_code ec;
    if (!fs::exists(file, ec)) return {};
    const json data = readJsonFile(file);
    try {
        if (data.is_array()) return data.get<std::vector<T>>();
        return data.value(key, json::array()).template get<std::vector<T>>();
    } catch (const json::exception& e) {
        throw StorageError(file.filename().string() + " has an entry in the wrong shape.\nDetails: " + e.what());
    }
}

}  // namespace

fs::path findDataDir(const char* argv0) {
    if (const std::string env = getEnv("LOTM_DATA_DIR"); !env.empty() && hasPathways(env)) return env;
    if (auto found = searchUpward(fs::current_path())) return *found;
    if (argv0 && *argv0) {
        if (auto found = searchUpward(fs::path(argv0).parent_path())) return *found;
    }
#ifdef LOTM_SOURCE_DIR
    if (hasPathways(fs::path(LOTM_SOURCE_DIR) / "data")) return fs::path(LOTM_SOURCE_DIR) / "data";
#endif
    throw StorageError(
        "Could not find the data folder (the one containing pathways.json).\n"
        "Run the program from the project folder, or set LOTM_DATA_DIR to the data folder's path.");
}

std::string nowTimestamp() {
    const std::tm tm = localTime(std::time(nullptr));
    char buffer[32];
    std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &tm);
    return buffer;
}

Storage::Storage(fs::path dataDir) : dataDir_(std::move(dataDir)) {}

fs::path Storage::exportsDir() const { return dataDir_.parent_path() / "exports"; }

fs::path Storage::backupsDir() const { return dataDir_ / "backups"; }

fs::path Storage::customPathwaysFile() const { return dataDir_ / "custom_pathways.json"; }

void Storage::loadAll(Database& db) const {
    db.pathways = readPathways(dataDir_ / "pathways.json");
    std::error_code ec;
    if (fs::exists(customPathwaysFile(), ec)) {
        for (auto& p : readPathways(customPathwaysFile())) {
            p.custom = true;
            db.pathways.push_back(std::move(p));
        }
    }
    std::set<std::string> ids;
    for (auto& p : db.pathways) {
        const std::string file = p.custom ? "custom_pathways.json" : "pathways.json";
        if (p.id.empty() || p.name.empty()) throw StorageError(file + ": every pathway needs an id and a name.");
        if (!ids.insert(p.id).second) {
            throw StorageError(file + ": the id \"" + p.id + "\" is already used by another pathway. Give it a new id.");
        }
        std::sort(p.sequences.begin(), p.sequences.end(),
                  [](const SequenceInfo& a, const SequenceInfo& b) { return a.sequence > b.sequence; });
    }

    db.characters = readList<Character>(dataDir_ / "characters.json", "characters");
    db.artifacts = readList<Artifact>(dataDir_ / "artifacts.json", "artifacts");

    if (fs::exists(dataDir_ / "settings.json", ec)) {
        try {
            db.settings = readJsonFile(dataDir_ / "settings.json").get<Settings>();
        } catch (const json::exception&) {
            db.settings = Settings{};  // a broken settings file is not worth stopping for
        }
    }
}

void Storage::saveCharacters(const Database& db) const {
    writeJsonFile(dataDir_ / "characters.json", json{{"version", 1}, {"characters", db.characters}}, backupsDir(),
                  db.settings.backupsToKeep);
}

void Storage::saveArtifacts(const Database& db) const {
    writeJsonFile(dataDir_ / "artifacts.json", json{{"version", 1}, {"artifacts", db.artifacts}}, backupsDir(),
                  db.settings.backupsToKeep);
}

void Storage::saveSettings(const Database& db) const {
    writeJsonFile(dataDir_ / "settings.json", json(db.settings), backupsDir(), 0);
}

void Storage::saveCustomPathways(const Database& db) const {
    // Keeps the notes at the top of the file if someone has rewritten them.
    ordered_json about = kCustomPathwaysAbout;
    std::error_code ec;
    if (fs::exists(customPathwaysFile(), ec)) {
        try {
            const json old = readJsonFile(customPathwaysFile());
            if (old.contains("about")) about = old["about"];
        } catch (const StorageError&) {
        }
    }
    ordered_json pathways = ordered_json::array();
    for (const auto& p : db.pathways) {
        if (p.custom) pathways.push_back(pathwayToJson(p));
    }
    ordered_json file;
    file["about"] = about;
    file["version"] = 1;
    file["pathways"] = pathways;
    writeJsonFile(customPathwaysFile(), file, backupsDir(), db.settings.backupsToKeep);
}

}  // namespace lotm
