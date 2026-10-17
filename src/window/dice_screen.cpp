// Dice tab: a check for one character against a difficulty, a contest between two, or any dice written
// like 2d6+3. The dice are thrown onto a tray, tumble and land; a natural 20 bursts gold and a natural 1
// shakes red. Every roll is listed below the tray until the window is closed.
#include <algorithm>
#include <cmath>

#include "rules.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTumbleSeconds = 1.05f;  // from leaving the hand to coming to rest
constexpr size_t kLogLength = 60;

// A number from 0 to 1, always the same for the same inputs: how each die tumbles and the faces that flicker past.
float noise(unsigned seed, unsigned a, unsigned b = 0) {
    unsigned h = seed * 747796405u + a * 2891336453u + b * 277803737u + 0x9E3779B9u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

ImVec4 mixColour(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

// The character a roll is for: as being edited on the Characters tab, or as saved.
const Character* rollerFor(const WindowState& w, int id) {
    if (id == 0) return nullptr;
    const auto& draft = w.characters.draft;
    if (draft.open && !draft.isNew && draft.value.id == id) return &draft.value;
    return w.app.db.findCharacter(id);
}

std::string lower(std::string text) { return ui::toLower(std::move(text)); }

// The colour each side of a contest is drawn in, as on the Compare tab.
ImVec4 sideColour(int side) { return palette().series[static_cast<size_t>(side) % palette().series.size()]; }

ImVec4 toneColour(DiceTone tone) {
    switch (tone) {
        case DiceTone::Success: return palette().success;
        case DiceTone::Danger: return palette().danger;
        case DiceTone::Muted: return palette().muted;
        case DiceTone::First: return sideColour(0);
        case DiceTone::Second: return sideColour(1);
        case DiceTone::Gold: break;
    }
    return palette().gold;
}

DiceTone checkTone(const CheckOutcome& o) {
    if (o.roll.natural() == 20) return DiceTone::Gold;
    return o.success ? DiceTone::Success : DiceTone::Danger;
}

DiceTone contestTone(const ContestOutcome& o) {
    return o.winner < 0 ? DiceTone::Muted : o.winner == 0 ? DiceTone::First : DiceTone::Second;
}

ImVec4 checkColour(const CheckOutcome& o) { return toneColour(checkTone(o)); }
ImVec4 contestColour(const ContestOutcome& o) { return toneColour(contestTone(o)); }

// ---------------------------------------------------------------- the dice

// One die, lying flat and turned by `angle`: its outline for the number of sides, its facets, and the number
// (pips on a d6). Drawn upright, then every point is turned, the number with it.
void drawDie(const WindowState& w, ImDrawList* draw, ImVec2 c, float r, float angle, int sides, int value,
             const ImVec4& accent, float alpha) {
    const int first = draw->VtxBuffer.Size;
    const ImVec4 face = mixColour(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg), accent, 0.17f);
    const ImU32 fill = colourU32(face, alpha);
    const ImU32 edge = colourU32(accent, alpha);
    const ImU32 facet = colourU32(accent, 0.42f * alpha);
    const float thick = std::max(1.5f, r * 0.07f), thin = std::max(1.0f, r * 0.035f);
    auto ring = [&](int n, float radius, float start) {
        std::vector<ImVec2> points;
        for (int k = 0; k < n; ++k) {
            const float a = start + 2.0f * kPi * static_cast<float>(k) / static_cast<float>(n);
            points.push_back(ImVec2(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius));
        }
        return points;
    };
    auto outline = [&](const std::vector<ImVec2>& points) {
        draw->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fill);
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), edge, ImDrawFlags_Closed, thick);
    };
    ImVec2 textAt = c;
    float textScale = 0.9f;
    switch (sides) {
        case 4: {
            outline(ring(3, r * 1.18f, -kPi / 2.0f));
            textAt.y += r * 0.18f;
            textScale = 0.62f;
            break;
        }
        case 6: {
            const float half = r * 0.86f;
            draw->AddRectFilled(ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half), fill, r * 0.24f);
            draw->AddRect(ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half), edge, r * 0.24f, 0, thick);
            break;
        }
        case 8: {
            const auto points = ring(4, r * 1.14f, -kPi / 2.0f);
            outline(points);
            draw->AddLine(points[3], points[1], facet, thin);
            textAt.y -= r * 0.28f;
            textScale = 0.6f;
            break;
        }
        case 10:
        case 100: {
            const std::vector<ImVec2> points = {ImVec2(c.x, c.y - r * 1.14f), ImVec2(c.x + r * 1.02f, c.y + r * 0.1f),
                                                ImVec2(c.x, c.y + r * 0.98f), ImVec2(c.x - r * 1.02f, c.y + r * 0.1f)};
            outline(points);
            const ImVec2 low(c.x, c.y + r * 0.5f);
            draw->AddLine(points[1], low, facet, thin);
            draw->AddLine(points[3], low, facet, thin);
            draw->AddLine(low, points[2], facet, thin);
            textAt.y -= r * 0.18f;
            textScale = sides == 100 ? 0.5f : 0.62f;
            break;
        }
        case 12: {
            const auto outer = ring(5, r * 1.1f, -kPi / 2.0f);
            const auto inner = ring(5, r * 0.62f, -kPi / 2.0f);
            outline(outer);
            draw->AddPolyline(inner.data(), 5, facet, ImDrawFlags_Closed, thin);
            for (size_t k = 0; k < 5; ++k) draw->AddLine(inner[k], outer[k], facet, thin);
            textScale = 0.58f;
            break;
        }
        case 20: {
            // The way a d20 is usually drawn: a hexagon around a triangle, each corner joined to its neighbours.
            const auto hex = ring(6, r * 1.12f, -kPi / 2.0f);
            const auto tri = ring(3, r * 0.66f, -kPi / 2.0f);
            outline(hex);
            draw->AddPolyline(tri.data(), 3, facet, ImDrawFlags_Closed, thin);
            const int joins[3][3] = {{0, 1, 5}, {2, 1, 3}, {4, 3, 5}};  // the hexagon corners each triangle corner meets
            for (int k = 0; k < 3; ++k) {
                for (int j : joins[k]) draw->AddLine(tri[static_cast<size_t>(k)], hex[static_cast<size_t>(j)], facet, thin);
            }
            textAt.y += r * 0.1f;
            textScale = 0.5f;
            break;
        }
        default: {
            draw->AddCircleFilled(c, r, fill, 32);
            draw->AddCircle(c, r, edge, 32, thick);
            textScale = 0.7f;
            break;
        }
    }
    if (sides == 6 && value >= 1 && value <= 6) {
        // Pips, on a grid of three by three.
        static const int pips[7][9] = {{0}, {0, 0, 0, 0, 1, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 1},
                                       {1, 0, 0, 0, 1, 0, 0, 0, 1}, {1, 0, 1, 0, 0, 0, 1, 0, 1},
                                       {1, 0, 1, 0, 1, 0, 1, 0, 1}, {1, 0, 1, 1, 0, 1, 1, 0, 1}};
        for (int k = 0; k < 9; ++k) {
            if (!pips[value][k]) continue;
            const ImVec2 at(c.x + static_cast<float>(k % 3 - 1) * r * 0.48f, c.y + static_cast<float>(k / 3 - 1) * r * 0.48f);
            draw->AddCircleFilled(at, r * 0.13f, edge, 12);
        }
    } else {
        const std::string text = std::to_string(value);
        const float digits = text.size() >= 3 ? 0.75f : text.size() == 2 ? 0.88f : 1.0f;
        const float size = r * textScale * digits * 1.25f;
        if (size >= 1.0f) {  // ImGui stops on a text size under a pixel in a debug build
            const ImVec2 textSize = w.fonts.title->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
            draw->AddText(w.fonts.title, size, ImVec2(textAt.x - textSize.x * 0.5f, textAt.y - textSize.y * 0.5f), edge,
                          text.c_str());
        }
    }
    if (angle != 0.0f) {
        const float cs = std::cos(angle), sn = std::sin(angle);
        for (int i = first; i < draw->VtxBuffer.Size; ++i) {
            ImVec2& p = draw->VtxBuffer[i].pos;
            const float dx = p.x - c.x, dy = p.y - c.y;
            p = ImVec2(c.x + dx * cs - dy * sn, c.y + dx * sn + dy * cs);
        }
    }
}

