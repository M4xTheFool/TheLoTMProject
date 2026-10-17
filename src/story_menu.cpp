// The console's Timeline and Relationship web menus, and the Stages step of the character creator.
#include <algorithm>
#include <cctype>
#include <iostream>

#include "app.hpp"
#include "organizations.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "web.hpp"

namespace lotm {

namespace {

bool isNumber(const std::string& text) {
    return !text.empty() && text.size() <= 4 &&
           std::all_of(text.begin(), text.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
}

}  // namespace

int chooseCharacter(const App& app, const std::string& title, const std::string& zeroLabel) {
    if (app.db.characters.empty()) {
        ui::info("No characters yet.");
        return 0;
    }
    std::vector<std::string> names;
    for (const Character& c : app.db.characters) names.push_back(c.name + " (" + characterCode(c.id) + ", " + levelText(c, app.db) + ")");
    const int pick = ui::choose(title, names, zeroLabel);
    return pick >= 0 ? app.db.characters[static_cast<size_t>(pick)].id : 0;
}

// ---------------------------------------------------------------- dates

StoryDate readStoryDate(const std::string& prompt, const StoryDate& current, bool required) {
    while (true) {
        const std::string shown = current.empty() ? "" : " [" + storyDateText(current) + "]";
        const std::string text = ui::trim(ui::readLine(prompt + shown + ": "));
        if (text.empty() && (!current.empty() || !required)) return current;
        if (text == "-" && !required) return {};
        std::string problem;
        if (const auto date = parseStoryDate(text, &problem); date && !date->empty()) {
            if (ui::trim(text) != storyDateText(*date)) ui::info("Reads as " + storyDateText(*date) + ".");
            return *date;
        }
        ui::info(problem.empty() ? "Type a date like 3 July 1360, July 1360 or 1360." : problem);
    }
}

namespace {

// ---------------------------------------------------------------- stages

// A stage of theirs (askDate), or what changed for them at an event (dated on the event).
void editStage(const App& app, const Character& c, CharacterStage& stage, bool askDate = true) {
    if (askDate) {
        ui::info("What they were like from a date on. Enter keeps each value.");
        stage.date = readStoryDate("From which date? (e.g. 3 July 1360, or just 1360)", stage.date, true);
    } else {
        ui::info("What they are like from this event on, until their next stage. Enter keeps each value.");
    }
    stage.label = ui::readText("What changed, in a few words (optional)", stage.label);
    const int pathway = choosePathway(app, "Their pathway then", "No pathway (an ordinary mortal)", true);
    if (pathway == ui::Choice::kZero) stage.pathwayId.clear();
    else if (pathway >= 0) stage.pathwayId = app.db.pathways[static_cast<size_t>(pathway)].id;
    if (const Pathway* p = app.db.findPathway(stage.pathwayId)) {
        for (const auto& s : p->sequences) {
            std::cout << "  " << s.sequence << ") " << (s.name.empty() ? "(no name yet)" : s.name) << "\n";
        }
        stage.sequence = ui::readInt("Their Sequence then", 0, 9, stage.sequence);
        stage.aboveSequence = stage.sequence == 0 && ui::yesNo("Above the Sequence?", stage.aboveSequence);
    }
    // Sequence 1: Beyonder characteristics and the Uniqueness (a King of Angels); Sequence 0: a second pathway.
    Character level = c;
    putStageLevel(level, stage);
    if (sequenceOneChoices(level) || absorbedUniqueness(level)) editSequenceOne(app, level);
    if (dualPathwayEligible(level)) editSecondPathway(app, level);
    takeStageLevel(stage, level);
    if (isKingOfAngels(level)) ui::info("A King of Angels then.");
    stage.stateOfMind = ui::choosePreset("State of mind then", kStatesOfMind, stage.stateOfMind);
    stage.affiliation.organization =
        ui::choosePreset("Organization then", organizationSuggestions(app.db), stage.affiliation.organization);
    if (!stage.affiliation.organization.empty()) {
        const auto ranks = rankSuggestions(app.db, stage.affiliation.organization);
        stage.affiliation.rank = ranks.empty() ? ui::readText("Rank or role", stage.affiliation.rank)
                                               : ui::choosePreset("Rank or role", ranks, stage.affiliation.rank);
    }
    stage.status = ui::choosePreset("Status then", kStatuses, stage.status);
    if (ui::yesNo("Change their titles or aliases then?", false)) {
        ui::editList("Titles then", stage.titles);
        ui::editList("Aliases then", stage.aliases);
    }
    if (!app.db.artifacts.empty() && ui::yesNo("Change the Sealed Artifacts they held then?", false)) {
        while (true) {
            for (size_t i = 0; i < app.db.artifacts.size(); ++i) {
                const Artifact& a = app.db.artifacts[i];
                const bool held = std::find(stage.artifactIds.begin(), stage.artifactIds.end(), a.id) != stage.artifactIds.end();
                std::cout << "  [" << (held ? "x" : " ") << "] " << (i + 1) << ") " << a.name << "\n";
            }
            const std::string value = ui::readLine("Type a number to give or take it, Enter when done: ");
            if (value.empty()) break;
            if (!isNumber(value) || std::stoul(value) < 1 || std::stoul(value) > app.db.artifacts.size()) continue;
            const int id = app.db.artifacts[std::stoul(value) - 1].id;
            auto it = std::find(stage.artifactIds.begin(), stage.artifactIds.end(), id);
            if (it == stage.artifactIds.end()) stage.artifactIds.push_back(id);
            else stage.artifactIds.erase(it);
        }
    }
    stage.note = ui::readText("Anything else about this time (optional)", stage.note);
}

// ---------------------------------------------------------------- events

std::string personText(const App& app, const EventPerson& person) {
    const std::string name = eventPersonName(app.db, person);
    return (person.role.empty() ? name : name + " (" + person.role + ")") + (person.characterId ? "" : ", not a saved character") +
           (person.changed ? "; after this: " + stageLevelText(person.change, app.db) : "");
}

void addPerson(const App& app, StoryEvent& e, const std::string& typed) {
    EventPerson person;
    person.name = typed;
    const auto matches = app.db.findCharactersByName(typed, 0);
    if (matches.size() == 1 && ui::toLower(matches[0]->name) == ui::toLower(typed)) {
        person.characterId = matches[0]->id;
    } else if (!matches.empty()) {
        std::vector<std::string> names;
        for (const Character* m : matches) names.push_back(m->name + " (" + characterCode(m->id) + ")");
        const int pick = ui::choose("Is this one of your saved characters?", names, "No, just add \"" + typed + "\"");
        if (pick >= 0) person.characterId = matches[static_cast<size_t>(pick)]->id;
    }
    if (const Character* c = app.db.findCharacter(person.characterId)) person.name = c->name;
    person.role = ui::readText("Their part in it (attacker, witness ...; optional)", "");
    e.people.push_back(person);
}

void editPeople(const App& app, StoryEvent& e) {
    while (true) {
        std::cout << "Who was there:\n";
        if (e.people.empty()) std::cout << "  (nobody yet)\n";
        for (size_t i = 0; i < e.people.size(); ++i) std::cout << "  " << (i + 1) << ") " << personText(app, e.people[i]) << "\n";
        const std::string value =
            ui::readLine("  P picks a saved character, a typed name adds anyone, a number changes or removes someone, Enter when done: ");
        if (value.empty()) return;
        if (value == "p" || value == "P") {
            if (const int id = chooseCharacter(app, "Who?")) {
                EventPerson person = eventPerson(*app.db.findCharacter(id));
                person.role = ui::readText("Their part in it (optional)", "");
                e.people.push_back(person);
            }
            continue;
        }
        if (!isNumber(value)) {
            addPerson(app, e, value);
            continue;
        }
        const size_t index = std::stoul(value);
        if (index < 1 || index > e.people.size()) {
            ui::info("There is no entry " + value + ".");
            continue;
        }
        EventPerson& person = e.people[index - 1];
        const Character* c = app.db.findCharacter(person.characterId);
        std::vector<std::string> options = {"Change their part in it"};
        if (c) {
            options.push_back(person.changed ? "Change what changed for them here"
                                             : "Something changed for them here (they advanced, joined someone ...)");
        }
        if (c && person.changed) options.push_back("Remove what changed for them");
        options.push_back("Remove them");
        const int pick = ui::choose(eventPersonName(app.db, person), options, "Back");
        const std::string chosen = pick >= 0 ? options[static_cast<size_t>(pick)] : "";
        if (chosen.rfind("Change their", 0) == 0) {
            person.role = ui::readText("Their part in it", person.role);
        } else if (chosen.rfind("Something", 0) == 0 || chosen.rfind("Change what", 0) == 0) {
            if (!person.changed) {  // starting from how they came to it
                person.change = stageFromNow(characterAsOf(*c, e.date, app.db, e.id).character);
                person.changed = true;
            }
            if (e.date.empty()) ui::info("Give the event a date too; without one this change isn't used.");
            editStage(app, *c, person.change, false);
        } else if (chosen.rfind("Remove what", 0) == 0) {
            person.changed = false;
            person.change = CharacterStage{};
        } else if (chosen == "Remove them") {
            e.people.erase(e.people.begin() + static_cast<std::ptrdiff_t>(index - 1));
        }
    }
}

void editEventArtifacts(const App& app, StoryEvent& e) {
    if (app.db.artifacts.empty()) {
        ui::info("No Sealed Artifacts exist yet.");
        return;
    }
    while (true) {
        for (size_t i = 0; i < app.db.artifacts.size(); ++i) {
            const Artifact& a = app.db.artifacts[i];
            const bool used = std::find(e.artifactIds.begin(), e.artifactIds.end(), a.id) != e.artifactIds.end();
            std::cout << "  [" << (used ? "x" : " ") << "] " << (i + 1) << ") " << a.name << "\n";
        }
        const std::string value = ui::readLine("Type a number to tick or untick a Sealed Artifact, Enter when done: ");
        if (value.empty()) return;
        if (!isNumber(value) || std::stoul(value) < 1 || std::stoul(value) > app.db.artifacts.size()) continue;
        const int id = app.db.artifacts[std::stoul(value) - 1].id;
        auto it = std::find(e.artifactIds.begin(), e.artifactIds.end(), id);
        if (it == e.artifactIds.end()) e.artifactIds.push_back(id);
        else e.artifactIds.erase(it);
    }
}

void stepKind(StoryEvent& e) { e.kind = ui::choosePreset("What kind of event", kEventKinds, e.kind); }

// Creates (isNew) or changes a story event, ending on a review screen. Returns true when it was saved.
bool runEventEditor(App& app, StoryEvent e, bool isNew) {
    if (isNew) {
        ui::header("A new story event");
        ui::info("Something that happens in your story. Enter skips anything optional.");
        e.title = ui::readRequired("What happened, in a few words (the title)");
        e.date = readStoryDate("When? (e.g. 3 July 1360, July 1360 or just 1360; Enter for no date)", e.date);
        stepKind(e);
        e.place = ui::readText("Where (optional)", e.place);
        editPeople(app, e);
        e.description = ui::readMultiline("What happened (optional)", e.description);
    }
    while (true) {
        ui::header(isNew ? "Review the new event" : "Change the event");
        std::cout << renderText(buildEventSheet(e, app.db));
        const int pick = ui::choose("\nChange something, or save:",
                                    {"Title", "Date", "Kind", "Place", "Chapter", "Who was there", "Sealed Artifacts",
                                     "What happened", "Notes", "Save"},
                                    "Cancel without saving");
        switch (pick) {
            case 0: e.title = ui::readRequired("Title", e.title); break;
            case 1: e.date = readStoryDate("When", e.date); break;
            case 2: stepKind(e); break;
            case 3: e.place = ui::readText("Where", e.place); break;
            case 4: e.chapter = ui::readText("Where it is in your book", e.chapter); break;
            case 5: editPeople(app, e); break;
            case 6: editEventArtifacts(app, e); break;
            case 7: e.description = ui::readMultiline("What happened", e.description); break;
            case 8: e.notes = ui::readMultiline("Notes (for you)", e.notes); break;
            case 9:
                try {
                    saveEvent(app, e, isNew);
                    ui::info("Saved " + e.title + " as " + eventCode(e.id) + ".");
                    return true;
                } catch (const StorageError& error) {
                    ui::info(std::string("Saving failed: ") + error.what());
                }
                break;
            default:
                if (ui::yesNo("Discard these changes?", false)) return false;
        }
    }
}

// One event from the timeline, earliest first.
const StoryEvent* chooseEvent(const App& app, const std::string& title) {
    const auto events = eventsInOrder(app.db);
    if (events.empty()) {
        ui::info("No story events yet.");
        return nullptr;
    }
    std::vector<std::string> names;
    for (const StoryEvent* e : events) {
        const std::string summary = eventSummary(*e);
        names.push_back(e->title + (summary.empty() ? "" : " (" + summary + ")"));
    }
    const int pick = ui::choose(title, names, "Cancel");
    return pick >= 0 ? events[static_cast<size_t>(pick)] : nullptr;
}

void everyoneAsOf(const App& app) {
    const StoryDate date = readStoryDate("Which date? (e.g. 3 July 1360)", {}, true);
    ui::header("Everyone as of " + storyDateText(date));
    for (const Character& c : app.db.characters) {
        const AsOf then = characterAsOf(c, date, app.db);
        std::string line = c.name + ": " + levelText(then.character, app.db);
        if (!then.character.affiliation.organization.empty()) line += ", " + then.character.affiliation.organization;
        if (then.beforeKnown) line += " (before anything recorded about them)";
        else if (!then.inStage() && !stagesOf(c, app.db).empty()) line += " (as they are now)";
        ui::info(line);
    }
    const auto events = eventsInOrder(app.db);
    const auto on = std::count_if(events.begin(), events.end(), [&](const StoryEvent* e) { return sameStoryDate(e->date, date); });
    if (on > 0) {
        ui::blank();
        ui::info("On that date:");
        for (const StoryEvent* e : events) {
            if (sameStoryDate(e->date, date)) ui::info("  " + e->title + (e->kind.empty() ? "" : " (" + e->kind + ")"));
        }
    }
    ui::pause();
}

// ---------------------------------------------------------------- the web in words

void connectionsOf(const App& app) {
    const int id = chooseCharacter(app, "Whose connections?");
    if (!id) return;
    WebOptions options;
    options.unsavedPeople = true;
    const Web web = buildWeb(app.db, options);
    const int node = web.nodeOf(id);
    ui::header("Who " + app.db.findCharacter(id)->name + " knows");
    bool any = false;
    for (int b = 0; b < kBondCount; ++b) {
        std::vector<std::string> lines;
        for (const WebEdge& e : web.edges) {
            if (static_cast<int>(e.bond) != b || node < 0) continue;
            if (e.a != static_cast<size_t>(node) && e.b != static_cast<size_t>(node)) continue;
            const size_t other = e.a == static_cast<size_t>(node) ? e.b : e.a;
            const std::string& how = e.a == static_cast<size_t>(node) ? e.fromA : e.fromB;
            lines.push_back(web.nodes[other].name + (how.empty() ? "" : ": " + how) + (e.note.empty() ? "" : " (" + e.note + ")"));
        }
        if (lines.empty()) continue;
        any = true;
        ui::info(std::string(bondName(static_cast<Bond>(b))) + ":");
        for (const auto& line : lines) ui::info("  " + line);
    }
    if (!any) ui::info("No relationships yet. Add them in the character's Affiliation and relationships step.");
    ui::pause();
}

void pathBetween(const App& app) {
    const int from = chooseCharacter(app, "From whom?");
    if (!from) return;
    const int to = chooseCharacter(app, "To whom?");
    if (!to) return;
    WebOptions options;
    options.unsavedPeople = true;
    const Web web = buildWeb(app.db, options);
    const int a = web.nodeOf(from), b = web.nodeOf(to);
    const auto path = a >= 0 && b >= 0 ? shortestPath(web, static_cast<size_t>(a), static_cast<size_t>(b)) : std::vector<size_t>{};
    ui::blank();
    if (from == to) ui::info("That is the same person.");
    else if (path.empty()) ui::info("They aren't connected through anyone, as far as the relationships go.");
    else {
        ui::info(std::to_string(path.size() - 1) + (path.size() == 2 ? " step: " : " steps: ") + describePath(web, path));
        ui::info("(each name in brackets is how the one before them lists them)");
    }
    ui::pause();
}

void mostConnected(const App& app) {
    const Web web = buildWeb(app.db);
    std::vector<const WebNode*> nodes;
    for (const WebNode& n : web.nodes) nodes.push_back(&n);
    std::stable_sort(nodes.begin(), nodes.end(), [](const WebNode* a, const WebNode* b) { return a->connections > b->connections; });
    ui::header("Most connected");
    for (size_t i = 0; i < nodes.size() && i < 10; ++i) {
        ui::info(std::to_string(i + 1) + ") " + nodes[i]->name + ": " + std::to_string(nodes[i]->connections) +
                 (nodes[i]->connections == 1 ? " connection" : " connections"));
    }
    std::vector<std::string> loners;
    for (const WebNode& n : web.nodes) {
        if (n.connections == 0) loners.push_back(n.name);
    }
    if (!loners.empty()) {
        std::string text;
        for (size_t i = 0; i < loners.size(); ++i) text += (i == 0 ? "" : i + 1 == loners.size() ? " and " : ", ") + loners[i];
        ui::info("Not connected to any other saved character yet: " + text + ".");
    }
    ui::pause();
}

}  // namespace

void editStages(const App& app, Character& c) {
    while (true) {
        ui::header("Stages: how they were earlier in the story");
        ui::info("A stage holds their pathway and Sequence, state of mind, organization, status, titles, aliases");
        ui::info("and Sealed Artifacts from a date on; everything else is as they are now. The timeline shows");
        ui::info("everyone as they were on any date from these.");
        ui::info("As they are now since: " + (c.nowSince.empty() ? std::string("(not set; now is the end of the story)")
                                                                : storyDateText(c.nowSince)));
        std::vector<std::string> options = {"Add a stage (starting from how they are now)"};
        if (!c.stages.empty()) {
            ui::blank();
            for (size_t i = 0; i < c.stages.size(); ++i) ui::info(std::to_string(i + 1) + ") " + stageSummary(c.stages[i], app.db));
            options.push_back("Change a stage");
            options.push_back("Remove a stage");
        }
        std::vector<StageRef> atEvents;
        for (StageRef& ref : stagesOf(c, app.db)) {
            if (ref.eventId != 0) atEvents.push_back(std::move(ref));
        }
        if (!atEvents.empty()) {
            ui::blank();
            ui::info("Changes at story events (change these in the event, in the Timeline menu):");
            for (const StageRef& ref : atEvents) ui::info("  " + stageSummary(ref.stage, app.db));
        }
        options.push_back("Set the date they became as they are now");
        const int pick = ui::choose("", options, "Done");
        const std::string chosen = pick >= 0 ? options[static_cast<size_t>(pick)] : "";
        if (pick < 0) return;
        if (chosen.rfind("Add", 0) == 0) {
            CharacterStage stage = stageFromNow(c);
            editStage(app, c, stage);
            c.stages.push_back(stage);
        } else if (chosen.rfind("Change", 0) == 0 || chosen.rfind("Remove", 0) == 0) {
            std::vector<std::string> names;
            for (const auto& stage : c.stages) names.push_back(stageSummary(stage, app.db));
            const int which = ui::choose("Which stage?", names, "Cancel");
            if (which < 0) continue;
            if (chosen.rfind("Change", 0) == 0) editStage(app, c, c.stages[static_cast<size_t>(which)]);
            else c.stages.erase(c.stages.begin() + which);
        } else {
            c.nowSince = readStoryDate("Since when are they as they are now? (\"-\" for not set)", c.nowSince);
        }
        std::stable_sort(c.stages.begin(), c.stages.end(),
                         [](const CharacterStage& a, const CharacterStage& b) { return storyDateBefore(a.date, b.date); });
    }
}

void runTimelineMenu(App& app) {
    while (true) {
        ui::header("Timeline");
        size_t stages = 0;
        for (const Character& c : app.db.characters) stages += stagesOf(c, app.db).size();
        ui::info(std::to_string(app.db.events.size()) + (app.db.events.size() == 1 ? " story event, " : " story events, ") +
                 std::to_string(stages) + (stages == 1 ? " character stage." : " character stages."));
        const int pick = ui::choose("", {"See the whole timeline", "One character's story", "Add a story event",
                                         "Change or delete a story event", "Everyone as they were on a date",
                                         "Export the timeline"},
                                    "Back to the main menu");
        if (pick == 0) {
            std::cout << renderText(buildTimelineSheet(app.db));
            ui::pause();
        } else if (pick == 1) {
            const int id = chooseCharacter(app, "Whose story?");
            if (!id) continue;
            const Character* c = app.db.findCharacter(id);
            std::cout << renderText(buildTimelineSheet(app.db, id));
            if (const auto stages = stagesOf(*c, app.db); !stages.empty()) {
                ui::info("Their stages:");
                for (const StageRef& ref : stages) ui::info("  " + stageSummary(ref.stage, app.db));
            }
            ui::pause();
        } else if (pick == 2) {
            StoryEvent e;
            e.id = app.db.nextEventId();
            runEventEditor(app, e, true);
        } else if (pick == 3) {
            const StoryEvent* e = chooseEvent(app, "Which event?");
            if (!e) continue;
            const int action = ui::choose(e->title, {"Change it", "Delete it"}, "Back");
            if (action == 0) {
                runEventEditor(app, *e, false);
            } else if (action == 1 && ui::yesNo("Delete " + e->title + " for good?", false)) {
                const std::string title = e->title;
                try {
                    deleteEvent(app, e->id);
                    ui::info("Deleted " + title + ".");
                } catch (const StorageError& error) {
                    ui::info(std::string("Deleting failed: ") + error.what());
                }
            }
        } else if (pick == 4) {
            if (app.db.characters.empty()) ui::info("No characters yet.");
            else everyoneAsOf(app);
        } else if (pick == 5) {
            const int which = ui::choose("Export which timeline?", {"Every event", "One character's events"}, "Cancel");
            if (which == 0) exportTimeline(app, 0);
            if (which == 1) {
                if (const int id = chooseCharacter(app, "Whose?")) exportTimeline(app, id);
            }
        } else {
            return;
        }
    }
}

void runWebMenu(App& app) {
    while (true) {
        ui::header("Relationship web");
        const Web web = buildWeb(app.db);
        ui::info(std::to_string(web.nodes.size()) + " characters and " + std::to_string(web.edges.size()) +
                 " connections between them. The app window's Web tab draws it as a picture.");
        const int pick = ui::choose("", {"Who one character knows", "How are two characters connected?",
                                         "Who is the most connected", "Export the web (a picture in HTML)"},
                                    "Back to the main menu");
        if (pick == 0) connectionsOf(app);
        else if (pick == 1) pathBetween(app);
        else if (pick == 2) mostConnected(app);
        else if (pick == 3) exportWeb(app);
        else return;
    }
}

}  // namespace lotm
