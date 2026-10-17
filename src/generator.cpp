#include "generator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>

#include "presets.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

// ---- names, a few dozen of each, none of them the books' main characters

struct NamePool {
    std::vector<std::string> male;
    std::vector<std::string> female;
    std::vector<std::string> family;
};

const NamePool& namePool(const std::string& origin) {
    // Loen: the Victorian kingdom of Backlund and Tingen.
    static const NamePool loen = {
        {"Arthur", "Edwin", "Harold", "Thomas", "Cecil", "Lionel", "Albert", "Edgar", "Gilbert", "Reginald", "Percival",
         "Oswald", "Walter", "Frederick", "Herbert", "Leopold", "Ambrose", "Basil", "Clement", "Horace", "Jasper",
         "Rupert", "Silas", "Ernest"},
        {"Margaret", "Eleanor", "Beatrice", "Rosalind", "Clara", "Florence", "Winifred", "Agatha", "Harriet", "Edith",
         "Matilda", "Cordelia", "Evelyn", "Lavinia", "Philippa", "Theodora", "Imogen", "Prudence", "Violet", "Adelaide",
         "Mabel", "Henrietta", "Lucy", "Josephine"},
        {"Ashford", "Blackwood", "Hartley", "Kingsley", "Thornton", "Whitlock", "Harrow", "Pembroke", "Carrow", "Fenwick",
         "Marlowe", "Sterling", "Hawthorne", "Calloway", "Lockwood", "Dunmore", "Grimsby", "Pickering", "Wainwright",
         "Holloway", "Ainsworth", "Cartwright", "Fairfax", "Haversham", "Merriweather", "Pennington", "Thackeray",
         "Underhill"}};
    // Intis: the republic of Trier, with French-sounding names.
    static const NamePool intis = {
        {"Jean", "Louis", "Pierre", "Etienne", "Lucien", "Philippe", "Remy", "Bastien", "Theo", "Gaston", "Armand",
         "Julien", "Henri", "Victor", "Marcel", "Antoine", "Felix", "Hugo", "Olivier", "Raoul"},
        {"Camille", "Amelie", "Gabrielle", "Margot", "Sylvie", "Helene", "Colette", "Juliette", "Odette", "Simone",
         "Celeste", "Elise", "Lucienne", "Mireille", "Nadine", "Sabine", "Vivienne", "Isabelle", "Claire", "Manon"},
        {"Dubois", "Moreau", "Laurent", "Fontaine", "Girard", "Marchand", "Rousseau", "Duval", "Lefevre", "Beaumont",
         "Delacroix", "Valois", "Chevalier", "Garnier", "Mercier", "Bonnet", "Lambert", "Fournier", "Renard",
         "Blanchard", "Picard", "Lemaire", "Gautier", "Boucher"}};
    // Feysac: the cold northern empire, Slavic and Nordic names. A family name ending in -ov or -ev takes
    // an -a for a woman (Volkov, Volkova).
    static const NamePool feysac = {
        {"Ivan", "Dmitri", "Boris", "Yuri", "Mikhail", "Sergei", "Viktor", "Oleg", "Pavel", "Anton", "Grigori", "Leif",
         "Sven", "Ragnar", "Erik", "Bjorn", "Konstantin", "Fyodor", "Arkady", "Stepan"},
        {"Anya", "Katya", "Olga", "Irina", "Natalia", "Vera", "Sofia", "Marina", "Galina", "Yelena", "Ingrid", "Sigrid",
         "Astrid", "Freya", "Ludmila", "Tatiana", "Daria", "Ksenia", "Alina", "Zoya"},
        {"Volkov", "Sokolov", "Morozov", "Petrov", "Orlov", "Belov", "Zaitsev", "Lebedev", "Kuznetsov", "Medvedev",
         "Popov", "Novikov", "Halvorsen", "Lindqvist", "Berg", "Dahl", "Strand", "Nyberg", "Holm", "Eklund"}};
    if (origin == "Intis") return intis;
    if (origin == "Feysac") return feysac;
    return loen;
}

// ---- a line about them: "A cautious dock worker who never sits with their back to a door."

const std::vector<std::string> kTemperaments = {
    "cautious", "cheerful",     "quiet",       "hot-tempered", "curious", "proud",    "kind-hearted",
    "ruthless", "ambitious",    "weary",       "reckless",     "devout",  "cynical",  "gentle",
    "stubborn", "sharp-tongued", "soft-spoken", "nervous",     "charming", "secretive"};
