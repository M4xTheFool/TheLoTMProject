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

enum class BlockKind { Field, Paragraph, Bullet, Subheading, Quote, Stats };

struct Block {
    BlockKind kind = BlockKind::Paragraph;
    std::string label;  // Field only
    std::string text;
    std::vector<StatRow> stats;  // Stats only
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
    std::string kind;      // "Character" or "Sealed Artifact"
    std::string title;
    std::string subtitle;
    std::vector<Section> sections;
    std::string footer;
};

Sheet buildCharacterSheet(const Character& c, const Database& db);
Sheet buildArtifactSheet(const Artifact& a, const Database& db);

}  // namespace lotm
