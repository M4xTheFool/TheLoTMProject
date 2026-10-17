// Small self-contained checks for the rules, the save format and the exports.
// Run with: ctest --test-dir build -C Release   (or run build/lotm_tests directly)
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "app.hpp"
#include "compare.hpp"
#include "dice.hpp"
#include "exporter.hpp"
#include "list_filter.hpp"
#include "model_json.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "render.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "web.hpp"

namespace fs = std::filesystem;
using namespace lotm;

static int failures = 0;

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": check failed: " #condition "\n"; \
            ++failures;                                                                    \
        }                                                                                  \
    } while (false)

static Pathway warrior() {
    Pathway p;
    p.id = "warrior";
    p.name = "Warrior";
    p.god = "Twilight Giant";
    p.primaryStat = "STR";
    p.secondaryStat = "CON";
    p.speedGrade = "Fast";
    p.movement = {{6, "Leaping"}, {2, "Flight"}};
    for (int s = 9; s >= 0; --s) p.sequences.push_back({s, "Seq" + std::to_string(s), {"Power " + std::to_string(s)}});
    return p;
}

static void testModifiers() {
    CHECK(abilityModifier(10) == 0);
    CHECK(abilityModifier(11) == 0);
    CHECK(abilityModifier(12) == 1);
    CHECK(abilityModifier(9) == -1);
    CHECK(abilityModifier(8) == -1);
    CHECK(abilityModifier(1) == -5);
    CHECK(abilityModifier(30) == 10);
    CHECK(signedNumber(2) == "+2");
    CHECK(signedNumber(0) == "0");
    CHECK(signedNumber(-3) == "-3");
}

static void testTiers() {
    CHECK(tierForSequence(9) == 1);
    CHECK(tierForSequence(8) == 1);
    CHECK(tierForSequence(7) == 2);
    CHECK(tierForSequence(5) == 2);
    CHECK(tierForSequence(4) == 3);
    CHECK(tierForSequence(3) == 3);
    CHECK(tierForSequence(2) == 4);
    CHECK(tierForSequence(1) == 4);
    CHECK(tierForSequence(0) == 5);
    Character mortal;
    CHECK(speedTierOf(mortal).tier == 0);
}

static void testPathwayBonuses() {
    const Pathway p = warrior();
    Character c;
    c.pathwayId = "warrior";

    // The plan's examples: Sequence 5 Warrior gets STR +2, CON +1; Sequence 1 gets STR +4, CON +2.
    c.sequence = 5;
    auto bonus = statBonuses(c, &p);
    CHECK(bonus[0] == 2);
    CHECK(bonus[2] == 1);
    CHECK(bonus[1] == 0);
    c.sequence = 1;
    bonus = statBonuses(c, &p);
    CHECK(bonus[0] == 4);
    CHECK(bonus[2] == 2);

    c.stats.base[0] = 14;
    CHECK(statTotals(c, &p)[0] == 18);

    Character mortal;
    CHECK(statBonuses(mortal, &p)[0] == 0);
}

static void testSpeed() {
    const Pathway p = warrior();
    Character c;
    c.pathwayId = "warrior";
    c.sequence = 7;
    c.stats.speedBase = 12;
    CHECK(effectiveSpeed(c, &p) == 14);
    CHECK(speedGradeModifier("Slow") == -2);
    CHECK(speedGradeModifier("Very fast") == 4);
    CHECK(speedGradeModifier("anything else") == 0);
    c.stats.speedBase = 1;
    Pathway slow = p;
    slow.speedGrade = "Slow";
    CHECK(effectiveSpeed(c, &slow) == 1);  // never below 1

    c.sequence = 7;
    CHECK(movementModes(c, &p).empty());
    c.sequence = 6;
    CHECK(movementModes(c, &p).size() == 1);
    c.sequence = 1;
    CHECK(movementModes(c, &p).size() == 2);
    CHECK(movementUnlockedAt(p, 6).size() == 1 && movementUnlockedAt(p, 6)[0] == "Leaping");
    CHECK(movementUnlockedAt(p, 5).empty());
}

static void testThreatLevel() {
    Character c;
    ThreatAssessment t = assessThreat(c);  // mortal, nothing on file
    CHECK(t.total == 0 && t.level == "Low" && t.explanation == "Mortal (0) = 0");

    c.pathwayId = "warrior";
    c.sequence = 9;
    CHECK(assessThreat(c).level == "Low");

    c.sequence = 7;
    c.dossier.recentActions = "Violent incidents";
    c.artifactIds = {1, 2};
    t = assessThreat(c);
    CHECK(t.total == 5 && t.level == "High");
    CHECK(t.explanation == "Sequence 7 (3) + violent incidents (+1) + 2 Sealed Artifacts (+1) = 5");

    c.artifactIds = {1, 2, 3, 4};
    CHECK(assessThreat(c).total == 6);  // three or more artifacts count +2

    c.sequence = 0;
    c.artifactIds.clear();
    c.dossier.recentActions = "Peaceful, no known incidents";
    t = assessThreat(c);
    CHECK(t.total == 9 && t.level == "Catastrophic");
    CHECK(t.explanation == "Sequence 0 (10) + peaceful (-1) = 9");

    c.sequence = 4;
    c.dossier.recentActions = "Mass casualties or disaster";
    CHECK(assessThreat(c).level == "Extreme");  // 5 + 2

    // A level set by hand wins; empty means automatic.
    CHECK(threatLevelOf(c) == "Extreme");
    c.dossier.threatLevel = "Low";
    CHECK(threatLevelOf(c) == "Low");
}

static void testTwoWayRelationships() {
    CHECK(reciprocalType("Mentor") == "Student");
    CHECK(reciprocalType("student") == "Mentor");
    CHECK(reciprocalType("Parent") == "Child");
    CHECK(reciprocalType("Superior") == "Subordinate");
    CHECK(reciprocalType("Close friend") == "Close friend");

    Database db;
    Character fors;
    fors.id = 1;
    fors.name = "Fors Wall";
    Character audrey;
    audrey.id = 2;
    audrey.name = "Audrey Hall";
    audrey.relationships = {{0, "klein moretti", "Friend", ""}};  // listed by name before Klein existed
    db.characters = {fors, audrey};

    // A new character linking to Fors as Mentor: Fors gets Student back, and Audrey's name-only entry links up.
    Character klein;
    klein.id = 3;
    klein.name = "Klein Moretti";
    klein.relationships = {{1, "Fors Wall", "Mentor", "Taught her divination"}};
    auto changes = syncRelationships(db, klein, nullptr);
    const Character* f = db.findCharacter(1);
    CHECK(f->relationships.size() == 1 && f->relationships[0].characterId == 3 && f->relationships[0].type == "Student");
    const Character* a = db.findCharacter(2);
    CHECK(a->relationships[0].characterId == 3);
    CHECK(klein.relationships.size() == 2 && klein.relationships[1].characterId == 2 &&
          klein.relationships[1].type == "Friend");
    CHECK(changes.size() == 2);
    db.characters.push_back(klein);

    // Saving again changes nothing.
    Character again = klein;
    CHECK(syncRelationships(db, again, &klein).empty());

    // Removing the Mentor link removes Fors's Student link too; Audrey's stays.
    Character edited = klein;
    edited.relationships.erase(edited.relationships.begin());
    changes = syncRelationships(db, edited, &klein);
    CHECK(changes.size() == 1);
    CHECK(db.findCharacter(1)->relationships.empty());
    CHECK(db.findCharacter(2)->relationships.size() == 1);
}

static void testAbilitiesAndEligibility() {
    const Pathway p = warrior();
    auto list = abilitiesUpTo(p, 7);
    CHECK(list.size() == 3);
    CHECK(list.front()->sequence == 9);
    CHECK(list.back()->sequence == 7);

    Character c;
    c.pathwayId = "warrior";
    c.sequence = 4;
    CHECK(!honorificEligible(c));
    c.sequence = 3;
    CHECK(honorificEligible(c));

    c.sequence = 5;
    c.honorificName = {"a", "b", "c"};
    c.uniquenessForm = "something";
    c.uniquenessAbilities = {"Fooling: deceives reality"};
    c.hasUniqueness = true;
    c.beyonderCharacteristics = 2;
    auto notes = normalizeCharacter(c);
    CHECK(c.honorificName.empty());
    CHECK(!c.hasUniqueness && c.uniquenessForm.empty() && c.uniquenessAbilities.empty());
    CHECK(c.beyonderCharacteristics == 1);
    CHECK(notes.size() == 3);

    // Only Sequence 1 chooses Beyonder characteristics and a Uniqueness; Sequence 0 has absorbed it.
    c.sequence = 1;
    CHECK(sequenceOneChoices(c) && !absorbedUniqueness(c));
    c.sequence = 0;
    CHECK(!sequenceOneChoices(c) && absorbedUniqueness(c));
    c.sequence = 1;
    c.hasUniqueness = true;
    c.beyonderCharacteristics = 2;
    c.uniquenessAbilities = {"Fooling: deceives reality"};
    CHECK(normalizeCharacter(c).empty());
    CHECK(c.hasUniqueness && c.beyonderCharacteristics == 2 && c.uniquenessAbilities.size() == 1);
    c.hasUniqueness = false;  // turning it off drops its details
    normalizeCharacter(c);
    CHECK(c.uniquenessAbilities.empty());

    // Moving up to Sequence 0 keeps the description of the absorbed Uniqueness, quietly.
    c.hasUniqueness = true;
    c.beyonderCharacteristics = 1;
    c.uniquenessForm = "A cloak of shifting silk";
    c.uniquenessAbilities = {"Fooling: deceives reality"};
    c.sequence = 0;
    CHECK(normalizeCharacter(c).empty());
    CHECK(!c.hasUniqueness && c.uniquenessForm == "A cloak of shifting silk" && c.uniquenessAbilities.empty());
    c.sequence = 4;  // dropping below Sequence 1 loses it
    normalizeCharacter(c);
    CHECK(c.uniquenessForm.empty());
    Character mortal;
    mortal.sequence = 1;  // no pathway: no Sequence 1 choices
    CHECK(!sequenceOneChoices(mortal));
}

// A small world for the god-level rules: the Fool / Door / Error group with its Sefirah, plus a
// pathway of your own that no Sefirah covers.
static Database godDatabase() {
    Database db;
    auto make = [](const std::string& id, const std::string& name, const std::string& god, const std::string& group,
                   const std::string& primary, const std::string& secondary) {
        Pathway p;
        p.id = id;
        p.name = name;
        p.god = god;
        p.group = group;
        p.primaryStat = primary;
        p.secondaryStat = secondary;
        for (int s = 9; s >= 0; --s) {
            p.sequences.push_back({s, name + " " + std::to_string(s), {name + " power " + std::to_string(s) + ": does things"}});
        }
        return p;
    };
    db.pathways = {make("seer", "Seer", "The Fool", "Fool / Door / Error", "WIS", "DEX"),
                   make("apprentice", "Apprentice", "Door", "Fool / Door / Error", "INT", "DEX"),
                   make("marauder", "Marauder", "Error", "Fool / Door / Error", "DEX", "INT"),
                   make("acrobat", "Acrobat", "The Maestro", "Maestro", "CHA", "DEX")};
    db.pathways[1].movement = {{1, "Star travel"}};
    Sefirah castle;
    castle.id = "sefirah_castle";
    castle.name = "Sefirah Castle";
    castle.greatOldOne = "Lord of the Mysteries";
    castle.pathwayIds = {"seer", "apprentice", "marauder"};
    castle.knownHolders = {"Klein Moretti"};
    db.sefirot = {castle};
    return db;
}