// Where each die comes to rest, and how big they are: one group in the middle, or one for each side of a contest.
struct Resting {
    ImVec2 at;
    float radius = 10.0f;
};

std::vector<Resting> restingPlaces(const DiceThrow& t, ImVec2 at, ImVec2 size, float font) {
    std::vector<Resting> out(t.dice.size());
    const int groups = t.contest ? 2 : 1;
    const float top = t.contest ? font * 2.6f : 0.0f;  // room for the names above a contest
    for (int g = 0; g < groups; ++g) {
        std::vector<size_t> mine;
        for (size_t i = 0; i < t.dice.size(); ++i) {
            if (t.dice[i].side == g || groups == 1) mine.push_back(i);
        }
        if (mine.empty()) continue;
        const float regionWidth = size.x / static_cast<float>(groups);
        const ImVec2 region(at.x + regionWidth * static_cast<float>(g), at.y + top);
        const ImVec2 regionSize(regionWidth, size.y - top - (t.contest ? font * 2.4f : 0.0f));
        const int n = static_cast<int>(mine.size());
        // The number of columns that lets the dice be biggest.
        int columns = 1;
        float radius = 0.0f;
        for (int cols = 1; cols <= n; ++cols) {
            const int rows = (n + cols - 1) / cols;
            const float r = std::min(regionSize.x / static_cast<float>(cols), regionSize.y / static_cast<float>(rows)) * 0.33f;
            if (r > radius) {
                radius = r;
                columns = cols;
            }
        }
        radius = std::min({radius, font * 3.4f, size.y * 0.22f});
        const int rows = (n + columns - 1) / columns;
        const float cell = radius * 2.75f;
        for (int k = 0; k < n; ++k) {
            const int row = k / columns;
            const int inRow = row == rows - 1 ? n - row * columns : columns;
            const int column = k % columns;
            const float x = region.x + regionSize.x * 0.5f + (static_cast<float>(column) - (static_cast<float>(inRow) - 1.0f) * 0.5f) * cell;
            const float y = region.y + regionSize.y * 0.5f + (static_cast<float>(row) - (static_cast<float>(rows) - 1.0f) * 0.5f) * cell;
            out[mine[static_cast<size_t>(k)]] = {ImVec2(x, y), radius};
        }
    }
    return out;
}

