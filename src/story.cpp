#include "story.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <tuple>

#include "model_json.hpp"
#include "pathway_colours.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

const char* kEpochWords[] = {"first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth", "ninth"};

std::string lowercase(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool allDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

// 1-12 for a month's name or its first three letters or more ("Sep", "Sept"), 0 otherwise.
int monthNumber(const std::string& word) {
    if (word.size() < 3) return 0;
    for (size_t i = 0; i < kMonths.size(); ++i) {
        const std::string month = lowercase(kMonths[i]);
        if (month.compare(0, word.size(), word) == 0) return static_cast<int>(i) + 1;
    }
    return 0;
}

// 1-9 for "first" .. "ninth" or "1st" .. "9th", 0 otherwise.
int epochNumber(const std::string& word) {
    for (int i = 0; i < 9; ++i) {
        if (word == kEpochWords[i]) return i + 1;
    }
    if (word.size() == 3 && std::isdigit(static_cast<unsigned char>(word[0])) && word[0] != '0') {
        const std::string suffix = word.substr(1);
        if (suffix == "st" || suffix == "nd" || suffix == "rd" || suffix == "th") return word[0] - '0';
    }
    return 0;
}

// "3rd" -> "3", so an ordinal day reads like a number.
std::string withoutOrdinal(const std::string& word) {
    if (word.size() < 3) return word;
    const std::string suffix = word.substr(word.size() - 2);
    const std::string number = word.substr(0, word.size() - 2);
    if ((suffix == "st" || suffix == "nd" || suffix == "rd" || suffix == "th") && allDigits(number)) return number;
    return word;
}

int daysIn(int month) {
    static const int kDays[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month >= 1 && month <= 12 ? kDays[month - 1] : 31;
}

std::tuple<int, int, int, int> sortKey(const StoryDate& d) { return {d.epoch, d.year, d.month, d.day}; }

std::uint64_t textHash(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string join(const std::vector<std::string>& items, const std::string& separator = ", ") {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) out += (i > 0 ? separator : "") + items[i];
    return out;
}

// "A, B and C"
std::string joinAnd(const std::vector<std::string>& items) {
    if (items.size() <= 1) return join(items);
    return join(std::vector<std::string>(items.begin(), items.end() - 1)) + " and " + items.back();
}

void addSection(Sheet& sheet, Section section) {
    if (!section.blocks.empty()) sheet.sections.push_back(std::move(section));
}

// The heading a date goes under on a timeline: its year, with the epoch when it isn't the Fifth.
std::string yearHeading(const StoryDate& d) {
    if (d.empty()) return "No date";
    StoryDate year;
    year.epoch = d.epoch;
    year.year = d.year;
    return storyDateText(year);
}

}  // namespace

// ---------------------------------------------------------------- dates

std::string epochName(int epoch) {
    if (epoch >= 1 && epoch <= 9) {
        std::string word = kEpochWords[epoch - 1];
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        return word + " Epoch";
    }
    return "Epoch " + std::to_string(epoch);
}

std::string storyDateText(const StoryDate& date) {
    if (date.empty()) return "";
    std::string text;
    if (date.month >= 1 && date.month <= 12) {
        if (date.day > 0) text = std::to_string(date.day) + " ";
        text += kMonths[static_cast<size_t>(date.month - 1)] + " ";
    }
    text += std::to_string(date.year);
    if (date.epoch != 5) text += ", " + epochName(date.epoch);
    return text;
}

std::optional<StoryDate> parseStoryDate(const std::string& text, std::string* problem) {
    auto fail = [&](const std::string& why) -> std::optional<StoryDate> {
        if (problem) *problem = why;
        return std::nullopt;
    };
    // Words, with commas and full stops as spaces: "July 3, 1360" -> july 3 1360.
    std::vector<std::string> words;
    {
        std::string cleaned;
        for (char ch : lowercase(text)) cleaned += (ch == ',' || ch == '.') ? ' ' : ch;
        std::istringstream in(cleaned);
        for (std::string word; in >> word;) words.push_back(word);
    }
    StoryDate date;
    if (words.empty()) return date;

    bool epochGiven = false;
    std::vector<int> numbers;  // day and year, in the order they were written
    std::vector<bool> markedYear;
    bool nextIsYear = false;
    for (size_t i = 0; i < words.size(); ++i) {
        const std::string& word = words[i];
        if (word == "of" || word == "the" || word == "in" || word == "ad" || word == "on") continue;
        if (word == "year") {
            nextIsYear = true;
            continue;
        }
        if (word == "epoch") {
            // "Epoch 4" names it after the word; "Fourth Epoch" before it (handled there).
            if (i + 1 < words.size() && allDigits(words[i + 1]) && words[i + 1].size() == 1) {
                date.epoch = words[i + 1][0] - '0';
                epochGiven = true;
                ++i;
                continue;
            }
            if (!epochGiven) return fail("Name the epoch, like \"Fourth Epoch\".");
            continue;
        }
        if (const int epoch = epochNumber(word); epoch > 0 && i + 1 < words.size() && words[i + 1] == "epoch") {
            if (epochGiven && date.epoch != epoch) return fail("It names two epochs.");
            date.epoch = epoch;
            epochGiven = true;
            ++i;
            continue;
        }
        if (const int month = monthNumber(word); month > 0) {
            if (date.month != 0) return fail("It names two months.");
            date.month = month;
            continue;
        }
        const std::string number = withoutOrdinal(word);
        if (allDigits(number) && number.size() <= 6) {
            numbers.push_back(std::stoi(number));
            markedYear.push_back(nextIsYear);
            nextIsYear = false;
            continue;
        }
        return fail("\"" + word + "\" isn't part of a date. Write it like 3 July 1360, July 1360 or 1360.");
    }

    // The year is the number marked with "year", else the one above 31, else the last one.
    int yearAt = -1;
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (markedYear[i]) yearAt = static_cast<int>(i);
    }
    for (size_t i = 0; yearAt < 0 && i < numbers.size(); ++i) {
        if (numbers[i] > 31) yearAt = static_cast<int>(i);
    }
    if (yearAt < 0 && !numbers.empty()) yearAt = static_cast<int>(numbers.size()) - 1;
    if (yearAt < 0 || numbers[static_cast<size_t>(yearAt)] <= 0) return fail("Add the year, like 1360.");
    date.year = numbers[static_cast<size_t>(yearAt)];
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (static_cast<int>(i) == yearAt) continue;
        if (date.day != 0) return fail("It has more numbers than a day and a year.");
        date.day = numbers[i];
    }
    if (date.day != 0) {
        if (date.month == 0) return fail("A day needs its month, like 3 July 1360.");
        if (date.day < 1 || date.day > daysIn(date.month)) {
            return fail(kMonths[static_cast<size_t>(date.month - 1)] + " has no day " + std::to_string(date.day) + ".");
        }
    }
    if (problem) problem->clear();
    return date;
}