static void testAboveTheSequence() {
    Database db = godDatabase();
    const Pathway* seer = db.findPathway("seer");
    Character c;
    c.id = 1;
    c.name = "Klein Moretti";
    c.pathwayId = "seer";
    c.sequence = 0;
    c.aboveSequence = true;
    c.sefirahId = "sefirah_castle";

    // A tier of its own: bigger pathway bonuses, threat, HP and Spirituality than Sequence 0.
    CHECK(aboveTheSequence(c) && speedTierOf(c).tier == kAboveSequenceTier && speedTierOf(c).name == "Above the Sequence");
    CHECK(statBonuses(c, seer)[4] == 6 && statBonuses(c, seer)[1] == 3);  // WIS +6, DEX +3
    CHECK(assessThreat(c).total == 12 && assessThreat(c).level == "Catastrophic");
    CHECK(assessThreat(c).explanation == "Above the Sequence (12) = 12");
    CHECK(levelName(c) == "Above the Sequence");
    Character god = c;
    god.aboveSequence = false;
    CHECK(suggestedHp(c, seer) > suggestedHp(god, seer));
    CHECK(suggestedSpirituality(c, seer) > suggestedSpirituality(god, seer));

    // Above the Sequence includes Sequence 0, and a mortal can't be there.
    c.sequence = 4;
    CHECK(normalizeCharacter(c).empty() && c.sequence == 0 && c.aboveSequence);
    Character mortal;
    mortal.aboveSequence = true;
    normalizeCharacter(mortal);
    CHECK(!mortal.aboveSequence && !aboveTheSequence(mortal));

    // The whole group is theirs: their own Sequences written out, then one line per other pathway.
    const auto groups = abilityGroups(c, db);
    CHECK(groups.size() == 12);
    CHECK(groups[10].heading == "Apprentice (Door)" && groups[10].abilities.empty());
    CHECK(groups[10].summary == "All abilities of the Apprentice pathway (Door), Sequence 9 to 0");
    CHECK(groups[11].heading == "Marauder (Error)" && groups[11].wholePathwayCount == 10);
    CHECK(abilityCount(c, db) == 30);
    CHECK(db.pathwayGroup("seer").size() == 3 && db.pathwayGroup("seer")[0] == seer);
    CHECK(db.pathwayGroup("acrobat").size() == 1);  // no Sefirah, no neighbours: only itself
    CHECK(db.sefirahForPathway("marauder") == &db.sefirot[0] && db.sefirahForPathway("acrobat") == nullptr);

    const std::string text = renderText(buildCharacterSheet(c, db));
    CHECK(text.find("Above the Sequence: Lord of the Mysteries, Seer pathway (The Fool)") != std::string::npos);
    CHECK(text.find("Uniquenesses held: Seer, Apprentice and Marauder pathways") != std::string::npos);
    CHECK(text.find("Sefirah: Sefirah Castle (Lord of the Mysteries)") != std::string::npos);
    CHECK(text.find("Their group's Sefirah") == std::string::npos);  // they hold it
    CHECK(text.find("Tier 6 Above the Sequence") != std::string::npos);
    c.sefirahId.clear();
    CHECK(renderText(buildCharacterSheet(c, db)).find("Their group's Sefirah: Sefirah Castle") != std::string::npos);

    // Compare ranks it above a Sequence 0 god.
    db.characters = {c, god};
    db.characters[1].id = 2;
    db.characters[1].name = "Another god";
    const Comparison result = buildComparison({&db.characters[0], &db.characters[1]}, db);
    const CompareRow& sequence = result.sections[0].rows[1];
    CHECK(sequence.values[0] == "Above the Sequence" && sequence.highest == std::vector<bool>({true, false}));

    // Above the Sequence is one more step, written on the Sefirah: its symbolism and the Great Old
    // One's abilities come right after Sequence 0.
    db.sefirot[0].symbolism = "Mysteries and the gray fog";
    db.sefirot[0].abilities = {"Lord of the Mysteries: Rules every mystery.", "Gray Fog: Hides anything."};
    c.sefirahId = "sefirah_castle";
    CHECK(greatOldOneSefirah(c, db) == &db.sefirot[0]);
    const auto withGoo = abilityGroups(c, db);
    CHECK(withGoo.size() == 13);
    CHECK(withGoo[10].heading == "Above the Sequence: Lord of the Mysteries");
    CHECK(withGoo[10].symbolism == "Mysteries and the gray fog" && withGoo[10].abilities.size() == 2);
    CHECK(withGoo[11].heading == "Apprentice (Door)");
    CHECK(abilityCount(c, db) == 32);
    const std::string gooText = renderText(buildCharacterSheet(c, db));
    CHECK(gooText.find("Symbolism: Mysteries and the gray fog") != std::string::npos);
    CHECK(gooText.find("Gray Fog: Hides anything.") != std::string::npos);
    // A Sefirah of their own that gathers their pathway wins over the group's; one that doesn't, doesn't.
    Sefirah own;
    own.id = "theatre";
    own.name = "Grand Theatre";
    own.greatOldOne = "The Ringmaster";
    own.pathwayIds = {"seer"};
    own.abilities = {"Curtain Call: Ends any act."};
    own.custom = true;
    db.sefirot.push_back(own);
    c.sefirahId = "theatre";
    CHECK(greatOldOneSefirah(c, db)->id == "theatre");
    CHECK(abilityGroups(c, db)[10].heading == "Above the Sequence: The Ringmaster");
    db.sefirot.back().pathwayIds = {"acrobat"};
    CHECK(greatOldOneSefirah(c, db)->id == "sefirah_castle");
    // A god at Sequence 0 doesn't get it.
    CHECK(abilityGroups(god, db).size() == 10);
    // The pathway sheet shows the step after Sequence 0.
    const std::string pathwayText = renderText(buildPathwaySheet(*seer, db));
    CHECK(pathwayText.find("Above the Sequence: Lord of the Mysteries") != std::string::npos);
    CHECK(pathwayText.find("Lord of the Mysteries: Rules every mystery.") != std::string::npos);
    CHECK(renderText(buildPathwaySheet(*db.findPathway("acrobat"), db)).find("No Sefirah gathers this pathway yet") ==
          std::string::npos);  // the Grand Theatre gathers it now
    CHECK(renderText(buildSefirahSheet(db.sefirot[0], db)).find("Symbolism: Mysteries and the gray fog") != std::string::npos);
}

static void testDualPathway() {
    Database db = godDatabase();
    Character amon;
    amon.id = 1;
    amon.name = "Amon";
    amon.pathwayId = "marauder";
    amon.sequence = 0;
    amon.secondPathway.pathwayId = "apprentice";
    amon.secondPathway.uniquenessForm = "A door of stars";
    amon.secondPathway.uniquenessAbilities = {"Apprentice power 0: does things"};  // from an older save
    CHECK(dualPathwayEligible(amon) && isDualPathwayGod(amon));
    CHECK(normalizeCharacter(amon).empty() && amon.secondPathway.pathwayId == "apprentice");
    CHECK(amon.secondPathway.uniquenessAbilities.empty());  // the whole pathway is theirs now

    // The whole second pathway, Sequence 9 to 0, as one line with its movement.
    const auto groups = abilityGroups(amon, db);
    CHECK(groups.size() == 10 + 1);
    CHECK(groups[10].heading == "Apprentice (Door)");
    CHECK(groups[10].summary == "All abilities of the Apprentice pathway (Door), Sequence 9 to 0");
    CHECK(groups[10].movement == std::vector<std::string>{"Star travel"});
    CHECK(abilityCount(amon, db) == 20);
    CHECK(statBonuses(amon, db.findPathway("marauder"))[3] == 2);  // stat bonuses follow the main pathway only

    const std::string text = renderText(buildCharacterSheet(amon, db));
    CHECK(text.find("Sequence 0 Marauder 0, Marauder pathway (Error) and Apprentice pathway (Door)") != std::string::npos);
    CHECK(text.find("-- Second Pathway --") != std::string::npos);
    CHECK(text.find("Beyonder characteristics: 1 of Sequence 1: Apprentice 1") != std::string::npos);
    CHECK(text.find("A door of stars") != std::string::npos);
    CHECK(text.find("- Movement: Star travel") != std::string::npos);
    CHECK(text.find("All abilities of the Apprentice pathway (Door), Sequence 9 to 0.") != std::string::npos);

    db.characters = {amon};
    Character klein;
    klein.id = 2;
    klein.name = "Klein";
    db.characters.push_back(klein);
    const Comparison result = buildComparison({&db.characters[0], &db.characters[1]}, db);
    CHECK(result.sections[0].rows[0].values[0] == "Marauder (Error) and Apprentice (Door)");

    // Leaving Sequence 0, going Above the Sequence, or picking their own pathway drops it, with a note.
    Character below = amon;
    below.sequence = 1;
    CHECK(normalizeCharacter(below).size() == 1 && below.secondPathway.empty());
    Character above = amon;
    above.aboveSequence = true;
    CHECK(normalizeCharacter(above).size() == 1 && above.secondPathway.empty());
    Character same = amon;
    same.secondPathway.pathwayId = "marauder";
    CHECK(normalizeCharacter(same).size() == 1 && same.secondPathway.empty());
    // Not holding the Uniqueness drops its description and powers; characteristics stay 1 or 2.
    Character noUniqueness = amon;
    noUniqueness.secondPathway.hasUniqueness = false;
    noUniqueness.secondPathway.beyonderCharacteristics = 5;
    normalizeCharacter(noUniqueness);
    CHECK(noUniqueness.secondPathway.uniquenessForm.empty() && noUniqueness.secondPathway.uniquenessAbilities.empty());
    CHECK(noUniqueness.secondPathway.beyonderCharacteristics == 2);
}

// Max's example: a Sequence 5 Spectator who drank a Sequence 4 Demoness (Assassin pathway) potion.
static void testMixedPathways() {
    Database db = godDatabase();
    auto extra = [](const std::string& id, const std::string& name, const std::string& god, const std::string& group) {
        Pathway p;
        p.id = id;
        p.name = name;
        p.god = god;
        p.group = group;
        p.primaryStat = "DEX";
        p.secondaryStat = "CHA";
        for (int s = 9; s >= 0; --s) {
            p.sequences.push_back({s, name + " " + std::to_string(s), {name + " power " + std::to_string(s) + ": does things"}});
        }
        return p;
    };
    db.pathways.push_back(extra("spectator", "Spectator", "Visionary", "Visionary / Sun / Tyrant / White Tower / Hanged Man"));
    db.pathways.push_back(extra("assassin", "Assassin", "Demoness", "Red Priest / Demoness"));

    Character c;
    c.id = 1;
    c.name = "Mixed Mira";
    c.pathwayId = "spectator";
    c.sequence = 5;
    CHECK(!isMixedPathway(c) && pathwaySwitches(c, db).empty());
    CHECK(switchPathway(c, "assassin"));
    CHECK(c.pathwayId == "assassin" && c.sequence == 4);
    CHECK(c.mixedSequences.size() == 5 && c.mixedSequences.front().sequence == 9 && c.mixedSequences.back().sequence == 5);
    CHECK(pathwayIdAt(c, 9) == "spectator" && pathwayIdAt(c, 5) == "spectator" && pathwayIdAt(c, 4) == "assassin");
    CHECK(isMixedPathway(c) && earlierPathwayIds(c) == std::vector<std::string>{"spectator"});
    const auto switches = pathwaySwitches(c, db);
    CHECK(switches.size() == 1 && switches[0].atSequence == 4 && switches[0].fromPathwayId == "spectator");
    CHECK(switches[0].toPathwayId == "assassin" && !switches[0].neighbouring && riskySwitch(c, db));
    CHECK(normalizeCharacter(c).empty() && c.mixedSequences.size() == 5);

    // Each Sequence's abilities come from its own pathway; stat bonuses follow the pathway they are on now.
    const auto groups = abilityGroups(c, db);
    CHECK(groups.size() == 6);
    CHECK(groups[4].heading == "Sequence 5: Spectator 5 (Spectator pathway)" && groups[4].pathwayId == "spectator");
    CHECK(groups[5].heading == "Sequence 4: Assassin 4 (Assassin pathway)");
    CHECK(groups[5].abilities == std::vector<std::string>{"Assassin power 4: does things"});
    CHECK(abilityCount(c, db) == 6);

    // The sheet: subtitle, badges, the ladder and the switch, in every format.
    c.stateOfMind = "Half mad";
    c.stateOfMindNote = "hears the Demoness whisper";
    const Sheet sheet = buildCharacterSheet(c, db);
    CHECK(sheet.subtitle == "Sequence 4 Assassin 4, Assassin pathway (Demoness), switched from the Spectator pathway");
    CHECK(std::any_of(sheet.badges.begin(), sheet.badges.end(),
                      [](const Badge& b) { return b.text == "Mixed pathways, risky switch" && b.tone == "danger"; }));
    CHECK(std::any_of(sheet.badges.begin(), sheet.badges.end(),
                      [](const Badge& b) { return b.text == "Half mad" && b.tone == "orange"; }));
    const auto rungs = ladderRungs(c, db);
    CHECK(rungs.size() == 6 && rungs[4].pathwayId == "spectator" && !rungs[4].switched);
    CHECK(rungs[5].switched && rungs[5].risky && rungs[5].current && rungs[5].name == "Assassin 4");
    CHECK(rungs[0].colour == "#eca04e" && rungs[5].colour == "#de5a5a");
    const std::string text = renderText(sheet);
    CHECK(text.find("State of mind: Half mad: hears the Demoness whisper") != std::string::npos);
    CHECK(text.find("-- Sequence Ladder --") != std::string::npos);
    CHECK(text.find("Sequences 9-5  Spectator pathway: Spectator 9, Spectator 8, Spectator 7, Spectator 6, Spectator 5") !=
          std::string::npos);
    CHECK(text.find("Sequence 4     Assassin pathway: Assassin 4 (now)") != std::string::npos);
    CHECK(text.find("Switch at Sequence 4: From the Spectator pathway (Visionary) to the Assassin pathway (Demoness). "
                    "Not neighbours") != std::string::npos);
    CHECK(text.find("\n Saint | Mixed pathways, risky switch | Half mad | Threat: High\n") != std::string::npos);
    const std::string markdown = renderMarkdown(sheet);
    CHECK(markdown.find("- **Sequences 9-5**, Spectator pathway: Spectator 9") != std::string::npos);
    const std::string html = renderHtmlPage("Mira", {sheet}, "auto");
    CHECK(html.find("class=\"rung current switched risky\" style=\"--cd:#de5a5a;--cl:#8a3838\"") != std::string::npos);
    CHECK(html.find("<span class=\"badge tone-orange\">Half mad</span>") != std::string::npos);

    // Saved and loaded again; an older save without these fields loads as one pathway.
    const Character back = nlohmann::json(c).get<Character>();
    CHECK(back.mixedSequences.size() == 5 && back.mixedSequences[0].pathwayId == "spectator");
    CHECK(back.stateOfMind == "Half mad" && back.stateOfMindNote == "hears the Demoness whisper");
    const Character old = nlohmann::json::parse(R"({"id": 2, "name": "Old", "pathwayId": "seer", "sequence": 4})").get<Character>();
    CHECK(old.mixedSequences.empty() && old.stateOfMind.empty() && !isMixedPathway(old));

    // A neighbouring switch is safe.
    Character neighbour;
    neighbour.pathwayId = "seer";
    neighbour.sequence = 7;
    CHECK(switchPathway(neighbour, "marauder") && neighbour.sequence == 6);
    CHECK(pathwaySwitches(neighbour, db).at(0).neighbouring && !riskySwitch(neighbour, db));
    // Switching back to an earlier pathway: only the Sequences from other pathways stay listed.
    Character back2 = c;
    CHECK(switchPathway(back2, "spectator") && back2.sequence == 3);
    CHECK(back2.mixedSequences.size() == 1 && back2.mixedSequences[0].sequence == 4 &&
          back2.mixedSequences[0].pathwayId == "assassin");
    CHECK(pathwaySwitches(back2, db).size() == 2);
    // No switching for a mortal, a Sequence 0 god, Above the Sequence, or to their own pathway.
    Character mortal;
    CHECK(!switchPathway(mortal, "seer"));
    Character god;
    god.pathwayId = "seer";
    god.sequence = 0;
    CHECK(!switchPathway(god, "marauder"));
    CHECK(!switchPathway(c, "assassin"));

    // Moving down to a weaker Sequence drops the mixed Sequences they no longer hold under it.
    Character weaker = c;
    weaker.sequence = 6;
    CHECK(normalizeCharacter(weaker).size() == 1);
    CHECK(weaker.mixedSequences.size() == 3 && weaker.mixedSequences.back().sequence == 7);
    Character mortalAgain = c;
    mortalAgain.pathwayId.clear();
    normalizeCharacter(mortalAgain);
    CHECK(mortalAgain.mixedSequences.empty());

    // Lists: the pathway filter finds them under either pathway, and a pathway in use can't be deleted.
    db.characters = {c};
    RecordFilter filter;
    filter.pathwayId = "spectator";
    CHECK(filterCharacters(db, filter).size() == 1);
    filter.pathwayId = "seer";
    CHECK(filterCharacters(db, filter).empty());
    CHECK(db.pathwayUsers("spectator") == std::vector<std::string>{"Mixed Mira (C-001)"});

    // Compare shows where they switched from, and their state of mind.
    Character other;
    other.id = 2;
    other.name = "Plain Pat";
    db.characters.push_back(other);
    const Comparison result = buildComparison({&db.characters[0], &db.characters[1]}, db);
    CHECK(result.sections[0].rows[0].values[0] == "Assassin (Demoness), switched from Spectator (Visionary)");
    CHECK(std::any_of(result.sections[0].rows.begin(), result.sections[0].rows.end(), [](const CompareRow& row) {
        return row.label == "State of mind" && row.values[0] == "Half mad" && row.values[1].empty();
    }));

    CHECK(stateOfMindLevel("Sane") == 0 && stateOfMindLevel("Lost control") == 4 && stateOfMindLevel("Dreamy") == -1);
}

