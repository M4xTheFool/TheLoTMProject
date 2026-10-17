#include "compare.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstddef>

#include "organizations.hpp"
#include "presets.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "story.hpp"

namespace lotm {

namespace {

constexpr int kMissing = INT_MIN;  // a ranked row where this character has no value

// "Superior" reads as "Dunn's superior"; a type someone typed themselves is kept as written.
std::string inSentence(std::string type) {
    if (std::find(kRelationshipTypes.begin(), kRelationshipTypes.end(), type) == kRelationshipTypes.end()) return type;
    for (char& ch : type) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return type;
}

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

// A row where none of them has anything filled in, such as Spirituality when nobody uses it.
bool saysNothing(const CompareRow& row) {
    if (row.ranked) return std::all_of(row.numbers.begin(), row.numbers.end(), [](int n) { return n == kMissing; });
    return std::all_of(row.values.begin(), row.values.end(),
                       [](const std::string& value) { return value.empty() || value == "-"; });
}

// "Klein Moretti and Amon: Enemy", or "Dunn Smith is Leonard Mitchell's superior".
std::string pairLine(const std::string& from, const std::string& to, const std::string& type) {
    return reciprocalType(type) == type ? from + " and " + to + ": " + type : to + " is " + from + "'s " + inSentence(type);
}

// The relationships between the compared characters, each pair described once, and then the ones that
// reach them through an organization.
std::vector<std::string> relationshipsBetween(const std::vector<const Character*>& characters, const Database& db) {
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
                std::string line = pairLine(from.name, to.name, r.type);
                if (!r.note.empty()) line += " (" + r.note + ")";
                lines.push_back(line);
            }
        }
    }
    for (size_t i = 0; i < characters.size(); ++i) {
        for (size_t j = i + 1; j < characters.size(); ++j) {
            for (const auto& [type, organization] : tiesBetween(db, *characters[i], *characters[j])) {
                lines.push_back(pairLine(characters[i]->name, characters[j]->name, type) + ", through the " + organization);
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
    overview.rows.push_back(textRow("Pathway", each([&](const Character& c, const Pathway*) -> std::string {
        if (c.pathwayId.empty()) return "Ordinary mortal";
        std::string text = pathwayLabel(db, c.pathwayId);
        if (isDualPathwayGod(c)) text += " and " + pathwayLabel(db, c.secondPathway.pathwayId);
        for (const auto& id : earlierPathwayIds(c)) text += ", switched from " + pathwayLabel(db, id);
        return text;
    })));
    overview.rows.push_back(rankedRow(
        "Sequence",
        each([](const Character& c, const Pathway* p) -> std::string {
            if (c.pathwayId.empty()) return "-";
            if (c.aboveSequence) return "Above the Sequence";
            const SequenceInfo* seq = p ? p->findSequence(c.sequence) : nullptr;
            return std::to_string(c.sequence) + (seq && !seq->name.empty() ? ": " + seq->name : "");
        }),
        // Stronger counts higher: Sequence 9 is 1, Sequence 0 is 10, Above the Sequence 11.
        numbers([](const Character& c, const Pathway*) {
            if (c.pathwayId.empty()) return 0;
            return c.aboveSequence ? 11 : 10 - c.sequence;
        })));
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
    overview.rows.push_back(textRow("State of mind", each([](const Character& c, const Pathway*) { return c.stateOfMind; })));
    {
        // Only when one of them has died or has their own words for it.
        CompareRow life = textRow("Alive or dead", each([&](const Character& c, const Pathway*) {
            const Death death = deathOf(c, db);
            if (death.dead) return deathText(death, db);
            return c.life.empty() ? std::string("Alive") : c.life;
        }));
        if (std::any_of(life.values.begin(), life.values.end(), [](const std::string& v) { return v != "Alive"; })) {
            overview.rows.push_back(life);
        }
    }
    overview.rows.push_back(textRow("Sefirah", each([&](const Character& c, const Pathway*) {
        return c.sefirahId.empty() ? std::string() : sefirahLabel(db, c.sefirahId);
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

    CompareSection powers{"Powers", {}};
    powers.rows.push_back(rankedRow(
        "Abilities", each([&](const Character& c, const Pathway*) { return std::to_string(abilityCount(c, db)); }),
        numbers([&](const Character& c, const Pathway*) { return abilityCount(c, db); })));
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
    for (CompareSection& section : out.sections) {
        section.rows.erase(std::remove_if(section.rows.begin(), section.rows.end(), saysNothing), section.rows.end());
    }

    out.between = relationshipsBetween(characters, db);
    return out;
}

Comparison buildOrganizationComparison(const std::vector<const Organization*>& organizations, const Database& db) {
    Comparison out;
    std::vector<OrganizationStrength> strengths;
    for (const Organization* o : organizations) {
        out.names.push_back(o->name);
        strengths.push_back(organizationStrength(*o, db));
    }
    auto each = [&](auto valueOf) {
        std::vector<std::string> values;
        for (size_t i = 0; i < organizations.size(); ++i) values.push_back(valueOf(*organizations[i], strengths[i]));
        return values;
    };
    auto numbers = [&](auto numberOf) {
        std::vector<int> result;
        for (size_t i = 0; i < organizations.size(); ++i) result.push_back(numberOf(*organizations[i], strengths[i]));
        return result;
    };
    using Strength = OrganizationStrength;

    CompareSection overview{"Overview", {}};
    overview.rows.push_back(textRow("Kind", each([](const Organization& o, const Strength&) { return o.kind; })));
    overview.rows.push_back(textRow("Status", each([](const Organization& o, const Strength&) { return o.status; })));
    overview.rows.push_back(textRow("Leader", each([&](const Organization& o, const Strength&) { return leaderName(o, db); })));
    overview.rows.push_back(textRow("Headquarters", each([](const Organization& o, const Strength&) { return o.headquarters; })));
    overview.rows.push_back(textRow("Founded", each([](const Organization& o, const Strength&) { return storyDateText(o.founded); })));
    overview.rows.push_back(textRow("Pathways", each([&](const Organization& o, const Strength&) {
        std::vector<std::string> names;
        for (const auto& id : o.pathwayIds) {
            const Pathway* p = db.findPathway(id);
            names.push_back(p ? p->name : id);
        }
        return join(names);
    })));
    out.sections.push_back(overview);

    CompareSection power{"Strength", {}};
    power.rows.push_back(rankedRow(
        "Members", each([](const Organization&, const Strength& s) {
            std::string text = std::to_string(s.members);
            if (s.others > 0) text += " and " + std::to_string(s.others) + " not saved";
            return text;
        }),
        numbers([](const Organization&, const Strength& s) { return s.members + s.others; })));
    power.rows.push_back(rankedRow(
        "Strongest", each([&](const Organization&, const Strength& s) {
            return s.strongest ? s.strongest->name + ", " + levelName(*s.strongest) : std::string();
        }),
        numbers([](const Organization&, const Strength& s) { return s.strongest ? powerScore(*s.strongest) : kMissing; })));
    auto tierRow = [&](const std::string& label, std::vector<int> tiers) {
        const bool anyone = std::any_of(strengths.begin(), strengths.end(), [&](const Strength& s) {
            return std::any_of(tiers.begin(), tiers.end(), [&](int t) { return s.tiers[static_cast<size_t>(t)] > 0; });
        });
        if (!anyone) return;  // a row of noughts
        power.rows.push_back(rankedRow(
            label, each([&](const Organization&, const Strength& s) {
                int n = 0;
                for (int t : tiers) n += s.tiers[static_cast<size_t>(t)];
                return std::to_string(n);
            }),
            numbers([&](const Organization&, const Strength& s) {
                int n = 0;
                for (int t : tiers) n += s.tiers[static_cast<size_t>(t)];
                return n;
            })));
    };
    tierRow("Great Old Ones and gods", {kAboveSequenceTier, 5});
    tierRow("Angels (Sequence 2-1)", {4});
    tierRow("Saints (Sequence 4-3)", {3});
    tierRow("Beyonders (Sequence 9-5)", {2, 1});
    power.rows.push_back(rankedRow(
        "Most dangerous", each([](const Organization&, const Strength& s) {
            return s.mostDangerous ? s.mostDangerous->name + " (" + s.threatLevel + ")" : std::string();
        }),
        numbers([](const Organization&, const Strength& s) {
            const auto it = std::find(kThreatLevels.begin(), kThreatLevels.end(), s.threatLevel);
            return it == kThreatLevels.end() ? kMissing : static_cast<int>(it - kThreatLevels.begin());
        })));
    power.rows.push_back(rankedRow(
        "Combined threat", each([](const Organization&, const Strength& s) { return std::to_string(s.threatScore); }),
        numbers([](const Organization&, const Strength& s) { return s.members > 0 ? s.threatScore : kMissing; })));
    power.rows.push_back(rankedRow(
        "Sealed Artifacts held", each([](const Organization&, const Strength& s) { return std::to_string(s.artifacts); }),
        numbers([](const Organization&, const Strength& s) { return s.artifacts; })));
    out.sections.push_back(power);
    for (CompareSection& section : out.sections) {
        section.rows.erase(std::remove_if(section.rows.begin(), section.rows.end(), saysNothing), section.rows.end());
    }

    // Who knows whom across them: each member listed with their organization, once per pair.
    std::vector<const Character*> people;
    std::vector<size_t> groupOf;
    for (size_t i = 0; i < organizations.size(); ++i) {
        for (const Member& m : membersOf(*organizations[i], db)) {
            people.push_back(m.character);
            groupOf.push_back(i);
        }
    }
    for (size_t i = 0; i < people.size(); ++i) {
        for (size_t j = 0; j < people.size(); ++j) {
            if (groupOf[i] == groupOf[j]) continue;
            const Character& from = *people[i];
            const Character& to = *people[j];
            for (const Relationship& r : from.relationships) {
                if (r.characterId != to.id || r.type.empty()) continue;
                if (j < i) {
                    const bool saidAlready = std::any_of(to.relationships.begin(), to.relationships.end(), [&](const Relationship& back) {
                        return back.characterId == from.id && back.type == reciprocalType(r.type);
                    });
                    if (saidAlready) continue;
                }
                const std::string fromName = from.name + " (" + organizations[groupOf[i]]->name + ")";
                const std::string toName = to.name + " (" + organizations[groupOf[j]]->name + ")";
                std::string line = pairLine(fromName, toName, r.type);
                if (!r.note.empty()) line += " (" + r.note + ")";
                out.between.push_back(line);
            }
        }
    }
    // A member of one with a relationship with another as a whole.
    for (size_t i = 0; i < people.size(); ++i) {
        for (const Relationship& r : people[i]->relationships) {
            for (size_t j = 0; j < organizations.size(); ++j) {
                if (j == groupOf[i] || r.organizationId != organizations[j]->id || r.type.empty()) continue;
                std::string line = pairLine(people[i]->name + " (" + organizations[groupOf[i]]->name + ")",
                                            "the " + organizations[j]->name, r.type);
                line[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));  // "The Aurora Order is ..."
                if (!r.note.empty()) line += " (" + r.note + ")";
                out.between.push_back(line);
            }
        }
    }
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
