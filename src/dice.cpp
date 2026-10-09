#include "dice.hpp"

#include <algorithm>

namespace lotm {

std::string FourD6Roll::describe() const {
    std::string text;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) text += ", ";
        if (i == droppedIndex) {
            text += "(" + std::to_string(dice[i]) + ")";
        } else {
            text += std::to_string(dice[i]);
        }
    }
    return text + " = " + std::to_string(total);
}

Dice::Dice() : engine_(std::random_device{}()) {}

Dice::Dice(unsigned int seed) : engine_(seed) {}

int Dice::roll(int sides) {
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(engine_);
}

FourD6Roll Dice::roll4d6DropLowest() {
    FourD6Roll r;
    for (auto& d : r.dice) d = roll(6);
    r.droppedIndex = static_cast<int>(std::min_element(r.dice.begin(), r.dice.end()) - r.dice.begin());
    for (int i = 0; i < 4; ++i) {
        if (i != r.droppedIndex) r.total += r.dice[i];
    }
    return r;
}

}  // namespace lotm
