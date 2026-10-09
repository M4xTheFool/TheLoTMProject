#include "compare.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>

#include "relations.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

constexpr int kMissing = INT_MIN;  // a ranked row where this character has no value

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& item : items) out += (out.empty() ? "" : ", ") + item;
    return out;
}

CompareRow textRow(const std::string& label, std::vector<std::string> values) {
    CompareRow row;
    row.label = label;
    row.values = std::move(values);
    return row;
}

CompareRow rankedRow(const std::string& label, std::vector<std::string> values, std::vector<int> numbers) {
    CompareRow row = textRow(label, std::move(values));
    row.ranked = true;
    row.numbers = std::move(numbers);
    row.highest.assign(row.numbers.size(), false);
    int top = kMissing;
    int lowest = INT_MAX;
    int present = 0;
    for (int n : row.numbers) {
        if (n == kMissing) continue;
        top = std::max(top, n);
        lowest = std::min(lowest, n);
        ++present;
    }
    if (present < 2 || top == lowest) return row;  // nothing to tell apart
    for (size_t i = 0; i < row.numbers.size(); ++i) row.highest[i] = row.numbers[i] == top;
    return row;
}

// The relationships between the compared characters, each pair described once.
std::vector<std::string> relationshipsBetween(const std::vector<const Character*>& characters) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < characters.size(); ++i) {
        for (size_t j = 0; j < characters.size(); ++j) {
            if (i == j) continue;
            const Character& from = *characters[i];
            const Character& to = *characters[j];
            for (const Relationship& r : from.relationships) {
                if (r.characterId != to.id || r.type.empty()) continue;
                // The other side lists the same relationship (Superior here, Subordinate there): say it once.
                if (j < i) {
                    const bool saidAlready = std::any_of(
                        to.relationships.begin(), to.relationships.end(), [&](const Relationship& back) {
                            return back.characterId == from.id && back.type == reciprocalType(r.type);
                        });
                    if (saidAlready) continue;
                }
                std::string line = reciprocalType(r.type) == r.type
                                       ? from.name + " and " + to.name + ": " + r.type
                                       : to.name + " is " + from.name + "'s " + r.type;
                if (!r.note.empty()) line += " (" + r.note + ")";
                lines.push_back(line);
            }
        }
    }
    return lines;
}

// Number of UTF-8 characters, so names with accents line up in the console.
size_t displayWidth(const std::string& text) {
    size_t width = 0;
    for (unsigned char ch : text) {
        if ((ch & 0xC0) != 0x80) ++width;
    }
    return width;
}

