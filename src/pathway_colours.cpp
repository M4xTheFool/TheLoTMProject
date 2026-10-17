#include "pathway_colours.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>

namespace lotm {

namespace {

// FNV-1a: a simple text hash that gives the same number on every computer and compiler.
std::uint64_t textHash(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

// Hue, saturation and value from 0 to 1, to red, green and blue from 0 to 255.
Rgb fromHsv(float h, float s, float v) {
    const float sector = std::fmod(h, 1.0f) * 6.0f;
    const int i = static_cast<int>(sector);
    const float f = sector - static_cast<float>(i);
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    float r = v, g = t, b = p;
    switch (i) {
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;
    }
    auto channel = [](float x) { return static_cast<int>(std::lround(x * 255.0f)); };
    return {channel(r), channel(g), channel(b)};
}

}  // namespace

Rgb pathwayRgb(const Database& db, const std::string& pathwayId) {
    static const std::map<std::string, Rgb> kGroups = {
        {"Fool / Door / Error", {160, 128, 220}},
        {"Visionary / Sun / Tyrant / White Tower / Hanged Man", {236, 160, 78}},
        {"Darkness / Death / Twilight Giant", {112, 140, 220}},
        {"Red Priest / Demoness", {222, 90, 90}},
        {"Mother / Moon", {122, 196, 118}},
        {"Abyss / Chained", {196, 98, 150}},
        {"Black Emperor / Justiciar", {160, 174, 192}},
        {"Hermit / Paragon", {84, 190, 190}},
        {"Wheel of Fortune", {214, 200, 96}},
    };
    const Pathway* p = db.findPathway(pathwayId);
    if (!p) return {};
    if (auto it = kGroups.find(p->group); it != kGroups.end()) return it->second;
    const std::string key = p->group.empty() ? p->id : p->group;
    return fromHsv(static_cast<float>(textHash(key) % 360) / 360.0f, 0.5f, 0.86f);
}

Rgb organizationRgb(const Database& db, const Organization& o) {
    for (const auto& id : o.pathwayIds) {
        if (db.findPathway(id)) return pathwayRgb(db, id);
    }
    if (const Character* leader = o.leaderId != 0 ? db.findCharacter(o.leaderId) : nullptr) {
        if (db.findPathway(leader->pathwayId)) return pathwayRgb(db, leader->pathwayId);
    }
    std::string name;
    for (unsigned char ch : o.name) name += static_cast<char>(std::tolower(ch));
    return fromHsv(static_cast<float>(textHash("organization " + name) % 360) / 360.0f, 0.45f, 0.84f);
}

Rgb onLightBackground(const Rgb& colour) {
    auto darken = [](int channel) { return static_cast<int>(std::lround(channel * 0.62)); };
    return {darken(colour.r), darken(colour.g), darken(colour.b)};
}

std::string hexColour(const Rgb& colour) {
    char buffer[8];
    std::snprintf(buffer, sizeof buffer, "#%02x%02x%02x", colour.r & 0xff, colour.g & 0xff, colour.b & 0xff);
    return buffer;
}

}  // namespace lotm