const std::vector<std::string> kOccupations = {
    "clerk",         "dock worker",   "journalist",    "private detective", "schoolteacher",  "pharmacist",
    "tutor",         "street performer", "bartender",  "priest",            "factory worker", "antique dealer",
    "sailor",        "retired soldier", "mercenary",   "librarian",         "fortune teller", "gambler",
    "servant in a great house",        "police constable", "tailor", "coal miner",        "bookbinder", "bank teller",
    "doctor",        "student",       "painter",       "cab driver",        "adventurer", "smuggler"};
const std::vector<std::string> kHabits = {
    "hums when nervous",
    "collects old coins",
    "owes money to the wrong people",
    "keeps a diary in code",
    "is afraid of mirrors",
    "feeds every stray cat in the street",
    "never sits with their back to a door",
    "writes letters to a sibling abroad",
    "cannot resist a puzzle",
    "keeps a revolver under the pillow",
    "talks to the dead, or says so",
    "lies about where they grew up",
    "is saving up for a ticket across the sea",
    "reads tarot cards for strangers",
    "sleeps with a lamp on",
    "tips far too generously",
    "trusts nobody in a uniform",
    "has a laugh you can hear across a pub",
    "is looking for a lost brother",
    "knows every alley in the city"};

const std::string& pick(Dice& dice, const std::vector<std::string>& list) {
    return list[static_cast<size_t>(dice.roll(static_cast<int>(list.size())) - 1)];
}