// Splits text into lines of at most `width` characters, breaking at spaces where it can.
std::vector<std::string> wrap(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    std::string line;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.size();
        std::string word = text.substr(start, end - start);
        while (displayWidth(word) > width) {  // a word longer than the column is cut
            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            size_t cut = 0, count = 0;
            while (cut < word.size() && count < width) {
                ++cut;
                while (cut < word.size() && (static_cast<unsigned char>(word[cut]) & 0xC0) == 0x80) ++cut;
                ++count;
            }
            lines.push_back(word.substr(0, cut));
            word = word.substr(cut);
        }
        if (!line.empty() && displayWidth(line) + 1 + displayWidth(word) > width) {
            lines.push_back(line);
            line.clear();
        }
        if (!word.empty()) line += (line.empty() ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

std::string padded(const std::string& text, size_t width) {
    const size_t used = displayWidth(text);
    return text + std::string(used < width ? width - used : 0, ' ');
}

}  // namespace

Comparison buildComparison(const std::vector<const Character*>& characters, const Database& db) {
    Comparison out;
    const size_t count = characters.size();
    std::vector<const Pathway*> pathways;
    for (const Character* c : characters) {
        out.names.push_back(c->name);
        pathways.push_back(db.findPathway(c->pathwayId));
    }
    auto each = [&](auto valueOf) {
        std::vector<std::string> values;
        for (size_t i = 0; i < count; ++i) values.push_back(valueOf(*characters[i], pathways[i]));
        return values;
    };
    auto numbers = [&](auto numberOf) {
        std::vector<int> result;
        for (size_t i = 0; i < count; ++i) result.push_back(numberOf(*characters[i], pathways[i]));
        return result;
    };

    CompareSection overview{"Overview", {}};
    overview.rows.push_back(textRow("Pathway", each([&](const Character& c, const Pathway* p) -> std::string {
        if (c.pathwayId.empty()) return "Ordinary mortal";
        if (!p) return c.pathwayId;
        return p->god.empty() ? p->name : p->name + " (" + p->god + ")";
    })));
    overview.rows.push_back(rankedRow(
        "Sequence",
        each([](const Character& c, const Pathway* p) -> std::string {
            if (c.pathwayId.empty()) return "-";
            const SequenceInfo* seq = p ? p->findSequence(c.sequence) : nullptr;
            return std::to_string(c.sequence) + (seq && !seq->name.empty() ? ": " + seq->name : "");
        }),
        numbers([](const Character& c, const Pathway*) { return c.pathwayId.empty() ? 0 : 10 - c.sequence; })));
    overview.rows.push_back(rankedRow("Power tier", each([](const Character& c, const Pathway*) {
                                          return speedTierOf(c).name;
                                      }),
                                      numbers([](const Character& c, const Pathway*) { return speedTierOf(c).tier; })));
    overview.rows.push_back(rankedRow(
        "Threat level", each([](const Character& c, const Pathway*) { return threatLevelOf(c); }),
        numbers([](const Character& c, const Pathway*) {
            const auto it = std::find(kThreatLevels.begin(), kThreatLevels.end(), threatLevelOf(c));
            return it == kThreatLevels.end() ? kMissing : static_cast<int>(it - kThreatLevels.begin());
        })));
    overview.rows.push_back(textRow("Affiliation", each([](const Character& c, const Pathway*) {
        const Affiliation& a = c.affiliation;
        if (a.rank.empty()) return a.organization;
        return a.organization.empty() ? a.rank : a.organization + ", " + a.rank;
    })));
    overview.rows.push_back(textRow("Alignment", each([](const Character& c, const Pathway*) { return c.alignment; })));
    overview.rows.push_back(textRow("Age", each([](const Character& c, const Pathway*) { return c.age; })));
    overview.rows.push_back(textRow("Gender", each([](const Character& c, const Pathway*) { return c.gender; })));
    out.sections.push_back(overview);

    CompareSection stats{"Stats", {}};
    for (int s = 0; s < kStatCount; ++s) {
        std::vector<std::string> values;
        std::vector<int> totals;
        for (size_t i = 0; i < count; ++i) {
            const int total = statTotals(*characters[i], pathways[i])[static_cast<size_t>(s)];
            totals.push_back(total);
            values.push_back(std::to_string(total) + " (" + signedNumber(abilityModifier(total)) + ")");
        }
        stats.rows.push_back(rankedRow(kStatNames[static_cast<size_t>(s)], values, totals));
    }
    stats.rows.push_back(rankedRow(
        "Speed", each([](const Character& c, const Pathway* p) { return std::to_string(effectiveSpeed(c, p)); }),
        numbers([](const Character& c, const Pathway* p) { return effectiveSpeed(c, p); })));
    stats.rows.push_back(rankedRow(
        "HP", each([](const Character& c, const Pathway*) {
            return c.stats.hpIncluded ? std::to_string(c.stats.hp) : std::string("not used");
        }),
        numbers([](const Character& c, const Pathway*) { return c.stats.hpIncluded ? c.stats.hp : kMissing; })));
    stats.rows.push_back(rankedRow(
        "Spirituality", each([](const Character& c, const Pathway*) {
            return c.stats.spirituality > 0 ? std::to_string(c.stats.spirituality) : std::string("-");
        }),
        numbers([](const Character& c, const Pathway*) {
            return c.stats.spirituality > 0 ? c.stats.spirituality : kMissing;
        })));
    out.sections.push_back(stats);

    auto abilityCount = [](const Character& c, const Pathway* p) {
        int n = 0;
        if (p) {
            for (const SequenceInfo* seq : abilitiesUpTo(*p, c.sequence)) n += static_cast<int>(seq->abilities.size());
        }
        return n + static_cast<int>(c.uniquenessAbilities.size());
    };
    CompareSection powers{"Powers", {}};
    powers.rows.push_back(rankedRow(
        "Abilities", each([&](const Character& c, const Pathway* p) { return std::to_string(abilityCount(c, p)); }),
        numbers(abilityCount)));
    powers.rows.push_back(textRow("Movement", each([](const Character& c, const Pathway* p) {
        return join(movementModes(c, p));
    })));
    powers.rows.push_back(rankedRow(
        "Sealed Artifacts", each([&](const Character& c, const Pathway*) {
            std::vector<std::string> names;
            for (int id : c.artifactIds) {
                if (const Artifact* a = db.findArtifact(id)) names.push_back(a->name);
            }
            return names.empty() ? std::string("none") : join(names);
        }),
        numbers([&](const Character& c, const Pathway*) {
            return static_cast<int>(std::count_if(c.artifactIds.begin(), c.artifactIds.end(),
                                                  [&](int id) { return db.findArtifact(id) != nullptr; }));
        })));
    out.sections.push_back(powers);

    out.between = relationshipsBetween(characters);
    return out;
}

std::string renderComparisonText(const Comparison& comparison) {
    const size_t count = comparison.names.size();
    size_t labelWidth = 0;
    for (const auto& section : comparison.sections) {
        for (const auto& row : section.rows) labelWidth = std::max(labelWidth, displayWidth(row.label));
    }
    const size_t column = count <= 2 ? 30 : count == 3 ? 24 : 19;

    std::string out;
    auto line = [&](const std::string& label, const std::vector<std::vector<std::string>>& cells) {
        size_t height = 1;
        for (const auto& cell : cells) height = std::max(height, cell.size());
        for (size_t l = 0; l < height; ++l) {
            std::string text = "  " + padded(l == 0 ? label : "", labelWidth);
            for (const auto& cell : cells) text += "  " + padded(l < cell.size() ? cell[l] : "", column);
            while (!text.empty() && text.back() == ' ') text.pop_back();
            out += text + "\n";
        }
    };

    std::vector<std::vector<std::string>> names;
    for (const auto& name : comparison.names) names.push_back(wrap(name, column));
    line("", names);
    for (const auto& section : comparison.sections) {
        out += "\n  " + section.heading + "\n";
        for (const auto& row : section.rows) {
            std::vector<std::vector<std::string>> cells;
            for (size_t i = 0; i < count; ++i) {
                const bool top = row.ranked && row.highest[i];
                cells.push_back(wrap((row.values[i].empty() ? "-" : row.values[i]) + (top ? " *" : ""), column));
            }
            line(row.label, cells);
        }
    }
    out += "\n  * highest in that row\n";
    if (!comparison.between.empty()) {
        out += "\n  Between them\n";
        for (const auto& text : comparison.between) out += "    - " + text + "\n";
    }
    return out;
}

}  // namespace lotm
