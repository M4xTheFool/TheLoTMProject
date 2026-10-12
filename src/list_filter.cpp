#include "list_filter.hpp"

#include <algorithm>
#include <cctype>

#include "rules.hpp"

namespace lotm {

namespace {

std::string lower(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

// "C-012", "c12", "C 12" or "12" all mean id 12 when the prefix is right (or missing).
bool searchesForId(const std::string& search, char prefix, int id) {
    std::string digits;
    bool sawPrefix = false;
    for (char ch : search) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isdigit(c)) {
            digits += ch;
        } else if (digits.empty() && !sawPrefix && std::tolower(c) == std::tolower(static_cast<unsigned char>(prefix))) {
            sawPrefix = true;
        } else if (digits.empty() && (ch == '-' || ch == ' ' || ch == '#')) {
            continue;
        } else {
            return false;
        }
    }
    return !digits.empty() && digits.size() <= 6 && std::stoi(digits) == id;
}

bool commonMatch(const RecordFilter& f, const std::string& name, const std::string& pathwayId, int band) {
    if (f.letter != 0 && firstLetter(name) != f.letter) return false;
    if (f.power != kAnyPower && band != f.power) return false;
    if (!f.pathwayId.empty() && pathwayId != f.pathwayId) return false;
    return true;
}

template <typename Record>
void sortRecords(const Database& db, SortOrder sort, std::vector<const Record*>& rows) {
    std::stable_sort(rows.begin(), rows.end(), [&](const Record* a, const Record* b) {
        if (sort == SortOrder::Id) return a->id < b->id;
        if (sort == SortOrder::Power) {
            // Strongest first; an unknown level goes last.
            const int pa = powerBand(*a) == kUnknownPower ? -1 : powerBand(*a);
            const int pb = powerBand(*b) == kUnknownPower ? -1 : powerBand(*b);
            if (pa != pb) return pa > pb;
        }
        if (sort == SortOrder::Pathway && a->pathwayId != b->pathwayId) {
            if (a->pathwayId.empty() != b->pathwayId.empty()) return b->pathwayId.empty();
            return lower(pathwayLabel(db, a->pathwayId)) < lower(pathwayLabel(db, b->pathwayId));
        }
        return lower(a->name) < lower(b->name);
    });
}

}  // namespace

std::string powerBandName(int band) {
    static const char* kNames[] = {"Mortal",       "Sequence 9-8", "Sequence 7-5",       "Sequence 4-3",
                                   "Sequence 2-1", "Sequence 0",   "Above the Sequence", "Level unknown"};
    if (band < 0 || band > kUnknownPower) return "Any power";
    return kNames[band];
}

int powerBand(const Character& c) { return speedTierOf(c).tier; }

int powerBand(const Artifact& a) {
    if (a.sequenceLevel == kUnknownSequence) return kUnknownPower;
    return tierForSequence(a.sequenceLevel);
}

std::vector<int> characterPowerBands() { return {0, 1, 2, 3, 4, 5, 6}; }
std::vector<int> artifactPowerBands() { return {1, 2, 3, 4, 5, kUnknownPower}; }

std::string sortOrderName(SortOrder order) {
    switch (order) {
        case SortOrder::Id: return "ID";
        case SortOrder::Power: return "power (strongest first)";
        case SortOrder::Pathway: return "pathway";
        default: return "name";
    }
}

bool RecordFilter::active() const { return !search.empty() || letter != 0 || power != kAnyPower || !pathwayId.empty(); }

void RecordFilter::clear() {
    search.clear();
    letter = 0;
    power = kAnyPower;
    pathwayId.clear();
}

std::string RecordFilter::describe(const Database& db) const {
    std::string text = "Sorted by " + sortOrderName(sort);
    if (!search.empty()) text += ", matching \"" + search + "\"";
    if (letter != 0) text += ", first letter " + std::string(1, letter);
    if (power != kAnyPower) text += ", " + powerBandName(power);
    if (!pathwayId.empty()) text += ", pathway " + pathwayLabel(db, pathwayId);
    return text;
}

char firstLetter(const std::string& name) {
    for (char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isspace(c)) continue;
        return std::isalpha(c) && c < 128 ? static_cast<char>(std::toupper(c)) : 0;
    }
    return 0;
}

std::vector<char> firstLetters(const std::vector<std::string>& names) {
    std::vector<char> letters;
    for (const auto& name : names) {
        const char letter = firstLetter(name);
        if (letter != 0 && std::find(letters.begin(), letters.end(), letter) == letters.end()) letters.push_back(letter);
    }
    std::sort(letters.begin(), letters.end());
    return letters;
}

bool matches(const RecordFilter& f, const Character& c) {
    if (!commonMatch(f, c.name, c.pathwayId, powerBand(c))) return false;
    if (f.search.empty() || contains(c.name, f.search) || searchesForId(f.search, 'C', c.id)) return true;
    return std::any_of(c.aliases.begin(), c.aliases.end(), [&](const std::string& a) { return contains(a, f.search); });
}

bool matches(const RecordFilter& f, const Artifact& a) {
    if (!commonMatch(f, a.name, a.pathwayId, powerBand(a))) return false;
    return f.search.empty() || contains(a.name, f.search) || searchesForId(f.search, 'A', a.id);
}

std::vector<const Character*> filterCharacters(const Database& db, const RecordFilter& filter) {
    std::vector<const Character*> rows;
    for (const auto& c : db.characters) {
        if (matches(filter, c)) rows.push_back(&c);
    }
    sortRecords(db, filter.sort, rows);
    return rows;
}

std::vector<const Artifact*> filterArtifacts(const Database& db, const RecordFilter& filter) {
    std::vector<const Artifact*> rows;
    for (const auto& a : db.artifacts) {
        if (matches(filter, a)) rows.push_back(&a);
    }
    sortRecords(db, filter.sort, rows);
    return rows;
}

}  // namespace lotm
