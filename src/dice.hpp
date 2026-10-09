// Dice rolling for the stat block.
#pragma once

#include <array>
#include <random>
#include <string>

namespace lotm {

struct FourD6Roll {
    std::array<int, 4> dice{};
    int droppedIndex = 0;  // the lowest die, not counted
    int total = 0;

    std::string describe() const;  // "6, 4, 3, (1) = 13"
};

class Dice {
public:
    Dice();                            // seeded from the system's random device
    explicit Dice(unsigned int seed);  // fixed seed, for tests

    int roll(int sides);
    FourD6Roll roll4d6DropLowest();

private:
    std::mt19937 engine_;
};

}  // namespace lotm
