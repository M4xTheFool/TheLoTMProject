// A colour for every pathway group, shared by the app window and the HTML export so a pathway looks
// the same everywhere: Fool / Door / Error violet, Red Priest / Demoness crimson, and so on.
#pragma once

#include <string>

#include "model.hpp"

namespace lotm {

struct Rgb {
    int r = 150;
    int g = 145;
    int b = 138;
};

// The colour of the pathway's group, bright enough for a dark background. Your own pathways get one
// made from their group's name (or their own), the same on every computer; mortals and pathways
// that can't be found are grey.
Rgb pathwayRgb(const Database& db, const std::string& pathwayId);
// An organization's colour: its first pathway's, or else its leader's pathway's, or else one made from
// its name, the same on every computer.
Rgb organizationRgb(const Database& db, const Organization& o);
// The same colour darkened, so it reads on a light background.
Rgb onLightBackground(const Rgb& colour);
std::string hexColour(const Rgb& colour);  // "#a080dc"

}  // namespace lotm