bool storyDateBefore(const StoryDate& a, const StoryDate& b) {
    if (a.empty() || b.empty()) return !a.empty() && b.empty();
    return sortKey(a) < sortKey(b);
}

bool sameStoryDate(const StoryDate& a, const StoryDate& b) {
    if (a.empty() || b.empty()) return a.empty() && b.empty();
    return sortKey(a) == sortKey(b);
}

void to_json(nlohmann::json& j, const StoryDate& date) { j = storyDateText(date); }

void from_json(const nlohmann::json& j, StoryDate& date) {
    if (j.is_null()) {
        date = StoryDate{};
        return;
    }
    const std::string text = j.get<std::string>();
    std::string problem;
    const auto parsed = parseStoryDate(text, &problem);
    if (!parsed) throw nlohmann::json::other_error::create(599, "the date \"" + text + "\" can't be read: " + problem, &j);
    date = *parsed;
}

// ---------------------------------------------------------------- events

std::string eventKindColour(const std::string& kind) {
    static const std::pair<const char*, Rgb> kColours[] = {
        {"meeting", {102, 176, 220}},  {"battle", {226, 98, 82}},     {"advancement", {217, 173, 84}},
        {"death", {158, 146, 176}},    {"discovery", {96, 192, 142}}, {"ritual", {180, 138, 224}},
        {"journey", {80, 192, 180}},   {"pact or alliance", {128, 178, 104}}, {"betrayal", {222, 132, 52}},
        {"disaster", {204, 82, 128}},  {"other", {163, 158, 148}}};
    const std::string key = lowercase(kind);
    if (key.empty()) return hexColour({163, 158, 148});
    for (const auto& [name, colour] : kColours) {
        if (key == name) return hexColour(colour);
    }
    // Your own kinds: a colour made from the word.
    const float hue = static_cast<float>(textHash(key) % 360) / 360.0f;
    const float sector = hue * 6.0f;
    const int i = static_cast<int>(sector);
    const float f = sector - static_cast<float>(i), v = 0.86f, s = 0.5f;
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    const float rgb[6][3] = {{v, t, p}, {q, v, p}, {p, v, t}, {p, q, v}, {t, p, v}, {v, p, q}};
    auto channel = [](float x) { return static_cast<int>(std::lround(x * 255.0f)); };
    return hexColour({channel(rgb[i % 6][0]), channel(rgb[i % 6][1]), channel(rgb[i % 6][2])});
}

