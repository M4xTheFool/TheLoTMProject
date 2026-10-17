#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>

#include "app.hpp"
#include "organizations.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "relations.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "story.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

const Pathway* pathwayOf(const App& app, const Character& c) { return app.db.findPathway(c.pathwayId); }

// ---------------------------------------------------------------- steps 1-3

void stepName(Character& c) {
    ui::header("1. Name");
    c.name = ui::readRequired("Character name", c.name);
}

// Alive or dead, as on the window's Identity tab: for someone dead by this, when and how they died.
void askLife(const App& app, Character& c) {
    Character before = c;  // without this setting: what their stages and story events say
    before.life.clear();
    const Death elsewhere = deathOf(before, app.db);
    if (elsewhere.dead) {
        ui::info(deathText(elsewhere, app.db) + (elsewhere.eventId != 0 ? ", as the story event says." : ", in one of their stages."));
        ui::info("Alive here means they came back.");
    }
    c.life = ui::choosePreset("Alive or dead", kLifeStates, c.life);
    if (elsewhere.dead && ui::toLower(c.life) == "alive") c.life = "Alive";
    if (isDead(c.life)) {
        c.diedOn = readStoryDate("When did they die? (e.g. 3 July 1360; Enter for not known)", c.diedOn);
        c.lifeNote = ui::readText("How they died (optional)", c.lifeNote);
    } else if (!c.life.empty()) {
        c.lifeNote = ui::readText("More about it (optional)", c.lifeNote);
    } else {
        c.lifeNote.clear();
    }
}

void stepLooks(const App& app, Character& c) {
    ui::header("2. Looks and bio");
    ui::info("Pick from the lists or type your own. Enter keeps the current value, 0 skips.");
    c.appearance.hair = ui::choosePreset("Hair colour", kHair, c.appearance.hair);
    c.appearance.hairLength = ui::choosePreset("Hair length", kHairLength, c.appearance.hairLength);
    c.appearance.hairStyle = ui::choosePreset("Hair style", kHairStyle, c.appearance.hairStyle);
    c.appearance.eyes = ui::choosePreset("Eye colour", kEyes, c.appearance.eyes);
    c.appearance.skin = ui::choosePreset("Skin", kSkin, c.appearance.skin);
    c.appearance.face = ui::choosePreset("Face", kFace, c.appearance.face);
    c.appearance.build = ui::choosePreset("Build", kBuild, c.appearance.build);
    c.appearance.height = ui::choosePreset("Height", kHeight, c.appearance.height);
    c.appearance.voice = ui::choosePreset("Voice", kVoice, c.appearance.voice);
    c.appearance.clothing = ui::choosePreset("Clothing", kClothing, c.appearance.clothing);
    c.appearance.distinguishingMark = ui::choosePreset("Distinguishing mark", kMarks, c.appearance.distinguishingMark);
    c.appearance.description = ui::readMultiline("Describe their look in your own words (optional)", c.appearance.description);
    ui::blank();
    ui::info("About them (all optional):");
    c.age = ui::readText("Age", c.age);
    c.gender = ui::readText("Gender", c.gender);
    askLife(app, c);
    c.shortDescription = ui::readMultiline("Short description", c.shortDescription);
    c.backstory = ui::readMultiline("Backstory", c.backstory);
}

void stepSequence(const App& app, Character& c);

void stepPathway(const App& app, Character& c, bool askSequence) {
    ui::header("3. Pathway");
    std::string current = c.pathwayId.empty() ? "none" : pathwayLabel(app.db, c.pathwayId);
    int pick = choosePathway(app, "Choose a pathway [current: " + current + "]", "No pathway (ordinary mortal)", true);
    if (pick == ui::Choice::kKeep) return;
    const std::string before = c.pathwayId;
    c.pathwayId = pick == ui::Choice::kZero ? "" : app.db.pathways[pick].id;
    if (const Pathway* p = pathwayOf(app, c)) {
        ui::info("Speed grade: " + p->speedGrade + ". Stat bonuses go to " + p->primaryStat + " and " +
                 p->secondaryStat + ".");
    }
    if (askSequence && !c.pathwayId.empty() && c.pathwayId != before) stepSequence(app, c);
}

