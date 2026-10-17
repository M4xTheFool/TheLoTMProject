// The random Beyonder generator ("Surprise me"): a whole character in one go, a pathway and a Sequence
// in the range asked for, rolled stats, looks from the suggestion lists, a personality, and a name that
// sounds like it comes from Loen, Intis or Feysac. Any part can be rolled again on its own. Shared by
// the console and the app window.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "dice.hpp"
#include "model.hpp"

namespace lotm {

// Where a name comes from; "" in the options is anywhere.
inline const std::vector<std::string> kNameOrigins = {"Loen", "Intis", "Feysac"};

struct GeneratorOptions {
    std::string pathwayId;              // this pathway; empty for any
    bool mortal = false;                // an ordinary mortal instead, with no pathway
    bool organizationPathways = false;  // one of the organization's pathways (any, if it has none)
    int weakest = 9;                    // the Sequence between these two, both included: 9 to 7
    int strongest = 7;
    std::string origin;                 // one of kNameOrigins, or empty for any
    int organizationId = 0;             // they belong to this saved organization, with one of its ranks
};

// The parts that can be rolled again on their own.
enum class GeneratorPart { Name, Looks, Power, Stats, Personality };
inline constexpr int kGeneratorPartCount = 5;
// "Name", "Looks", "Pathway and Sequence", "Stats", "Personality"
std::string generatorPartName(GeneratorPart part);

// One line on what a part came out as, shown next to its re-roll button:
//   Name         "Edwin Hartley, a man of 34"
//   Looks        "Brown hair, short, wavy, grey eyes, pale skin, sharp features, slim, tall, ..."
//   Power        "Seer, Sequence 8: Clown; Nighthawks, Captain", or "An ordinary mortal"
//   Stats        "STR 12, DEX 15, CON 9, INT 14, WIS 11, CHA 13, Speed 10" (before pathway bonuses)
//   Personality  "Chaotic Good, sane. A cautious clerk who keeps a diary in code."
std::string generatorPartSummary(const Character& c, GeneratorPart part, const Database& db);

// A new character (id 0) made from the options. Its name is one no saved character has; its Spirituality
// is the suggested one.
Character generateCharacter(const Database& db, const GeneratorOptions& options, Dice& dice);
// Rolls one part again, leaving the rest as it is:
//   Name         their name, gender and age (the name from the options' origin), and any look that
//                no longer fits them: a woman loses the beard
//   Looks        hair, eyes, skin, face, build, height, voice, clothing and a mark
//   Power        pathway and Sequence (as the options say), and their organization and rank
//   Stats        the six stats and Speed, each 4d6 dropping the lowest
// After Power or Stats, Spirituality (and HP, for one with HP) is set to what is suggested for them.
//   Personality  alignment, state of mind and a line about them
void rerollPart(Character& c, GeneratorPart part, const Database& db, const GeneratorOptions& options, Dice& dice);
// The pathways the options allow, in the order the database lists them. Empty for a mortal, or when no
// pathway has a Sequence in the range.
std::vector<const Pathway*> generatorPathways(const Database& db, const GeneratorOptions& options);
// A name like "Edwin Hartley", "Camille Laurent" or "Anya Volkova", for "Male" or "Female".
std::string randomName(Dice& dice, const std::string& origin, const std::string& gender);

}  // namespace lotm
