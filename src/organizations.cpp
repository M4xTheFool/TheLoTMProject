#include "organizations.hpp"

#include <algorithm>
#include <cctype>
#include <map>

#include "pathway_colours.hpp"
#include "presets.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "story.hpp"

namespace lotm {

namespace {

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string lower(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

// The text as it is compared: no spaces at the ends, lower case.
std::string key(const std::string& text) { return lower(trimmed(text)); }

// "nighthawks (church of the evernight goddess)" -> "nighthawks"; empty when there is no bracket.
std::string beforeBracket(const std::string& k) {
    const auto bracket = k.find('(');
    if (bracket == std::string::npos || bracket == 0) return "";
    return trimmed(k.substr(0, bracket));
}

// How well an affiliation names an organization: 3 its name, 2 a name it is also known as, 1 the part
// before a bracket, 0 not at all.
int match(const Organization& o, const std::string& affiliation) {
    const std::string wanted = key(affiliation);
    if (wanted.empty() || isIndependent(wanted)) return 0;
    if (key(o.name) == wanted) return 3;
    for (const auto& other : o.alsoKnownAs) {
        if (!key(other).empty() && key(other) == wanted) return 2;
    }
    const std::string wantedBase = beforeBracket(wanted);
    auto loose = [&](const std::string& name) {
        const std::string k = key(name);
        if (k.empty()) return false;
        const std::string base = beforeBracket(k);
        // Only when one side has no bracket: "Church (Storms)" doesn't name "Church (Evernight)".
        return (wantedBase.empty() && !base.empty() && base == wanted) || (base.empty() && !wantedBase.empty() && wantedBase == k);
    };
    if (loose(o.name)) return 1;
    for (const auto& other : o.alsoKnownAs) {
        if (loose(other)) return 1;
    }
    return 0;
}

void addOnce(std::vector<std::string>& list, const std::string& text) {
    const std::string k = key(text);
    if (k.empty()) return;
    if (std::none_of(list.begin(), list.end(), [&](const std::string& x) { return key(x) == k; })) list.push_back(trimmed(text));
}

std::string join(const std::vector<std::string>& items, const std::string& separator = ", ") {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) out += (i > 0 ? separator : "") + items[i];
    return out;
}

// "a, b and c"
std::string joinAnd(const std::vector<std::string>& items) {
    if (items.size() <= 1) return join(items);
    return join(std::vector<std::string>(items.begin(), items.end() - 1)) + " and " + items.back();
}

std::string count(int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

int threatIndex(const Character& c) {
    const auto it = std::find(kThreatLevels.begin(), kThreatLevels.end(), threatLevelOf(c));
    return it == kThreatLevels.end() ? -1 : static_cast<int>(it - kThreatLevels.begin());
}

}  // namespace

bool isIndependent(const std::string& affiliation) {
    const std::string k = key(affiliation);
    return k.empty() || k == "independent";
}

bool namesOrganization(const Organization& o, const std::string& affiliation) { return match(o, affiliation) > 0; }

const Organization* organizationNamed(const Database& db, const std::string& affiliation) {
    const Organization* best = nullptr;
    int bestScore = 0;
    for (const Organization& o : db.organizations) {
        const int score = match(o, affiliation);
        if (score > bestScore || (score > 0 && score == bestScore && o.id < best->id)) {
            best = &o;
            bestScore = score;
        }
    }
    return best;
}

bool belongsTo(const Database& db, const Organization& o, const std::string& affiliation) {
    const int score = match(o, affiliation);
    if (score == 0) return false;
    return std::none_of(db.organizations.begin(), db.organizations.end(), [&](const Organization& other) {
        if (other.id == o.id) return false;
        const int theirs = match(other, affiliation);
        return theirs > score || (theirs == score && other.id < o.id);
    });
}

std::vector<Member> membersOf(const Organization& o, const Database& db) {
    std::vector<Member> members;
    for (const Character& c : db.characters) {
        if (!belongsTo(db, o, c.affiliation.organization)) continue;
        Member m;
        m.character = &c;
        m.rank = c.affiliation.rank;
        for (size_t i = 0; i < o.ranks.size(); ++i) {
            if (!key(m.rank).empty() && key(o.ranks[i]) == key(m.rank)) {
                m.rankIndex = static_cast<int>(i);
                break;
            }
        }
        m.leader = o.leaderId != 0 && o.leaderId == c.id;
        members.push_back(m);
    }
    std::stable_sort(members.begin(), members.end(), [](const Member& a, const Member& b) {
        if (a.leader != b.leader) return a.leader;
        const int ra = a.rankIndex < 0 ? 1 << 20 : a.rankIndex, rb = b.rankIndex < 0 ? 1 << 20 : b.rankIndex;
        if (ra != rb) return ra < rb;
        return lower(a.character->name) < lower(b.character->name);
    });
    return members;
}

std::string memberRole(const Member& m) {
    const std::string rank = trimmed(m.rank);
    if (!m.leader) return rank;
    if (rank.empty() || lower(rank).find("leader") != std::string::npos) return rank.empty() ? "Leader" : rank;
    return rank + ", leader";
}

std::vector<FormerMember> formerMembersOf(const Organization& o, const Database& db) {
    std::vector<FormerMember> former;
    for (const Character& c : db.characters) {
        if (belongsTo(db, o, c.affiliation.organization)) continue;
        const std::vector<StageRef> stages = stagesOf(c, db);
        int first = -1, last = -1;
        for (size_t i = 0; i < stages.size(); ++i) {
            if (!belongsTo(db, o, stages[i].stage.affiliation.organization)) continue;
            if (first < 0) first = static_cast<int>(i);
            last = static_cast<int>(i);
        }
        if (first < 0) continue;
        FormerMember m;
        m.character = &c;
        m.rank = stages[static_cast<size_t>(last)].stage.affiliation.rank;
        m.from = stages[static_cast<size_t>(first)].stage.date;
        m.until = static_cast<size_t>(last) + 1 < stages.size() ? stages[static_cast<size_t>(last) + 1].stage.date : c.nowSince;
        former.push_back(m);
    }
    std::sort(former.begin(), former.end(), [](const FormerMember& a, const FormerMember& b) {
        if (!sameStoryDate(a.from, b.from)) return storyDateBefore(a.from, b.from);
        return lower(a.character->name) < lower(b.character->name);
    });
    return former;
}

std::string formerMemberDates(const FormerMember& m) {
    if (m.from.empty()) return m.until.empty() ? "" : "until " + storyDateText(m.until);
    if (m.until.empty()) return "from " + storyDateText(m.from);
    return storyDateText(m.from) + " to " + storyDateText(m.until);
}

std::string leaderName(const Organization& o, const Database& db) {
    if (const Character* c = o.leaderId != 0 ? db.findCharacter(o.leaderId) : nullptr) return c->name;
    return trimmed(o.leader);
}

void linkLeaderByName(const Database& db, Organization& o) {
    if (o.leaderId != 0) {
        if (const Character* c = db.findCharacter(o.leaderId)) {
            o.leader = c->name;
            return;
        }
        o.leaderId = 0;
    }
    o.leader = trimmed(o.leader);
    if (o.leader.empty()) return;
    const Character* found = nullptr;
    int matches = 0;
    for (const Character& c : db.characters) {
        if (key(c.name) == key(o.leader)) {
            found = &c;
            ++matches;
        }
    }
    if (matches == 1) {
        o.leaderId = found->id;
        o.leader = found->name;
    }
}

int powerScore(const Character& c) {
    if (c.pathwayId.empty()) return 0;
    return c.aboveSequence ? 11 : 10 - c.sequence;
}

OrganizationStrength organizationStrength(const Organization& o, const Database& db) {
    OrganizationStrength s;
    for (const auto& line : o.otherMembers) {
        if (!trimmed(line).empty()) ++s.others;
    }
    int strongestScore = -1, dangerIndex = -2, dangerScore = 0;
    for (const Member& m : membersOf(o, db)) {
        const Character& c = *m.character;
        if (deadNow(c, db)) {
            ++s.dead;
            continue;
        }
        ++s.members;
        const ThreatAssessment threat = assessThreat(c);
        s.threatScore += threat.total;
        const int tier = std::clamp(speedTierOf(c).tier, 0, kAboveSequenceTier);
        ++s.tiers[static_cast<size_t>(tier)];
        const int index = threatIndex(c);
        if (index >= 0) ++s.threats[static_cast<size_t>(index)];
        s.artifacts += static_cast<int>(std::count_if(c.artifactIds.begin(), c.artifactIds.end(),
                                                      [&](int id) { return db.findArtifact(id) != nullptr; }));
        const int score = powerScore(c) * 2 + (isKingOfAngels(c) || isDualPathwayGod(c) ? 1 : 0);
        if (score > strongestScore) {
            strongestScore = score;
            s.strongest = &c;
        }
        if (index > dangerIndex || (index == dangerIndex && threat.total > dangerScore)) {
            dangerIndex = index;
            dangerScore = threat.total;
            s.mostDangerous = &c;
        }
    }
    if (s.mostDangerous) s.threatLevel = threatLevelOf(*s.mostDangerous);
    return s;
}

std::string tierCounts(const OrganizationStrength& strength) {
    static const std::array<std::pair<const char*, const char*>, 7> kNames = {{
        {"mortal", "mortals"},
        {"low-Sequence Beyonder", "low-Sequence Beyonders"},
        {"mid-Sequence Beyonder", "mid-Sequence Beyonders"},
        {"saint", "saints"},
        {"angel", "angels"},
        {"god", "gods"},
        {"Great Old One", "Great Old Ones"},
    }};
    std::vector<std::string> parts;
    for (int tier = kAboveSequenceTier; tier >= 0; --tier) {
        const int n = strength.tiers[static_cast<size_t>(tier)];
        if (n > 0) parts.push_back(count(n, kNames[static_cast<size_t>(tier)].first, kNames[static_cast<size_t>(tier)].second));
    }
    return joinAnd(parts);
}

std::vector<std::string> organizationSuggestions(const Database& db) {
    std::vector<std::string> names;
    for (const Organization* o : organizationsByName(db)) addOnce(names, o->name);
    // Names typed on characters that no saved organization has yet, most used first.
    for (const UnsavedOrganization& u : organizationsNotSaved(db)) addOnce(names, u.name);
    for (const std::string& preset : kOrganizations) {
        if (!organizationNamed(db, preset)) addOnce(names, preset);
    }
    return names;
}

std::vector<std::string> keeperSuggestions(const Database& db) {
    std::vector<std::string> names = organizationSuggestions(db);
    names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& n) { return isIndependent(n); }), names.end());
    return names;
}