void stepSequence(const App& app, Character& c) {
    ui::header("4. Sequence");
    const Pathway* p = pathwayOf(app, c);
    if (!p) {
        ui::info("Ordinary mortals have no Sequence. Choose a pathway first.");
        return;
    }
    for (const auto& s : p->sequences) {
        std::cout << "  " << s.sequence << ") " << (s.name.empty() ? "(no name yet)" : s.name) << "\n";
    }
    ui::info("9 is the weakest, 0 is a god. A god can go further: Above the Sequence.");
    c.sequence = ui::readInt("Sequence", 0, 9, c.sequence);
    if (c.sequence != 0) {
        c.aboveSequence = false;
    } else {
        ui::info("Above the Sequence: a Great Old One, who holds their group's Sefirah and every Uniqueness of its");
        ui::info("pathways (the Fool, Door and Error pathways for the Lord of the Mysteries).");
        c.aboveSequence = ui::yesNo("Is this god Above the Sequence?", c.aboveSequence);
        const Sefirah* own = app.db.sefirahForPathway(c.pathwayId);
        if (c.aboveSequence && c.sefirahId.empty() && own) {
            c.sefirahId = own->id;
            ui::info("They hold " + sefirahLabel(app.db, own->id) + " now; change it in Customization.");
        }
    }
    size_t count = 0;  // each Sequence from the pathway it came from
    for (int s = 9; s >= c.sequence; --s) {
        const Pathway* from = app.db.findPathway(pathwayIdAt(c, s));
        if (const SequenceInfo* info = from ? from->findSequence(s) : nullptr) count += info->abilities.size();
    }
    if (const SequenceInfo* s = p->findSequence(c.sequence)) {
        ui::info(sequenceLabel(p, c.sequence) + " adds:");
        for (const auto& a : s->abilities) ui::info("  - " + a);
        for (const auto& mode : movementUnlockedAt(*p, c.sequence)) ui::info("  - Movement: " + mode);
    }
    if (c.sequence == 1) ui::info("Sequence 1: set Beyonder characteristics and describe the Uniqueness in Customization.");
    if (c.sequence == 0) ui::info("Sequence 0: you can describe the absorbed Uniqueness in Customization.");
    if (dualPathwayEligible(c)) ui::info("A Sequence 0 god can also take a second pathway in Customization.");
    ui::info("The sheet lists " + std::to_string(count) + " abilities from Sequence 9 down to " +
             std::to_string(c.sequence) + (c.aboveSequence ? ", plus every ability of the other pathways in their group." : "."));
}

// ---------------------------------------------------------------- pathway switches and state of mind

// Sequence 9 up to theirs, with the pathway each came from, and every switch of pathway.
void printLadder(const App& app, const Character& c) {
    std::cout << "Their Sequences, weakest first:\n";
    for (int s = 9; s >= c.sequence; --s) {
        const std::string id = pathwayIdAt(c, s);
        const Pathway* p = app.db.findPathway(id);
        const SequenceInfo* info = p ? p->findSequence(s) : nullptr;
        std::string name = info && !info->name.empty() ? info->name : "(no name yet)";
        if (name.size() < 22) name += std::string(22 - name.size(), ' ');
        std::cout << "  " << s << ") " << name << (p ? p->name + " pathway" : id + " (pathway not found)")
                  << (s == c.sequence ? (c.aboveSequence ? "  <- then Above the Sequence" : "  <- now") : "") << "\n";
    }
    for (const PathwaySwitch& change : pathwaySwitches(c, app.db)) {
        ui::info("Switch at Sequence " + std::to_string(change.atSequence) + ": from " +
                 pathwayLabel(app.db, change.fromPathwayId) + " to " + pathwayLabel(app.db, change.toPathwayId) +
                 (change.neighbouring ? ", neighbouring pathways." : ", NOT neighbours: a risky switch."));
    }
}