std::vector<const StoryEvent*> eventsInOrder(const Database& db) {
    std::vector<const StoryEvent*> events;
    for (const auto& e : db.events) events.push_back(&e);
    std::stable_sort(events.begin(), events.end(), [](const StoryEvent* a, const StoryEvent* b) {
        if (storyDateBefore(a->date, b->date)) return true;
        if (storyDateBefore(b->date, a->date)) return false;
        return a->id < b->id;
    });
    return events;
}

std::vector<const StoryEvent*> eventsOf(const Database& db, int characterId) {
    std::vector<const StoryEvent*> theirs;
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (std::any_of(e->people.begin(), e->people.end(),
                        [&](const EventPerson& p) { return p.characterId != 0 && p.characterId == characterId; })) {
            theirs.push_back(e);
        }
    }
    return theirs;
}

std::string eventPersonName(const Database& db, const EventPerson& person) {
    if (const Character* c = person.characterId != 0 ? db.findCharacter(person.characterId) : nullptr) return c->name;
    return person.name.empty() ? "(unknown)" : person.name;
}

std::string eventPeopleText(const Database& db, const StoryEvent& e) {
    std::vector<std::string> names;
    for (const EventPerson& p : e.people) {
        names.push_back(eventPersonName(db, p) + (p.role.empty() ? "" : " (" + p.role + ")"));
    }
    return joinAnd(names);
}

std::string eventSummary(const StoryEvent& e) {
    std::vector<std::string> parts;
    if (!e.date.empty()) parts.push_back(storyDateText(e.date));
    if (!e.kind.empty()) parts.push_back(e.kind);
    if (!e.place.empty()) parts.push_back(e.place);
    return join(parts);
}

// ---------------------------------------------------------------- characters as of a date

Character characterInStage(const Character& c, const CharacterStage& stage, const Database& db) {
    Character then = c;
    then.pathwayId = stage.pathwayId;
    then.sequence = stage.sequence;
    then.aboveSequence = stage.aboveSequence;
    then.sefirahId = stage.sefirahId;
    if (stage.stateOfMind != c.stateOfMind) then.stateOfMindNote.clear();
    then.stateOfMind = stage.stateOfMind;
    then.affiliation = stage.affiliation;
    then.dossier.status = stage.status;
    then.titles = stage.titles;
    then.aliases = stage.aliases;
    then.artifactIds = stage.artifactIds;
    // The Sequences they took from other pathways, up to the one they were on then.
    std::vector<SequenceStep> steps;
    for (const SequenceStep& step : c.mixedSequences) {
        if (step.sequence > stage.sequence && step.pathwayId != stage.pathwayId) steps.push_back(step);
    }
    then.mixedSequences = steps;
    normalizeCharacter(then);  // drops what doesn't apply at that Sequence: an honorific name, a Uniqueness ...

    // HP and Spirituality grow with them, so a stage at another level gets the suggested values.
    const bool sameLevel = then.pathwayId == c.pathwayId && then.sequence == c.sequence &&
                           then.aboveSequence == c.aboveSequence;
    if (!sameLevel) {
        const Pathway* pathway = db.findPathway(then.pathwayId);
        if (then.stats.hpIncluded) then.stats.hp = suggestedHp(then, pathway);
        if (then.stats.spirituality > 0) then.stats.spirituality = suggestedSpirituality(then, pathway);
    }
    return then;
}