// Honorific names can be longer than three lines; blank lines are not part of the name.
static void testLongHonorificName() {
    Database db = godDatabase();
    Character c;
    c.name = "Klein";
    c.pathwayId = "seer";
    c.sequence = 3;
    c.honorificName = {"The Fool that doesn't belong to this era,", "", "The mysterious ruler above the gray fog;",
                       "The King of Yellow and Black who wields good luck.", "Line four", "Line five"};
    CHECK(normalizeCharacter(c).empty());
    CHECK(c.honorificName.size() == 5 && c.honorificName[1] == "The mysterious ruler above the gray fog;");
    const std::string text = renderText(buildCharacterSheet(c, db));
    CHECK(text.find("\"Line five\"") != std::string::npos);
}

static void testSefirot() {
    const Storage source(fs::path(LOTM_SOURCE_DIR) / "data");
    Database db;
    source.loadAll(db);
    // Nine come with the program; data/custom_sefirot.json may add Max's own.
    CHECK(std::count_if(db.sefirot.begin(), db.sefirot.end(), [](const Sefirah& s) { return !s.custom; }) == 9);
    // Every built-in pathway belongs to exactly one Sefirah, and every Sefirah names real pathways.
    for (const auto& p : db.pathways) {
        if (p.custom) continue;
        const auto count = std::count_if(db.sefirot.begin(), db.sefirot.end(), [&](const Sefirah& s) {
            return std::find(s.pathwayIds.begin(), s.pathwayIds.end(), p.id) != s.pathwayIds.end();
        });
        if (count != 1) std::cerr << p.id << " is in " << count << " Sefirot\n";
        CHECK(count == 1);
    }
    for (const auto& s : db.sefirot) {
        CHECK(!s.description.empty() && !s.greatOldOne.empty() && !s.pathwayIds.empty());
        for (const auto& id : s.pathwayIds) CHECK(db.findPathway(id) != nullptr);
        // The neighbouring pathways of a group share the group name in pathways.json.
        for (const auto& id : s.pathwayIds) CHECK(db.findPathway(id)->group == db.findPathway(s.pathwayIds[0])->group);
    }
    const Sefirah* castle = db.findSefirah("sefirah_castle");
    CHECK(castle && castle->greatOldOne == "Lord of the Mysteries" && db.sefirahForPathway("seer") == castle);
    CHECK(sefirahLabel(db, "sefirah_castle") == "Sefirah Castle (Lord of the Mysteries)");
    CHECK(sefirahLabel(db, "") == "None");

    // Holders: the characters that name it. Their Sefirot go into the catalogue export.
    Character klein;
    klein.id = 1;
    klein.name = "Klein Moretti";
    klein.pathwayId = "seer";
    klein.sequence = 0;
    klein.sefirahId = "sefirah_castle";
    db.characters = {klein};
    CHECK(db.sefirahHolders("sefirah_castle").size() == 1 && db.sefirahHolders("chaos_sea").empty());
    const std::string text = renderText(buildSefirahSheet(*castle, db));
    CHECK(text.find("SEFIRAH CASTLE") != std::string::npos);
    CHECK(text.find("Great Old One: Lord of the Mysteries") != std::string::npos);
    CHECK(text.find("Seer (The Fool)") != std::string::npos);
    CHECK(text.find("Klein Moretti (C-001), Sequence 0") != std::string::npos);
    const auto sheets = catalogueSheets(db);
    CHECK(std::count_if(sheets.begin(), sheets.end(), [](const Sheet& s) { return s.kind == "Sefirah"; }) == 1);
    const std::string html = renderHtmlPage("All", sheets, "light");
    CHECK(html.find("<h2>Sefirot</h2>") != std::string::npos);
    CHECK(exportName(*castle) == "sefirah-sefirah-castle");
}

// Your own Sefirot: saved to custom_sefirot.json (sefirot.json is never written), edited and deleted
// through the shared records, and taken away from their holders when deleted.
static void testCustomSefirot() {
    const fs::path temp = fs::temp_directory_path() / "lotm_tests_custom_sefirot";
    fs::remove_all(temp);
    fs::create_directories(temp);
    const fs::path source = fs::path(LOTM_SOURCE_DIR) / "data";
    fs::copy_file(source / "pathways.json", temp / "pathways.json");
    fs::copy_file(source / "sefirot.json", temp / "sefirot.json");

    App app{Database{}, Storage(temp), Dice{}};
    app.storage.loadAll(app.db);
    CHECK(app.db.sefirot.size() == 9);
    CHECK(app.db.newSefirahId("Sefirah Castle") == "sefirah_castle_2");
    CHECK(app.db.newSefirahId("Sefirah of Masks!") == "sefirah_of_masks");

    Sefirah masks;
    masks.name = "Sefirah of Masks";
    masks.greatOldOne = "The Faceless";
    masks.pathwayIds = {"spectator", "seer"};
    masks.knownHolders = {"The first Faceless"};
    masks.symbolism = "Faces and lies";
    masks.abilities = {"Thousand Faces: Wears any face."};
    saveCustomSefirah(app, masks, true);
    CHECK(masks.id == "sefirah_of_masks" && masks.custom);
    CHECK(fs::exists(temp / "custom_sefirot.json"));
    CHECK(fs::file_size(source / "sefirot.json") == fs::file_size(temp / "sefirot.json"));  // never rewritten
    // Seer is already gathered by Sefirah Castle, which the editors point out.
    CHECK(app.db.otherSefirahFor("seer", masks.id) == app.db.findSefirah("sefirah_castle"));
    CHECK(app.db.otherSefirahFor("seer", "sefirah_castle") == app.db.findSefirah("sefirah_of_masks"));

    Character ada;
    ada.name = "Ada";
    ada.pathwayId = "spectator";
    ada.sequence = 0;
    ada.sefirahId = masks.id;
    saveCharacter(app, ada, true);

    Database reloaded;
    app.storage.loadAll(reloaded);
    const Sefirah* loaded = reloaded.findSefirah("sefirah_of_masks");
    CHECK(reloaded.sefirot.size() == 10 && loaded && loaded->custom && loaded->greatOldOne == "The Faceless");
    CHECK(loaded && loaded->symbolism == "Faces and lies" && loaded->abilities.size() == 1);
    CHECK(!reloaded.findSefirah("sefirah_castle")->custom);
    CHECK(sefirahLabel(reloaded, ada.sefirahId) == "Sefirah of Masks (The Faceless)");
    CHECK(buildSefirahSheet(*loaded, reloaded).footer == "Stored in custom_sefirot.json");
    CHECK(buildSefirahSheet(*reloaded.findSefirah("sefirah_castle"), reloaded).footer == "Stored in sefirot.json");
    CHECK(reloaded.sefirahHolders("sefirah_of_masks").size() == 1);

    masks.description = "Every face it has worn.";
    saveCustomSefirah(app, masks, false);
    CHECK(app.db.sefirot.size() == 10 && app.db.findSefirah("sefirah_of_masks")->description == "Every face it has worn.");

    bool refused = false;
    try {
        deleteCustomSefirah(app, "sefirah_castle");
    } catch (const StorageError&) {
        refused = true;
    }
    CHECK(refused && app.db.sefirot.size() == 10);
    deleteCustomSefirah(app, "sefirah_of_masks");
    CHECK(app.db.sefirot.size() == 9 && app.db.findCharacter(ada.id)->sefirahId.empty());
    Database afterDelete;
    app.storage.loadAll(afterDelete);
    CHECK(afterDelete.sefirot.size() == 9 && afterDelete.characters.front().sefirahId.empty());

    // An id used in both files is refused with a message naming the file.
    Sefirah clash = masks;
    clash.id = "chaos_sea";
    clash.custom = true;
    app.db.sefirot.push_back(clash);
    app.storage.saveCustomSefirot(app.db);
    refused = false;
    try {
        Database broken;
        app.storage.loadAll(broken);
    } catch (const StorageError& e) {
        refused = std::string(e.what()).find("custom_sefirot.json") != std::string::npos;
    }
    CHECK(refused);
    fs::remove_all(temp);
}

static void testListFilters() {
    Database db;
    db.pathways.push_back(warrior());
    Character klein;
    klein.id = 12;
    klein.name = "Klein Moretti";
    klein.aliases = {"Sherlock Moriarty", "Gehrman Sparrow"};
    Character dunn;
    dunn.id = 3;
    dunn.name = "Dunn Smith";
    dunn.pathwayId = "warrior";
    dunn.sequence = 5;
    Character god;
    god.id = 7;
    god.name = "Adam";
    god.pathwayId = "warrior";
    god.sequence = 0;
    db.characters = {klein, dunn, god};

    auto names = [](const std::vector<const Character*>& rows) {
        std::string out;
        for (const Character* c : rows) out += (out.empty() ? "" : ",") + c->name;
        return out;
    };
    RecordFilter f;
    CHECK(!f.active());
    CHECK(names(filterCharacters(db, f)) == "Adam,Dunn Smith,Klein Moretti");
    f.search = "sparrow";
    CHECK(names(filterCharacters(db, f)) == "Klein Moretti");
    f.search = "C-012";
    CHECK(names(filterCharacters(db, f)) == "Klein Moretti");
    f.search = "c3";
    CHECK(names(filterCharacters(db, f)) == "Dunn Smith");
    f.search = "7";
    CHECK(names(filterCharacters(db, f)) == "Adam");
    CHECK(f.active());
    f.clear();
    f.letter = 'D';
    CHECK(names(filterCharacters(db, f)) == "Dunn Smith");
    f.clear();
    f.power = 2;  // Sequence 7-5
    CHECK(names(filterCharacters(db, f)) == "Dunn Smith");
    f.power = 0;
    CHECK(names(filterCharacters(db, f)) == "Klein Moretti");
    f.clear();
    f.pathwayId = "warrior";
    CHECK(names(filterCharacters(db, f)) == "Adam,Dunn Smith");
    f.clear();
    f.sort = SortOrder::Id;
    CHECK(names(filterCharacters(db, f)) == "Dunn Smith,Adam,Klein Moretti");
    f.sort = SortOrder::Power;
    CHECK(names(filterCharacters(db, f)) == "Adam,Dunn Smith,Klein Moretti");
    f.letter = 'K';
    f.clear();
    CHECK(f.sort == SortOrder::Power);  // clearing keeps the sort order
    CHECK(firstLetters({"klein", "Adam", "Amon", "42"}) == std::vector<char>({'A', 'K'}));

    Artifact a1;
    a1.id = 1;
    a1.name = "Creeping Hunger";
    a1.sequenceLevel = 5;
    Artifact a2;
    a2.id = 2;
    a2.name = "Blasphemy Slate";
    db.artifacts = {a1, a2};
    RecordFilter af;
    af.power = kUnknownPower;
    auto rows = filterArtifacts(db, af);
    CHECK(rows.size() == 1 && rows[0]->name == "Blasphemy Slate");
    af.clear();
    af.search = "A-001";
    rows = filterArtifacts(db, af);
    CHECK(rows.size() == 1 && rows[0]->name == "Creeping Hunger");
    CHECK(powerBandName(kUnknownPower) == "Level unknown");
}