std::string lowercase(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool nameTaken(const Database& db, const std::string& name) {
    return std::any_of(db.characters.begin(), db.characters.end(),
                       [&](const Character& c) { return lowercase(c.name) == lowercase(name); });
}

std::string withArticle(const std::string& word) {
    const char first = static_cast<char>(std::tolower(static_cast<unsigned char>(word.empty() ? 'x' : word[0])));
    return (std::string("aeiou").find(first) != std::string::npos ? "An " : "A ") + word;
}

void rollName(Character& c, const Database& db, const GeneratorOptions& options, Dice& dice) {
    c.gender = dice.roll(2) == 1 ? "Male" : "Female";
    // Each name once among your characters: a second "Edwin Hartley" would stop names typed by hand from
    // finding either of them.
    for (int attempt = 0; attempt < 40; ++attempt) {
        const std::string origin = options.origin.empty() ? pick(dice, kNameOrigins) : options.origin;
        c.name = randomName(dice, origin, c.gender);
        if (!nameTaken(db, c.name)) break;
    }
    c.age = std::to_string(16 + dice.roll(45));  // 17 to 61
}

std::vector<std::string> without(std::vector<std::string> list, std::initializer_list<const char*> drop) {
    for (const char* item : drop) list.erase(std::remove(list.begin(), list.end(), item), list.end());
    return list;
}

// Picks their looks from what fits them: no beard or bald head on a woman, no grey hair or lined face at
// 25, nothing youthful at 60, no bun in cropped hair. With `keepWhatFits`, a look they have stays while
// it still fits, so after a new name, gender and age only what no longer fits changes.
void rollLooks(Character& c, Dice& dice, bool keepWhatFits) {
    const bool woman = c.gender == "Female";
    const int age = std::atoi(c.age.c_str());
    Appearance& a = c.appearance;
    auto choose = [&](std::string& value, const std::vector<std::string>& choices) {
        if (keepWhatFits && std::find(choices.begin(), choices.end(), value) != choices.end()) return;
        value = pick(dice, choices);
    };
    std::vector<std::string> hair = kHair;
    if (woman) hair = without(hair, {"Bald"});
    if (age > 0 && age < 30) hair = without(hair, {"Grey", "White"});
    choose(a.hair, hair);
    if (a.hair == "Bald") {
        a.hairLength.clear();
        a.hairStyle.clear();
    } else {
        choose(a.hairLength, kHairLength);
        if (a.hairLength == "Shaved") {
            a.hairStyle.clear();
        } else if (a.hairLength == "Cropped" || a.hairLength == "Short") {
            choose(a.hairStyle, without(kHairStyle, {"Tied back", "Braided", "In a bun"}));
        } else {
            choose(a.hairStyle, kHairStyle);
        }
    }
    choose(a.eyes, kEyes);
    choose(a.skin, kSkin);
    std::vector<std::string> faces = kFace;
    if (woman) faces = without(faces, {"Full beard", "Moustache", "Clean-shaven"});
    if (age > 0 && age < 35) faces = without(faces, {"Lined with age"});
    if (age >= 50) faces = without(faces, {"Youthful"});
    choose(a.face, faces);
    choose(a.build, kBuild);
    choose(a.height, kHeight);
    choose(a.voice, kVoice);
    choose(a.clothing, woman ? without(kClothing, {"Gentleman's suit and top hat"}) : kClothing);
    // Half of them have something that stands out.
    if (!keepWhatFits) a.distinguishingMark = dice.roll(2) == 1 ? pick(dice, kMarks) : "";
}

void rollPower(Character& c, const Database& db, const GeneratorOptions& options, Dice& dice) {
    // Everything that only fits a particular pathway or Sequence starts over.
    c.pathwayId.clear();
    c.sequence = 9;
    c.mixedSequences.clear();
    c.aboveSequence = false;
    c.beyonderCharacteristics = 1;
    c.hasUniqueness = false;
    c.uniquenessForm.clear();
    c.uniquenessAbilities.clear();
    c.secondPathway = SecondPathway{};
    c.sefirahId.clear();
    c.honorificName.clear();
    const auto pathways = generatorPathways(db, options);
    if (!pathways.empty()) {
        const Pathway& p = *pathways[static_cast<size_t>(dice.roll(static_cast<int>(pathways.size())) - 1)];
        const int strongest = std::clamp(std::min(options.strongest, options.weakest), 0, 9);
        const int weakest = std::clamp(std::max(options.strongest, options.weakest), 0, 9);
        std::vector<int> sequences;
        for (int s = weakest; s >= strongest; --s) {
            if (p.findSequence(s)) sequences.push_back(s);
        }
        c.pathwayId = p.id;
        c.sequence = sequences[static_cast<size_t>(dice.roll(static_cast<int>(sequences.size())) - 1)];
    }
    c.affiliation = Affiliation{};
    if (const Organization* o = db.findOrganization(options.organizationId)) {
        c.affiliation.organization = o->name;
        // Any rank but the highest, which is usually its leader's.
        if (o->ranks.size() == 1) c.affiliation.rank = o->ranks.front();
        if (o->ranks.size() > 1) {
            c.affiliation.rank = o->ranks[static_cast<size_t>(dice.roll(static_cast<int>(o->ranks.size()) - 1))];
        }
    }
}

void rollStats(Character& c, Dice& dice) {
    for (int& stat : c.stats.base) stat = dice.roll4d6DropLowest().total;
    c.stats.speedBase = dice.roll4d6DropLowest().total;
}

void rollPersonality(Character& c, Dice& dice) {
    c.alignment = pick(dice, kAlignments);
    // Acting the potion keeps most Beyonders sane; some have started to slip. A mortal drank nothing.
    const int mind = dice.roll(20);
    c.stateOfMind = c.pathwayId.empty() || mind <= 15 ? "Sane" : mind <= 19 ? "Not completely sane" : "Half mad";
    c.stateOfMindNote.clear();
    c.shortDescription = withArticle(pick(dice, kTemperaments)) + " " + pick(dice, kOccupations) + " who " +
                         pick(dice, kHabits) + ".";
}

// Spirituality as suggested for their tier and Wisdom, and HP for their tier and Constitution when they
// have HP: both change with the Sequence and the stats.
void suggestPools(Character& c, const Database& db) {
    const Pathway* p = db.findPathway(c.pathwayId);
    c.stats.spirituality = suggestedSpirituality(c, p);
    if (c.stats.hpIncluded) c.stats.hp = suggestedHp(c, p);
}

}  // namespace

std::string generatorPartName(GeneratorPart part) {
    switch (part) {
        case GeneratorPart::Name: return "Name";
        case GeneratorPart::Looks: return "Looks";
        case GeneratorPart::Power: return "Pathway and Sequence";
        case GeneratorPart::Stats: return "Stats";
        case GeneratorPart::Personality: return "Personality";
    }
    return "";
}

