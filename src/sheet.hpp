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

// One event on a timeline in a sheet (see story.hpp).
struct TimelineEntry {
    std::string date;     // "3 July 1360"; empty when it has no date
    std::string title;
    std::string kind;     // Battle, Meeting ...
    std::string colour;   // the kind's colour as hex
    std::string place;
    std::string chapter;
    std::string people;     // "Klein Moretti (attacker) and Audrey Hall"
    std::string artifacts;  // "Creeping Hunger and Sealed Artifact 0-08"
    std::string description;
};

// The relationship web as a picture (see web.hpp). HTML draws it; text and Markdown leave it out, and
// the web's sheet lists the same connections in words.
struct FigureNode {
    float x = 0.0f;  // the centre, inside the figure's width and height
    float y = 0.0f;
    float radius = 18.0f;
    std::string badge;   // initials
    std::string name;
    std::string colour;  // the pathway's colour as hex
    std::string tip;     // shown when the pointer rests on it
    std::string link;    // "#c-1" when the same export has their sheet
    bool saved = true;   // false: someone who isn't a saved character, drawn smaller and grey
    bool dead = false;   // a saved character who is dead now, drawn grey
};
struct FigureEdge {
    size_t a = 0;  // indexes into the nodes
    size_t b = 0;
    std::string label;
    std::string colour;  // the kind of relationship's colour as hex
    std::string tip;
    bool through = false;  // it comes through an organization: drawn dashed
};
struct WebFigure {
    float width = 860.0f;
    float height = 600.0f;
    std::vector<FigureNode> nodes;
    std::vector<FigureEdge> edges;
    std::vector<std::pair<std::string, std::string>> legend;  // name and colour of each kind of relationship
};

enum class BlockKind { Field, Paragraph, Bullet, Subheading, Quote, Stats, Ladder, Timeline, Web };

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    std::string label;  // Field only
    std::string text;
    std::vector<StatRow> stats;           // Stats only
    std::vector<LadderRung> rungs;        // Ladder only
    std::vector<TimelineEntry> entries;   // Timeline only
    WebFigure web;                        // Web only
};

// A short coloured label under a sheet's title: the power tier, state of mind, threat level ...
struct Badge {
    std::string text;
    // "pathway" (in the colour below), "colour" (any colour below), "good", "gold", "orange", "danger" or "muted"
    std::string tone = "muted";
    std::string pathwayId;  // tone "pathway" only
    std::string colour;     // tones "pathway" and "colour": the colour as hex
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

// until: a date in the story, to show only the stages and events up to it (see buildCharacterSheetAsOf
// in story.hpp, which also shows the character as they were then).
Sheet buildCharacterSheet(const Character& c, const Database& db, const StoryDate* until = nullptr);
// Sequence 9 up to theirs, each from the pathway it came from, then Above the Sequence. Empty for a mortal.
std::vector<LadderRung> ladderRungs(const Character& c, const Database& db);
Sheet buildArtifactSheet(const Artifact& a, const Database& db);
Sheet buildPathwaySheet(const Pathway& p, const Database& db);  // db: for its Sefirah, Above the Sequence
Sheet buildSefirahSheet(const Sefirah& s, const Database& db);

}  // namespace lotm