static void testPointBuy() {
    CHECK(pointBuyCost(8) == 0);
    CHECK(pointBuyCost(13) == 5);
    CHECK(pointBuyCost(15) == 9);
    CHECK(pointBuyCost(16) == -1);
    CHECK(pointBuyCost(7) == -1);
}

static void testDice() {
    Dice dice(1234);
    for (int i = 0; i < 500; ++i) {
        FourD6Roll r = dice.roll4d6DropLowest();
        CHECK(r.total >= 3 && r.total <= 18);
        int sum = 0, lowest = 7;
        for (int d : r.dice) {
            CHECK(d >= 1 && d <= 6);
            sum += d;
            lowest = std::min(lowest, d);
        }
        CHECK(r.dice[r.droppedIndex] == lowest);
        CHECK(r.total == sum - lowest);
    }
}

static void testJsonRoundTrip() {
    Character c;
    c.id = 7;
    c.name = "Klein \"Sherlock\" Moriarty";
    c.pathwayId = "seer";
    c.sequence = 7;
    c.titles = {"Detective"};
    c.stats.base = {11, 15, 12, 14, 16, 13};
    c.stats.hpIncluded = true;
    c.stats.hp = 22;
    c.artifactIds = {1, 3};
    c.relationships = {{2, "Fors Wall", "Rival", "Old grudge"}, {0, "Azik Eggers", "Mentor", ""}};
    nlohmann::json j = c;
    Character back = j.get<Character>();
    CHECK(back.name == c.name);
    CHECK(back.stats.base == c.stats.base);
    CHECK(back.stats.hp == 22);
    CHECK(back.artifactIds == c.artifactIds);
    CHECK(back.relationships.size() == 2 && back.relationships[0].type == "Rival");
    CHECK(back.relationships[1].characterId == 0 && back.relationships[1].name == "Azik Eggers");

    c.appearance.hairStyle = "Wavy";
    c.dossier.recentActions = "Violent incidents";
    c.uniquenessAbilities = {"Fooling: deceives reality"};
    c.beyonderCharacteristics = 2;
    back = nlohmann::json(c).get<Character>();
    CHECK(back.appearance.hairStyle == "Wavy" && back.dossier.recentActions == "Violent incidents");
    CHECK(back.uniquenessAbilities.size() == 1 && back.beyonderCharacteristics == 2);

    // Relationships saved before names existed still load.
    Relationship oldLink = nlohmann::json::parse(R"({"characterId": 4, "type": "Ally"})").get<Relationship>();
    CHECK(oldLink.characterId == 4 && oldLink.name.empty() && oldLink.type == "Ally");

    // Older files with missing fields still load, using defaults.
    Character partial = nlohmann::json::parse(R"({"id": 3, "name": "Old Save"})").get<Character>();
    CHECK(partial.name == "Old Save");
    CHECK(partial.sequence == 9);
    CHECK(partial.stats.base[0] == 10);
    CHECK(partial.beyonderCharacteristics == 1 && partial.dossier.empty());
    CHECK(!partial.aboveSequence && partial.secondPathway.empty() && partial.secondPathway.hasUniqueness);
    CHECK(partial.sefirahId.empty());

    c.sequence = 0;
    c.aboveSequence = true;
    c.sefirahId = "sefirah_castle";
    c.secondPathway.pathwayId = "apprentice";
    c.secondPathway.beyonderCharacteristics = 2;
    c.secondPathway.uniquenessAbilities = {"Door: opens anything"};
    back = nlohmann::json(c).get<Character>();
    CHECK(back.aboveSequence && back.sefirahId == "sefirah_castle");
    CHECK(back.secondPathway.pathwayId == "apprentice" && back.secondPathway.beyonderCharacteristics == 2);
    CHECK(back.secondPathway.uniquenessAbilities.size() == 1);
}

static void testRelationships() {
    Database db;
    Character klein;
    klein.id = 1;
    klein.name = "Klein Moretti";
    Character fors;
    fors.id = 2;
    fors.name = "Fors Wall";
    Character forsCopy = fors;
    forsCopy.id = 3;
    forsCopy.name = "Fors Wall (copy)";
    db.characters = {klein, fors, forsCopy};

    // An exact name (any case) finds just that character; part of a name finds every match.
    auto exact = db.findCharactersByName("fors wall", klein.id);
    CHECK(exact.size() == 1 && exact[0]->id == 2);
    CHECK(db.findCharactersByName("Fors", klein.id).size() == 2);
    CHECK(db.findCharactersByName("Klein", klein.id).empty());  // never yourself
    CHECK(db.findCharactersByName("Audrey", klein.id).empty());
    CHECK(db.findCharactersByName("", klein.id).empty());

    // Saved characters show their current name and code; anyone else shows the typed name.
    CHECK(relationshipName(db, {2, "Old name", "Friend", ""}) == "Fors Wall (C-002)");
    CHECK(relationshipName(db, {0, "Audrey Hall", "Friend", ""}) == "Audrey Hall");
    CHECK(relationshipName(db, {9, "Gone", "Friend", ""}) == "Gone");
    CHECK(relationshipName(db, {0, "", "Friend", ""}) == "(unknown)");

    // Both kinds appear on the character sheet.
    db.characters[0].relationships = {{2, "Fors Wall", "Friend", "Met at the Tarot Club"}, {0, "Audrey Hall", "Ally", ""}};
    const std::string text = renderText(buildCharacterSheet(db.characters[0], db));
    CHECK(text.find("Friend: Fors Wall (C-002), Met at the Tarot Club") != std::string::npos);
    CHECK(text.find("Ally: Audrey Hall") != std::string::npos);
}

static void testPathwayDatabase() {
    const fs::path dataDir = fs::path(LOTM_SOURCE_DIR) / "data";
    Database db;
    Storage(dataDir).loadAll(db);
    // 22 built-in pathways, then the ones from custom_pathways.json (the Maestro).
    CHECK(db.pathways.size() == 23);
    CHECK(std::count_if(db.pathways.begin(), db.pathways.end(), [](const Pathway& p) { return p.custom; }) == 1);
    const Pathway* maestro = db.findPathway("maestro");
    CHECK(maestro != nullptr && maestro->custom && maestro->god == "The Maestro");
    CHECK(maestro && maestro->findSequence(4) && maestro->findSequence(4)->name == "Director");
    CHECK(maestro && maestro->uniqueness.rfind("The Shifting Silk Cloak: ", 0) == 0);
    for (const auto& p : db.pathways) {
        CHECK(p.sequences.size() == 10);
        CHECK(statIndex(p.primaryStat) >= 0);
        CHECK(statIndex(p.secondaryStat) >= 0);
        CHECK(p.speedGrade == "Slow" || p.speedGrade == "Average" || p.speedGrade == "Fast" ||
              p.speedGrade == "Very fast");
        for (int s = 0; s <= 9; ++s) CHECK(p.findSequence(s) != nullptr);
        // Every Sequence is filled in, and every ability reads "Name: what it does".
        for (const auto& seq : p.sequences) {
            CHECK(!seq.abilities.empty());
            for (const auto& a : seq.abilities) {
                const size_t colon = a.find(": ");
                CHECK(colon != std::string::npos && colon > 0 && colon <= 48);
                CHECK(a.rfind("Edit me", 0) != 0);
            }
        }
    }
}

static void testSaveLoadAndBackups() {
    const fs::path temp = fs::temp_directory_path() / "lotm_tests_data";
    fs::remove_all(temp);
    fs::create_directories(temp);
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", temp / "pathways.json");

    Database db;
    Storage storage(temp);
    storage.loadAll(db);
    db.settings.backupsToKeep = 2;
    Artifact a;
    a.id = db.nextArtifactId();
    a.name = "Creeping Hunger";
    a.pathwayId = "seer";
    a.sequenceLevel = 5;
    db.artifacts.push_back(a);
    for (int i = 0; i < 4; ++i) storage.saveArtifacts(db);  // 3 overwrites -> 3 backups, 2 kept

    Database reloaded;
    storage.loadAll(reloaded);
    CHECK(reloaded.artifacts.size() == 1);
    CHECK(reloaded.artifacts[0].name == "Creeping Hunger");
    CHECK(reloaded.artifacts[0].sequenceLevel == 5);

    int backups = 0;
    for (const auto& entry : fs::directory_iterator(temp / "backups")) {
        if (entry.path().filename().string().rfind("artifacts-", 0) == 0) ++backups;
    }
    CHECK(backups == 2);
    fs::remove_all(temp);
}

static void testCustomPathways() {
    const fs::path temp = fs::temp_directory_path() / "lotm_tests_pathways";
    fs::remove_all(temp);
    fs::create_directories(temp);
    const fs::path source = fs::path(LOTM_SOURCE_DIR) / "data";
    fs::copy_file(source / "pathways.json", temp / "pathways.json");

    // No custom file: only the built-in pathways, and saving creates the file.
    Database db;
    Storage storage(temp);
    storage.loadAll(db);
    CHECK(db.pathways.size() == 22);
    CHECK(db.newPathwayId("Mystery Pryer") == "mystery_pryer_2");
    CHECK(db.newPathwayId("Night Weaver!") == "night_weaver");
    CHECK(db.newPathwayId("??") == "pathway");

    Pathway p;
    p.id = db.newPathwayId("Night Weaver");
    p.name = "Night Weaver";
    p.god = "The Loom";
    p.primaryStat = "DEX";
    p.uniqueness = "A spindle of moonlight";
    p.custom = true;
    for (int s = 9; s >= 0; --s) p.sequences.push_back({s, "", {}});
    p.sequences[0] = {9, "Night Weaver", {"Thread Sight: Sees the threads between people."}};
    db.pathways.push_back(p);
    storage.saveCustomPathways(db);
    CHECK(fs::exists(temp / "custom_pathways.json"));
    CHECK(fs::file_size(source / "pathways.json") == fs::file_size(temp / "pathways.json"));  // never rewritten

    Database reloaded;
    storage.loadAll(reloaded);
    CHECK(reloaded.pathways.size() == 23);
    const Pathway* loaded = reloaded.findPathway("night_weaver");
    CHECK(loaded && loaded->custom && loaded->god == "The Loom" && loaded->uniqueness == "A spindle of moonlight");
    CHECK(loaded && loaded->findSequence(9)->abilities.size() == 1);
    CHECK(!reloaded.findPathway("seer")->custom);

    // A pathway in use can't simply vanish: pathwayUsers lists what still points at it.
    Character c;
    c.id = 1;
    c.name = "Ada";
    c.pathwayId = "night_weaver";
    reloaded.characters.push_back(c);
    CHECK(reloaded.pathwayUsers("night_weaver") == std::vector<std::string>{"Ada (C-001)"});
    CHECK(reloaded.pathwayUsers("seer").empty());

    // The shipped custom file loads and saves back byte for byte, so the program keeps its layout.
    // Git on Windows checks files out with CRLF line endings, so those are ignored here.
    fs::copy_file(source / "custom_pathways.json", temp / "custom_pathways.json", fs::copy_options::overwrite_existing);
    Database shipped;
    storage.loadAll(shipped);
    storage.saveCustomPathways(shipped);
    auto readAll = [](const fs::path& file) {
        std::ifstream in(file, std::ios::binary);
        std::string text(std::istreambuf_iterator<char>(in), {});
        std::erase(text, '\r');
        return text;
    };
    CHECK(readAll(temp / "custom_pathways.json") == readAll(source / "custom_pathways.json"));

    // An id used in both files is refused with a message naming the file.
    shipped.pathways.back().id = "seer";
    storage.saveCustomPathways(shipped);
    bool refused = false;
    try {
        Database clash;
        storage.loadAll(clash);
    } catch (const StorageError& e) {
        refused = std::string(e.what()).find("custom_pathways.json") != std::string::npos;
    }
    CHECK(refused);

    // The pathway sheet lists every Sequence and its abilities.
    const std::string text = renderText(buildPathwaySheet(*loaded, reloaded));
    CHECK(text.find("NIGHT WEAVER PATHWAY") != std::string::npos);
    CHECK(text.find("one of your own pathways") != std::string::npos);
    CHECK(text.find("Thread Sight: Sees the threads between people.") != std::string::npos);
    CHECK(text.find("No abilities written yet.") != std::string::npos);
    CHECK(text.find("Primary stat: Dexterity (DEX)") != std::string::npos);
    fs::remove_all(temp);
}