void stepPathwaySwitches(const App& app, Character& c) {
    ui::header("Pathway switches and state of mind");
    if (c.pathwayId.empty()) {
        ui::info("Ordinary mortals have no pathway to switch.");
    } else {
        ui::info("A Beyonder can switch pathways on the way up: to a neighbouring pathway (the same Sefirah or group)");
        ui::info("safely, to any other at a cost to their sanity. Each Sequence keeps the abilities of the pathway it");
        ui::info("came from; stat bonuses and the speed grade follow the pathway they are on now.");
        while (true) {
            ui::blank();
            printLadder(app, c);
            const bool canSwitch = c.sequence > 0 && !c.aboveSequence;
            std::string prompt = "  ";
            if (canSwitch) prompt += "S switches to another pathway (on to Sequence " + std::to_string(c.sequence - 1) + "), ";
            if (c.sequence < 9) prompt += "9-" + std::to_string(c.sequence + 1) + " picks that Sequence's pathway, ";
            if (isMixedPathway(c)) prompt += "O makes them one pathway again, ";
            const std::string value = ui::readLine(prompt + "Enter when done: ");
            if (value.empty()) break;
            if ((value == "s" || value == "S") && canSwitch) {
                const int pick = choosePathway(app, "They drink the Sequence " + std::to_string(c.sequence - 1) +
                                                        " potion of which pathway?",
                                               "Cancel", false);
                if (pick < 0) continue;
                const std::string from = c.pathwayId;
                if (!switchPathway(c, app.db.pathways[pick].id)) {
                    ui::info("That is their pathway already.");
                    continue;
                }
                const SequenceInfo* now = app.db.pathways[pick].findSequence(c.sequence);
                ui::info("Now Sequence " + std::to_string(c.sequence) + (now && !now->name.empty() ? " " + now->name : "") +
                         " of the " + app.db.pathways[pick].name + " pathway.");
                if (!neighbouringPathways(app.db, from, c.pathwayId)) {
                    ui::info("Not a neighbour of the pathway before: a risky switch that tends to cost sanity.");
                }
                continue;
            }
            if ((value == "o" || value == "O") && isMixedPathway(c)) {
                c.mixedSequences.clear();
                continue;
            }
            int sequence = -1;
            try {
                size_t used = 0;
                sequence = std::stoi(value, &used);
                if (used != value.size()) sequence = -1;
            } catch (const std::exception&) {
            }
            if (sequence <= c.sequence || sequence > 9) {
                ui::info(c.sequence < 9 ? "Type S, a Sequence from 9 to " + std::to_string(c.sequence + 1) + ", or Enter."
                                        : "Type S or press Enter.");
                continue;
            }
            const std::string current = pathwayLabel(app.db, pathwayIdAt(c, sequence));
            const int pick = choosePathway(app, "Sequence " + std::to_string(sequence) + " came from which pathway? [current: " +
                                                    current + "]",
                                           "Their pathway now (" + pathwayLabel(app.db, c.pathwayId) + ")", true);
            if (pick == ui::Choice::kKeep) continue;
            auto& steps = c.mixedSequences;
            steps.erase(std::remove_if(steps.begin(), steps.end(),
                                       [&](const SequenceStep& step) { return step.sequence == sequence; }),
                        steps.end());
            if (pick >= 0 && app.db.pathways[pick].id != c.pathwayId) steps.push_back({sequence, app.db.pathways[pick].id});
            std::sort(steps.begin(), steps.end(),
                      [](const SequenceStep& a, const SequenceStep& b) { return a.sequence > b.sequence; });
        }
    }

    ui::blank();
    if (riskySwitch(c, app.db)) {
        ui::info("They switched to a pathway that isn't a neighbour, which tends to cost a Beyonder their sanity.");
    }
    c.stateOfMind = ui::choosePreset("State of mind", kStatesOfMind, c.stateOfMind);
    if (!c.stateOfMind.empty()) {
        c.stateOfMindNote = ui::readText("More about their state of mind (optional)", c.stateOfMindNote);
    } else {
        c.stateOfMindNote.clear();
    }
}

// ---------------------------------------------------------------- step 5

