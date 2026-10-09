// Ready-made choices offered by the character creator, the pathway editor and the app window.
// Every list is a suggestion: the user can always type their own value instead.
#pragma once

#include <string>
#include <vector>

namespace lotm {

inline const std::vector<std::string> kHair = {"Black", "Brown", "Blond", "Red", "Grey", "White", "Silver", "Bald"};
inline const std::vector<std::string> kHairLength = {"Shaved", "Cropped", "Short", "Chin-length", "Shoulder-length",
                                                     "Long", "Very long"};
inline const std::vector<std::string> kHairStyle = {"Straight",      "Wavy",      "Curly",   "Coiled",
                                                    "Slicked back",  "Neatly parted", "Tied back", "Braided",
                                                    "In a bun",      "Messy"};
inline const std::vector<std::string> kEyes = {"Brown", "Blue", "Green", "Grey", "Hazel", "Black", "Amber", "Red"};
inline const std::vector<std::string> kSkin = {"Pale",  "Fair", "Olive",     "Tanned",
                                               "Brown", "Dark", "Weathered", "Freckled"};
inline const std::vector<std::string> kFace = {"Sharp features", "Soft features", "Round face", "Gaunt", "Square jaw",
                                               "Full beard", "Moustache", "Clean-shaven", "Youthful", "Lined with age"};
inline const std::vector<std::string> kVoice = {"Deep", "Soft", "Raspy", "Melodic", "Booming", "Quiet", "Accented",
                                                "Flat and calm"};
inline const std::vector<std::string> kBuild = {"Slim", "Average", "Athletic", "Muscular", "Stocky", "Heavy", "Frail"};
inline const std::vector<std::string> kHeight = {"Short", "Below average", "Average", "Tall", "Very tall"};
inline const std::vector<std::string> kClothing = {"Gentleman's suit and top hat", "Worker's clothes", "Church robes",
                                                   "Sailor's coat",  "Noble finery",     "Detective's coat and hat",
                                                   "Long dark cloak"};
inline const std::vector<std::string> kMarks = {"Scar", "Monocle", "Tattoo", "Burn mark", "Mismatched eyes",
                                                "Missing finger", "Walking cane"};
inline const std::vector<std::string> kAlignments = {"Lawful Good", "Neutral Good", "Chaotic Good",
                                                     "Lawful Neutral", "True Neutral", "Chaotic Neutral",
                                                     "Lawful Evil", "Neutral Evil", "Chaotic Evil"};
inline const std::vector<std::string> kOrganizations = {
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
// Mentor/Student, Parent/Child and Superior/Subordinate flip on the other character's sheet (relations.cpp).
inline const std::vector<std::string> kRelationshipTypes = {
    "Lover",  "Spouse", "Close friend", "Friend",  "Acquaintance", "Ally",      "Colleague",   "Rival", "Enemy",
    "Mentor", "Student", "Parent",      "Child",   "Sibling",      "Relative",  "Superior",    "Subordinate"};
inline const std::vector<std::string> kStatuses = {"Active", "At large", "Under watch", "Cooperating", "In custody",
                                                   "Missing", "Deceased", "Unknown"};
inline const std::vector<std::string> kSpeedGrades = {"Slow", "Average", "Fast", "Very fast"};
inline const std::vector<std::string> kHpModes = {"ask", "always", "never"};
inline const std::vector<std::string> kExportThemes = {"auto", "light", "dark"};

}  // namespace lotm