static void testRendering() {
    Database db;
    db.pathways.push_back(warrior());
    Artifact a;
    a.id = 1;
    a.name = "Sword <of> Dawn";
    a.ability = "Cuts light";
    db.artifacts.push_back(a);
    Character c;
    c.id = 1;
    c.name = "Leonard & Co";
    c.pathwayId = "warrior";
    c.sequence = 3;
    c.honorificName = {"Line one", "Line two", "Line three"};
    c.artifactIds = {1};
    c.stats.hpIncluded = true;
    c.stats.hp = 40;
    c.appearance.hair = "Black";
    c.appearance.hairLength = "Shoulder-length";
    c.appearance.hairStyle = "Wavy";
    c.dossier.filedBy = "Nighthawks";
    c.dossier.status = "Under watch";
    db.characters.push_back(c);
    db.pathways[0].sequences[3].abilities = {"Dawn Armour: Covers the body in light."};  // Sequence 6

    const Sheet sheet = buildCharacterSheet(c, db);
    const std::string text = renderText(sheet);
    CHECK(text.find("LEONARD & CO") != std::string::npos);
    CHECK(text.find("Sword <of> Dawn") != std::string::npos);
    CHECK(text.find("HP: 40") != std::string::npos);
    CHECK(text.find("Sequence 3: Seq3") != std::string::npos);
    CHECK(text.find("Hair: Black, shoulder-length, wavy") != std::string::npos);
    // Movement sits with the abilities of the Sequence that unlocks it, not in the stat block.
    CHECK(text.find("Movement: Leaping") != std::string::npos);
    CHECK(text.find("Movement: Leaping") > text.find("[Sequence 6: Seq6]"));
    CHECK(text.find("Movement: Leaping") < text.find("[Sequence 5: Seq5]"));
    CHECK(text.find("  Movement:") == std::string::npos);
    // The dossier: file number, who filed it, and the automatic threat level with its sum.
    CHECK(text.find("File: C-001") != std::string::npos);
    CHECK(text.find("Filed by: Nighthawks") != std::string::npos);
    CHECK(text.find("Threat level: High\n") != std::string::npos);
    CHECK(text.find("Assessment: Sequence 3 (5) + 1 Sealed Artifact (+1) = 6") != std::string::npos);

    const std::string html = renderHtmlPage("Test", {sheet, buildArtifactSheet(a, db)}, "dark");
    CHECK(html.find("Leonard &amp; Co") != std::string::npos);
    CHECK(html.find("Sword &lt;of&gt; Dawn") != std::string::npos);
    CHECK(html.find("<of>") == std::string::npos);
    CHECK(html.find("data-theme=\"dark\"") != std::string::npos);
    CHECK(html.find("class=\"contents\"") != std::string::npos);  // table of contents for 2 sheets
    CHECK(html.find("<li><strong>Dawn Armour:</strong> Covers the body in light.</li>") != std::string::npos);
    CHECK(html.find("<section class=\"dossier\">") != std::string::npos);

    const std::string md = renderMarkdown(sheet);
    CHECK(md.find("# Leonard & Co") != std::string::npos);
    CHECK(md.find("| Strength |") != std::string::npos);
    CHECK(md.find("- **Dawn Armour:** Covers the body in light.") != std::string::npos);

    // Sequence 1 with a Uniqueness lists the Sequence 0 abilities it grants; Sequence 0 has absorbed it.
    Character angel = c;
    angel.sequence = 1;
    angel.beyonderCharacteristics = 2;
    angel.hasUniqueness = true;
    angel.uniquenessAbilities = {"Twilight: Ages whatever it touches."};
    const std::string angelText = renderText(buildCharacterSheet(angel, db));
    CHECK(angelText.find("Beyonder characteristics: 2") != std::string::npos);
    CHECK(angelText.find("[From the Uniqueness: Sequence 0: Seq0]") != std::string::npos);
    CHECK(angelText.find("- Twilight: Ages whatever it touches.") != std::string::npos);
    Character god = c;
    god.sequence = 0;
    god.uniquenessForm = "A crown of dusk";
    const std::string godText = renderText(buildCharacterSheet(god, db));
    CHECK(godText.find("Has absorbed the Uniqueness of the Warrior pathway.") != std::string::npos);
    CHECK(godText.find("A crown of dusk") != std::string::npos);

    // Held By shows on the artifact sheet.
    const std::string artifactText = renderText(buildArtifactSheet(a, db));
    CHECK(artifactText.find("Leonard & Co (C-001)") != std::string::npos);
}

// The save, delete and export helpers that both the console screens and the app window use.
static void testSharedRecords() {
    const fs::path root = fs::temp_directory_path() / "lotm_tests_records";
    fs::remove_all(root);
    fs::create_directories(root / "data");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", root / "data" / "pathways.json");
    App app{Database{}, Storage(root / "data"), Dice{}};
    app.storage.loadAll(app.db);

    // Ability text splits into a name and a description, and goes back together.
    CHECK(splitAbility("Spirit Vision: sees auras") == std::make_pair(std::string("Spirit Vision"), std::string("sees auras")));
    CHECK(splitAbility("Sees auras. Also: more") == std::make_pair(std::string(), std::string("Sees auras. Also: more")));
    CHECK(joinAbility("Spirit Vision", "sees auras") == "Spirit Vision: sees auras");
    CHECK(joinAbility("Spirit Vision", "") == "Spirit Vision");
    CHECK(joinAbility("", "sees auras") == "sees auras");

    Character klein;
    klein.id = app.db.nextCharacterId();
    klein.name = "Klein Moretti";
    klein.pathwayId = "seer";
    klein.sequence = 7;
    saveCharacter(app, klein, true);

    // A name typed in the window links to the saved character, and the link comes back the other way.
    Character audrey;
    audrey.id = app.db.nextCharacterId();
    audrey.name = "Audrey Hall";
    audrey.relationships.push_back({0, "klein moretti", "Friend", ""});
    linkRelationshipsByName(app.db, audrey);
    CHECK(audrey.relationships[0].characterId == klein.id);
    const CharacterSaveReport report = saveCharacter(app, audrey, true);
    CHECK(!report.linked.empty());
    const Character* savedKlein = app.db.findCharacter(klein.id);
    CHECK(savedKlein && savedKlein->relationships.size() == 1 && savedKlein->relationships[0].characterId == audrey.id);

    // Renaming the row to someone else unlinks it.
    audrey.relationships[0].name = "Dunn Smith";
    linkRelationshipsByName(app.db, audrey);
    CHECK(audrey.relationships[0].characterId == 0);

    const int copyId = duplicateCharacter(app, klein.id);
    CHECK(copyId != klein.id && app.db.findCharacter(copyId)->name == "Klein Moretti (copy)");
    deleteCharacter(app, klein.id);
    CHECK(app.db.findCharacter(klein.id) == nullptr);
    CHECK(app.db.findCharacter(audrey.id)->relationships[0].characterId == 0);
    CHECK(app.db.findCharacter(audrey.id)->relationships[0].name == "Klein Moretti");

    // Deleting a Sealed Artifact takes it away from whoever held it.
    Artifact glove;
    glove.id = app.db.nextArtifactId();
    glove.name = "Creeping Hunger";
    saveArtifact(app, glove, true);
    app.db.findCharacter(audrey.id)->artifactIds.push_back(glove.id);
    deleteArtifact(app, glove.id);
    CHECK(app.db.artifacts.empty());
    CHECK(app.db.findCharacter(audrey.id)->artifactIds.empty());

    // A new pathway of your own gets an id from its name and lands in custom_pathways.json.
    Pathway weaver;
    weaver.name = "Night Weaver";
    weaver.custom = true;
    saveCustomPathway(app, weaver, true);
    CHECK(weaver.id == "night_weaver");
    Database reloaded;
    app.storage.loadAll(reloaded);
    CHECK(reloaded.findPathway("night_weaver") != nullptr);
    CHECK(reloaded.characters.size() == 2);
    deleteCustomPathway(app, "night_weaver");
    CHECK(app.db.findPathway("night_weaver") == nullptr);

    // Exports go to the exports folder next to the data folder.
    CHECK(fileSlug("Klein Moretti") == "klein-moretti");
    const Character* kept = app.db.findCharacter(audrey.id);
    const fs::path file = writeExport(app, exportName(*kept), ExportFormat::Markdown,
                                      {buildCharacterSheet(*kept, app.db)}, kept->name);
    CHECK(file == root / "exports" / (exportName(*kept) + ".md"));
    {
        std::ifstream in(file);  // closed before the folder is removed: Windows can't delete open files
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        CHECK(text.find("Audrey Hall") != std::string::npos);
    }
    fs::remove_all(root);
}

