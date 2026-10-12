// Filtering and sorting the character and Sealed Artifact lists, shared by both programs:
// a search box (name, alias or ID), power, first letter and pathway, and a sort order.
#pragma once

#include <string>
#include <vector>

#include "model.hpp"

namespace lotm {

// Power bands, the same steps as the speed tiers: 0 Mortal, 1 Sequence 9-8, 2 Sequence 7-5,
// 3 Sequence 4-3, 4 Sequence 2-1, 5 Sequence 0, 6 Above the Sequence, and for Sealed Artifacts
// 7 for a level nobody knows.
inline constexpr int kAnyPower = -1;
inline constexpr int kUnknownPower = 7;
std::string powerBandName(int band);  // "Sequence 7-5", "Above the Sequence", "Level unknown"
int powerBand(const Character& c);
int powerBand(const Artifact& a);
// The bands that make sense for each list, weakest first.
std::vector<int> characterPowerBands();
std::vector<int> artifactPowerBands();

enum class SortOrder { Name, Id, Power, Pathway };
inline constexpr SortOrder kSortOrders[] = {SortOrder::Name, SortOrder::Id, SortOrder::Power, SortOrder::Pathway};
std::string sortOrderName(SortOrder order);  // "name", "ID", "power (strongest first)", "pathway"

struct RecordFilter {
    std::string search;      // part of the name or an alias, or an ID: "C-012", "c12" or "12"
    char letter = 0;         // the name's first letter, 'A' to 'Z'; 0 for any
    int power = kAnyPower;   // a power band, or kAnyPower
    std::string pathwayId;   // empty for any pathway
    SortOrder sort = SortOrder::Name;

    bool active() const;     // anything set besides the sort order
    void clear();
    // A filter that keeps the list in the order the records were made in.
    static RecordFilter inIdOrder() {
        RecordFilter f;
        f.sort = SortOrder::Id;
        return f;
    }
    std::string describe(const Database& db) const;  // "Sorted by name, first letter A, Sequence 0"
};

// The name's first letter, upper case, or 0 when it doesn't start with a plain letter.
char firstLetter(const std::string& name);
// The first letters used by these names, A to Z.
std::vector<char> firstLetters(const std::vector<std::string>& names);

bool matches(const RecordFilter& filter, const Character& c);
bool matches(const RecordFilter& filter, const Artifact& a);
std::vector<const Character*> filterCharacters(const Database& db, const RecordFilter& filter);
std::vector<const Artifact*> filterArtifacts(const Database& db, const RecordFilter& filter);

}  // namespace lotm