std::vector<std::string> rankSuggestions(const Database& db, const std::string& affiliation) {
    std::vector<std::string> ranks;
    const Organization* o = organizationNamed(db, affiliation);
    if (o) {
        for (const auto& rank : o->ranks) addOnce(ranks, rank);
    }
    for (const Character& c : db.characters) {
        const bool same = o ? organizationNamed(db, c.affiliation.organization) == o
                            : !isIndependent(affiliation) && key(c.affiliation.organization) == key(affiliation);
        if (same) addOnce(ranks, c.affiliation.rank);
    }
    return ranks;
}

std::vector<UnsavedOrganization> organizationsNotSaved(const Database& db) {
    struct Tally {
        std::map<std::string, int> spellings;  // how often each way of writing it is used
        int members = 0;
    };
    std::map<std::string, Tally> found;  // by key
    auto note = [&](const std::string& affiliation, bool now) {
        if (isIndependent(affiliation) || organizationNamed(db, affiliation)) return;
        Tally& t = found[key(affiliation)];
        ++t.spellings[trimmed(affiliation)];
        if (now) ++t.members;
    };
    for (const Character& c : db.characters) {
        note(c.affiliation.organization, true);
        for (const CharacterStage& stage : c.stages) note(stage.affiliation.organization, false);
    }
    for (const StoryEvent& e : db.events) {
        for (const EventPerson& p : e.people) {
            if (p.changed) note(p.change.affiliation.organization, false);
        }
    }
    std::vector<UnsavedOrganization> list;
    for (const auto& [k, tally] : found) {
        const auto most = std::max_element(tally.spellings.begin(), tally.spellings.end(),
                                           [](const auto& a, const auto& b) { return a.second < b.second; });
        list.push_back({most->first, tally.members});
    }
    std::stable_sort(list.begin(), list.end(), [](const UnsavedOrganization& a, const UnsavedOrganization& b) {
        if (a.members != b.members) return a.members > b.members;
        return lower(a.name) < lower(b.name);
    });
    return list;
}