// The faint markings on the tray: a double frame and a circle with a six-pointed star, in gold.
void drawTrayCloth(ImDrawList* draw, ImVec2 at, ImVec2 end, float font) {
    const float rounding = font * 0.7f;
    const ImVec4 cloth = mixColour(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), palette().gold, 0.035f);
    draw->AddRectFilled(at, end, colourU32(cloth), rounding);
    draw->AddRect(at, end, colourU32(palette().gold, 0.38f), rounding, 0, 1.5f);
    const float inset = font * 0.45f;
    draw->AddRect(ImVec2(at.x + inset, at.y + inset), ImVec2(end.x - inset, end.y - inset), colourU32(palette().gold, 0.12f),
                  rounding * 0.6f);
    const ImVec2 c((at.x + end.x) * 0.5f, (at.y + end.y) * 0.5f);
    const float r = std::min(end.x - at.x, end.y - at.y) * 0.36f;
    const ImU32 faint = colourU32(palette().gold, 0.06f);
    draw->AddCircle(c, r, faint, 64, 1.5f);
    draw->AddCircle(c, r * 0.93f, faint, 64, 1.0f);
    for (int k = 0; k < 2; ++k) {
        ImVec2 points[3];
        for (int j = 0; j < 3; ++j) {
            const float a = -kPi / 2.0f + static_cast<float>(k) * kPi + static_cast<float>(j) * 2.0f * kPi / 3.0f;
            points[j] = ImVec2(c.x + std::cos(a) * r * 0.93f, c.y + std::sin(a) * r * 0.93f);
        }
        draw->AddPolyline(points, 3, faint, ImDrawFlags_Closed, 1.0f);
    }
}

// A gold burst around a natural 20: a glow, turning rays and sparks falling away.
void drawBurst(ImDrawList* draw, ImVec2 c, float r, float since, unsigned seed) {
    const float t = std::clamp(since / 1.5f, 0.0f, 1.0f);
    const ImVec4 gold = palette().gold;
    // A soft glow that stays while the roll is on show.
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f);
    draw->AddCircleFilled(c, r * (1.45f + 0.08f * pulse), colourU32(gold, 0.10f + 0.05f * pulse), 48);
    if (t >= 1.0f) return;
    const float grow = easeOut(t);
    draw->AddCircleFilled(c, r * (1.2f + 1.3f * grow), colourU32(gold, 0.22f * (1.0f - t)), 48);
    const float turn = static_cast<float>(ImGui::GetTime()) * 0.7f;
    for (int k = 0; k < 14; ++k) {
        const float a = turn + 2.0f * kPi * static_cast<float>(k) / 14.0f;
        const float from = r * 1.25f, to = r * (1.5f + 1.6f * grow) * (k % 2 ? 0.75f : 1.0f);
        draw->AddLine(ImVec2(c.x + std::cos(a) * from, c.y + std::sin(a) * from), ImVec2(c.x + std::cos(a) * to, c.y + std::sin(a) * to),
                      colourU32(gold, 0.75f * (1.0f - t)), std::max(1.0f, r * 0.07f));
    }
    for (unsigned k = 0; k < 22; ++k) {
        const float a = 2.0f * kPi * noise(seed, k, 1);
        const float distance = r * (1.1f + (2.2f + 1.6f * noise(seed, k, 2)) * grow);
        const ImVec2 p(c.x + std::cos(a) * distance, c.y + std::sin(a) * distance + r * 1.4f * t * t);
        draw->AddCircleFilled(p, std::max(1.0f, r * 0.075f * (1.0f - t)), colourU32(gold, 1.0f - t), 8);
    }
}

