// Your story over time: dates in the story's world, story events on a timeline, and characters as
// they were on any date (their stages). Shared by the console, the app window and the exports.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model.hpp"
#include "sheet.hpp"

namespace lotm {

// ---- Dates

// "3 July 1360", "July 1360", "1360"; another epoch is named after it: "1200, Fourth Epoch".
// Empty for no date.
std::string storyDateText(const StoryDate& date);
// Reads a date written the way storyDateText writes it, and a few other ways: "July 3, 1360",
// "3rd July 1360", "Jul 1360", "Fourth Epoch 1200", "1349 of the Fifth Epoch". Empty text is no date.
// On text it can't read it returns nothing and says why in `problem`.
std::optional<StoryDate> parseStoryDate(const std::string& text, std::string* problem = nullptr);
// Earliest first: by epoch, year, month, then day. A date without a month comes before the months of
// its year. No date at all comes after every date.
bool storyDateBefore(const StoryDate& a, const StoryDate& b);
bool sameStoryDate(const StoryDate& a, const StoryDate& b);
// "First Epoch" .. "Ninth Epoch"
std::string epochName(int epoch);
inline const std::vector<std::string> kMonths = {"January", "February", "March",     "April",   "May",      "June",
                                                 "July",    "August",   "September", "October", "November", "December"};

// ---- Story events

// Suggestions for an event's kind; any other word works too.
inline const std::vector<std::string> kEventKinds = {"Meeting",  "Battle",  "Advancement", "Death",
                                                     "Discovery", "Ritual", "Journey",     "Pact or alliance",
                                                     "Betrayal", "Disaster", "Other"};
// Each kind's colour as hex, bright enough for a dark background; your own words get one made from
// the word, the same on every computer.
std::string eventKindColour(const std::string& kind);

// Every event, earliest first (events without a date last, then by id).
std::vector<const StoryEvent*> eventsInOrder(const Database& db);
// The events a character takes part in, earliest first.
std::vector<const StoryEvent*> eventsOf(const Database& db, int characterId);
// The name to show for someone in an event: a saved character's current name, or the name typed.
std::string eventPersonName(const Database& db, const EventPerson& person);
// "Klein Moretti (attacker), Audrey Hall and Fors Wall"
std::string eventPeopleText(const Database& db, const StoryEvent& e);
// "3 July 1360, Battle, Backlund" (whatever of these is filled in)
std::string eventSummary(const StoryEvent& e);

// ---- A character as of a date

struct AsOf {
    Character character;   // how they were then
    int stageIndex = -1;   // the stage (index into the character's stages) used; -1: as they are now
    // The date is before anything known about them: the earliest stage is shown (or now, without stages).
    bool beforeKnown = false;
};
// The character as they were on a date: their latest stage on or before it, applied to how they are
// now. On or after nowSince (when it's given), or with no date, they are as they are now. Before their
// first stage the first stage is used and beforeKnown is set.
AsOf characterAsOf(const Character& c, const StoryDate& date, const Database& db);
// The character as they were in one of their stages.
Character characterInStage(const Character& c, const CharacterStage& stage, const Database& db);
// A new stage holding what they are like now, to save before moving them on.
CharacterStage stageFromNow(const Character& c);
// "Sequence 7 Magician, Seer pathway", "Above the Sequence, Seer pathway", "Mortal"
std::string stageLevelText(const CharacterStage& stage, const Database& db);
// "From 3 July 1357: Sequence 5 Player, Acrobat pathway. Joins the Troupe"
std::string stageSummary(const CharacterStage& stage, const Database& db);
// Every date where something changes: event dates and stage dates, earliest first, each once.
std::vector<StoryDate> storyDates(const Database& db);

// ---- Sheets

Sheet buildEventSheet(const StoryEvent& e, const Database& db);
// Every event, or one character's (characterId != 0), grouped by year.
Sheet buildTimelineSheet(const Database& db, int characterId = 0);
// The character's sheet as they were on a date, with only the events up to it.
Sheet buildCharacterSheetAsOf(const Character& c, const StoryDate& date, const Database& db);
// One line per event for a sheet: date, title, kind, place, people. Used by the timeline sheets.
TimelineEntry timelineEntry(const StoryEvent& e, const Database& db);

}  // namespace lotm
