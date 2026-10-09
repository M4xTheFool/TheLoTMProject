// Small self-contained checks for the rules, the save format and the exports.
// Run with: ctest --test-dir build -C Release   (or run build/lotm_tests directly)
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

#include "dice.hpp"
#include "model_json.hpp"
#include "render.hpp"
#include "rules.hpp"
#include "sheet.hpp"
#include "storage.hpp"

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
    c.hasUniqueness = false;
    auto notes = normalizeCharacter(c);
    CHECK(c.honorificName.empty());
    CHECK(c.uniquenessForm.empty());
    CHECK(notes.size() == 2);
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
    c.relationships = {{2, "Rival", "Old grudge"}};
    nlohmann::json j = c;
    Character back = j.get<Character>();
    CHECK(back.name == c.name);
    CHECK(back.stats.base == c.stats.base);
    CHECK(back.stats.hp == 22);
    CHECK(back.artifactIds == c.artifactIds);
    CHECK(back.relationships.size() == 1 && back.relationships[0].type == "Rival");

    // Older files with missing fields still load, using defaults.
    Character partial = nlohmann::json::parse(R"({"id": 3, "name": "Old Save"})").get<Character>();
    CHECK(partial.name == "Old Save");
    CHECK(partial.sequence == 9);
    CHECK(partial.stats.base[0] == 10);
}

static void testPathwayDatabase() {
    const fs::path dataDir = fs::path(LOTM_SOURCE_DIR) / "data";
    Database db;
    Storage(dataDir).loadAll(db);
    CHECK(db.pathways.size() == 22);
    for (const auto& p : db.pathways) {
        CHECK(p.sequences.size() == 10);
        CHECK(statIndex(p.primaryStat) >= 0);
        CHECK(statIndex(p.secondaryStat) >= 0);
        CHECK(p.speedGrade == "Slow" || p.speedGrade == "Average" || p.speedGrade == "Fast" ||
              p.speedGrade == "Very fast");
        for (int s = 0; s <= 9; ++s) CHECK(p.findSequence(s) != nullptr);
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
    db.characters.push_back(c);

    const Sheet sheet = buildCharacterSheet(c, db);
    const std::string text = renderText(sheet);
    CHECK(text.find("LEONARD & CO") != std::string::npos);
    CHECK(text.find("Sword <of> Dawn") != std::string::npos);
    CHECK(text.find("HP: 40") != std::string::npos);
    CHECK(text.find("Sequence 3: Seq3") != std::string::npos);

    const std::string html = renderHtmlPage("Test", {sheet, buildArtifactSheet(a, db)}, "dark");
    CHECK(html.find("Leonard &amp; Co") != std::string::npos);
    CHECK(html.find("Sword &lt;of&gt; Dawn") != std::string::npos);
    CHECK(html.find("<of>") == std::string::npos);
    CHECK(html.find("data-theme=\"dark\"") != std::string::npos);
    CHECK(html.find("class=\"contents\"") != std::string::npos);  // table of contents for 2 sheets

    const std::string md = renderMarkdown(sheet);
    CHECK(md.find("# Leonard & Co") != std::string::npos);
    CHECK(md.find("| Strength |") != std::string::npos);

    // Held By shows on the artifact sheet.
    const std::string artifactText = renderText(buildArtifactSheet(a, db));
    CHECK(artifactText.find("Leonard & Co (C-001)") != std::string::npos);
}

int main() {
    testModifiers();
    testTiers();
    testPathwayBonuses();
    testSpeed();
    testAbilitiesAndEligibility();
    testPointBuy();
    testDice();
    testJsonRoundTrip();
    testPathwayDatabase();
    testSaveLoadAndBackups();
    testRendering();
    if (failures == 0) {
        std::cout << "All checks passed.\n";
        return 0;
    }
    std::cout << failures << " check(s) failed.\n";
    return 1;
}