// The tray: the dice of the last throw, tumbling in or lying where they landed. Returns true when clicked.
bool drawTray(WindowState& w, ImVec2 size) {
    const DiceThrow& t = w.dice.last;
    const float font = ImGui::GetFontSize();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 end(at.x + size.x, at.y + size.y);
    ImGui::InvisibleButton("tray", size);
    const bool clicked = ImGui::IsItemClicked();
    if (ImGui::IsItemHovered() && !t.dice.empty()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawTrayCloth(draw, at, end, font);
    draw->PushClipRect(at, end, true);
    const double now = ImGui::GetTime();

    if (t.dice.empty()) {
        // Before the first roll: a still d20 in the middle, and what to do.
        const ImVec2 c((at.x + end.x) * 0.5f, (at.y + end.y) * 0.5f - font * 0.6f);
        const float sway = animations() ? std::sin(static_cast<float>(now) * 0.8f) * 0.12f : 0.0f;
        drawDie(w, draw, c, std::min(font * 2.4f, size.y * 0.2f), sway, 20, 20, palette().gold, 0.35f);
        const char* hint = "Pick what to roll on the left, then Roll";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        draw->AddText(ImVec2(c.x - hintSize.x * 0.5f, c.y + std::min(font * 2.4f, size.y * 0.2f) * 1.4f), colourU32(palette().muted), hint);
        draw->PopClipRect();
        return clicked;
    }

    const std::vector<Resting> places = restingPlaces(t, at, size, font);
    const bool landedAll = now >= t.landsAt();

    // A contest: each side's name above its dice, and its total below once they land.
    if (t.contest) {
        for (int side = 0; side < 2; ++side) {
            const CheckRoll& roll = side == 0 ? t.contest->first : t.contest->second;
            const float centreX = at.x + size.x * (side == 0 ? 0.25f : 0.75f);
            const std::string who = roll.who.empty() ? std::string(side == 0 ? "The first" : "The second") : roll.who;
            const std::string name = who + "  \xC2\xB7  " + checkName(roll.setup.kind);
            const ImVec2 nameSize = w.fonts.bold->CalcTextSizeA(font, FLT_MAX, 0.0f, name.c_str());
            draw->AddText(w.fonts.bold, font, ImVec2(centreX - nameSize.x * 0.5f, at.y + font * 0.9f), colourU32(sideColour(side)),
                          name.c_str());
            if (landedAll) {
                const float shown = easeIn(t.landsAt(), 0.35f);
                const bool won = t.contest->winner == side;
                const std::string total = std::to_string(roll.total);
                const float totalFont = font * 1.7f;
                const ImVec2 totalSize = w.fonts.title->CalcTextSizeA(totalFont, FLT_MAX, 0.0f, total.c_str());
                const ImVec2 totalAt(centreX - totalSize.x * 0.5f, end.y - font * 2.3f + (1.0f - shown) * font * 0.6f);
                draw->AddText(w.fonts.title, totalFont, totalAt, colourU32(won ? sideColour(side) : palette().muted, shown), total.c_str());
            }
        }
        // The line between the two sides.
        draw->AddLine(ImVec2(at.x + size.x * 0.5f, at.y + font * 0.8f), ImVec2(at.x + size.x * 0.5f, end.y - font * 0.8f),
                      colourU32(palette().gold, 0.15f));
    }

    for (size_t i = 0; i < t.dice.size(); ++i) {
        const ThrownDie& die = t.dice[i];
        const Resting& rest = places[i];
        const float r = rest.radius;
        const float flying = animations() ? static_cast<float>(now - t.at) - die.delay : kTumbleSeconds + 10.0f;
        const float progress = std::clamp(flying / kTumbleSeconds, 0.0f, 1.0f);
        const unsigned id = static_cast<unsigned>(i);
        // Thrown in from the near side (each side of a contest from its own), hopping as it slows.
        const bool fromRight = t.contest ? die.side == 1 : false;
        const ImVec2 start(fromRight ? end.x + r * 2.0f : at.x - r * 2.0f,
                           at.y + size.y * (0.35f + 0.5f * noise(t.seed, id, 3)));
        const float travel = easeOut(progress);
        ImVec2 ground(start.x + (rest.at.x - start.x) * travel, start.y + (rest.at.y - start.y) * travel);
        const float hops = 2.2f + noise(t.seed, id, 4) * 1.2f;
        const float lift = std::fabs(std::sin(progress * kPi * hops)) * size.y * 0.2f * std::pow(1.0f - progress, 1.6f);
        const float spin = (noise(t.seed, id, 5) > 0.5f ? 1.0f : -1.0f) * (3.0f + 3.0f * noise(t.seed, id, 6)) * kPi;
        float angle = spin * (1.0f - travel);
        ImVec2 c(ground.x, ground.y - lift);

        // Until it lands its face keeps changing.
        int face = die.value;
        if (progress < 1.0f) {
            const unsigned tick = static_cast<unsigned>(std::max(0.0f, flying) / 0.075f);
            face = 1 + static_cast<int>(noise(t.seed, id, 100 + tick) * static_cast<float>(die.sides)) % die.sides;
        }
        const float sinceLanding = flying - kTumbleSeconds;
        const bool landed = progress >= 1.0f;
        const bool scored = (t.check || t.contest) && die.sides == 20 && die.kept;
        const bool critical = landed && scored && die.value == 20;
        const bool fumble = landed && scored && die.value == 1;

        ImVec4 accent = t.contest ? sideColour(die.side) : palette().gold;
        float alpha = 1.0f;
        float scale = 1.0f;
        if (landed && sinceLanding < 0.3f) scale = 1.0f + 0.14f * std::sin(kPi * sinceLanding / 0.3f);  // a little bump
        if (landed && !die.kept) alpha = 1.0f - 0.62f * easeOut(sinceLanding / 0.35f);
        if (fumble) {
            accent = palette().danger;
            const float shake = std::clamp(sinceLanding / 0.6f, 0.0f, 1.0f);
            c.x += std::sin(sinceLanding * 55.0f) * r * 0.16f * (1.0f - shake);
            angle += std::sin(sinceLanding * 40.0f) * 0.12f * (1.0f - shake);
        }

        // A shadow on the cloth, smaller and fainter the higher the die is.
        const float height = std::clamp(lift / (size.y * 0.2f), 0.0f, 1.0f);
        draw->AddEllipseFilled(ImVec2(ground.x + r * 0.12f, ground.y + r * 0.92f), ImVec2(r * (0.95f - 0.3f * height), r * 0.22f),
                               colourU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f), (0.28f - 0.16f * height) * alpha), 0.0f, 24);
        if (critical) drawBurst(draw, c, r, sinceLanding, t.seed + id);
        if (fumble) {
            const float ring = std::clamp(sinceLanding / 0.7f, 0.0f, 1.0f);
            if (ring < 1.0f) draw->AddCircle(c, r * (1.15f + 0.9f * ring), colourU32(palette().danger, 0.6f * (1.0f - ring)), 48, 2.0f);
            draw->AddCircleFilled(c, r * 1.35f, colourU32(palette().danger, 0.08f), 48);
        }
        if (landed && sinceLanding < 0.45f && !critical && !fumble) {  // a ring of dust where it lands
            const float ring = sinceLanding / 0.45f;
            draw->AddCircle(ground, r * (1.1f + 0.7f * ring), colourU32(accent, 0.35f * (1.0f - ring)), 48, 1.5f);
        }
        drawDie(w, draw, c, r * scale, angle, die.sides, face, accent, alpha);
        if (landed && !die.kept) {  // the d20 set aside, struck through
            const float strike = easeOut(sinceLanding / 0.35f);
            draw->AddLine(ImVec2(c.x - r * 1.1f, c.y + r * 0.5f), ImVec2(c.x - r * 1.1f + r * 2.2f * strike, c.y + r * 0.5f - r * 1.0f * strike),
                          colourU32(palette().muted, 0.8f), std::max(1.5f, r * 0.06f));
        }
    }
    draw->PopClipRect();
    return clicked;
}

// ---------------------------------------------------------------- throwing

unsigned nextSeed() {
    static unsigned count = 0;
    return static_cast<unsigned>(ImGui::GetTime() * 1000.0) * 2654435761u + (++count) * 40503u;
}

