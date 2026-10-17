// Two to four characters (or organizations) side by side: the same rows for each, with the highest
// number in a row marked. Built once here and drawn by the console (renderComparisonText) and the app window.
#pragma once

#include <string>
#include <vector>

#include "model.hpp"

namespace lotm {

struct CompareRow {
    std::string label;
    std::vector<std::string> values;  // one per character (or organization)
    // Rows that can be ranked: the number each value is judged on (bigger = higher, so a stronger
    // Sequence counts higher), and who holds the highest one. Nobody is marked when they're all equal.
    bool ranked = false;
    std::vector<int> numbers;
    std::vector<bool> highest;
};

struct CompareSection {
    std::string heading;
    std::vector<CompareRow> rows;
};

struct Comparison {
    std::vector<std::string> names;
    std::vector<CompareSection> sections;  // Overview, Stats, Powers
    std::vector<std::string> between;      // their relationships to each other, one line each
};

Comparison buildComparison(const std::vector<const Character*>& characters, const Database& db);
// Two to four organizations side by side: what they are (Overview) and how strong their members make
// them (Strength). "Between them" lists the relationships between members of different ones.
Comparison buildOrganizationComparison(const std::vector<const Organization*>& organizations, const Database& db);

// A table for the console, with the highest value in each ranked row marked with *.
std::string renderComparisonText(const Comparison& comparison);

}  // namespace lotm
