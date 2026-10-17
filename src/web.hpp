// The relationship web: every saved character as a point, with a line for each relationship between
// them, arranged so that people who know each other sit close together. Built here once and drawn
// by the app window (Web tab), the console (as lists) and the HTML export (as a picture).
#pragma once

#include <array>
#include <string>
#include <vector>

#include "model.hpp"
#include "sheet.hpp"

namespace lotm {

// What kind of bond a relationship is, from its word: Lover, Close friend, Rival ... Words of your
// own are sorted by what they contain ("Sworn brother" is family, "Old enemy" conflict), else Other.
enum class Bond { Love, Family, Friendship, Allies, Teaching, Conflict, Other };
inline constexpr int kBondCount = 7;
Bond bondOf(const std::string& type);
const char* bondName(Bond bond);  // "Love", "Family", "Friends", "Allies and work", "Teaching", "Conflict", "Other"
std::string bondColour(Bond bond);  // as hex, bright enough for a dark background

struct WebNode {
    int characterId = 0;  // 0: someone who isn't a saved character, named in a relationship
    std::string name;
    std::string pathwayId;
    std::string badge;  // their initials
    std::string level;  // "Sequence 7", "Above the Sequence", "Mortal"; empty for people who aren't saved
    std::string organization;
    int connections = 0;
};

struct WebEdge {
    size_t a = 0;  // indexes into the nodes
    size_t b = 0;
    Bond bond = Bond::Other;
    // How a sees b and b sees a: "Mentor" and "Student". The same word on both sides for most.
    std::string fromA;
    std::string fromB;
    std::string note;
    std::string label() const;  // "Friend", "Mentor / Student"
};

struct WebOptions {
    std::array<bool, kBondCount> bonds{true, true, true, true, true, true, true};  // which kinds to show
    bool unsavedPeople = false;  // people named in relationships who aren't saved characters
    bool loners = true;          // characters without any relationship shown
    std::string organization;    // only members of this organization; empty for everyone
};

struct Web {
    std::vector<WebNode> nodes;
    std::vector<WebEdge> edges;
    // The node of a saved character, or -1.
    int nodeOf(int characterId) const;
    // A key that stays the same for the same person between builds: "c12", or "n:fors wall".
    std::string key(size_t node) const;
};

// Both sides of a relationship between two saved characters become one line.
Web buildWeb(const Database& db, const WebOptions& options = {});

// The shortest chain of relationships from one node to another (both ends included), or empty when
// they aren't connected. "Audrey Hall -> Klein Moretti -> Amon".
std::vector<size_t> shortestPath(const Web& web, size_t from, size_t to);
// "Audrey Hall is Friend of Klein Moretti, who is Enemy of Amon."
std::string describePath(const Web& web, const std::vector<size_t>& path);
// The edge between two nodes, or nullptr.
const WebEdge* edgeBetween(const Web& web, size_t a, size_t b);

// Where each node sits. A spring layout: lines pull the people they join together, everyone pushes
// everyone else away (hard when they'd overlap), and a little pull to the middle keeps separate groups
// in view. People without any line wait in a ring around the rest. Each step moves things a little
// less, so the app window can show it settling; settle() runs it to the end at once.
struct WebLayout {
    std::vector<float> x;
    std::vector<float> y;
    std::vector<bool> pinned;  // held in place (dragged by hand)
    float temperature = 0.0f;  // how far a node may move in one step; 0 when settled
    // While someone is dragged (their node), until it settles: only the people linked to them move,
    // and anyone they would overlap, so the rest of the web stays where it was. -1 for everyone.
    int around = -1;

    // Starts everyone on a circle, grouped by pathway, the same way every time.
    void start(const Web& web, const Database& db);
    // Keeps where the people already placed are (matched by Web::key), places newcomers next to
    // someone they know, and warms up a little so the web settles again.
    void adapt(const Web& oldWeb, const Web& newWeb, const Database& db);
    // One step. Returns true while it is still moving.
    bool step(const Web& web);
    void settle(const Web& web, int steps = 400);
    bool settled() const { return temperature <= 0.0f; }
};

// The web as a picture for a sheet, scaled to fit the figure. `links`: link each saved character to
// their sheet ("#c-1"), for an export that has their sheets too.
WebFigure webFigure(const Web& web, const WebLayout& layout, const Database& db, bool links);
// A sheet with the picture, then every connection in words, grouped by kind, and who has the most.
Sheet buildWebSheet(const Web& web, const WebLayout& layout, const Database& db, bool links);
// The same for everyone saved, laid out from the start and settled.
Sheet buildWebSheet(const Database& db, bool links);

}  // namespace lotm
