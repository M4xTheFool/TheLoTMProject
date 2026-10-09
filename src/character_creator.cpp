#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>

#include "app.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

const std::vector<std::string> kHair = {"Black", "Brown", "Blond", "Red", "Grey", "White", "Silver", "Bald"};
const std::vector<std::string> kEyes = {"Brown", "Blue", "Green", "Grey", "Hazel", "Black", "Amber", "Red"};
const std::vector<std::string> kBuild = {"Slim", "Average", "Athletic", "Muscular", "Stocky", "Heavy", "Frail"};
const std::vector<std::string> kHeight = {"Short", "Below average", "Average", "Tall", "Very tall"};
const std::vector<std::string> kClothing = {"Gentleman's suit and top hat", "Worker's clothes", "Church robes",
                                            "Sailor's coat",  "Noble finery",     "Detective's coat and hat",
                                            "Long dark cloak"};
const std::vector<std::string> kMarks = {"Scar", "Monocle", "Tattoo", "Burn mark", "Mismatched eyes",
                                         "Missing finger", "Walking cane"};
const std::vector<std::string> kAlignments = {"Lawful Good", "Neutral Good", "Chaotic Good",
                                              "Lawful Neutral", "True Neutral", "Chaotic Neutral",
                                              "Lawful Evil", "Neutral Evil", "Chaotic Evil"};
const std::vector<std::string> kOrganizations = {
    "Nighthawks (Church of the Evernight Goddess)",
    "Mandated Punishers (Church of the Lord of Storms)",
    "Machinery Hivemind (Church of the God of Steam and Machinery)",
    "Tarot Club",
    "Aurora Order",
    "Rose School of Thought",
    "Psychology Alchemists",
    "Moses Ascetic Order",
    "Twilight Hermit Order",
    "Independent"};
const std::vector<std::string> kRelationshipTypes = {"Ally", "Friend", "Rival", "Enemy",
                                                     "Mentor", "Student", "Family", "Partner"};

const Pathway* pathwayOf(const App& app, const Character& c) { return app.db.findPathway(c.pathwayId); }

// ---------------------------------------------------------------- steps 1-3

void stepName(Character& c) {
    ui::header("1. Name");
    c.name = ui::readRequired("Character name", c.name);
}

void stepLooks(Character& c) {
    ui::header("2. Looks and bio");
    ui::info("Pick from the lists or type your own. Enter keeps the current value, 0 skips.");
    c.appearance.hair = ui::choosePreset("Hair", kHair, c.appearance.hair);
    c.appearance.eyes = ui::choosePreset("Eyes", kEyes, c.appearance.eyes);
    c.appearance.build = ui::choosePreset("Build", kBuild, c.appearance.build);
    c.appearance.height = ui::choosePreset("Height", kHeight, c.appearance.height);
    c.appearance.clothing = ui::choosePreset("Clothing", kClothing, c.appearance.clothing);
    c.appearance.distinguishingMark = ui::choosePreset("Distinguishing mark", kMarks, c.appearance.distinguishingMark);
    c.appearance.description = ui::readMultiline("Describe their look in your own words (optional)", c.appearance.description);
    ui::blank();
    ui::info("About them (all optional):");
    c.age = ui::readText("Age", c.age);
    c.gender = ui::readText("Gender", c.gender);
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
        std::cout << "  " << s.sequence << ") " << s.name << "\n";
    }
    ui::info("9 is the weakest, 0 is a god.");
    c.sequence = ui::readInt("Sequence", 0, 9, c.sequence);
    const auto all = abilitiesUpTo(*p, c.sequence);
    size_t count = 0;
    for (const SequenceInfo* s : all) count += s->abilities.size();
    if (const SequenceInfo* s = p->findSequence(c.sequence)) {
        ui::info(s->name + " adds:");
        for (const auto& a : s->abilities) ui::info("  - " + a);
    }
    ui::info("The sheet lists " + std::to_string(count) + " abilities from Sequence 9 down to " +
             std::to_string(c.sequence) + ".");
}

// ---------------------------------------------------------------- step 5