// Toggles which of the pathway's Sequence 0 abilities the Uniqueness grants; typed text adds one of your own.
void chooseUniquenessAbilities(const Pathway* p, std::vector<std::string>& held) {
    std::vector<std::string> options;
    if (const SequenceInfo* zero = p ? p->findSequence(0) : nullptr) {
        for (const auto& a : zero->abilities) {
            if (a.rfind("Edit me", 0) != 0) options.push_back(a);
        }
    }
    for (const auto& a : held) {
        if (std::find(options.begin(), options.end(), a) == options.end()) options.push_back(a);
    }
    while (true) {
        std::cout << "Sequence 0 abilities the Uniqueness grants:\n";
        if (options.empty()) std::cout << "  (the pathway lists none yet; type your own)\n";
        for (size_t i = 0; i < options.size(); ++i) {
            const bool on = std::find(held.begin(), held.end(), options[i]) != held.end();
            std::cout << "  [" << (on ? "x" : " ") << "] " << (i + 1) << ") " << options[i] << "\n";
        }
        std::string value = ui::readLine("  Type a number to give or take it, your own text to add one, or Enter when done: ");
        if (value.empty()) return;
        const bool isNumber =
            std::all_of(value.begin(), value.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
        if (!isNumber) {
            options.push_back(value);
            held.push_back(value);
            continue;
        }
        size_t index = value.size() <= 4 ? std::stoul(value) : 0;
        if (index < 1 || index > options.size()) {
            ui::info("There is no entry " + value + ".");
            continue;
        }
        auto it = std::find(held.begin(), held.end(), options[index - 1]);
        if (it == held.end()) held.push_back(options[index - 1]);
        else held.erase(it);
    }
}

// A Sequence 0 god can hold a second pathway's Sequence 1 characteristic(s) and its Uniqueness.
void stepSecondPathway(const App& app, Character& c) {
    if (!dualPathwayEligible(c)) {
        ui::info(c.aboveSequence ? "Above the Sequence they hold every Uniqueness of their group, so no second pathway."
                                 : "A second pathway (a dual-pathway god) is open to a Sequence 0 god.");
        return;
    }
    SecondPathway& second = c.secondPathway;
    ui::info("Dual-pathway god: a second pathway's Sequence 1 Beyonder characteristic, usually its Uniqueness, and every");
    ui::info("ability of that pathway.");
    ui::info("Amon, Sequence 0 Error, took the Door Uniqueness and a Key of Stars characteristic this way.");
    const std::string current = second.empty() ? "none" : pathwayLabel(app.db, second.pathwayId);
    const int pick = choosePathway(app, "Second pathway [current: " + current + "]", "None (one pathway)", true);
    if (pick == ui::Choice::kKeep && second.empty()) return;
    if (pick == ui::Choice::kZero) {
        second = SecondPathway{};
        return;
    }
    if (pick >= 0) {
        if (app.db.pathways[pick].id == c.pathwayId) {
            ui::info("That is their own pathway; the second one has to be another.");
            return;
        }
        second.pathwayId = app.db.pathways[pick].id;
    }
    const Pathway* other = app.db.findPathway(second.pathwayId);
    const auto group = app.db.pathwayGroup(c.pathwayId);
    if (std::find(group.begin(), group.end(), other) == group.end()) {
        ui::info("Note: canon dual-pathway gods combine neighbouring pathways; this one isn't a neighbour.");
    }
    second.beyonderCharacteristics = ui::readInt("Its Sequence 1 Beyonder characteristics they hold (1 or 2)", 1, 2,
                                                 second.beyonderCharacteristics);
    second.hasUniqueness = ui::yesNo("Do they hold that pathway's Uniqueness too?", second.hasUniqueness);
    if (second.hasUniqueness) {
        const std::string start = second.uniquenessForm.empty() && other ? other->uniqueness : second.uniquenessForm;
        second.uniquenessForm = ui::readMultiline("Describe that Uniqueness and the shape it takes (optional)", start);
    }
    if (other) ui::info("They have every ability of the " + other->name + " pathway, Sequence 9 to 0.");
}

void chooseSefirah(const App& app, Character& c) {
    if (app.db.sefirot.empty()) return;
    std::vector<std::string> names;
    for (const auto& s : app.db.sefirot) names.push_back(sefirahLabel(app.db, s.id));
    const std::string current = c.sefirahId.empty() ? "none" : sefirahLabel(app.db, c.sefirahId);
    const int pick = ui::choose("Sefirah they hold [current: " + current + "]", names, "None", true);
    if (pick == ui::Choice::kZero) c.sefirahId.clear();
    else if (pick >= 0) c.sefirahId = app.db.sefirot[pick].id;
}

// Sequence 1: one or two Beyonder characteristics and the Uniqueness; Sequence 0: the absorbed Uniqueness.
void stepUniqueness(const App& app, Character& c) {
    const Pathway* p = pathwayOf(app, c);
    if (sequenceOneChoices(c)) {
        c.beyonderCharacteristics =
            ui::readInt("Beyonder characteristics they hold (1 or 2)", 1, 2, c.beyonderCharacteristics);
        c.hasUniqueness = ui::yesNo("Does this character hold the pathway's Uniqueness?", c.hasUniqueness);
        if (c.hasUniqueness) {
            // A pathway that describes its Uniqueness gives the starting text; Enter keeps it.
            const std::string current = c.uniquenessForm.empty() && p ? p->uniqueness : c.uniquenessForm;
            c.uniquenessForm = ui::readMultiline("Describe the Uniqueness: what it looks like and what shape it takes",
                                                 current);
            ui::info("The Uniqueness grants a couple of the Sequence 0 powers. Pick them:");
            chooseUniquenessAbilities(p, c.uniquenessAbilities);
        }
    } else if (absorbedUniqueness(c)) {
        ui::info(c.aboveSequence ? "Above the Sequence they have absorbed every Uniqueness of their pathway group."
                                 : "At Sequence 0 the Uniqueness has been absorbed, so its powers are all theirs.");
        const std::string current = c.uniquenessForm.empty() && p ? p->uniqueness : c.uniquenessForm;
        c.uniquenessForm = ui::readMultiline("Describe the absorbed Uniqueness and how it shows itself now (optional)",
                                             current);
    } else {
        ui::info("Uniqueness (its look and the Sequence 0 powers it grants) and a second Beyonder characteristic:");
        ui::info("these are set at Sequence 1. Change the Sequence to 1 to describe them.");
    }
}

void stepCustomization(const App& app, Character& c) {
    ui::header("5. Customization");
    int pick = ui::choose("Alignment" + (c.alignment.empty() ? std::string() : " [current: " + c.alignment + "]"),
                          kAlignments, "Unaligned / skip", true);
    if (pick == ui::Choice::kZero) c.alignment.clear();
    else if (pick >= 0) c.alignment = kAlignments[pick];

    ui::blank();
    ui::editList("Titles", c.titles);
    ui::blank();
    ui::editList("Aliases", c.aliases);

    ui::blank();
    if (honorificEligible(c)) {
        ui::info("Honorific name: lines that, when spoken, reach the character. Usually three, but it can be longer.");
        ui::info("Example shape: \"The ... who ...\" / \"The ... of ...\" / \"The ... that ...\"");
        if (ui::yesNo("Write or change the honorific name?", c.honorificName.empty())) {
            std::vector<std::string> lines = c.honorificName;
            const int count = ui::readInt("How many lines", 1, 20, std::max<int>(3, static_cast<int>(lines.size())));
            lines.resize(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) lines[i] = ui::readText("  Line " + std::to_string(i + 1), lines[i]);
            lines.erase(std::remove(lines.begin(), lines.end(), std::string()), lines.end());
            c.honorificName = lines;
        }
    } else {
        ui::info("Honorific name: available from Sequence 3 upward.");
    }

    ui::blank();
    stepUniqueness(app, c);
    ui::blank();
    stepSecondPathway(app, c);
    ui::blank();
    chooseSefirah(app, c);
}

// ---------------------------------------------------------------- step 6

// The type and note of a new relationship, then it is added.
void finishRelationship(const App& app, Character& c, Relationship r) {
    if (const Character* other = app.db.findCharacter(r.characterId)) r.name = other->name;
    r.type = ui::choosePreset("Relationship to " + r.name, kRelationshipTypes, "");
    if (r.type.empty()) r.type = "Other";
    r.note = ui::readText("Note (optional)", "");
    c.relationships.push_back(r);
}

// Anyone can be named. If the name matches a saved character, the two are linked; failing that, a saved
// organization's name links it, and the relationship reaches every member.
void addRelationship(const App& app, Character& c, const std::string& typed) {
    Relationship r;
    r.name = typed;
    const auto matches = app.db.findCharactersByName(typed, c.id);
    if (matches.size() == 1 && ui::toLower(matches[0]->name) == ui::toLower(typed)) {
        r.characterId = matches[0]->id;
        ui::info("Linked to the saved character " + relationshipName(app.db, r) + ".");
    } else if (!matches.empty()) {
        std::vector<std::string> names;
        for (const Character* m : matches) names.push_back(m->name + " (" + characterCode(m->id) + ")");
        int pick = ui::choose("Is this one of your saved characters?", names, "No, just add \"" + typed + "\"");
        if (pick >= 0) r.characterId = matches[pick]->id;
    }
    if (r.characterId == 0) {
        if (const Organization* o = organizationNamed(app.db, typed)) {
            r.organizationId = o->id;
            r.name = o->name;
            ui::info("Linked to the organization " + relationshipName(app.db, r) + ": it reaches all its members.");
        }
    }
    finishRelationship(app, c, r);
}

// Picks one of the saved organizations: the relationship is with it as a whole, so with every member.
void pickOrganization(const App& app, Character& c) {
    const std::vector<const Organization*> offered = organizationsByName(app.db);
    if (offered.empty()) {
        ui::info("There are no saved organizations yet. Add them in the Organizations menu.");
        return;
    }
    std::vector<std::string> names;
    for (const Organization* o : offered) {
        const size_t members = membersOf(*o, app.db).size();
        names.push_back(o->name + " (" + organizationCode(o->id) + ", " + std::to_string(members) +
                        (members == 1 ? " saved member)" : " saved members)"));
    }
    const int pick = ui::choose("Which organization?", names, "Cancel");
    if (pick < 0) return;
    Relationship r;
    r.organizationId = offered[static_cast<size_t>(pick)]->id;
    r.name = offered[static_cast<size_t>(pick)]->name;
    ui::info("It reaches all its members, unless one has a relationship of their own with " +
             (c.name.empty() ? std::string("them") : c.name) + ".");
    finishRelationship(app, c, r);
}

// Picks one of the saved characters from a numbered list, without typing the name.
void pickSavedCharacter(const App& app, Character& c) {
    std::vector<const Character*> offered;
    std::vector<std::string> names;
    for (const auto& other : app.db.characters) {
        if (other.id == c.id) continue;
        offered.push_back(&other);
        const Pathway* p = app.db.findPathway(other.pathwayId);
        names.push_back(other.name + " (" + characterCode(other.id) + ", " +
                        (other.pathwayId.empty() ? std::string("mortal") : (p ? p->name + ", " : "") + levelName(other)) +
                        ")");
    }
    if (offered.empty()) {
        ui::info("There are no other saved characters yet. Type a name instead.");
        return;
    }
    const int pick = ui::choose("Which saved character?", names, "Cancel");
    if (pick < 0) return;
    Relationship r;
    r.characterId = offered[static_cast<size_t>(pick)]->id;
    finishRelationship(app, c, r);
}

void editRelationships(const App& app, Character& c) {
    while (true) {
        std::cout << "Relationships:\n";
        if (c.relationships.empty()) std::cout << "  (none yet)\n";
        for (size_t i = 0; i < c.relationships.size(); ++i) {
            const auto& r = c.relationships[i];
            std::cout << "  " << (i + 1) << ") " << r.type << ": " << relationshipName(app.db, r);
            if (!r.note.empty()) std::cout << ", " << r.note;
            std::cout << "\n";
        }
        std::string value = ui::readLine(
            "  P picks one of your saved characters, O an organization, a typed name adds anyone, a number removes one,\n"
            "  Enter when done: ");
        if (value.empty()) return;
        if (value == "p" || value == "P") {
            pickSavedCharacter(app, c);
            continue;
        }
        if (value == "o" || value == "O") {
            pickOrganization(app, c);
            continue;
        }
        const bool isNumber =
            std::all_of(value.begin(), value.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
        if (!isNumber) {
            addRelationship(app, c, value);
            continue;
        }
        size_t index = value.size() <= 4 ? std::stoul(value) : 0;
        if (index >= 1 && index <= c.relationships.size()) {
            const Relationship& r = c.relationships[index - 1];
            ui::info("Removed " + r.type + ": " + relationshipName(app.db, r) + ".");
            c.relationships.erase(c.relationships.begin() + static_cast<std::ptrdiff_t>(index - 1));
            continue;
        }
        ui::info("There is no entry " + value + ".");
    }
}

void stepAffiliation(const App& app, Character& c) {
    ui::header("6. Affiliation and relationships");
    c.affiliation.organization = ui::choosePreset("Organization", organizationSuggestions(app.db), c.affiliation.organization);
    if (!c.affiliation.organization.empty()) {
        const auto ranks = rankSuggestions(app.db, c.affiliation.organization);
        c.affiliation.rank = ranks.empty() ? ui::readText("Rank or role", c.affiliation.rank)
                                           : ui::choosePreset("Rank or role", ranks, c.affiliation.rank);
    } else {
        c.affiliation.rank.clear();
    }
    ui::blank();
    editRelationships(app, c);
}

// ---------------------------------------------------------------- step 7

void printStatPreview(const App& app, const Character& c) {
    const Pathway* p = pathwayOf(app, c);
    const auto bonus = statBonuses(c, p);
    const auto totals = statTotals(c, p);
    for (int i = 0; i < kStatCount; ++i) {
        std::cout << "  " << kStatCodes[i] << " " << c.stats.base[i];
        if (bonus[i] != 0) std::cout << " " << signedNumber(bonus[i]) << " = " << totals[i];
        std::cout << "  (" << signedNumber(abilityModifier(totals[i])) << ")\n";
    }
    const SpeedTier tier = speedTierOf(c);
    std::cout << "  Speed: tier " << tier.tier << " " << tier.name << ", score " << effectiveSpeed(c, p) << "\n";
}

// Asks for one value: R rolls 4d6 (drop the lowest) as often as you like, a number types it in.
int rollOrType(App& app, const std::string& name, int current) {
    while (true) {
        std::string value =
            ui::readLine(name + ": R to roll, or type a value (Enter keeps " + std::to_string(current) + "): ");
        if (value.empty()) return current;
        if (value == "r" || value == "R") {
            while (true) {
                FourD6Roll roll = app.dice.roll4d6DropLowest();
                ui::info("Rolled " + roll.describe());
                std::string answer = ui::readLine("  Enter to accept, R to roll again, or type a value: ");
                if (answer.empty() || answer == "y" || answer == "Y") return roll.total;
                if (answer == "r" || answer == "R") continue;
                try {
                    int typed = std::stoi(answer);
                    if (typed >= 1 && typed <= 99) return typed;
                } catch (const std::exception&) {
                }
                ui::info("Keeping the roll is Enter, rolling again is R.");
            }
        }
        try {
            size_t used = 0;
            int typed = std::stoi(value, &used);
            if (used == value.size() && typed >= 1 && typed <= 99) return typed;
        } catch (const std::exception&) {
        }
        ui::info("Type R or a number from 1 to 99.");
    }
}

void rollAllSix(App& app, Character& c) {
    std::array<FourD6Roll, kStatCount> rolls;
    for (auto& r : rolls) r = app.dice.roll4d6DropLowest();
    while (true) {
        for (int i = 0; i < kStatCount; ++i) {
            std::cout << "  " << (i + 1) << ") " << kStatCodes[i] << ": " << rolls[i].describe() << "\n";
        }
        std::string value = ui::readLine("Enter to accept, R to re-roll all, or 1-6 to re-roll one stat: ");
        if (value.empty()) break;
        if (value == "r" || value == "R") {
            for (auto& r : rolls) r = app.dice.roll4d6DropLowest();
            continue;
        }
        if (value.size() == 1 && value[0] >= '1' && value[0] <= '6') {
            rolls[value[0] - '1'] = app.dice.roll4d6DropLowest();
            continue;
        }
        ui::info("Type Enter, R, or a number from 1 to 6.");
    }
    for (int i = 0; i < kStatCount; ++i) c.stats.base[i] = rolls[i].total;
}

void pointBuy(Character& c) {
    ui::info("Every stat starts at 8. Costs: 8=0 9=1 10=2 11=3 12=4 13=5 14=7 15=9. Budget: 27 points.");
    std::array<int, kStatCount> scores{8, 8, 8, 8, 8, 8};
    int spent = 0;
    for (int i = 0; i < kStatCount; ++i) {
        while (true) {
            ui::info(std::to_string(kPointBuyBudget - spent) + " points left.");
            int value = ui::readInt(kStatNames[i], 8, 15, 8);
            int cost = pointBuyCost(value);
            if (spent + cost <= kPointBuyBudget) {
                scores[i] = value;
                spent += cost;
                break;
            }
            ui::info(std::to_string(value) + " costs " + std::to_string(cost) + " points, which is too many.");
        }
    }
    if (spent < kPointBuyBudget) ui::info(std::to_string(kPointBuyBudget - spent) + " points were left unspent.");
    c.stats.base = scores;
}

void stepStats(App& app, Character& c) {
    ui::header("7. Stat block");
    printStatPreview(app, c);
    int method = ui::choose("\nHow do you want to set the six stats?",
                            {"Stat by stat: roll or type each one", "Roll all six at once", "Point buy (27 points)"},
                            "Keep the current stats");
    if (method == 0) {
        for (int i = 0; i < kStatCount; ++i) c.stats.base[i] = rollOrType(app, kStatNames[i], c.stats.base[i]);
    } else if (method == 1) {
        rollAllSix(app, c);
    } else if (method == 2) {
        pointBuy(c);
    }

    ui::blank();
    ui::info("Speed is its own stat: the Sequence sets the tier, this score ranks you inside it.");
    c.stats.speedBase = rollOrType(app, "Speed", c.stats.speedBase);

    const Pathway* p = pathwayOf(app, c);
    const std::string& mode = app.db.settings.hpMode;
    if (mode == "always") {
        c.stats.hpIncluded = true;
    } else if (mode == "never") {
        c.stats.hpIncluded = false;
    } else {
        ui::blank();
        c.stats.hpIncluded = ui::yesNo("Include HP for this character?", c.stats.hpIncluded);
    }
    if (c.stats.hpIncluded) {
        const int suggested = suggestedHp(c, p);
        ui::info("Suggested HP for this tier and Constitution: " + std::to_string(suggested));
        c.stats.hp = ui::readInt("HP", 1, 999999, c.stats.hp > 0 ? c.stats.hp : suggested);
    }

    const int suggestedSp = suggestedSpirituality(c, p);
    ui::info("Suggested Spirituality for this tier and Wisdom: " + std::to_string(suggestedSp));
    c.stats.spirituality =
        ui::readInt("Spirituality", 0, 999999, c.stats.spirituality > 0 ? c.stats.spirituality : suggestedSp);

    ui::blank();
    ui::info("Result:");
    printStatPreview(app, c);
}

// ---------------------------------------------------------------- steps 8-9

void stepArtifacts(App& app, Character& c) {
    ui::header("8. Sealed Artifacts");
    while (true) {
        if (app.db.artifacts.empty()) ui::info("No Sealed Artifacts exist yet.");
        for (size_t i = 0; i < app.db.artifacts.size(); ++i) {
            const Artifact& a = app.db.artifacts[i];
            const bool held = std::find(c.artifactIds.begin(), c.artifactIds.end(), a.id) != c.artifactIds.end();
            std::cout << "  [" << (held ? "x" : " ") << "] " << (i + 1) << ") " << a.name << " ("
                      << artifactCode(a.id) << ")\n";
        }
        std::string value = ui::readLine("Type a number to give or take it, N to create a new one, Enter when done: ");
        if (value.empty()) return;
        if (value == "n" || value == "N") {
            if (auto id = runArtifactCreator(app)) c.artifactIds.push_back(*id);
            ui::header("8. Sealed Artifacts");
            continue;
        }
        try {
            int index = std::stoi(value);
            if (index >= 1 && index <= static_cast<int>(app.db.artifacts.size())) {
                const int id = app.db.artifacts[index - 1].id;
                auto it = std::find(c.artifactIds.begin(), c.artifactIds.end(), id);
                if (it == c.artifactIds.end()) c.artifactIds.push_back(id);
                else c.artifactIds.erase(it);
                continue;
            }
        } catch (const std::exception&) {
        }
        ui::info("Type a number from the list, N, or press Enter.");
    }
}

// ---------------------------------------------------------------- step 9

void stepDossier(const App& app, Character& c) {
    ui::header("9. Dossier");
    ui::info("An official file on the character, as the Nighthawks or another agency would keep it. All optional.");
    Dossier& d = c.dossier;
    d.filedBy = ui::choosePreset("Filed by", keeperSuggestions(app.db), d.filedBy);

    std::string current = d.recentActions.empty() ? "" : " [current: " + d.recentActions + "]";
    int pick = ui::choose("Recent actions" + current, kRecentActions, "Nothing on file", true);
    if (pick == ui::Choice::kZero) d.recentActions.clear();
    else if (pick >= 0) d.recentActions = kRecentActions[pick];
    d.recentActionsNote = ui::readText("What did they do (optional)", d.recentActionsNote);

    const ThreatAssessment threat = assessThreat(c);
    ui::blank();
    ui::info("Threat level from the rules: " + threat.level + " (" + threat.explanation + ").");
    ui::info("Automatic keeps it in step when the Sequence, recent actions or Sealed Artifacts change.");
    current = d.threatLevel.empty() ? " [current: automatic]" : " [current: " + d.threatLevel + ", set by hand]";
    pick = ui::choose("Threat level" + current, kThreatLevels, "Automatic", true);
    if (pick == ui::Choice::kZero) d.threatLevel.clear();
    else if (pick >= 0) d.threatLevel = kThreatLevels[pick];

    d.status = ui::choosePreset("Status", kStatuses, d.status);
    d.lastSeen = ui::readText("Last seen", d.lastSeen);
    d.remarks = ui::readMultiline("Remarks", d.remarks);
}

void stepNotes(Character& c) {
    ui::header("10. Notes");
    c.notes = ui::readMultiline("Anything else worth remembering (optional)", c.notes);
}

bool save(App& app, Character& c, bool isNew) {
    try {
        const CharacterSaveReport report = saveCharacter(app, c, isNew);
        for (const auto& note : report.cleanups) ui::info(note);
        ui::info("Saved " + c.name + " as " + characterCode(c.id) + ".");
        for (const auto& change : report.linked) ui::info(change);
        return true;
    } catch (const StorageError& e) {
        ui::info(std::string("Saving failed: ") + e.what());
        return false;
    }
}

}  // namespace

void editSequenceOne(const App& app, Character& c) { stepUniqueness(app, c); }
void editSecondPathway(const App& app, Character& c) { stepSecondPathway(app, c); }

std::optional<int> runCharacterCreator(App& app, std::optional<int> editId) {
    Character c;
    bool isNew = true;
    if (editId) {
        if (const Character* existing = app.db.findCharacter(*editId)) {
            c = *existing;
            isNew = false;
        }
    }

    if (isNew) {
        c.id = app.db.nextCharacterId();
        ui::header("Create a character");
        ui::info("Ten short steps. Enter keeps a value or skips an optional one; \"-\" clears text.");
        ui::info("Everything can be changed on the review screen at the end.");
        stepName(c);
        stepLooks(app, c);
        stepPathway(app, c, false);
        if (!c.pathwayId.empty()) {
            stepSequence(app, c);
            if (ui::yesNo("Did they switch pathways on the way up (a mixed-pathway Beyonder)?", false)) {
                stepPathwaySwitches(app, c);
            }
        }
        stepCustomization(app, c);
        stepAffiliation(app, c);
        stepStats(app, c);
        stepArtifacts(app, c);
        stepDossier(app, c);
        stepNotes(c);
    }

    while (true) {
        ui::header(isNew ? "Review the new character" : "Edit character");
        std::cout << renderText(buildCharacterSheet(c, app.db));
        int pick = ui::choose("\nChange something, or save:",
                              {"Name", "Looks and bio (and alive or dead)", "Pathway", "Sequence",
                               "Pathway switches and state of mind (a mixed-pathway Beyonder)",
                               "Customization (alignment, titles, aliases, honorific, Uniqueness, second pathway, Sefirah)",
                               "Affiliation and relationships", "Stat block, HP and Spirituality",
                               "Sealed Artifacts", "Dossier (threat level, status, remarks)", "Notes",
                               "Stages (how they were earlier in the story)", "Save"},
                              "Cancel without saving");
        switch (pick) {
            case 0: stepName(c); break;
            case 1: stepLooks(app, c); break;
            case 2: stepPathway(app, c, true); break;
            case 3: stepSequence(app, c); break;
            case 4: stepPathwaySwitches(app, c); break;
            case 5: stepCustomization(app, c); break;
            case 6: stepAffiliation(app, c); break;
            case 7: stepStats(app, c); break;
            case 8: stepArtifacts(app, c); break;
            case 9: stepDossier(app, c); break;
            case 10: stepNotes(c); break;
            case 11: editStages(app, c); break;
            case 12:
                if (save(app, c, isNew)) return c.id;
                break;
            default:
                if (ui::yesNo("Discard these changes?", false)) return std::nullopt;
        }
    }
}

}  // namespace lotm