void remember(DiceScreen& s, const std::string& headline, const std::string& text, DiceTone tone) {
    s.log.insert(s.log.begin(), {headline, text, tone, s.last.landsAt()});
    if (s.log.size() > kLogLength) s.log.resize(kLogLength);
}

void throwDice(WindowState& w) {
    auto& s = w.dice;
    DiceThrow t;
    t.at = ImGui::GetTime();
    t.seed = nextSeed();
    if (s.mode == DiceScreen::Check) {
        const auto& side = s.sides[0];
        const CheckOutcome outcome = checkAgainst(rollCheck(w.app.dice, rollerFor(w, side.characterId), w.app.db, side.setup), s.difficulty);
        for (size_t i = 0; i < outcome.roll.d20s.size(); ++i) {
            t.dice.push_back({20, outcome.roll.d20s[i], 0, i == outcome.roll.kept, 0.09f * static_cast<float>(i)});
        }
        t.check = outcome;
        s.last = t;
        remember(s, outcome.headline(), outcome.describe(), checkTone(outcome));
    } else if (s.mode == DiceScreen::Contest) {
        CheckRoll rolls[2];
        for (int side = 0; side < 2; ++side) {
            const auto& picked = s.sides[static_cast<size_t>(side)];
            rolls[side] = rollCheck(w.app.dice, rollerFor(w, picked.characterId), w.app.db, picked.setup);
            for (size_t i = 0; i < rolls[side].d20s.size(); ++i) {
                t.dice.push_back({20, rolls[side].d20s[i], side, i == rolls[side].kept, 0.09f * static_cast<float>(i) + 0.05f * static_cast<float>(side)});
            }
        }
        const ContestOutcome outcome = contest(rolls[0], rolls[1]);
        t.contest = outcome;
        s.last = t;
        remember(s, outcome.headline(), outcome.describe(), contestTone(outcome));
    } else {
        const auto formula = parseDiceFormula(s.formula, &s.formulaProblem);
        if (!formula) return;
        const DiceRoll roll = rollDice(w.app.dice, *formula);
        // Many dice set off one after another, but all of them within half a second.
        const float stagger = std::min(0.06f, 0.5f / static_cast<float>(std::max<size_t>(1, roll.dice.size())));
        for (size_t i = 0; i < roll.dice.size(); ++i) {
            t.dice.push_back({roll.dice[i].sides, roll.dice[i].value, 0, true, stagger * static_cast<float>(i)});
        }
        t.free = roll;
        s.last = t;
        remember(s, std::to_string(roll.total), roll.describe(), DiceTone::Gold);
    }
}

// ---------------------------------------------------------------- the controls on the left

// A row of buttons of equal width, one of them lit. Returns the one pressed, or -1.
int segmented(const char* id, const std::vector<std::string>& labels, int selected, const std::vector<std::string>& tips = {}) {
    ImGui::PushID(id);
    const float spacing = ImGui::GetStyle().ItemSpacing.x * 0.5f;
    const float width = (ImGui::GetContentRegionAvail().x - spacing * static_cast<float>(labels.size() - 1)) / static_cast<float>(labels.size());
    int pressed = -1;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) ImGui::SameLine(0.0f, spacing);
        const bool on = static_cast<int>(i) == selected;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(palette().gold.x, palette().gold.y, palette().gold.z, 0.26f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(palette().gold.x, palette().gold.y, palette().gold.z, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_Text, palette().gold);
        }
        if (ImGui::Button((labels[i] + "##" + std::to_string(i)).c_str(), ImVec2(std::max(1.0f, width), 0.0f))) pressed = static_cast<int>(i);
        if (on) ImGui::PopStyleColor(3);
        if (i < tips.size() && !tips[i].empty()) ImGui::SetItemTooltip("%s", tips[i].c_str());
    }
    ImGui::PopID();
    return pressed;
}