AsOf characterAsOf(const Character& c, const StoryDate& date, const Database& db) {
    AsOf result{c, -1, false};
    if (date.empty()) return result;
    if (!c.nowSince.empty() && !storyDateBefore(date, c.nowSince)) return result;
    int best = -1, earliest = -1;
    for (size_t i = 0; i < c.stages.size(); ++i) {
        const StoryDate& when = c.stages[i].date;
        if (when.empty()) continue;
        const int index = static_cast<int>(i);
        if (earliest < 0 || storyDateBefore(when, c.stages[static_cast<size_t>(earliest)].date)) earliest = index;
        const bool started = !storyDateBefore(date, when);
        if (started && (best < 0 || !storyDateBefore(when, c.stages[static_cast<size_t>(best)].date))) best = index;
    }
    if (best < 0 && earliest >= 0) {
        best = earliest;
        result.beforeKnown = true;
    }
    if (best >= 0) {
        result.character = characterInStage(c, c.stages[static_cast<size_t>(best)], db);
        result.stageIndex = best;
        return result;
    }
    result.beforeKnown = !c.nowSince.empty();  // no stages, and the date is before "now" began
    return result;
}

CharacterStage stageFromNow(const Character& c) {
    CharacterStage stage;
    stage.pathwayId = c.pathwayId;
    stage.sequence = c.sequence;
    stage.aboveSequence = c.aboveSequence;
    stage.sefirahId = c.sefirahId;
    stage.stateOfMind = c.stateOfMind;
    stage.affiliation = c.affiliation;
    stage.status = c.dossier.status;
    stage.titles = c.titles;
    stage.aliases = c.aliases;
    stage.artifactIds = c.artifactIds;
    return stage;
}

std::string stageLevelText(const CharacterStage& stage, const Database& db) {
    if (stage.pathwayId.empty()) return "Mortal";
    const Pathway* p = db.findPathway(stage.pathwayId);
    const std::string pathway = (p ? p->name : stage.pathwayId) + " pathway";
    if (stage.aboveSequence) return "Above the Sequence, " + pathway;
    const SequenceInfo* seq = p ? p->findSequence(stage.sequence) : nullptr;
    return "Sequence " + std::to_string(stage.sequence) + (seq && !seq->name.empty() ? " " + seq->name : "") + ", " +
           pathway;
}

std::string stageSummary(const CharacterStage& stage, const Database& db) {
    std::string text = stage.date.empty() ? "No date yet: " : "From " + storyDateText(stage.date) + ": ";
    text += stageLevelText(stage, db);
    if (!stage.label.empty()) text += ". " + stage.label;
    return text;
}

std::vector<StoryDate> storyDates(const Database& db) {
    std::vector<StoryDate> dates;
    auto add = [&](const StoryDate& d) {
        if (d.empty()) return;
        if (std::none_of(dates.begin(), dates.end(), [&](const StoryDate& x) { return sameStoryDate(x, d); })) {
            dates.push_back(d);
        }
    };
    for (const auto& e : db.events) add(e.date);
    for (const auto& c : db.characters) {
        for (const auto& s : c.stages) add(s.date);
        add(c.nowSince);
    }
    std::sort(dates.begin(), dates.end(), storyDateBefore);
    return dates;
}

// ---------------------------------------------------------------- sheets

TimelineEntry timelineEntry(const StoryEvent& e, const Database& db) {
    TimelineEntry entry;
    entry.date = storyDateText(e.date);
    entry.title = e.title.empty() ? "(untitled event)" : e.title;
    entry.kind = e.kind;
    entry.colour = eventKindColour(e.kind);
    entry.place = e.place;
    entry.chapter = e.chapter;
    entry.people = eventPeopleText(db, e);
    std::vector<std::string> artifacts;
    for (int id : e.artifactIds) {
        if (const Artifact* a = db.findArtifact(id)) artifacts.push_back(a->name);
    }
    entry.artifacts = joinAnd(artifacts);
    entry.description = e.description;
    return entry;
}