void stepCustomization(Character& c) {
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
        ui::info("Honorific name: three lines that, when spoken, reach the character.");
        ui::info("Example shape: \"The ... who ...\" / \"The ... of ...\" / \"The ... that ...\"");
        if (ui::yesNo("Write or change the honorific name?", c.honorificName.empty())) {
            std::vector<std::string> lines = c.honorificName;
            lines.resize(3);
            for (int i = 0; i < 3; ++i) lines[i] = ui::readText("  Line " + std::to_string(i + 1), lines[i]);
            lines.erase(std::remove(lines.begin(), lines.end(), std::string()), lines.end());
            c.honorificName = lines;
        }
    } else {
        ui::info("Honorific name: available from Sequence 3 upward.");
    }

    ui::blank();
    c.hasUniqueness = ui::yesNo("Does this character hold a Uniqueness?", c.hasUniqueness);
    if (c.hasUniqueness) {
        c.uniquenessForm = ui::readMultiline("What shape does the Uniqueness take?", c.uniquenessForm);
    }
}

// ---------------------------------------------------------------- step 6

// Anyone can be named. If the name matches a saved character, the two are linked.
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
    if (const Character* other = app.db.findCharacter(r.characterId)) r.name = other->name;
    r.type = ui::choosePreset("Relationship to " + r.name, kRelationshipTypes, "");
    if (r.type.empty()) r.type = "Other";
    r.note = ui::readText("Note (optional)", "");
    c.relationships.push_back(r);
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
        std::string value = ui::readLine("  Type a name to add someone, a number to remove one, or Enter when done: ");
        if (value.empty()) return;
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
    c.affiliation.organization = ui::choosePreset("Organization", kOrganizations, c.affiliation.organization);
    if (!c.affiliation.organization.empty()) {
        c.affiliation.rank = ui::readText("Rank or role", c.affiliation.rank);
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

void stepNotes(Character& c) {
    ui::header("9. Notes");
    c.notes = ui::readMultiline("Anything else worth remembering (optional)", c.notes);
}

bool save(App& app, Character& c, bool isNew) {
    for (const auto& note : normalizeCharacter(c)) ui::info(note);
    const auto backup = app.db.characters;
    c.updatedAt = nowTimestamp();
    if (isNew) {
        c.createdAt = c.updatedAt;
        app.db.characters.push_back(c);
    } else if (Character* existing = app.db.findCharacter(c.id)) {
        *existing = c;
    }
    try {
        app.storage.saveCharacters(app.db);
    } catch (const StorageError& e) {
        app.db.characters = backup;
        ui::info(std::string("Saving failed: ") + e.what());
        return false;
    }
    ui::info("Saved " + c.name + " as " + characterCode(c.id) + ".");
    return true;
}

}  // namespace

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
        ui::info("Nine short steps. Enter keeps a value or skips an optional one; \"-\" clears text.");
        ui::info("Everything can be changed on the review screen at the end.");
        stepName(c);
        stepLooks(c);
        stepPathway(app, c, false);
        if (!c.pathwayId.empty()) stepSequence(app, c);
        stepCustomization(c);
        stepAffiliation(app, c);
        stepStats(app, c);
        stepArtifacts(app, c);
        stepNotes(c);
    }

    while (true) {
        ui::header(isNew ? "Review the new character" : "Edit character");
        std::cout << renderText(buildCharacterSheet(c, app.db));
        int pick = ui::choose("\nChange something, or save:",
                              {"Name", "Looks and bio", "Pathway", "Sequence",
                               "Customization (alignment, titles, aliases, honorific, Uniqueness)",
                               "Affiliation and relationships", "Stat block, HP and Spirituality",
                               "Sealed Artifacts", "Notes", "Save"},
                              "Cancel without saving");
        switch (pick) {
            case 0: stepName(c); break;
            case 1: stepLooks(c); break;
            case 2: stepPathway(app, c, true); break;
            case 3: stepSequence(app, c); break;
            case 4: stepCustomization(c); break;
            case 5: stepAffiliation(app, c); break;
            case 6: stepStats(app, c); break;
            case 7: stepArtifacts(app, c); break;
            case 8: stepNotes(c); break;
            case 9:
                if (save(app, c, isNew)) return c.id;
                break;
            default:
                if (ui::yesNo("Discard these changes?", false)) return std::nullopt;
        }
    }
}

}  // namespace lotm