static void testSampleImport() {
    const fs::path root = fs::temp_directory_path() / "lotm_tests_samples";
    fs::remove_all(root);
    fs::create_directories(root / "data");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", root / "data" / "pathways.json");
    App app{Database{}, Storage(root / "data"), Dice{}};
    app.storage.loadAll(app.db);

    // Already saved: one character the sample also has, and one the sample only names.
    Character mine;
    mine.id = app.db.nextCharacterId();
    mine.name = "Klein Moretti";
    mine.notes = "my own version";
    saveCharacter(app, mine, true);
    Character dunn;
    dunn.id = app.db.nextCharacterId();
    dunn.name = "Dunn Smith";
    saveCharacter(app, dunn, true);

    SampleSet set;
    set.title = "Test set";
    Artifact glove;
    glove.id = 7;
    glove.name = "Creeping Hunger";
    set.artifacts.push_back(glove);
    Character klein;
    klein.id = 1;
    klein.name = "klein moretti";
    Character audrey;
    audrey.id = 2;
    audrey.name = "Audrey Hall";
    audrey.artifactIds = {7, 99};
    audrey.relationships = {{1, "Klein Moretti", "Superior", ""}, {0, "Dunn Smith", "Acquaintance", ""}};
    Character leonard;
    leonard.id = 3;
    leonard.name = "Leonard Mitchell";
    leonard.relationships = {{2, "Audrey Hall", "Ally", ""}};
    audrey.relationships.push_back({3, "Leonard Mitchell", "Ally", ""});
    set.characters = {klein, audrey, leonard};

    const SampleImportReport report = importSampleSet(app, set);
    CHECK(report.addedCharacters.size() == 2);
    CHECK(report.addedArtifacts.size() == 1);
    CHECK(report.skipped == std::vector<std::string>{"klein moretti"});
    CHECK(app.db.characters.size() == 4);
    const auto* addedAudrey = app.db.findCharactersByName("Audrey Hall", 0).at(0);
    const auto* addedLeonard = app.db.findCharactersByName("Leonard Mitchell", 0).at(0);
    CHECK(addedAudrey->id > dunn.id && addedLeonard->id > dunn.id);
    // Holders point at the new artifact id; an id the sample doesn't have is dropped.
    CHECK(addedAudrey->artifactIds == std::vector<int>{app.db.artifacts.at(0).id});
    // Links go to the saved Klein and Dunn, and to the newly added Leonard, and come back the other way.
    CHECK(addedAudrey->relationships.at(0).characterId == mine.id);
    CHECK(addedAudrey->relationships.at(1).characterId == dunn.id);
    CHECK(addedAudrey->relationships.at(2).characterId == addedLeonard->id);
    CHECK(addedLeonard->relationships.size() == 1 && addedLeonard->relationships[0].characterId == addedAudrey->id);
    const Character* savedKlein = app.db.findCharacter(mine.id);
    CHECK(savedKlein->notes == "my own version");
    CHECK(savedKlein->relationships.size() == 1 && savedKlein->relationships[0].type == "Subordinate");
    CHECK(app.db.findCharacter(dunn.id)->relationships.size() == 1);

    // Everything is on disk, and a second import adds nothing.
    Database reloaded;
    app.storage.loadAll(reloaded);
    CHECK(reloaded.characters.size() == 4 && reloaded.artifacts.size() == 1);
    const SampleImportReport again = importSampleSet(app, set);
    CHECK(again.addedCharacters.empty() && again.addedArtifacts.empty() && again.skipped.size() == 4);
    CHECK(app.db.characters.size() == 4);

    // Sharing: Audrey (who holds the glove and knows Klein, Dunn and Leonard) and Leonard become a
    // sample set of their own; the glove comes along, and the links to Klein and Dunn keep only names.
    CHECK(sampleSetFileName("Max's Characters!") == "maxs_characters.json");
    CHECK(sampleSetFileName("???") == "my_sample_set.json");
    const SampleSet shared = makeSampleSet(app.db, "Max's Characters", "Mine.", {addedAudrey->id, addedLeonard->id}, {});
    CHECK(shared.characters.size() == 2 && shared.artifacts.size() == 1 && shared.artifacts[0].name == "Creeping Hunger");
    const Relationship& toKlein = shared.characters[0].relationships.at(0);
    CHECK(toKlein.characterId == 0 && toKlein.name == "Klein Moretti");
    CHECK(shared.characters[0].relationships.at(2).characterId == addedLeonard->id);  // inside the set: kept
    const fs::path file = app.storage.saveSampleSet(shared, sampleSetFileName(shared.title));
    CHECK(file.filename() == "maxs_characters.json" && fs::exists(file));
    const auto sets = app.storage.loadSampleSets();
    CHECK(sets.size() == 1 && sets[0].title == "Max's Characters" && sets[0].characters.size() == 2);
    CHECK(namesInSampleSets(sets, "").size() == 3 && namesInSampleSets(sets, "maxs_characters.json").empty());

    // Someone else pulls it and presses Import all: everything arrives, linked up, held artifacts too.
    const fs::path other = root / "other";
    fs::create_directories(other / "samples");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", other / "pathways.json");
    fs::copy_file(file, other / "samples" / file.filename());
    App friendApp{Database{}, Storage(other), Dice{}};
    friendApp.storage.loadAll(friendApp.db);
    Character friendKlein;
    friendKlein.id = friendApp.db.nextCharacterId();
    friendKlein.name = "Klein Moretti";
    saveCharacter(friendApp, friendKlein, true);
    const SampleImportReport all = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(all.addedCharacters.size() == 2 && all.addedArtifacts.size() == 1 && all.skipped.empty());
    const auto* friendAudrey = friendApp.db.findCharactersByName("Audrey Hall", 0).at(0);
    CHECK(friendAudrey->artifactIds.size() == 1);
    CHECK(friendAudrey->relationships.at(0).characterId == friendKlein.id);  // linked again by name
    CHECK(importAllSampleSets(friendApp, friendApp.storage.loadSampleSets()).addedCharacters.empty());

    // The friend links Audrey to someone of their own. Later the set is shared again: Audrey was
    // edited since (newer), Leonard changed but the friend edited theirs more recently.
    Character friendOwn;
    friendOwn.id = friendApp.db.nextCharacterId();
    friendOwn.name = "Fors Wall";
    friendOwn.relationships = {{friendAudrey->id, "Audrey Hall", "Friend", ""}};
    saveCharacter(friendApp, friendOwn, true);
    SampleSet changed = friendApp.storage.loadSampleSets().at(0);
    changed.characters[0].notes = "Now a Sequence 5 Dream Stealer.";
    changed.characters[0].updatedAt = "2999-01-01 00:00:00";
    changed.characters[1].notes = "An older edit.";
    changed.characters[1].updatedAt = "2000-01-01 00:00:00";
    friendApp.storage.saveSampleSet(changed, "maxs_characters.json");
    const SampleImportReport update = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(update.addedCharacters.empty() && update.updated == std::vector<std::string>{"Audrey Hall"});
    CHECK(friendApp.db.characters.size() == 4);
    const Character* updatedAudrey = friendApp.db.findCharactersByName("Audrey Hall", 0).at(0);
    CHECK(updatedAudrey->id == friendAudrey->id && updatedAudrey->notes == "Now a Sequence 5 Dream Stealer.");
    CHECK(updatedAudrey->updatedAt == "2999-01-01 00:00:00" && updatedAudrey->artifactIds.size() == 1);
    CHECK(std::any_of(updatedAudrey->relationships.begin(), updatedAudrey->relationships.end(),
                      [&](const Relationship& r) { return r.characterId == friendOwn.id && r.type == "Friend"; }));
    CHECK(friendApp.db.findCharactersByName("Leonard Mitchell", 0).at(0)->notes.empty());  // theirs is newer
    // Importing again changes nothing.
    const SampleImportReport settled = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(settled.updated.empty() && settled.addedCharacters.empty() && settled.addedArtifacts.empty());

    // Saving under the title of a set that's there replaces its file, whatever the file is called,
    // and starts with that set's records ticked.
    fs::rename(other / "samples" / "maxs_characters.json", other / "samples" / "max.json");
    const auto renamed = friendApp.storage.loadSampleSets();
    CHECK(sampleSetFileFor(renamed, "max's CHARACTERS") == "max.json");
    CHECK(sampleSetFileFor(renamed, "Something new") == "something_new.json");
    std::vector<int> pickedCharacters, pickedArtifacts;
    defaultSharePicks(friendApp.db, renamed, "max.json", pickedCharacters, pickedArtifacts);
    CHECK(pickedCharacters.size() == 2 && pickedArtifacts.size() == 1);
    CHECK(std::find(pickedCharacters.begin(), pickedCharacters.end(), friendOwn.id) == pickedCharacters.end());
    defaultSharePicks(friendApp.db, renamed, "something_new.json", pickedCharacters, pickedArtifacts);
    CHECK(pickedCharacters.size() == 2 && pickedArtifacts.empty());  // Klein and Fors, in no set yet
    fs::remove_all(root);
}

// The sample sets that ship in data/samples load, fit the rules, and import into empty saves cleanly.
// Sets shared with Settings > Share your own live there too, so the checks allow anything the program
// lets you enter: your own status or relationship type, and stats up to 99 (typed in the console).
static void testShippedSamples() {
    const Storage source(fs::path(LOTM_SOURCE_DIR) / "data");
    Database db;
    source.loadAll(db);
    const std::vector<SampleSet> sets = source.loadSampleSets();
    CHECK(!sets.empty());
    auto known = [](const std::vector<std::string>& list, const std::string& value) {
        return value.empty() || std::find(list.begin(), list.end(), value) != list.end();
    };
    for (const SampleSet& set : sets) {
        const std::string where = set.file.filename().string() + ": ";
        auto fail = [&](const std::string& what) {
            std::cerr << where << what << "\n";
            ++failures;
        };
        if (set.title.empty()) fail("no title");
        auto findSampleCharacter = [&](int id) -> const Character* {
            for (const auto& c : set.characters) {
                if (c.id == id) return &c;
            }
            return nullptr;
        };
        for (const Character& c : set.characters) {
            if (c.name.empty() || c.id <= 0) fail("a character without a name or id");
            if (!c.pathwayId.empty() && !db.findPathway(c.pathwayId)) fail(c.name + ": unknown pathway " + c.pathwayId);
            if (c.sequence < 0 || c.sequence > 9) fail(c.name + ": Sequence out of range");
            if (!c.sefirahId.empty() && !db.findSefirah(c.sefirahId)) fail(c.name + ": unknown Sefirah " + c.sefirahId);
            if (!c.secondPathway.empty() && !db.findPathway(c.secondPathway.pathwayId)) {
                fail(c.name + ": unknown second pathway " + c.secondPathway.pathwayId);
            }
            if (!known(kAlignments, c.alignment)) fail(c.name + ": unknown alignment " + c.alignment);
            if (!known(kRecentActions, c.dossier.recentActions)) fail(c.name + ": unknown recent actions");
            if (!known(kThreatLevels, c.dossier.threatLevel)) fail(c.name + ": unknown threat level " + c.dossier.threatLevel);
            for (int score : c.stats.base) {
                if (score < 1 || score > 99) fail(c.name + ": a base stat outside 1-99");
            }
            for (const SequenceStep& step : c.mixedSequences) {
                if (!db.findPathway(step.pathwayId)) fail(c.name + ": a Sequence from an unknown pathway " + step.pathwayId);
            }
            Character copy = c;
            if (!normalizeCharacter(copy).empty()) fail(c.name + ": has fields that don't apply at its Sequence");
            for (int id : c.artifactIds) {
                const bool found = std::any_of(set.artifacts.begin(), set.artifacts.end(),
                                               [id](const Artifact& a) { return a.id == id; });
                if (!found) fail(c.name + ": holds a Sealed Artifact the set doesn't have");
            }
            for (const Relationship& r : c.relationships) {
                if (r.characterId == 0) continue;
                const Character* other = findSampleCharacter(r.characterId);
                if (!other) {
                    fail(c.name + ": relationship to a missing id");
                    continue;
                }
                const bool back = std::any_of(other->relationships.begin(), other->relationships.end(),
                                              [&](const Relationship& o) {
                                                  return o.characterId == c.id && o.type == reciprocalType(r.type);
                                              });
                if (!back) fail(c.name + " -> " + other->name + ": the relationship isn't listed the other way");
            }
        }
        for (const Artifact& a : set.artifacts) {
            if (a.name.empty() || a.id <= 0) fail("a Sealed Artifact without a name or id");
            if (!a.pathwayId.empty() && !db.findPathway(a.pathwayId)) fail(a.name + ": unknown pathway " + a.pathwayId);
            if (a.sequenceLevel < kUnknownSequence || a.sequenceLevel > 9) fail(a.name + ": Sequence out of range");
        }

        // Importing into empty saves adds everything and changes no relationship.
        const fs::path root = fs::temp_directory_path() / "lotm_tests_shipped_samples";
        fs::remove_all(root);
        fs::create_directories(root / "data");
        fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", root / "data" / "pathways.json");
        App app{Database{}, Storage(root / "data"), Dice{}};
        app.storage.loadAll(app.db);
        const SampleImportReport report = importSampleSet(app, set);
        CHECK(report.addedCharacters.size() == set.characters.size());
        CHECK(report.addedArtifacts.size() == set.artifacts.size());
        size_t before = 0, after = 0;
        for (const auto& c : set.characters) before += c.relationships.size();
        for (const auto& c : app.db.characters) after += c.relationships.size();
        if (before != after) fail("importing added or lost relationships");
        fs::remove_all(root);
    }
}

static void testComparison() {
    Database db;
    db.pathways.push_back(warrior());
    Artifact glove;
    glove.id = 1;
    glove.name = "Creeping Hunger";
    db.artifacts.push_back(glove);

    Character a;
    a.id = 1;
    a.name = "Dunn Smith";
    a.pathwayId = "warrior";
    a.sequence = 5;
    a.stats.base = {14, 10, 12, 10, 10, 8};
    a.artifactIds = {1};
    a.relationships.push_back({2, "Leonard Mitchell", "Superior", "his captain"});
    Character b;
    b.id = 2;
    b.name = "Leonard Mitchell";
    b.stats.base = {10, 10, 10, 10, 10, 10};
    b.stats.hpIncluded = true;
    b.stats.hp = 30;
    b.relationships.push_back({1, "Dunn Smith", "Subordinate", ""});
    b.relationships.push_back({1, "Dunn Smith", "Friend", ""});
    db.characters = {a, b};

    const Comparison result = buildComparison({&db.characters[0], &db.characters[1]}, db);
    CHECK(result.names == std::vector<std::string>({"Dunn Smith", "Leonard Mitchell"}));
    CHECK(result.sections.size() == 3);
    const CompareRow& sequence = result.sections[0].rows[1];
    CHECK(sequence.label == "Sequence" && sequence.highest == std::vector<bool>({true, false}));
    CHECK(sequence.values[1] == "-");
    const CompareRow& wisdom = result.sections[1].rows[4];
    CHECK(wisdom.label == "Wisdom" && wisdom.highest == std::vector<bool>({false, false}));  // equal: nobody marked
    const CompareRow& hp = result.sections[1].rows[7];
    CHECK(hp.label == "HP" && hp.values[0] == "not used" && hp.highest == std::vector<bool>({false, false}));
    // Rows neither of them has filled in are left out.
    for (const CompareSection& section : result.sections) {
        for (const CompareRow& row : section.rows) CHECK(row.label != "Spirituality" && row.label != "Age");
    }
    const CompareRow& held = result.sections[2].rows[1];
    CHECK(held.values == std::vector<std::string>({"Creeping Hunger", "none"}));
    // Superior on one side and Subordinate on the other is said once; Friend appears too.
    CHECK(result.between.size() == 2);
    CHECK(result.between[0] == "Leonard Mitchell is Dunn Smith's superior (his captain)");
    CHECK(result.between[1] == "Leonard Mitchell and Dunn Smith: Friend");

    const std::string text = renderComparisonText(result);
    CHECK(text.find("Dunn Smith") != std::string::npos && text.find("Leonard Mitchell") != std::string::npos);
    CHECK(text.find("Creeping Hunger *") != std::string::npos);
    CHECK(text.find("Between them") != std::string::npos);
}

static StoryDate date(const std::string& text) { return parseStoryDate(text).value_or(StoryDate{}); }