// "Who": a dropdown of the saved characters with a search box, then their medallion, name and level.
void whoPicker(WindowState& w, DiceScreen::Side& side, const ImVec4& accent) {
    const Character* c = rollerFor(w, side.characterId);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##who", c ? c->name.c_str() : "Nobody in particular", ImGuiComboFlags_HeightLarge)) {
        if (ImGui::IsWindowAppearing()) {
            side.search.clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", "Search names", &side.search);
        if (ImGui::Selectable("Nobody in particular", side.characterId == 0)) side.characterId = 0;
        for (const Character& other : w.app.db.characters) {
            if (!side.search.empty() && lower(other.name).find(lower(side.search)) == std::string::npos) continue;
            ImGui::PushID(other.id);
            if (ImGui::Selectable(other.name.c_str(), other.id == side.characterId)) side.characterId = other.id;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", levelText(other, w.app.db).c_str());
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    c = rollerFor(w, side.characterId);
    const float font = ImGui::GetFontSize();
    if (c) {
        medallion(w, initials(c->name), pathwayColour(w.app.db, c->pathwayId), font * 1.2f);
    } else {
        medallion(w, "?", palette().muted, font * 1.2f);
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.3f);
    ImGui::PushStyleColor(ImGuiCol_Text, c ? accent : palette().muted);
    ImGui::TextUnformatted(c ? c->name.c_str() : "Nobody in particular");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (c) {
        // "Sequence 2 Discerner, Spectator pathway, an Angel"; Above the Sequence says it already.
        const SpeedTier tier = speedTierOf(*c);
        std::string level = levelText(*c, w.app.db);
        if (tier.tier > 0 && level.find(tier.name) == std::string::npos) level += "  \xC2\xB7  " + tier.name;
        ImGui::TextWrapped("%s", level.c_str());
    } else {
        ImGui::TextWrapped("Only the extra bonus is added");
    }
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

// What the check is made with, how the d20 is rolled, and what is added to it.
void checkSetupControls(WindowState& w, DiceScreen::Side& side) {
    const Character* c = rollerFor(w, side.characterId);
    CheckSetup& setup = side.setup;
    const Pathway* pathway = c ? w.app.db.findPathway(c->pathwayId) : nullptr;
    const auto totals = c ? statTotals(*c, pathway) : std::array<int, kStatCount>{};
    std::vector<std::string> labels, tips;
    for (int kind = 0; kind < kCheckKinds; ++kind) {
        const int score = !c ? 0 : kind == kSpeedCheck ? effectiveSpeed(*c, pathway) : totals[static_cast<size_t>(kind)];
        labels.push_back(c ? checkCode(kind) + " " + signedNumber(abilityModifier(score)) : checkCode(kind));
        tips.push_back(c ? checkName(kind) + " " + std::to_string(score) + ": " + signedNumber(abilityModifier(score))
                         : checkName(kind));
    }
    ImGui::TextColored(palette().muted, "Check with");
    // Two rows: four stats, then two and Speed.
    const std::vector<std::string> top(labels.begin(), labels.begin() + 4), bottom(labels.begin() + 4, labels.end());
    const std::vector<std::string> topTips(tips.begin(), tips.begin() + 4), bottomTips(tips.begin() + 4, tips.end());
    if (const int pick = segmented("kinds1", top, setup.kind < 4 ? setup.kind : -1, topTips); pick >= 0) setup.kind = pick;
    if (const int pick = segmented("kinds2", bottom, setup.kind >= 4 ? setup.kind - 4 : -1, bottomTips); pick >= 0) setup.kind = pick + 4;

    ImGui::TextColored(palette().muted, "Roll");
    const int mode = segmented("mode", {"Normal", "Advantage", "Disadvantage"}, static_cast<int>(setup.mode),
                               {"One d20", "Two d20s, the higher counts", "Two d20s, the lower counts"});
    if (mode >= 0) setup.mode = static_cast<RollMode>(mode);

    if (c && speedTierOf(*c).tier > 0) {
        const SpeedTier tier = speedTierOf(*c);
        ImGui::Checkbox(("Add their tier: +" + std::to_string(tier.tier) + " " + tier.name).c_str(), &setup.addTier);
        ImGui::SetItemTooltip("+1 for Sequence 9 and 8, up to +5 for a god and +6 Above the Sequence.");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Extra");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.5f);
    if (ImGui::InputInt("##extra", &setup.extra)) setup.extra = std::clamp(setup.extra, -50, 50);
    helpMarker("Anything else that helps or hinders: +2 for a good plan or the right tool, -5 in the dark.");

    // What it all comes to.
    const int bonus = checkWithDice(c, w.app.db, setup, {1}).bonus();
    ImGui::PushFont(w.fonts.bold, 0.0f);
    ImGui::TextColored(palette().gold, "d20 %s%s", bonus == 0 ? "" : (bonus > 0 ? "+ " + std::to_string(bonus) : "- " + std::to_string(-bonus)).c_str(),
                       setup.mode == RollMode::Advantage ? ", the higher of two" : setup.mode == RollMode::Disadvantage ? ", the lower of two" : "");
    ImGui::PopFont();
}

// A thin bar in parts: each a fraction of the width in its colour.
void partsBar(const std::vector<std::pair<float, ImVec4>>& parts, ImGuiID id) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = std::max(4.0f, ImGui::GetFontSize() * 0.32f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(at, ImVec2(at.x + width, at.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg), height);
    float x = at.x;
    for (size_t i = 0; i < parts.size(); ++i) {
        const float part = glideFrom(id + static_cast<ImGuiID>(i), 0.0f, parts[i].first, 8.0f) * width;
        if (part > 0.5f) draw->AddRectFilled(ImVec2(x, at.y), ImVec2(x + part, at.y + height), colourU32(parts[i].second), height);
        x += part;
    }
    ImGui::Dummy(ImVec2(width, height));
}

std::string percent(double chance) {
    const int whole = static_cast<int>(std::lround(chance * 100.0));
    if (whole == 0 && chance > 0.0) return "under 1%";
    if (whole == 100 && chance < 1.0) return "over 99%";
    return std::to_string(whole) + "%";
}

void difficultyControls(WindowState& w) {
    auto& s = w.dice;
    ImGui::TextColored(palette().muted, "Difficulty");
    std::vector<std::string> labels, tips;
    int selected = -1;
    for (size_t i = 0; i < kDifficulties.size(); ++i) {
        labels.push_back(std::to_string(kDifficulties[i].value));
        tips.push_back(kDifficulties[i].name);
        if (kDifficulties[i].value == s.difficulty) selected = static_cast<int>(i);
    }
    if (const int pick = segmented("difficulty", labels, selected, tips); pick >= 0) s.difficulty = kDifficulties[static_cast<size_t>(pick)].value;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
    ImGui::SliderInt("##dc", &s.difficulty, 1, 40, "to reach %d");
    s.difficulty = std::clamp(s.difficulty, 1, 60);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(difficultyName(s.difficulty).c_str());
}

void checkChanceLine(WindowState& w) {
    auto& s = w.dice;
    const DiceScreen::Side& side = s.sides[0];
    const int bonus = checkWithDice(rollerFor(w, side.characterId), w.app.db, side.setup, {1}).bonus();
    const double chance = checkChance(bonus, s.difficulty, side.setup.mode);
    const ImVec4 colour = chance >= 0.65 ? palette().success : chance >= 0.35 ? palette().gold : palette().danger;
    ImGui::Text("Chance of success:");
    ImGui::SameLine();
    ImGui::TextColored(colour, "%s", percent(chance).c_str());
    partsBar({{static_cast<float>(chance), colour}}, ImGui::GetID("chance"));
}

void contestChancesLine(WindowState& w) {
    auto& s = w.dice;
    int bonus[2];
    for (int side = 0; side < 2; ++side) {
        const auto& picked = s.sides[static_cast<size_t>(side)];
        bonus[side] = checkWithDice(rollerFor(w, picked.characterId), w.app.db, picked.setup, {1}).bonus();
    }
    const ContestChances chances = contestChances(bonus[0], s.sides[0].setup.mode, bonus[1], s.sides[1].setup.mode);
    ImGui::TextColored(sideColour(0), "First %s", percent(chances.first).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("tie %s", percent(chances.tie).c_str());
    ImGui::SameLine();
    ImGui::TextColored(sideColour(1), "second %s", percent(chances.second).c_str());
    partsBar({{static_cast<float>(chances.first), sideColour(0)},
              {static_cast<float>(chances.tie), palette().muted},
              {static_cast<float>(chances.second), sideColour(1)}},
             ImGui::GetID("contest chances"));
}

void anyDiceControls(WindowState& w) {
    auto& s = w.dice;
    ImGui::TextWrapped("Write the dice the way tabletop games do: d20, 2d6+3, 1d8 + 1d4 - 1, or d%% for a d100.");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##formula", "like 2d6+3", &s.formula, ImGuiInputTextFlags_EnterReturnsTrue)) throwDice(w);
    const auto formula = parseDiceFormula(s.formula, &s.formulaProblem);
    if (!formula) {
        ImGui::TextColored(palette().danger, "%s", s.formulaProblem.c_str());
    } else {
        int low = formula->modifier, high = formula->modifier;
        for (const DiceGroup& g : formula->groups) {
            low += g.count;
            high += g.count * g.sides;
        }
        ImGui::TextDisabled("From %d to %d, %s on average", low, high,
                            (std::to_string((low + high) / 2) + ((low + high) % 2 != 0 ? ".5" : "")).c_str());
    }
    ImGui::TextColored(palette().muted, "Or one of these");
    const std::vector<std::string> first = {"d4", "d6", "d8", "d10"}, second = {"d12", "d20", "d100"};
    const std::vector<std::string> third = {"2d6", "3d6", "4d6", "2d20"};
    for (const auto* row : {&first, &second, &third}) {
        if (const int pick = segmented(row->front().c_str(), *row, -1); pick >= 0) {
            s.formula = (*row)[static_cast<size_t>(pick)];
            throwDice(w);
        }
    }
}

// The big Roll button at the bottom of the controls.
bool rollButton(WindowState& w, std::string label, bool enabled) {
    ImGui::Spacing();
    ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 1.2f);
    if (ImGui::CalcTextSize(label.c_str()).x > ImGui::GetContentRegionAvail().x - ImGui::GetFontSize()) label = "Roll the dice";
    const ImVec4 gold = palette().gold;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(gold.x, gold.y, gold.z, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(gold.x, gold.y, gold.z, 0.36f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(gold.x, gold.y, gold.z, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, gold);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, ImGui::GetFontSize() * 2.2f));
    ImGui::EndDisabled();
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    return pressed;
}

// ---------------------------------------------------------------- the result and the list

// The number in large type, counting up from nothing as the dice settle.
void bigNumber(const WindowState& w, int value, double from, const ImVec4& colour, float scale = 2.6f) {
    const float shown = easeIn(from, 0.45f);
    const int counted = static_cast<int>(std::lround(static_cast<float>(value) * shown));
    ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * scale);
    ImGui::TextColored(colour, "%d", counted);
    ImGui::PopFont();
}

void resultPanel(WindowState& w) {
    const DiceThrow& t = w.dice.last;
    if (t.dice.empty()) return;
    const double now = ImGui::GetTime();
    if (now < t.landsAt()) {
        const int dots = static_cast<int>((now - t.at) * 6.0) % 4;
        ImGui::TextDisabled("Rolling%s", std::string(static_cast<size_t>(dots), '.').c_str());
        return;
    }
    // Fades and slides in once the dice are still.
    const float shown = easeIn(t.landsAt(), 0.35f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * shown);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - shown) * ImGui::GetFontSize() * 0.5f);
    std::string copy;
    if (t.check) {
        const CheckOutcome& o = *t.check;
        bigNumber(w, o.roll.total, t.landsAt(), checkColour(o));
        ImGui::SameLine(0.0f, ImGui::GetFontSize());
        ImGui::BeginGroup();
        ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::TextColored(checkColour(o), "%s", o.headline().c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("%s against %d (%s)", o.roll.who.empty() ? "A roll" : (o.roll.who + ", " + checkName(o.roll.setup.kind)).c_str(),
                            o.difficulty, difficultyName(o.difficulty).c_str());
        ImGui::EndGroup();
        ImGui::TextWrapped("%s", o.roll.working().c_str());
        copy = o.describe();
    } else if (t.contest) {
        const ContestOutcome& o = *t.contest;
        ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::TextColored(contestColour(o), "%s", o.headline().c_str());
        ImGui::PopFont();
        ImGui::TextColored(sideColour(0), "%s", (o.first.who.empty() ? std::string("The first") : o.first.who).c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", o.first.working().c_str());
        ImGui::TextColored(sideColour(1), "%s", (o.second.who.empty() ? std::string("The second") : o.second.who).c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", o.second.working().c_str());
        copy = o.describe();
    } else if (t.free) {
        const DiceRoll& roll = *t.free;
        bigNumber(w, roll.total, t.landsAt(), palette().gold);
        ImGui::SameLine(0.0f, ImGui::GetFontSize());
        ImGui::BeginGroup();
        ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::TextColored(palette().gold, "%s", roll.formula.text().c_str());
        ImGui::PopFont();
        // The dice one by one ("4 + 6 + 3 = 13"); the formula is just above.
        const std::string all = roll.describe(), prefix = roll.formula.text() + ": ";
        ImGui::TextWrapped("%s", all.rfind(prefix, 0) == 0 ? all.substr(prefix.size()).c_str() : all.c_str());
        ImGui::EndGroup();
        copy = roll.describe();
    }
    if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(copy.c_str());
        setStatus(w, "Copied the roll, ready to paste.");
    }
    ImGui::SetItemTooltip("%s", copy.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Click the tray to roll the same again.");
    ImGui::PopStyleVar();
}

void rollList(WindowState& w) {
    auto& s = w.dice;
    const double now = ImGui::GetTime();
    heading(w, "Rolls so far");
    if (s.log.empty() || now < s.log.back().showAt) {
        ImGui::TextDisabled("Every roll shows here until the window is closed.");
        return;
    }
    if (ImGui::SmallButton("Clear the list")) {
        s.log.clear();
        return;
    }
    ImGui::BeginChild("rolls", ImVec2(0, 0), ImGuiChildFlags_None);
    for (size_t i = 0; i < s.log.size(); ++i) {
        const DiceLogEntry& entry = s.log[i];
        if (now < entry.showAt) continue;  // still rolling: no spoilers
        const float shown = easeIn(entry.showAt, 0.4f);
        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.2f + 0.8f * shown));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (1.0f - shown) * ImGui::GetFontSize() * 1.5f);
        const ImVec4 colour = toneColour(entry.tone);
        statusDot(colour);
        ImGui::PushFont(w.fonts.bold, 0.0f);
        ImGui::TextColored(colour, "%s", entry.headline.c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", entry.text.c_str());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
    ImGui::EndChild();
}

}  // namespace

double DiceThrow::landsAt() const {
    float last = 0.0f;
    for (const ThrownDie& die : dice) last = std::max(last, die.delay);
    return animations() ? at + last + kTumbleSeconds : at;
}

void openCheck(WindowState& w, int characterId, int kind) {
    auto& s = w.dice;
    s.mode = DiceScreen::Check;
    s.sides[0].characterId = characterId;
    s.sides[0].setup.kind = std::clamp(kind, 0, kCheckKinds - 1);
    s.rollNow = true;
    w.switchTo = Tab::Dice;
}

void drawDiceScreen(WindowState& w) {
    auto& s = w.dice;
    for (auto& side : s.sides) {
        if (side.characterId != 0 && !rollerFor(w, side.characterId)) side.characterId = 0;  // deleted since
    }
    if (s.rollNow) {
        s.rollNow = false;
        throwDice(w);
    }
    const float font = ImGui::GetFontSize();

    // ---- what to roll
    ImGui::BeginChild("controls", ImVec2(font * 23.5f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (const int pick = segmented("modes", {"Check", "Contest", "Any dice"}, s.mode,
                                   {"One character against a difficulty", "Two characters against each other",
                                    "Any dice, like 2d6+3"});
        pick >= 0) {
        s.mode = pick;
    }
    ImGui::Separator();
    // The form scrolls when it is taller than the window; the chances and the Roll button stay in view below it.
    static float footerHeight = font * 6.0f;
    ImGui::BeginChild("form", ImVec2(0, -footerHeight));
    ImGui::Spacing();
    if (s.mode == DiceScreen::Check) {
        ImGui::TextColored(palette().muted, "Who");
        whoPicker(w, s.sides[0], palette().gold);
        ImGui::Spacing();
        checkSetupControls(w, s.sides[0]);
        ImGui::Spacing();
        difficultyControls(w);
    } else if (s.mode == DiceScreen::Contest) {
        for (int side = 0; side < 2; ++side) {
            ImGui::PushID(side);
            heading(w, side == 0 ? "First" : "Against");
            whoPicker(w, s.sides[static_cast<size_t>(side)], sideColour(side));
            checkSetupControls(w, s.sides[static_cast<size_t>(side)]);
            ImGui::PopID();
            ImGui::Spacing();
        }
    } else {
        anyDiceControls(w);
    }
    ImGui::EndChild();
    const float footerTop = ImGui::GetCursorPosY();
    ImGui::Separator();
    bool roll = false;
    if (s.mode == DiceScreen::Check) {
        checkChanceLine(w);
        roll = rollButton(w, "Roll the check", true);
    } else if (s.mode == DiceScreen::Contest) {
        contestChancesLine(w);
        roll = rollButton(w, "Roll for both", true);
    } else {
        const auto formula = parseDiceFormula(s.formula);
        roll = rollButton(w, formula ? "Roll " + formula->text() : std::string("Roll"), formula.has_value());
    }
    footerHeight = ImGui::GetCursorPosY() - footerTop + ImGui::GetStyle().ItemSpacing.y;
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the tray, what it came to, and the rolls so far
    ImGui::BeginChild("table", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 traySize(std::max(avail.x, 2.0f), std::max(2.0f, std::clamp(avail.y * 0.5f, font * 11.0f, font * 19.0f)));
    if (drawTray(w, traySize) && !s.last.dice.empty()) {
        // The same again: back to what was thrown last.
        s.mode = s.last.check ? DiceScreen::Check : s.last.contest ? DiceScreen::Contest : DiceScreen::AnyDice;
        roll = true;
    }
    ImGui::Spacing();
    resultPanel(w);
    ImGui::Spacing();
    rollList(w);
    ImGui::EndChild();

    if (roll) throwDice(w);
}

}  // namespace lotm::window