std::string generatorPartSummary(const Character& c, GeneratorPart part, const Database& db) {
    auto lower = [](std::string text) {
        if (!text.empty()) text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
        return text;
    };
    auto join = [](const std::vector<std::string>& items) {
        std::string out;
        for (const std::string& item : items) {
            if (item.empty()) continue;
            if (!out.empty()) out += ", ";
            out += item;
        }
        return out;
    };
    switch (part) {
        case GeneratorPart::Name: {
            const std::string who = c.gender == "Female" ? "woman" : c.gender == "Male" ? "man" : "person";
            return c.name + (c.age.empty() ? ", a " + who : ", a " + who + " of " + c.age);
        }
        case GeneratorPart::Looks: {
            const Appearance& a = c.appearance;
            const std::string hair = a.hair == "Bald" ? "Bald" : join({a.hair + " hair", lower(a.hairLength), lower(a.hairStyle)});
            const std::string height = a.height == "Average" || a.height == "Below average" ? a.height + " height" : a.height;
            return join({hair, a.eyes.empty() ? "" : lower(a.eyes) + " eyes", a.skin.empty() ? "" : lower(a.skin) + " skin",
                         lower(a.face), a.build == "Average" ? "average build" : lower(a.build), lower(height),
                         a.voice.empty() ? "" : lower(a.voice) + " voice", lower(a.clothing), lower(a.distinguishingMark)});
        }
        case GeneratorPart::Power: {
            const Pathway* p = db.findPathway(c.pathwayId);
            std::string out = c.pathwayId.empty() ? "An ordinary mortal"
                                                  : (p ? p->name : c.pathwayId) + ", " + sequenceLabel(p, c.sequence);
            if (!c.affiliation.organization.empty()) out += "; " + join({c.affiliation.organization, c.affiliation.rank});
            return out;
        }
        case GeneratorPart::Stats: {
            std::string out;
            for (size_t i = 0; i < kStatCount; ++i) {
                out += std::string(kStatCodes[i]) + " " + std::to_string(c.stats.base[i]) + ", ";
            }
            return out + "Speed " + std::to_string(c.stats.speedBase);
        }
        case GeneratorPart::Personality:
            return join({c.alignment, lower(c.stateOfMind)}) + (c.shortDescription.empty() ? "" : ". " + c.shortDescription);
    }
    return "";
}

std::string randomName(Dice& dice, const std::string& origin, const std::string& gender) {
    const NamePool& pool = namePool(origin);
    const std::string& first = pick(dice, gender == "Female" ? pool.female : pool.male);
    std::string family = pick(dice, pool.family);
    const bool slavic = family.size() > 2 && (family.ends_with("ov") || family.ends_with("ev"));
    if (gender == "Female" && slavic) family += "a";
    return first + " " + family;
}

std::vector<const Pathway*> generatorPathways(const Database& db, const GeneratorOptions& options) {
    std::vector<const Pathway*> found;
    if (options.mortal) return found;
    const int strongest = std::clamp(std::min(options.strongest, options.weakest), 0, 9);
    const int weakest = std::clamp(std::max(options.strongest, options.weakest), 0, 9);
    auto fits = [&](const Pathway& p) {
        for (int s = weakest; s >= strongest; --s) {
            if (p.findSequence(s)) return true;
        }
        return false;
    };
    std::vector<std::string> wanted;
    if (!options.pathwayId.empty()) {
        wanted.push_back(options.pathwayId);
    } else if (const Organization* o = options.organizationPathways ? db.findOrganization(options.organizationId) : nullptr) {
        wanted = o->pathwayIds;
    }
    for (const Pathway& p : db.pathways) {
        const bool asked = wanted.empty() || std::find(wanted.begin(), wanted.end(), p.id) != wanted.end();
        if (asked && fits(p)) found.push_back(&p);
    }
    // An organization known for pathways that aren't here (or don't reach the range): any pathway.
    if (found.empty() && options.pathwayId.empty() && !wanted.empty()) {
        for (const Pathway& p : db.pathways) {
            if (fits(p)) found.push_back(&p);
        }
    }
    return found;
}

Character generateCharacter(const Database& db, const GeneratorOptions& options, Dice& dice) {
    Character c;
    rollName(c, db, options, dice);
    rollLooks(c, dice, false);
    rollPower(c, db, options, dice);
    rollStats(c, dice);
    rollPersonality(c, dice);
    suggestPools(c, db);
    return c;
}

void rerollPart(Character& c, GeneratorPart part, const Database& db, const GeneratorOptions& options, Dice& dice) {
    switch (part) {
        case GeneratorPart::Name:
            rollName(c, db, options, dice);
            rollLooks(c, dice, true);
            break;
        case GeneratorPart::Looks: rollLooks(c, dice, false); break;
        case GeneratorPart::Power: rollPower(c, db, options, dice); break;
        case GeneratorPart::Stats: rollStats(c, dice); break;
        case GeneratorPart::Personality: rollPersonality(c, dice); break;
    }
    if (part == GeneratorPart::Power || part == GeneratorPart::Stats) suggestPools(c, db);
}

}  // namespace lotm