// Dates are read the way people write them and written back the way the books do.
static void testStoryDates() {
    const auto full = parseStoryDate("3 July 1360");
    CHECK(full && full->epoch == 5 && full->year == 1360 && full->month == 7 && full->day == 3);
    for (const std::string text : {"3 July 1360", "July 1360", "1360", "1200, Fourth Epoch", "29 February 1351"}) {
        CHECK(storyDateText(date(text)) == text);
    }
    CHECK(storyDateText(date("July 3, 1360")) == "3 July 1360");
    CHECK(storyDateText(date("3rd July 1360")) == "3 July 1360");
    CHECK(storyDateText(date("jul 1360")) == "July 1360");
    CHECK(storyDateText(date("Sept. 1349")) == "September 1349");
    CHECK(storyDateText(date("Fourth Epoch 1200")) == "1200, Fourth Epoch");
    CHECK(storyDateText(date("1349 of the Fifth Epoch")) == "1349");
    CHECK(storyDateText(date("Epoch 3, year 12")) == "12, Third Epoch");
    CHECK(epochName(1) == "First Epoch" && epochName(12) == "Epoch 12");

    // No text is no date; text that isn't a date says why.
    const auto none = parseStoryDate("   ");
    CHECK(none && none->empty() && storyDateText(*none).empty());
    std::string problem;
    CHECK(!parseStoryDate("31 February 1360", &problem) && problem == "February has no day 31.");
    CHECK(!parseStoryDate("3 1360", &problem) && problem.find("needs its month") != std::string::npos);
    CHECK(!parseStoryDate("July", &problem) && problem.find("year") != std::string::npos);
    CHECK(!parseStoryDate("July August 1360", &problem) && problem == "It names two months.");
    CHECK(!parseStoryDate("1 2 1360", &problem) && problem.find("more numbers") != std::string::npos);
    CHECK(!parseStoryDate("banana 1360", &problem) && problem.find("\"banana\"") != std::string::npos);
    CHECK(!parseStoryDate("Fourth Epoch Fifth Epoch 1200", &problem) && problem == "It names two epochs.");
    CHECK(parseStoryDate("3 July 1360", &problem) && problem.empty());

    // Earliest first: epochs, then years, a bare year before its months, a month before its days; no date last.
    CHECK(storyDateBefore(date("1200, Fourth Epoch"), date("1")));
    CHECK(storyDateBefore(date("1349"), date("January 1349")));
    CHECK(storyDateBefore(date("July 1349"), date("1 July 1349")));
    CHECK(storyDateBefore(date("30 June 1349"), date("1 July 1349")));
    CHECK(!storyDateBefore(date("1349"), date("1349")));
    CHECK(storyDateBefore(date("1349"), StoryDate{}) && !storyDateBefore(StoryDate{}, date("1349")));
    CHECK(!storyDateBefore(StoryDate{}, StoryDate{}));
    CHECK(sameStoryDate(date("July 1349"), date("Jul 1349")) && !sameStoryDate(date("July 1349"), date("1349")));
    CHECK(sameStoryDate(StoryDate{}, StoryDate{}));

    // Saved as text, and read back the same.
    const nlohmann::json j = date("3 July 1360");
    CHECK(j == "3 July 1360");
    CHECK(sameStoryDate(j.get<StoryDate>(), date("3 July 1360")));
    CHECK(nlohmann::json(StoryDate{}).get<StoryDate>().empty());
}

// A character as they were on a date: their latest stage up to it, on top of how they are now.
static void testCharacterAsOf() {
    Database db;
    db.pathways.push_back(warrior());
    Artifact sword;
    sword.id = 1;
    sword.name = "Sword of Dawn";
    db.artifacts.push_back(sword);
    Character c;
    c.id = 1;
    c.name = "Leonard Mitchell";
    c.pathwayId = "warrior";
    c.sequence = 5;
    c.titles = {"Captain"};
    c.dossier.status = "Active";
    c.affiliation.organization = "Nighthawks";
    c.artifactIds = {1};
    c.nowSince = date("1352");
    CharacterStage later = stageFromNow(c);
    CHECK(later.pathwayId == "warrior" && later.sequence == 5 && later.titles == c.titles);
    CHECK(later.status == "Active" && later.artifactIds == c.artifactIds && later.affiliation.organization == "Nighthawks");
    later.date = date("July 1350");
    later.sequence = 7;
    later.titles.clear();
    CharacterStage first;
    first.date = date("1349");
    first.pathwayId = "warrior";
    first.sequence = 9;
    first.status = "Recruit";
    first.label = "Joins the Nighthawks";
    c.stages = {later, first};  // in any order

    auto asOf = [&](const std::string& when) { return characterAsOf(c, date(when), db); };
    CHECK(asOf("1348").beforeKnown && asOf("1348").stageIndex == 1 && asOf("1348").character.sequence == 9);
    CHECK(!asOf("1349").beforeKnown && asOf("1349").stageIndex == 1);
    CHECK(asOf("1349").character.dossier.status == "Recruit" && asOf("1349").character.artifactIds.empty());
    CHECK(asOf("1349").character.name == "Leonard Mitchell" && asOf("1349").character.affiliation.organization.empty());
    CHECK(asOf("June 1350").stageIndex == 1);
    CHECK(asOf("3 July 1350").stageIndex == 0 && asOf("3 July 1350").character.sequence == 7);
    CHECK(asOf("3 July 1350").character.titles.empty() && asOf("3 July 1350").character.artifactIds == c.artifactIds);
    CHECK(asOf("1351").stageIndex == 0);
    CHECK(asOf("1352").stageIndex == -1 && asOf("1352").character.sequence == 5 && !asOf("1352").beforeKnown);
    CHECK(characterAsOf(c, StoryDate{}, db).stageIndex == -1);  // no date: as they are now
    CHECK(stageLevelText(first, db) == "Sequence 9 Seq9, Warrior pathway");
    CHECK(stageSummary(first, db).find("From 1349") == 0);

    // Without stages they are always as they are now; before nowSince that's before anything known.
    Character plain = c;
    plain.stages.clear();
    CHECK(characterAsOf(plain, date("1340"), db).stageIndex == -1 && characterAsOf(plain, date("1340"), db).beforeKnown);
    plain.nowSince = StoryDate{};
    CHECK(!characterAsOf(plain, date("1340"), db).beforeKnown);

    // Stages are saved with the character.
    const Character back = nlohmann::json(c).get<Character>();
    CHECK(back.stages.size() == 2 && back.stages[1].label == "Joins the Nighthawks");
    CHECK(sameStoryDate(back.stages[0].date, date("July 1350")) && sameStoryDate(back.nowSince, date("1352")));

    // Every date where something changes, earliest first, each once.
    db.characters.push_back(c);
    StoryEvent e;
    e.id = 1;
    e.title = "A fight";
    e.date = date("1349");
    db.events.push_back(e);
    const auto dates = storyDates(db);
    CHECK(dates.size() == 3 && storyDateText(dates[0]) == "1349" && storyDateText(dates[1]) == "July 1350" &&
          storyDateText(dates[2]) == "1352");
}

static Relationship knows(int id, const std::string& name, const std::string& type) { return {id, name, type, ""}; }

// The relationship web: one line per pair, kinds of bond, paths between people, and the layout.
static void testRelationshipWeb() {
    CHECK(bondOf("Lover") == Bond::Love && bondOf("Mentor") == Bond::Teaching && bondOf("Student") == Bond::Teaching);
    CHECK(bondOf("King") == Bond::Allies && bondOf("Sworn brother") == Bond::Family);
    CHECK(bondOf("Old enemy") == Bond::Conflict && bondOf("close FRIEND") == Bond::Friendship);
    CHECK(bondOf("Something odd") == Bond::Other && bondOf("") == Bond::Other);
    CHECK(std::string(bondName(Bond::Allies)) == "Allies and work" && bondColour(Bond::Love).front() == '#');

    Database db;
    db.pathways.push_back(warrior());
    auto add = [&](int id, const std::string& name) {
        Character c;
        c.id = id;
        c.name = name;
        db.characters.push_back(c);
        return &db.characters.back();
    };
    add(1, "Klein Moretti")->relationships = {knows(2, "Audrey Hall", "Mentor"), knows(3, "Alger Wilson", "Ally")};
    add(2, "Audrey Hall")->relationships = {knows(1, "Klein Moretti", "Student"), knows(0, "Fors Wall", "Friend")};
    add(3, "Alger Wilson")->relationships = {knows(1, "Klein Moretti", "Ally"), knows(2, "Audrey Hall", "Old enemy")};
    add(4, "Alista Tudor");

    const Web web = buildWeb(db);
    CHECK(web.nodes.size() == 4 && web.edges.size() == 3);
    const int klein = web.nodeOf(1), audrey = web.nodeOf(2), alger = web.nodeOf(3), alista = web.nodeOf(4);
    CHECK(klein >= 0 && audrey >= 0 && alger >= 0 && alista >= 0 && web.nodeOf(99) == -1 && web.nodeOf(0) == -1);
    const WebEdge* taught = edgeBetween(web, static_cast<size_t>(audrey), static_cast<size_t>(klein));
    CHECK(taught && taught->bond == Bond::Teaching && taught->label() == "Mentor / Student");
    const WebEdge* allies = edgeBetween(web, static_cast<size_t>(klein), static_cast<size_t>(alger));
    CHECK(allies && allies->bond == Bond::Allies && allies->label() == "Ally");
    CHECK(web.nodes[static_cast<size_t>(klein)].connections == 2 && web.nodes[static_cast<size_t>(alista)].connections == 0);
    CHECK(web.nodes[static_cast<size_t>(klein)].badge == "KM" && web.key(static_cast<size_t>(klein)) == "c1");

    // Options: people who aren't saved, no loners, only some kinds of bond, one organization.
    WebOptions options;
    options.unsavedPeople = true;
    const Web withFors = buildWeb(db, options);
    CHECK(withFors.nodes.size() == 5 && withFors.edges.size() == 4);
    CHECK(withFors.key(withFors.nodes.size() - 1) == "n:fors wall" && withFors.nodes.back().characterId == 0);
    options.loners = false;
    options.bonds[static_cast<size_t>(Bond::Conflict)] = false;
    const Web noConflict = buildWeb(db, options);
    CHECK(noConflict.nodes.size() == 4 && noConflict.edges.size() == 3 && noConflict.nodeOf(4) == -1);
    db.characters[0].affiliation.organization = "Tarot Club";
    db.characters[1].affiliation.organization = "tarot club";
    WebOptions club;
    club.organization = "Tarot Club";
    const Web members = buildWeb(db, club);
    CHECK(members.nodes.size() == 2 && members.edges.size() == 1);

    // The shortest chain between two people, in words.
    const auto fors = noConflict.nodes.size() - 1;
    const auto path = shortestPath(noConflict, fors, static_cast<size_t>(noConflict.nodeOf(3)));
    CHECK(path.size() == 4 && noConflict.nodes[path[1]].name == "Audrey Hall" && noConflict.nodes[path[2]].name == "Klein Moretti");
    const std::string words = describePath(noConflict, path);
    CHECK(words.find("Fors Wall") == 0 && words.find("Alger Wilson") != std::string::npos);
    CHECK(shortestPath(web, static_cast<size_t>(klein), static_cast<size_t>(alista)).empty());
    CHECK(shortestPath(web, static_cast<size_t>(klein), static_cast<size_t>(klein)).size() == 1);

    // The layout: the same every time, nobody on top of anyone else, everything a number.
    WebLayout layout, again;
    layout.start(web, db);
    layout.settle(web);
    again.start(web, db);
    again.settle(web);
    CHECK(layout.settled() && layout.x.size() == web.nodes.size() && layout.x == again.x && layout.y == again.y);
    float closest = 1e9f;
    for (size_t i = 0; i < web.nodes.size(); ++i) {
        CHECK(std::isfinite(layout.x[i]) && std::isfinite(layout.y[i]));
        for (size_t j = i + 1; j < web.nodes.size(); ++j) {
            closest = std::min(closest, std::hypot(layout.x[i] - layout.x[j], layout.y[i] - layout.y[j]));
        }
    }
    CHECK(closest > 0.9f);
    // People who know each other end up closer than the one who knows nobody.
    const auto distance = [&](int a, int b) {
        return std::hypot(layout.x[static_cast<size_t>(a)] - layout.x[static_cast<size_t>(b)],
                          layout.y[static_cast<size_t>(a)] - layout.y[static_cast<size_t>(b)]);
    };
    CHECK(distance(klein, audrey) < distance(klein, alista));
    // A rebuilt web keeps everyone where they were and warms up to fit in the newcomer.
    WebLayout adapted = layout;
    adapted.adapt(web, withFors, db);
    CHECK(adapted.x.size() == withFors.nodes.size() && !adapted.settled());
    const int kleinNow = withFors.nodeOf(1);
    CHECK(adapted.x[static_cast<size_t>(kleinNow)] == layout.x[static_cast<size_t>(klein)]);
    adapted.settle(withFors);
    for (size_t i = 0; i < withFors.nodes.size(); ++i) CHECK(std::isfinite(adapted.x[i]) && std::isfinite(adapted.y[i]));

    // As a sheet: the picture (linking to everyone's sheets), then the connections in words.
    const Sheet sheet = buildWebSheet(db, true);
    CHECK(sheet.title == "Who Knows Whom" && !sheet.sections.empty());
    const std::string html = renderHtmlPage("Web", {sheet}, "dark");
    CHECK(html.find("<svg class=\"web") != std::string::npos && html.find("href=\"#c-1\"") != std::string::npos);
    CHECK(html.find("Mentor / Student") != std::string::npos);
    const std::string text = renderText(sheet);
    CHECK(text.find("Klein Moretti") != std::string::npos && text.find("<svg") == std::string::npos);
}

