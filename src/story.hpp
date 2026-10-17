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
// A saved character as someone in an event.
EventPerson eventPerson(const Character& c);
// The name to show for someone in an event: a saved character's current name, or the name typed.
std::string eventPersonName(const Database& db, const EventPerson& person);
// "Klein Moretti (attacker), Audrey Hall and Fors Wall"
std::string eventPeopleText(const Database& db, const StoryEvent& e);
// "3 July 1360, Battle, Backlund" (whatever of these is filled in)
std::string eventSummary(const StoryEvent& e);

// ---- A character as of a date

// One of a character's stages: one of their own (on their Stages tab), or what changed for them at a
// story event (on the event's People tab).
struct StageRef {
    CharacterStage stage;  // a copy; an event's change has the event's date, and its label names the event
    int ownIndex = -1;     // one of their own: its index in the character's stages
    int eventId = 0;       // a change at a story event: the event
};
// Every stage of a character, their own and the changes at story events, earliest first (undated
// ones last). On the same date an event's change comes after their own stage, so it counts. The change
// at skipEventId is left out, to show someone as they were just before that event.
std::vector<StageRef> stagesOf(const Character& c, const Database& db, int skipEventId = 0);

struct AsOf {
    Character character;   // how they were then
    int stageIndex = -1;   // one of their own stages was used: its index in the character's stages
    int eventId = 0;       // the change at a story event was used: the event
    CharacterStage stage;  // the stage used, when one was (dated, as stagesOf gives it)
    // The date is before anything known about them: the earliest stage is shown (or now, without stages).
    bool beforeKnown = false;

    bool inStage() const { return stageIndex >= 0 || eventId != 0; }
};
// The character as they were on a date: their latest stage on or before it (see stagesOf), applied to
// how they are now. On or after nowSince (when it's given), or with no date, they are as they are now.
// Before their first stage the first stage is used and beforeKnown is set.
AsOf characterAsOf(const Character& c, const StoryDate& date, const Database& db, int skipEventId = 0);
// The character as they were in one of their stages.
Character characterInStage(const Character& c, const CharacterStage& stage, const Database& db);
// A new stage holding what they are like now, to save before moving them on.
CharacterStage stageFromNow(const Character& c);
// "Sequence 7 Magician, Seer pathway", "Sequence 1 Storyweaver, King of Angels, Acrobat pathway",
// "Sequence 0 Error, Error pathway and the Door pathway", "Above the Sequence, Seer pathway", "Mortal"
std::string levelText(const Character& c, const Database& db);
// levelText for what a stage holds.
std::string stageLevelText(const CharacterStage& stage, const Database& db);
// "From 3 July 1357: Sequence 5 Player, Acrobat pathway. Joins the Troupe"
std::string stageSummary(const CharacterStage& stage, const Database& db);
// Every date where something changes: event dates, stage dates and deaths, earliest first, each once.
std::vector<StoryDate> storyDates(const Database& db);

// ---- Alive or dead

// How someone died, as far as your story says. Either by their own Alive or dead setting (Identity tab),
// with the date they died, or by their latest stage or change at a story event, when that has them dead
// and nothing came after it. Their own words for being alive ("Revived") mean they are alive now.
struct Death {
    bool dead = false;
    StoryDate date;       // when they died; empty when not known
    std::string how;      // in your words: "killed by Amon"
    int eventId = 0;      // they died at this story event
    int stageIndex = -1;  // in this stage of their own
};
Death deathOf(const Character& c, const Database& db);
bool deadNow(const Character& c, const Database& db);
// "Died 3 July 1360 at "The Fall of Backlund": killed by Amon", "Dead", or empty for the living.
std::string deathText(const Death& death, const Database& db);
// A character as they are on a date (characterAsOf does this) is alive or dead as their stages, their own
// death and now say: before any of those they were alive. The copy says it in its own fields: life is
// "Alive" or as written, and diedOn and lifeNote are filled in for one who is dead then.

// ---- Sheets

Sheet buildEventSheet(const StoryEvent& e, const Database& db);
// Every event, or one character's (characterId != 0), grouped by year.
Sheet buildTimelineSheet(const Database& db, int characterId = 0);
// The character's sheet as they were on a date, with only the events up to it.
Sheet buildCharacterSheetAsOf(const Character& c, const StoryDate& date, const Database& db);
// One line per event for a sheet: date, title, kind, place, people. Used by the timeline sheets.
TimelineEntry timelineEntry(const StoryEvent& e, const Database& db);

}  // namespace lotm