Sheet buildEventSheet(const StoryEvent& e, const Database& db) {
    Sheet sheet;
    sheet.anchor = "e-" + std::to_string(e.id);
    sheet.kind = "Story event";
    sheet.title = e.title.empty() ? "(untitled event)" : e.title;
    sheet.subtitle = eventSummary(e);
    if (sheet.subtitle.empty()) sheet.subtitle = "No date yet";
    if (!e.kind.empty()) sheet.badges.push_back({e.kind, "colour", "", eventKindColour(e.kind)});
    if (!e.chapter.empty()) sheet.badges.push_back({e.chapter, "muted", "", ""});

    Section what{"What Happened"};
    what.paragraph(e.description);
    addSection(sheet, what);

    Section details{"Details"};
    details.field("Event", eventCode(e.id));
    details.field("Date", e.date.empty() ? "" : storyDateText(e.date));
    details.field("Kind", e.kind);
    details.field("Place", e.place);
    details.field("In the book", e.chapter);
    addSection(sheet, details);

    // Who was there, and what each of them was at the time.
    Section who{"Who Was There"};
    for (const EventPerson& p : e.people) {
        std::string line = eventPersonName(db, p);
        if (!p.role.empty()) line += " (" + p.role + ")";
        if (const Character* c = p.characterId != 0 ? db.findCharacter(p.characterId) : nullptr) {
            const AsOf then = characterAsOf(*c, e.date, db);
            line += ": " + levelName(then.character);
            if (const Pathway* path = db.findPathway(then.character.pathwayId)) {
                if (!then.character.aboveSequence) {
                    if (const SequenceInfo* seq = path->findSequence(then.character.sequence); seq && !seq->name.empty()) {
                        line += " " + seq->name;
                    }
                }
                line += ", " + path->name + " pathway";
            }
            if (then.beforeKnown) line += " (before anything recorded about them)";
        }
        who.bullet(line);
    }
    addSection(sheet, who);

    Section artifacts{"Sealed Artifacts"};
    for (int id : e.artifactIds) {
        if (const Artifact* a = db.findArtifact(id)) artifacts.bullet(a->name + " (" + artifactCode(a->id) + ")");
    }
    addSection(sheet, artifacts);

    Section notes{"Notes"};
    notes.paragraph(e.notes);
    addSection(sheet, notes);

    if (!e.createdAt.empty()) {
        sheet.footer = "Created " + e.createdAt;
        if (!e.updatedAt.empty() && e.updatedAt != e.createdAt) sheet.footer += ", last edited " + e.updatedAt;
    }
    return sheet;
}

Sheet buildTimelineSheet(const Database& db, int characterId) {
    Sheet sheet;
    const Character* who = characterId != 0 ? db.findCharacter(characterId) : nullptr;
    sheet.anchor = who ? "timeline-" + std::to_string(characterId) : "timeline";
    sheet.kind = "Timeline";
    sheet.title = who ? "Timeline: " + who->name : "Timeline";
    const auto events = who ? eventsOf(db, characterId) : eventsInOrder(db);
    if (events.empty()) {
        sheet.subtitle = "No events yet";
        return sheet;
    }
    sheet.subtitle = std::to_string(events.size()) + (events.size() == 1 ? " event" : " events");
    const StoryEvent* first = events.front();
    const StoryEvent* last = nullptr;
    for (const StoryEvent* e : events) {
        if (!e->date.empty()) last = e;
    }
    if (!first->date.empty() && last) {
        const std::string from = yearHeading(first->date), to = yearHeading(last->date);
        sheet.subtitle += from == to ? ", " + from : ", from " + from + " to " + to;
    }
    // One section per year, earliest first.
    for (const StoryEvent* e : events) {
        const std::string heading = yearHeading(e->date);
        if (sheet.sections.empty() || sheet.sections.back().heading != heading) sheet.sections.emplace_back(heading);
        Section& section = sheet.sections.back();
        if (section.blocks.empty()) section.blocks.push_back({BlockKind::Timeline, "", "", {}, {}, {}, {}});
        section.blocks.back().entries.push_back(timelineEntry(*e, db));
    }
    return sheet;
}

Sheet buildCharacterSheetAsOf(const Character& c, const StoryDate& date, const Database& db) {
    const AsOf then = characterAsOf(c, date, db);
    Sheet sheet = buildCharacterSheet(then.character, db, date.empty() ? nullptr : &date);
    if (date.empty()) return sheet;
    sheet.anchor += "-" + lowercase(storyDateText(date));
    for (char& ch : sheet.anchor) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '-';
    }
    std::vector<Badge> badges = {{"As of " + storyDateText(date), "gold", "", ""}};
    if (then.beforeKnown) badges.push_back({"Before anything recorded about them", "muted", "", ""});
    badges.insert(badges.end(), sheet.badges.begin(), sheet.badges.end());
    sheet.badges = badges;
    std::string from = then.stageIndex >= 0 ? "Their stage from " + storyDateText(c.stages[static_cast<size_t>(then.stageIndex)].date)
                                            : "As they are now";
    if (then.stageIndex >= 0 && !c.stages[static_cast<size_t>(then.stageIndex)].label.empty()) {
        from += ": " + c.stages[static_cast<size_t>(then.stageIndex)].label;
    }
    sheet.footer = from + (sheet.footer.empty() ? "" : ". " + sheet.footer);
    return sheet;
}

}  // namespace lotm