// Story events: saved in events.json, linked to characters by name, on the timeline, and cleaned up
// when a character or a Sealed Artifact they name is deleted.
static void testStoryEvents() {
    const fs::path root = fs::temp_directory_path() / "lotm_tests_events";
    fs::remove_all(root);
    fs::create_directories(root / "data");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", root / "data" / "pathways.json");
    App app{Database{}, Storage(root / "data"), Dice{}};
    app.storage.loadAll(app.db);

    Character klein;
    klein.id = app.db.nextCharacterId();
    klein.name = "Klein Moretti";
    saveCharacter(app, klein, true);
    Character audrey;
    audrey.id = app.db.nextCharacterId();
    audrey.name = "Audrey Hall";
    saveCharacter(app, audrey, true);
    Artifact mask;
    mask.id = app.db.nextArtifactId();
    mask.name = "Mask of Masks";
    saveArtifact(app, mask, true);
    Character savedAudrey = *app.db.findCharacter(audrey.id);
    savedAudrey.artifactIds = {mask.id};
    CharacterStage stage;
    stage.date = date("1349");
    stage.artifactIds = {mask.id};
    savedAudrey.stages = {stage};
    saveCharacter(app, savedAudrey, false);

    StoryEvent gathering;
    gathering.id = app.db.nextEventId();
    gathering.title = "The first gathering";
    gathering.date = date("3 July 1349");
    gathering.kind = "Meeting";
    gathering.place = "Above the grey fog";
    gathering.chapter = "Chapter 2";
    gathering.people = {{klein.id, "", "The Fool"}, {0, "audrey hall", "Justice"}, {0, "Fors Wall", ""}, {0, "", ""}};
    gathering.artifactIds = {mask.id};
    saveEvent(app, gathering, true);
    CHECK(eventCode(gathering.id) == "E-001");
    const StoryEvent* saved = app.db.findEvent(gathering.id);
    CHECK(saved && saved->people.size() == 3 && !saved->createdAt.empty());
    CHECK(saved->people[0].name == "Klein Moretti");  // a saved character's name is filled in
    CHECK(saved->people[1].characterId == audrey.id && saved->people[1].name == "Audrey Hall");  // linked by name
    CHECK(saved->people[2].characterId == 0);
    CHECK(eventSummary(*saved) == "3 July 1349, Meeting, Above the grey fog");
    CHECK(eventPeopleText(app.db, *saved) == "Klein Moretti (The Fool), Audrey Hall (Justice) and Fors Wall");

    StoryEvent undated;
    undated.id = app.db.nextEventId();
    undated.title = "Someday";
    saveEvent(app, undated, true);
    StoryEvent earlier;
    earlier.id = app.db.nextEventId();
    earlier.title = "Klein wakes up";
    earlier.date = date("June 1349");
    earlier.people = {{klein.id, "", ""}};
    saveEvent(app, earlier, true);
    const auto ordered = eventsInOrder(app.db);
    CHECK(ordered.size() == 3 && ordered[0]->title == "Klein wakes up" && ordered[2]->title == "Someday");
    CHECK(eventsOf(app.db, audrey.id).size() == 1 && eventsOf(app.db, klein.id).size() == 2);
    CHECK(fs::exists(app.storage.dataDir() / "events.json"));

    // Everything comes back from disk.
    Database reloaded;
    app.storage.loadAll(reloaded);
    CHECK(reloaded.events.size() == 3);
    const StoryEvent* back = reloaded.findEvent(gathering.id);
    CHECK(back && back->title == "The first gathering" && sameStoryDate(back->date, date("3 July 1349")));
    CHECK(back->chapter == "Chapter 2" && back->people.size() == 3 && back->people[1].role == "Justice");
    CHECK(reloaded.findCharacter(audrey.id)->stages.size() == 1);
    CHECK(reloaded.nextEventId() == 4);

    // The timeline, grouped by year, and the event's own sheet.
    const Sheet timeline = buildTimelineSheet(app.db);
    CHECK(timeline.title == "Timeline" && timeline.sections.size() == 2 && timeline.sections[1].heading == "No date");
    const std::string timelineText = renderText(timeline);
    CHECK(timelineText.find("3 July 1349") != std::string::npos && timelineText.find("Above the grey fog") != std::string::npos);
    CHECK(renderHtmlPage("Timeline", {timeline}, "dark").find("<ol class=\"timeline\">") != std::string::npos);
    CHECK(buildTimelineSheet(app.db, audrey.id).title == "Timeline: Audrey Hall");
    CHECK(renderText(buildEventSheet(*app.db.findEvent(gathering.id), app.db)).find("Mask of Masks") != std::string::npos);
    // Audrey as she was before the gathering doesn't list it.
    const std::string before = renderText(buildCharacterSheetAsOf(*app.db.findCharacter(audrey.id), date("1348"), app.db));
    const std::string after = renderText(buildCharacterSheetAsOf(*app.db.findCharacter(audrey.id), date("1350"), app.db));
    CHECK(before.find("The first gathering") == std::string::npos && after.find("The first gathering") != std::string::npos);

    // The catalogue export starts with the web and the timeline.
    Character savedKlein = *app.db.findCharacter(klein.id);
    savedKlein.relationships = {{audrey.id, "Audrey Hall", "Friend", ""}};
    saveCharacter(app, savedKlein, false);
    const auto sheets = catalogueSheets(app.db);
    CHECK(sheets.size() >= 4 && sheets[0].title == "Who Knows Whom" && sheets[1].title == "Timeline");

    // Deleting the Sealed Artifact takes it out of everyone's hands, their stages and the events.
    deleteArtifact(app, mask.id);
    CHECK(app.db.findCharacter(audrey.id)->artifactIds.empty() && app.db.findCharacter(audrey.id)->stages[0].artifactIds.empty());
    CHECK(app.db.findEvent(gathering.id)->artifactIds.empty());
    // Deleting Klein keeps him in the events by name.
    deleteCharacter(app, klein.id);
    CHECK(app.db.findEvent(gathering.id)->people[0].characterId == 0 && app.db.findEvent(gathering.id)->people[0].name == "Klein Moretti");
    Database afterDeletes;
    app.storage.loadAll(afterDeletes);
    CHECK(afterDeletes.findEvent(gathering.id)->artifactIds.empty() && afterDeletes.findEvent(earlier.id)->people[0].characterId == 0);
    // A new Klein links up again when the event is saved.
    Character newKlein;
    newKlein.id = app.db.nextCharacterId();
    newKlein.name = "Klein Moretti";
    saveCharacter(app, newKlein, true);
    StoryEvent relinked = *app.db.findEvent(gathering.id);
    saveEvent(app, relinked, false);
    CHECK(app.db.findEvent(gathering.id)->people[0].characterId == newKlein.id);

    deleteEvent(app, undated.id);
    CHECK(app.db.events.size() == 2 && !app.db.findEvent(undated.id));
    Database afterEventDelete;
    app.storage.loadAll(afterEventDelete);
    CHECK(afterEventDelete.events.size() == 2);
    fs::remove_all(root);
}

// Events travel in sample sets with the characters in them, and link up when imported.
static void testSampleSetEvents() {
    const fs::path root = fs::temp_directory_path() / "lotm_tests_sample_events";
    fs::remove_all(root);
    fs::create_directories(root / "data");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", root / "data" / "pathways.json");
    App app{Database{}, Storage(root / "data"), Dice{}};
    app.storage.loadAll(app.db);

    Artifact glove;
    glove.id = app.db.nextArtifactId();
    glove.name = "Creeping Hunger";
    saveArtifact(app, glove, true);
    Artifact mask;
    mask.id = app.db.nextArtifactId();
    mask.name = "Mask of Masks";
    saveArtifact(app, mask, true);
    Character klein;
    klein.id = app.db.nextCharacterId();
    klein.name = "Klein Moretti";
    CharacterStage gloved;
    gloved.date = date("1350");
    gloved.artifactIds = {glove.id};  // only held in a stage
    klein.stages = {gloved};
    saveCharacter(app, klein, true);
    Character audrey;
    audrey.id = app.db.nextCharacterId();
    audrey.name = "Audrey Hall";
    saveCharacter(app, audrey, true);

    auto event = [&](const std::string& title, const std::string& when, std::vector<EventPerson> people) {
        StoryEvent e;
        e.id = app.db.nextEventId();
        e.title = title;
        e.date = date(when);
        e.people = std::move(people);
        saveEvent(app, e, true);
        return e.id;
    };
    const int both = event("The first gathering", "3 July 1349", {{klein.id, "", "The Fool"}, {audrey.id, "", "Justice"}});
    app.db.findEvent(both)->artifactIds = {mask.id};
    app.storage.saveEvents(app.db);
    event("Audrey's ball", "1350", {{audrey.id, "", "Host"}});
    event("A storm at sea", "1351", {{0, "Some sailor", ""}});

    // Sharing Klein: his events and the events with nobody saved come along, and so do the glove
    // (held in a stage) and the mask (at his event). Audrey stays a name.
    const SampleSet set = makeSampleSet(app.db, "Klein's Story", "", {klein.id}, {});
    CHECK(set.characters.size() == 1 && set.artifacts.size() == 2);
    CHECK(set.events.size() == 2 && set.events[0].title == "The first gathering" && set.events[1].title == "A storm at sea");
    CHECK(set.events[0].people[1].characterId == 0 && set.events[0].people[1].name == "Audrey Hall");
    const fs::path file = app.storage.saveSampleSet(set, sampleSetFileName(set.title));
    const auto loaded = app.storage.loadSampleSets();
    CHECK(loaded.size() == 1 && loaded[0].events.size() == 2 && loaded[0].events[0].people.size() == 2);

    // A friend with their own Audrey (and their own ids) imports it.
    const fs::path other = root / "other";
    fs::create_directories(other / "samples");
    fs::copy_file(fs::path(LOTM_SOURCE_DIR) / "data" / "pathways.json", other / "pathways.json");
    fs::copy_file(file, other / "samples" / file.filename());
    App friendApp{Database{}, Storage(other), Dice{}};
    friendApp.storage.loadAll(friendApp.db);
    Artifact friendOwn;
    friendOwn.id = friendApp.db.nextArtifactId();
    friendOwn.name = "Their own artifact";
    saveArtifact(friendApp, friendOwn, true);
    Character friendAudrey;
    friendAudrey.id = friendApp.db.nextCharacterId();
    friendAudrey.name = "Audrey Hall";
    saveCharacter(friendApp, friendAudrey, true);
    const SampleImportReport report = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(report.addedCharacters.size() == 1 && report.addedArtifacts.size() == 2 && report.addedEvents.size() == 2);
    const Character* friendKlein = friendApp.db.findCharactersByName("Klein Moretti", 0).at(0);
    const Artifact* friendMask = nullptr;
    const Artifact* friendGlove = nullptr;
    for (const Artifact& a : friendApp.db.artifacts) {
        if (a.name == "Mask of Masks") friendMask = &a;
        if (a.name == "Creeping Hunger") friendGlove = &a;
    }
    CHECK(friendMask && friendGlove && friendMask->id != mask.id);
    CHECK(friendKlein->stages.size() == 1 && friendKlein->stages[0].artifactIds == std::vector<int>{friendGlove->id});
    const StoryEvent* gathering = nullptr;
    for (const StoryEvent& e : friendApp.db.events) {
        if (e.title == "The first gathering") gathering = &e;
    }
    CHECK(gathering && gathering->people.size() == 2);
    CHECK(gathering->people[0].characterId == friendKlein->id && gathering->people[0].role == "The Fool");
    CHECK(gathering->people[1].characterId == friendAudrey.id);  // linked to their Audrey by name
    CHECK(gathering->artifactIds == std::vector<int>{friendMask->id});
    Database friendReloaded;
    friendApp.storage.loadAll(friendReloaded);
    CHECK(friendReloaded.events.size() == 2);

    // Importing again adds nothing; a newer version of an event replaces the saved one, keeping its id.
    const SampleImportReport again = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(again.addedEvents.empty() && again.updated.empty() && friendApp.db.events.size() == 2);
    CHECK(std::count(again.skipped.begin(), again.skipped.end(), "The first gathering") == 1);
    SampleSet changed = friendApp.storage.loadSampleSets().at(0);
    changed.events[0].description = "Three strangers meet above the fog.";
    changed.events[0].updatedAt = "2999-01-01 00:00:00";
    friendApp.storage.saveSampleSet(changed, file.filename().string());
    const int gatheringId = gathering->id;
    const SampleImportReport update = importAllSampleSets(friendApp, friendApp.storage.loadSampleSets());
    CHECK(update.updated == std::vector<std::string>{"The first gathering"} && update.addedEvents.empty());
    const StoryEvent* updated = friendApp.db.findEvent(gatheringId);
    CHECK(updated && updated->description == "Three strangers meet above the fog." && updated->people[1].characterId == friendAudrey.id);
    fs::remove_all(root);
}

int main() {
    testModifiers();
    testTiers();
    testPathwayBonuses();
    testSpeed();
    testAbilitiesAndEligibility();
    testAboveTheSequence();
    testDualPathway();
    testMixedPathways();
    testLongHonorificName();
    testPointBuy();
    testDice();
    testThreatLevel();
    testTwoWayRelationships();
    testJsonRoundTrip();
    testRelationships();
    testPathwayDatabase();
    testSaveLoadAndBackups();
    testCustomPathways();
    testSharedRecords();
    testSampleImport();
    testComparison();
    testListFilters();
    testShippedSamples();
    testSefirot();
    testCustomSefirot();
    testRendering();
    testStoryDates();
    testCharacterAsOf();
    testRelationshipWeb();
    testStoryEvents();
    testSampleSetEvents();
    if (failures == 0) {
        std::cout << "All checks passed.\n";
        return 0;
    }
    std::cout << failures << " check(s) failed.\n";
    return 1;
}
