// A "sheet" is a format-neutral description of one character or artifact page.
// It is built once (sheet.cpp) and then drawn as plain text, Markdown or HTML (render.cpp),
// so every view and export shows exactly the same information.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "model.hpp"

namespace lotm {

struct StatRow {
    std::string name;   // "Strength"
    std::string code;   // "STR"
    int base = 10;
    int bonus = 0;
    int total = 10;
    int modifier = 0;
};

// One step of a character's Sequence ladder, which runs from Sequence 9 up to theirs, then Above the
// Sequence for a Great Old One. A mixed-pathway Beyonder's steps come from more than one pathway.
struct LadderRung {
    int sequence = 9;
    bool above = false;     // the step past Sequence 0
    std::string name;       // the Sequence's name, "Dreamwalker"; the Great Old One Above the Sequence
    std::string pathwayId;
    std::string pathway;    // "Spectator"
    std::string colour;     // the pathway's colour, "#eca04e" (see pathway_colours.hpp)
    bool current = false;   // the Sequence they are on now
    bool switched = false;  // the first Sequence after a change of pathway
    bool risky = false;     // ... to a pathway that isn't a neighbour
};

enum class BlockKind { Field, Paragraph, Bullet, Subheading, Quote, Stats, Ladder };

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    std::string label;  // Field only
    std::string text;
    std::vector<StatRow> stats;      // Stats only
    std::vector<LadderRung> rungs;   // Ladder only
};

// A short coloured label under a sheet's title: the power tier, state of mind, threat level ...
struct Badge {
    std::string text;
    // "pathway" (in the colour below), "good", "gold", "orange", "danger" or "muted"
    std::string tone = "muted";
    std::string pathwayId;  // tone "pathway" only
    std::string colour;     // tone "pathway" only: the pathway's colour as hex
};

struct Section {
    std::string heading;
    std::vector<Block> blocks;
    std::string style;  // HTML only: "dossier" draws the section as an official file

    explicit Section(std::string headingText, std::string styleName = "")
        : heading(std::move(headingText)), style(std::move(styleName)) {}

    void field(const std::string& label, const std::string& text);  // skipped when text is empty
    void paragraph(const std::string& text);                         // skipped when text is empty
    void bullet(const std::string& text);
    void subheading(const std::string& text);
    void quote(const std::string& text);
};

struct Sheet {
    std::string anchor;    // unique id used for links in a combined export, e.g. "c-001"
    std::string kind;      // "Character", "Sealed Artifact", "Pathway" or "Sefirah"
    std::string title;
    std::string subtitle;
    std::vector<Badge> badges;
    std::vector<Section> sections;
    std::string footer;
};

Sheet buildCharacterSheet(const Character& c, const Database& db);
// Sequence 9 up to theirs, each from the pathway it came from, then Above the Sequence. Empty for a mortal.
std::vector<LadderRung> ladderRungs(const Character& c, const Database& db);
Sheet buildArtifactSheet(const Artifact& a, const Database& db);
Sheet buildPathwaySheet(const Pathway& p, const Database& db);  // db: for its Sefirah, Above the Sequence
Sheet buildSefirahSheet(const Sefirah& s, const Database& db);

}  // namespace lotm