Organization organizationFromMembers(const Database& db, const std::string& name) {
    Organization o;
    o.name = trimmed(name);
    std::map<std::string, std::pair<int, std::string>> ranks;  // by key: how many use it, and how it's written
    for (const Character& c : db.characters) {
        if (isIndependent(c.affiliation.organization) || key(c.affiliation.organization) != key(name)) continue;
        if (trimmed(c.affiliation.rank).empty()) continue;
        auto& entry = ranks[key(c.affiliation.rank)];
        if (entry.first++ == 0) entry.second = trimmed(c.affiliation.rank);
    }
    std::vector<std::pair<int, std::string>> ordered;
    for (const auto& [k, entry] : ranks) ordered.push_back(entry);
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& entry : ordered) o.ranks.push_back(entry.second);
    return o;
}

std::vector<const Organization*> organizationsByName(const Database& db) {
    std::vector<const Organization*> list;
    for (const Organization& o : db.organizations) list.push_back(&o);
    std::stable_sort(list.begin(), list.end(),
                     [](const Organization* a, const Organization* b) { return lower(a->name) < lower(b->name); });
    return list;
}

Sheet buildOrganizationSheet(const Organization& o, const Database& db) {
    Sheet sheet;
    sheet.anchor = "o-" + std::to_string(o.id);
    sheet.kind = "Organization";
    sheet.title = o.name.empty() ? "(unnamed)" : o.name;
    sheet.subtitle = o.kind.empty() ? "Organization" : o.kind;
    if (!o.founded.empty()) sheet.subtitle += ", founded " + storyDateText(o.founded);

    const std::vector<Member> members = membersOf(o, db);
    const OrganizationStrength strength = organizationStrength(o, db);
    if (!o.status.empty()) {
        const std::string k = key(o.status);
        const std::string tone = k == "active" ? "good" : (k == "destroyed" || k == "disbanded") ? "muted" : "gold";
        sheet.badges.push_back({o.status, tone, "", ""});
    }
    sheet.badges.push_back({count(strength.members + strength.others, "member", "members"), "colour", "",
                            hexColour(organizationRgb(db, o))});
    if (strength.dead > 0) sheet.badges.push_back({count(strength.dead, "member died", "members died"), "muted", "", ""});
    if (!strength.threatLevel.empty()) {
        const auto level = std::find(kThreatLevels.begin(), kThreatLevels.end(), strength.threatLevel);
        const int index = level == kThreatLevels.end() ? -1 : static_cast<int>(level - kThreatLevels.begin());
        static const char* kTones[] = {"good", "gold", "orange"};
        sheet.badges.push_back({"Threat: " + strength.threatLevel, index < 0 ? "muted" : index < 3 ? kTones[index] : "danger", "", ""});
    }

    Section overview{"Overview"};
    overview.field("ID", organizationCode(o.id));
    overview.field("Also known as", join(o.alsoKnownAs));
    overview.field("Kind", o.kind);
    overview.field("Status", o.status);
    {
        std::string leader = leaderName(o, db);
        if (!leader.empty() && o.leaderId != 0) leader += " (" + characterCode(o.leaderId) + ")";
        overview.field("Leader", leader);
    }
    overview.field("Headquarters", o.headquarters);
    overview.field("Founded", storyDateText(o.founded));
    std::vector<std::string> pathways;
    for (const auto& id : o.pathwayIds) pathways.push_back(pathwayLabel(db, id));
    overview.field("Pathways", join(pathways));
    overview.field("Symbol", o.symbol);
    overview.paragraph(o.description);
    if (!overview.blocks.empty()) sheet.sections.push_back(overview);

    if (!o.ranks.empty()) {
        Section ranks{"Ranks"};
        for (size_t i = 0; i < o.ranks.size(); ++i) {
            std::vector<std::string> holders;
            for (const Member& m : members) {
                if (m.rankIndex == static_cast<int>(i)) holders.push_back(m.character->name);
            }
            ranks.bullet(holders.empty() ? o.ranks[i] : o.ranks[i] + ": " + joinAnd(holders));
        }
        sheet.sections.push_back(ranks);
    }

    Section people{"Members"};
    for (const Member& m : members) {
        std::string line = m.character->name + " (" + characterCode(m.character->id) + ")";
        if (const std::string role = memberRole(m); !role.empty()) line += ", " + (m.leader && trimmed(m.rank).empty() ? "leader" : role);
        line += ": " + levelText(*m.character, db);
        if (const Death death = deathOf(*m.character, db); death.dead) {
            line += ". " + (death.date.empty() ? std::string("Dead") : "Died " + storyDateText(death.date));
        }
        people.bullet(line);
    }
    if (!o.otherMembers.empty()) {
        if (!members.empty()) people.subheading("Not saved as characters");
        for (const auto& line : o.otherMembers) {
            if (!trimmed(line).empty()) people.bullet(trimmed(line));
        }
    }
    if (!people.blocks.empty()) sheet.sections.push_back(people);

    const std::vector<FormerMember> former = formerMembersOf(o, db);
    if (!former.empty()) {
        Section earlier{"Former Members"};
        for (const FormerMember& m : former) {
            std::string line = m.character->name + " (" + characterCode(m.character->id) + ")";
            if (!trimmed(m.rank).empty()) line += ", " + trimmed(m.rank);
            const std::string dates = formerMemberDates(m);
            earlier.bullet(dates.empty() ? line : line + ": " + dates);
        }
        sheet.sections.push_back(earlier);
    }

    // Relationships your characters have with the organization as a whole, as the organization sees them.
    Section ties{"Relationships"};
    for (const OrganizationRelationship& r : relationshipsWithOrganization(db, o.id)) {
        std::string line = reciprocalType(r.relationship->type) + ": " + r.character->name + " (" +
                           characterCode(r.character->id) + ")";
        if (!r.relationship->note.empty()) line += ", " + r.relationship->note;
        ties.bullet(line);
    }
    if (!ties.blocks.empty()) {
        ties.paragraph("Each of them has this relationship with every member, unless a member has one of their own "
                       "with them.");
        sheet.sections.push_back(ties);
    }

    if (strength.members > 0) {
        Section power{"Strength"};
        power.field("Members", tierCounts(strength));
        if (strength.strongest) power.field("Strongest", strength.strongest->name + ", " + levelText(*strength.strongest, db));
        if (strength.mostDangerous) {
            power.field("Most dangerous", strength.mostDangerous->name + " (" + strength.threatLevel + " threat)");
        }
        power.field("Combined threat", std::to_string(strength.threatScore) + " (every member's threat score added up)");
        if (strength.artifacts > 0) power.field("Sealed Artifacts held", std::to_string(strength.artifacts));
        if (strength.dead > 0) power.field("Not counted", count(strength.dead, "member who died", "members who died"));
        sheet.sections.push_back(power);
    }

    if (!trimmed(o.notes).empty()) {
        Section notes{"Notes"};
        notes.paragraph(o.notes);
        sheet.sections.push_back(notes);
    }
    sheet.footer = "Stored in organizations.json";
    return sheet;
}

}  // namespace lotm
