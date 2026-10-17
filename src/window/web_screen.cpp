// Web tab: every character as a circle in their pathway's colour, every relationship a line in its
// kind's colour. The web settles into place as you watch; drag people to place them, scroll to zoom,
// and rest the pointer on someone to see only their connections.
#include <algorithm>
#include <cmath>

#include "rules.hpp"
#include "storage.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

ImU32 colourU32(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

float mix(float a, float b, float t) { return a + (b - a) * t; }

// What the web is built from: who is saved, their pathways and relationships, and what is shown.
// When any of it changes, the web is built again.
std::string signatureOf(const Database& db, const WebOptions& options) {
    std::string sig;
    for (bool shown : options.bonds) sig += shown ? '1' : '0';
    sig += options.unsavedPeople ? 'u' : '-';
    sig += options.loners ? 'l' : '-';
    sig += options.organization + '\x1f';
    for (const Character& c : db.characters) {
        sig += std::to_string(c.id) + '\x1e' + c.name + '\x1e' + c.pathwayId + '\x1e' + std::to_string(c.sequence) +
               (c.aboveSequence ? "a" : "") + '\x1e' + c.affiliation.organization;
        for (const Relationship& r : c.relationships) {
            sig += '\x1d' + std::to_string(r.characterId) + '\x1c' + r.name + '\x1c' + r.type + '\x1c' + r.note;
        }
        sig += '\x1f';
    }
    return sig;
}

void rebuild(WindowState& w) {
    WebScreen& s = w.web;
    const Database& db = w.app.db;
    Web fresh = buildWeb(db, s.options);
    const bool first = s.web.nodes.empty();
    s.layout.adapt(s.web, fresh, db);  // starts afresh when there was nobody before

    // Newcomers grow in, one after another the first time.
    const double now = ImGui::GetTime();
    std::map<std::string, double> born;
    for (size_t i = 0; i < fresh.nodes.size(); ++i) {
        const std::string key = fresh.key(i);
        const auto found = s.bornAt.find(key);
        born[key] = found != s.bornAt.end() ? found->second
                                            : now + (first ? 0.03 * static_cast<double>(std::min<size_t>(i, 40)) : 0.0);
    }
    s.bornAt = born;
    s.web = std::move(fresh);
    s.dragging = -1;
    if (!s.selected.empty() && !s.bornAt.count(s.selected)) s.selected.clear();
    if (first) s.follow = true;

    // How many lines of each kind there would be with every kind shown.
    WebOptions all = s.options;
    all.bonds.fill(true);
    s.bondCounts.fill(0);
    for (const WebEdge& e : buildWeb(db, all).edges) ++s.bondCounts[static_cast<size_t>(e.bond)];
    s.organizations.clear();
    for (const Character& c : db.characters) {
        const std::string& org = c.affiliation.organization;
        if (!org.empty() && std::find(s.organizations.begin(), s.organizations.end(), org) == s.organizations.end()) {
            s.organizations.push_back(org);
        }
    }
    std::sort(s.organizations.begin(), s.organizations.end());
}

int nodeOfKey(const Web& web, const std::string& key) {
    for (size_t i = 0; i < web.nodes.size(); ++i) {
        if (web.key(i) == key) return static_cast<int>(i);
    }
    return -1;
}

// The size of a circle on screen: bigger when zoomed in (within limits), and a little bigger for
// people with many connections. People who aren't saved characters are smaller.
float nodeRadius(const WebScreen& s, const WebNode& n) {
    const float font = ImGui::GetFontSize();
    const float r = std::clamp(s.shownScale * 0.23f, font * 0.55f, font * 1.3f);
    if (n.characterId == 0) return r * 0.72f;
    return r * (1.0f + 0.02f * static_cast<float>(std::min(n.connections, 10)));
}

// A circle drawn as dashes, for people who aren't saved characters.
void dashedCircle(ImDrawList* draw, ImVec2 centre, float radius, ImU32 colour, float thickness) {
    const int dashes = std::max(8, static_cast<int>(radius * 0.7f));
    const float part = 6.2831853f / static_cast<float>(dashes);
    for (int i = 0; i < dashes; ++i) {
        const float a = part * static_cast<float>(i);
        draw->PathArcTo(centre, radius, a, a + part * 0.55f, 4);
        draw->PathStroke(colour, ImDrawFlags_None, thickness);
    }
}

// A line of moving dashes from a to b, for the way between two people.
void marchingLine(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 colour, float thickness) {
    const float length = std::hypot(b.x - a.x, b.y - a.y);
    if (length < 1.0f) return;
    const ImVec2 unit((b.x - a.x) / length, (b.y - a.y) / length);
    const float dash = 9.0f, gap = 6.0f;
    float at = -std::fmod(static_cast<float>(ImGui::GetTime()) * 28.0f, dash + gap);
    for (; at < length; at += dash + gap) {
        const float from = std::max(at, 0.0f), to = std::min(at + dash, length);
        if (to <= from) continue;
        draw->AddLine(ImVec2(a.x + unit.x * from, a.y + unit.y * from), ImVec2(a.x + unit.x * to, a.y + unit.y * to),
                      colour, thickness);
    }
}

// Pixels per unit and the middle that fit everyone into a canvas of this size.
void fitView(const WebScreen& s, ImVec2 size, ImVec2& centre, float& scale) {
    const float font = ImGui::GetFontSize();
    if (s.layout.x.empty()) {
        centre = ImVec2(0.0f, 0.0f);
        scale = font * 5.0f;
        return;
    }
    float minX = s.layout.x[0], maxX = minX, minY = s.layout.y[0], maxY = minY;
    for (size_t i = 1; i < s.layout.x.size(); ++i) {
        minX = std::min(minX, s.layout.x[i]);
        maxX = std::max(maxX, s.layout.x[i]);
        minY = std::min(minY, s.layout.y[i]);
        maxY = std::max(maxY, s.layout.y[i]);
    }
    // Room around everyone for the circles, the names under them and the hint along the bottom.
    const float side = font * 4.5f, top = font * 2.2f, bottom = font * 4.2f;
    const float spanX = std::max(maxX - minX, 0.5f), spanY = std::max(maxY - minY, 0.5f);
    scale = std::min((size.x - side * 2.0f) / spanX, (size.y - top - bottom) / spanY);
    scale = std::clamp(scale, font * 1.2f, font * 9.0f);
    // The middle of everyone goes in the middle of the room left for them.
    const float roomMiddle = top + (size.y - top - bottom) * 0.5f;
    centre = ImVec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f - (roomMiddle - size.y * 0.5f) / scale);
}

// Grey text that wraps at the edge of the panel.
void note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

std::string connectionsText(int n) { return std::to_string(n) + (n == 1 ? " connection" : " connections"); }

// ---------------------------------------------------------------- the panel on the left

void bondCheckboxes(WebScreen& s) {
    for (int b = 0; b < kBondCount; ++b) {
        const Bond bond = static_cast<Bond>(b);
        const ImVec4 colour = colourFromHex(bondColour(bond));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, colour);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(colour.x, colour.y, colour.z, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(colour.x, colour.y, colour.z, 0.3f));
        bool shown = s.options.bonds[static_cast<size_t>(b)];
        const std::string label = std::string(bondName(bond)) + "##bond" + std::to_string(b);
        if (ImGui::Checkbox(label.c_str(), &shown)) s.options.bonds[static_cast<size_t>(b)] = shown;
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::TextDisabled("%d", s.bondCounts[static_cast<size_t>(b)]);
    }
}

void characterPicker(const WindowState& w, const char* label, int& id) {
    const Character* picked = w.app.db.findCharacter(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 3.2f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo((std::string("##") + label).c_str(), picked ? picked->name.c_str() : "Pick someone",
                          ImGuiComboFlags_HeightLarge)) {
        for (const Character& c : w.app.db.characters) {
            if (w.web.web.nodeOf(c.id) < 0) continue;
            if (ImGui::Selectable((c.name + "##" + std::to_string(c.id)).c_str(), c.id == id)) id = c.id;
        }
        ImGui::EndCombo();
    }
}

// "How are they connected?": the shortest chain of relationships between two people.
void pathSection(WindowState& w, std::vector<size_t>& path) {
    WebScreen& s = w.web;
    heading(w, "How Are They Connected?");
    characterPicker(w, "From", s.pathFrom);
    characterPicker(w, "To", s.pathTo);
    path.clear();
    if (s.pathFrom == 0 || s.pathTo == 0) {
        note("Pick two people to find the shortest chain of relationships between them.");
        return;
    }
    if (ImGui::SmallButton("Swap")) std::swap(s.pathFrom, s.pathTo);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##path")) {
        s.pathFrom = s.pathTo = 0;
        return;
    }
    if (s.pathFrom == s.pathTo) {
        note("Pick two different people.");
        return;
    }
    const int from = s.web.nodeOf(s.pathFrom), to = s.web.nodeOf(s.pathTo);
    if (from >= 0 && to >= 0) path = shortestPath(s.web, static_cast<size_t>(from), static_cast<size_t>(to));
    if (path.empty()) {
        note("They aren't connected through anyone shown. Showing more kinds of relationship, or people who aren't "
             "saved characters, may find a way.");
        return;
    }
    ImGui::TextColored(palette().gold, "%zu %s apart", path.size() - 1, path.size() == 2 ? "step" : "steps");
    // A chain down the panel: each person, and how the one before knows them.
    const float font = ImGui::GetFontSize();
    for (size_t i = 0; i < path.size(); ++i) {
        const WebNode& node = s.web.nodes[path[i]];
        if (i > 0) {
            const WebEdge* edge = edgeBetween(s.web, path[i - 1], path[i]);
            std::string how = edge ? (edge->a == path[i - 1] ? edge->fromA : edge->fromB) : "";
            if (edge && how.empty()) how = edge->label();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + font * 0.55f);
            ImGui::TextColored(edge ? colourFromHex(bondColour(edge->bond)) : palette().muted, "|  %s", how.c_str());
        }
        const ImVec4 colour = node.characterId ? pathwayColour(w.app.db, node.pathwayId) : palette().muted;
        medallion(w, node.badge, colour, font * 0.7f);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(node.name.c_str());
    }
}

void pickedSection(WindowState& w) {
    WebScreen& s = w.web;
    const int index = nodeOfKey(s.web, s.selected);
    heading(w, "Picked");
    if (index < 0) {
        note("Click someone to see their connections here. Double-click them to open their sheet.");
        return;
    }
    const WebNode& node = s.web.nodes[static_cast<size_t>(index)];
    const float font = ImGui::GetFontSize();
    const ImVec4 colour = node.characterId ? pathwayColour(w.app.db, node.pathwayId) : palette().muted;
    medallion(w, node.badge, colour, font * 1.3f);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.35f);
    ImGui::TextWrapped("%s", node.name.c_str());
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    if (node.characterId) {
        const Pathway* p = w.app.db.findPathway(node.pathwayId);
        ImGui::TextWrapped("%s%s", node.level.c_str(), p ? (", " + p->name).c_str() : "");
        if (!node.organization.empty()) ImGui::TextWrapped("%s", node.organization.c_str());
    } else {
        ImGui::TextWrapped("Not a saved character");
    }
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    ImGui::TextDisabled("%s", connectionsText(node.connections).c_str());
    for (const WebEdge& e : s.web.edges) {
        if (e.a != static_cast<size_t>(index) && e.b != static_cast<size_t>(index)) continue;
        const size_t other = e.a == static_cast<size_t>(index) ? e.b : e.a;
        const std::string& how = e.a == static_cast<size_t>(index) ? e.fromA : e.fromB;
        statusDot(colourFromHex(bondColour(e.bond)));
        ImGui::PushID(static_cast<int>(other));
        if (ImGui::TextLink(s.web.nodes[other].name.c_str())) {
            // Pick them instead, and glide over to them.
            s.selected = s.web.key(other);
            s.targetCentre = ImVec2(s.layout.x[other], s.layout.y[other]);
            s.follow = false;
        }
        ImGui::PopID();
        if (!how.empty() || !e.label().empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", (how.empty() ? e.label() : how).c_str());
        }
        if (!e.note.empty()) ImGui::SetItemTooltip("%s", e.note.c_str());
    }
    ImGui::Spacing();
    if (node.characterId && ImGui::Button("Open their sheet")) openCharacter(w, node.characterId);
    const bool pinned = static_cast<size_t>(index) < s.layout.pinned.size() && s.layout.pinned[static_cast<size_t>(index)];
    if (pinned) {
        if (node.characterId) ImGui::SameLine();
        if (ImGui::Button("Let them move again")) {
            s.layout.pinned[static_cast<size_t>(index)] = false;
            s.layout.temperature = std::max(s.layout.temperature, 0.3f);
        }
    }
    if (node.characterId && ImGui::TextLink("Find the way from them to someone")) {
        s.pathFrom = node.characterId;
        if (s.pathTo == s.pathFrom) s.pathTo = 0;
    }
}

void exportButton(WindowState& w) {
    if (ImGui::Button("Export...")) ImGui::OpenPopup("exportWeb");
    if (!ImGui::BeginPopup("exportWeb")) return;
    ExportFormat format = ExportFormat::Html;
    bool chosen = false;
    if (ImGui::Selectable("HTML (the picture, then every connection)")) chosen = true;
    if (ImGui::Selectable("Markdown (the connections in words)")) {
        format = ExportFormat::Markdown;
        chosen = true;
    }
    if (ImGui::Selectable("Plain text")) {
        format = ExportFormat::Text;
        chosen = true;
    }
    if (chosen) {
        WebLayout settled = w.web.layout;  // the picture as it will be once everyone has settled
        settled.settle(w.web.web);
        exportAndReport(w, "relationship-web", format, {buildWebSheet(w.web.web, settled, w.app.db, false)},
                        "Who Knows Whom");
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- the canvas

void canvas(WindowState& w, const std::vector<size_t>& path) {
    WebScreen& s = w.web;
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 50.0f), std::max(ImGui::GetContentRegionAvail().y, 50.0f));
    const ImVec2 middle(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
    ImGuiIO& io = ImGui::GetIO();

    // The view glides to where it should be; following, that is wherever everyone fits.
    if (s.follow) fitView(s, size, s.targetCentre, s.targetScale);
    if (s.targetScale <= 0.0f) fitView(s, size, s.targetCentre, s.targetScale);
    if (s.shownScale <= 0.0f) {
        s.shownScale = s.targetScale;
        s.shownCentre = s.targetCentre;
    }
    const float ease = 1.0f - std::exp(-9.0f * frameSeconds());
    s.shownScale += (s.targetScale - s.shownScale) * ease;
    s.shownCentre.x += (s.targetCentre.x - s.shownCentre.x) * ease;
    s.shownCentre.y += (s.targetCentre.y - s.shownCentre.y) * ease;
    auto toScreen = [&](size_t i) {
        return ImVec2(middle.x + (s.layout.x[i] - s.shownCentre.x) * s.shownScale,
                      middle.y + (s.layout.y[i] - s.shownCentre.y) * s.shownScale);
    };

    ImGui::InvisibleButton("canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    // Who is under the pointer: on their circle (the last drawn is on top), or else on their name.
    const bool namesFit = s.shownScale > font * 2.4f;
    const float nameFont = font * 0.82f;
    int under = -1;
    if (hovered || active) {
        for (size_t i = s.web.nodes.size(); i-- > 0 && under < 0;) {
            const ImVec2 p = toScreen(i);
            const float r = nodeRadius(s, s.web.nodes[i]) + 3.0f;
            if ((io.MousePos.x - p.x) * (io.MousePos.x - p.x) + (io.MousePos.y - p.y) * (io.MousePos.y - p.y) <= r * r) {
                under = static_cast<int>(i);
            }
        }
        for (size_t i = s.web.nodes.size(); namesFit && i-- > 0 && under < 0;) {
            const ImVec2 p = toScreen(i);
            const float top = p.y + nodeRadius(s, s.web.nodes[i]) + font * 0.2f;
            const float half = ImGui::GetFont()->CalcTextSizeA(nameFont, FLT_MAX, 0.0f, s.web.nodes[i].name.c_str()).x * 0.5f + 3.0f;
            if (std::abs(io.MousePos.x - p.x) <= half && io.MousePos.y >= top && io.MousePos.y <= top + nameFont) {
                under = static_cast<int>(i);
            }
        }
    }

    // ---- pointer: drag people or the view, scroll to zoom, click to pick, double-click to open
    static bool pressedOnNode = false;
    auto pointerInWeb = [&] {
        return ImVec2(s.shownCentre.x + (io.MousePos.x - middle.x) / s.shownScale,
                      s.shownCentre.y + (io.MousePos.y - middle.y) / s.shownScale);
    };
    if (ImGui::IsItemActivated()) {
        pressedOnNode = under >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (pressedOnNode) {
            // Held where they were grabbed (the circle or the name), so they don't jump to the pointer.
            const size_t i = static_cast<size_t>(under);
            const ImVec2 at = pointerInWeb();
            s.dragging = under;
            s.grabOffset = ImVec2(s.layout.x[i] - at.x, s.layout.y[i] - at.y);
        }
    }
    if (active && s.dragging >= 0 && static_cast<size_t>(s.dragging) < s.web.nodes.size() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
        const size_t i = static_cast<size_t>(s.dragging);
        const ImVec2 at = pointerInWeb();
        const float moved = std::hypot(at.x + s.grabOffset.x - s.layout.x[i], at.y + s.grabOffset.y - s.layout.y[i]);
        s.layout.x[i] = at.x + s.grabOffset.x;
        s.layout.y[i] = at.y + s.grabOffset.y;
        if (s.layout.pinned.size() == s.web.nodes.size()) s.layout.pinned[i] = true;
        // The people linked to them follow, and only while they are moved: the whole web kept warm
        // wanders off and shakes, even with the pointer resting.
        if (s.layout.settled() || s.layout.around >= 0) {  // not while it first settles
            s.layout.around = static_cast<int>(i);
            s.layout.temperature = std::max(s.layout.temperature, std::min(moved, 0.06f));
        }
        s.follow = false;
    } else if (active && !pressedOnNode) {
        for (ImGuiMouseButton button : {ImGuiMouseButton_Left, ImGuiMouseButton_Right, ImGuiMouseButton_Middle}) {
            if (!ImGui::IsMouseDragging(button, 3.0f)) continue;
            s.targetCentre.x -= io.MouseDelta.x / s.shownScale;
            s.targetCentre.y -= io.MouseDelta.y / s.shownScale;
            s.shownCentre = s.targetCentre;
            s.follow = false;
            break;
        }
    }
    if (ImGui::IsItemDeactivated()) {
        const bool dragged = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x != 0.0f ||
                             ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y != 0.0f;
        if (!dragged && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            s.selected = under >= 0 ? s.web.key(static_cast<size_t>(under)) : "";
        }
        s.dragging = -1;
        pressedOnNode = false;
    }
    if (hovered && under >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (const int id = s.web.nodes[static_cast<size_t>(under)].characterId) openCharacter(w, id);
    }
    if (hovered && io.MouseWheel != 0.0f) {
        const float scale = std::clamp(s.targetScale * std::pow(1.18f, io.MouseWheel), font * 0.8f, font * 24.0f);
        // Keep the point under the pointer where it is.
        const ImVec2 offset(io.MousePos.x - middle.x, io.MousePos.y - middle.y);
        const ImVec2 point(s.targetCentre.x + offset.x / s.targetScale, s.targetCentre.y + offset.y / s.targetScale);
        s.targetCentre = ImVec2(point.x - offset.x / scale, point.y - offset.y / scale);
        s.targetScale = scale;
        s.follow = false;
    }
    static int menuFor = -1;
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        ImGui::GetMouseDragDelta(ImGuiMouseButton_Right).x == 0.0f && ImGui::GetMouseDragDelta(ImGuiMouseButton_Right).y == 0.0f) {
        menuFor = under;
        ImGui::OpenPopup("webMenu");
    }

    // ---- the highlight: resting on someone fades everyone they don't know
    if (under >= 0 && s.dragging < 0) s.hovered = s.web.key(static_cast<size_t>(under));
    s.focus += ((under >= 0 && s.dragging < 0 ? 1.0f : 0.0f) - s.focus) * (1.0f - std::exp(-12.0f * frameSeconds()));
    if (s.focus < 0.01f && under < 0) s.hovered.clear();
    const int lit = nodeOfKey(s.web, s.hovered);
    std::vector<bool> on(s.web.nodes.size(), lit < 0);
    if (lit >= 0) {
        on[static_cast<size_t>(lit)] = true;
        for (const WebEdge& e : s.web.edges) {
            if (e.a == static_cast<size_t>(lit)) on[e.b] = true;
            if (e.b == static_cast<size_t>(lit)) on[e.a] = true;
        }
    }
    const float focus = lit >= 0 ? s.focus : 0.0f;
    std::vector<bool> onPath(s.web.nodes.size(), false);
    for (size_t i : path) onPath[i] = true;

    // ---- the background: a faint grid of dots that moves with the view
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(ImGuiCol_FrameBg),
                        ImGui::GetStyle().ChildRounding);
    float spacing = s.shownScale * 0.5f;
    while (spacing < font * 1.4f) spacing *= 2.0f;
    const float startX = middle.x - std::fmod(s.shownCentre.x * s.shownScale, spacing) - std::ceil(size.x * 0.5f / spacing) * spacing;
    const float startY = middle.y - std::fmod(s.shownCentre.y * s.shownScale, spacing) - std::ceil(size.y * 0.5f / spacing) * spacing;
    const ImU32 dot = colourU32(ImGui::GetStyleColorVec4(ImGuiCol_Border), 0.7f);
    for (float x = startX; x < origin.x + size.x; x += spacing) {
        if (x < origin.x) continue;
        for (float y = startY; y < origin.y + size.y; y += spacing) {
            if (y >= origin.y) draw->AddRectFilled(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + 1.0f, y + 1.0f), dot);
        }
    }

    const double now = ImGui::GetTime();
    auto grown = [&](size_t i) {
        const auto found = s.bornAt.find(s.web.key(i));
        return found == s.bornAt.end() ? 1.0f : popIn(found->second);
    };

    // ---- the lines
    // Words on every line only once there is room for them: zoomed in, or few lines.
    const bool wordsFit = s.labels && (s.shownScale > font * 7.0f || (s.web.edges.size() <= 16 && s.shownScale > font * 4.0f));
    struct Label {
        ImVec2 at;
        std::string text;
        float alpha;
    };
    std::vector<Label> labels;
    for (const WebEdge& e : s.web.edges) {
        const ImVec2 a = toScreen(e.a), b = toScreen(e.b);
        const float appear = std::clamp(std::min(grown(e.a), grown(e.b)), 0.0f, 1.0f);
        const bool touches = lit >= 0 && (e.a == static_cast<size_t>(lit) || e.b == static_cast<size_t>(lit));
        const float alpha = (touches ? mix(0.55f, 1.0f, focus) : mix(0.55f, 0.06f, focus)) * appear;
        const ImVec4 colour = colourFromHex(bondColour(e.bond));
        // The line grows out from both people as they appear.
        const ImVec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        const ImVec2 from(mid.x + (a.x - mid.x) * appear, mid.y + (a.y - mid.y) * appear);
        const ImVec2 to(mid.x + (b.x - mid.x) * appear, mid.y + (b.y - mid.y) * appear);
        draw->AddLine(from, to, colourU32(colour, alpha), touches ? mix(1.8f, 3.0f, focus) : 1.8f);
        if ((wordsFit && focus < 0.5f) || (touches && focus > 0.3f)) {
            // Around someone hovered, each word sits nearer the other person, so they don't pile up in the middle.
            ImVec2 at = mid;
            if (touches && focus > 0.3f) {
                const ImVec2 far = e.a == static_cast<size_t>(lit) ? b : a, near = e.a == static_cast<size_t>(lit) ? a : b;
                at = ImVec2(near.x + (far.x - near.x) * 0.62f, near.y + (far.y - near.y) * 0.62f);
            }
            labels.push_back({at, e.label(), touches ? std::max(focus, 1.0f - focus) * appear : (1.0f - focus) * appear});
        }
    }
    // The way between two people: moving gold dashes over their lines.
    for (size_t i = 1; i < path.size(); ++i) {
        marchingLine(draw, toScreen(path[i - 1]), toScreen(path[i]), colourU32(palette().gold), 3.5f);
    }
    for (const Label& label : labels) {
        const float labelFont = font * 0.78f;
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(labelFont, FLT_MAX, 0.0f, label.text.c_str());
        const ImVec2 at(label.at.x - textSize.x * 0.5f, label.at.y - textSize.y * 0.5f);
        draw->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 1.0f), ImVec2(at.x + textSize.x + 4.0f, at.y + textSize.y + 1.0f),
                            colourU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), 0.85f * label.alpha), 4.0f);
        draw->AddText(ImGui::GetFont(), labelFont, at, colourU32(palette().muted, label.alpha), label.text.c_str());
    }

    // ---- the people
    const int picked = nodeOfKey(s.web, s.selected);
    for (size_t i = 0; i < s.web.nodes.size(); ++i) {
        const WebNode& node = s.web.nodes[i];
        const ImVec2 p = toScreen(i);
        const float grow = grown(i);
        if (grow <= 0.0f) continue;
        const float r = nodeRadius(s, node) * std::max(grow, 0.0f);
        const float alpha = (on[i] ? 1.0f : mix(1.0f, 0.18f, focus)) * std::clamp(grow, 0.0f, 1.0f);
        ImVec4 colour = node.characterId ? pathwayColour(db, node.pathwayId) : palette().muted;
        colour.w = alpha;
        draw->AddCircleFilled(p, r, colourU32(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg), alpha));
        if (node.characterId) {
            medallionAt(w, p, r, colour, node.badge);
        } else {
            dashedCircle(draw, p, r, colourU32(colour), 1.6f);
            const float textSize = r * 0.8f;
            if (textSize >= 1.0f) {  // not while it is still too small for ImGui to draw text (see drawMedallion)
                const ImVec2 t = w.fonts.bold->CalcTextSizeA(textSize, FLT_MAX, 0.0f, node.badge.c_str());
                draw->AddText(w.fonts.bold, textSize, ImVec2(p.x - t.x * 0.5f, p.y - t.y * 0.5f), colourU32(colour),
                              node.badge.c_str());
            }
        }
        if (static_cast<int>(i) == picked) {  // a slow gold pulse around the one picked
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 3.0f);
            draw->AddCircle(p, r + 4.0f + pulse * 2.5f, colourU32(palette().gold, alpha * (0.6f + 0.4f * pulse)), 0, 2.0f);
        } else if (onPath[i]) {
            draw->AddCircle(p, r + 4.0f, colourU32(palette().gold, alpha), 0, 2.0f);
        }
        if (i < s.layout.pinned.size() && s.layout.pinned[i]) {  // placed by hand: a small gold pin on top
            draw->AddCircleFilled(ImVec2(p.x, p.y - r - 1.0f), font * 0.17f, colourU32(palette().gold, alpha));
        }
        const bool showName = namesFit || static_cast<int>(i) == picked || onPath[i] || (focus > 0.2f && on[i]);
        if (showName) {
            const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(nameFont, FLT_MAX, 0.0f, node.name.c_str());
            const ImVec2 at(p.x - textSize.x * 0.5f, p.y + r + font * 0.2f);
            draw->AddRectFilled(ImVec2(at.x - 3.0f, at.y), ImVec2(at.x + textSize.x + 3.0f, at.y + textSize.y),
                                colourU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg), 0.7f * alpha), 3.0f);
            draw->AddText(ImGui::GetFont(), nameFont, at, ImGui::GetColorU32(ImGuiCol_Text, alpha), node.name.c_str());
        }
    }
    draw->PopClipRect();

    // ---- what the pointer is on
    if (under >= 0 && s.dragging < 0 && !ImGui::IsPopupOpen("webMenu") && ImGui::BeginTooltip()) {
        const WebNode& node = s.web.nodes[static_cast<size_t>(under)];
        ImGui::PushFont(w.fonts.bold, 0.0f);
        ImGui::TextUnformatted(node.name.c_str());
        ImGui::PopFont();
        if (node.characterId) {
            const Pathway* p = db.findPathway(node.pathwayId);
            ImGui::TextDisabled("%s%s", node.level.c_str(), p ? (", " + p->name + " pathway").c_str() : "");
            if (!node.organization.empty()) ImGui::TextDisabled("%s", node.organization.c_str());
        } else {
            ImGui::TextDisabled("Named in a relationship; not a saved character");
        }
        ImGui::TextDisabled("%s", connectionsText(node.connections).c_str());
        ImGui::TextDisabled("%s", node.characterId ? "Double-click to open their sheet. Drag to place them."
                                                   : "Drag to place them.");
        ImGui::EndTooltip();
    }

    // ---- the right-click menu
    if (ImGui::BeginPopup("webMenu")) {
        if (menuFor >= 0 && static_cast<size_t>(menuFor) < s.web.nodes.size()) {
            const size_t i = static_cast<size_t>(menuFor);
            const WebNode& node = s.web.nodes[i];
            ImGui::TextDisabled("%s", node.name.c_str());
            if (node.characterId && ImGui::MenuItem("Open their sheet")) openCharacter(w, node.characterId);
            if (node.characterId && ImGui::MenuItem("Find the way from them")) s.pathFrom = node.characterId;
            if (node.characterId && ImGui::MenuItem("Find the way to them")) s.pathTo = node.characterId;
            if (node.characterId && ImGui::MenuItem("New story event with them")) openNewEvent(w, {node.characterId});
            const bool pinned = i < s.layout.pinned.size() && s.layout.pinned[i];
            if (ImGui::MenuItem("Let them move again", nullptr, false, pinned)) {
                s.layout.pinned[i] = false;
                s.layout.temperature = std::max(s.layout.temperature, 0.3f);
            }
        } else {
            if (ImGui::MenuItem("Fit everyone in view")) s.follow = true;
            if (ImGui::MenuItem("Arrange everyone again")) {
                s.layout.start(s.web, db);
                s.follow = true;
            }
        }
        ImGui::EndPopup();
    }

    // ---- a hint along the bottom
    const char* hint = "Scroll to zoom. Drag the background to look around, or someone to place them.";
    draw->AddText(ImVec2(origin.x + font * 0.6f, origin.y + size.y - font * 1.5f), colourU32(palette().muted, 0.8f), hint);
}

}  // namespace

void drawWebScreen(WindowState& w) {
    WebScreen& s = w.web;
    const std::string signature = signatureOf(w.app.db, s.options);
    if (signature != s.builtFrom) {
        rebuild(w);
        s.builtFrom = signature;
    }
    // Two steps a frame: the web settles in a second or two.
    for (int i = 0; i < 2 && !s.layout.settled(); ++i) s.layout.step(s.web);

    // ---- the panel
    std::vector<size_t> path;
    ImGui::BeginChild("webPanel", ImVec2(ImGui::GetFontSize() * 19.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    heading(w, "Show");
    bondCheckboxes(s);
    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Who");
    ImGui::SameLine(ImGui::GetFontSize() * 3.2f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##organization", s.options.organization.empty() ? "Everyone" : s.options.organization.c_str())) {
        if (ImGui::Selectable("Everyone", s.options.organization.empty())) s.options.organization.clear();
        for (const std::string& org : s.organizations) {
            if (ImGui::Selectable(org.c_str(), org == s.options.organization)) s.options.organization = org;
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("People who aren't saved", &s.options.unsavedPeople);
    ImGui::SetItemTooltip("People named in a relationship who aren't saved characters, drawn dashed.");
    ImGui::Checkbox("Characters with no lines", &s.options.loners);
    ImGui::SetItemTooltip("Characters without any relationship shown wait in a ring around the rest.");
    ImGui::Checkbox("Words on the lines", &s.labels);
    ImGui::SetItemTooltip("Shown when zoomed in far enough, and always for the one under the pointer.");
    // The one picked comes first, so it can be seen without scrolling.
    if (nodeOfKey(s.web, s.selected) >= 0) {
        ImGui::Spacing();
        pickedSection(w);
    }
    ImGui::Spacing();
    pathSection(w, path);
    if (nodeOfKey(s.web, s.selected) < 0) {
        ImGui::Spacing();
        pickedSection(w);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the canvas
    ImGui::BeginChild("webCanvas", ImVec2(0, 0), ImGuiChildFlags_None);
    size_t saved = 0;
    for (const WebNode& n : s.web.nodes) saved += n.characterId != 0 ? 1 : 0;
    if (ImGui::Button("Fit everyone in view")) s.follow = true;
    ImGui::SameLine();
    if (ImGui::Button("Arrange again")) {
        s.layout.start(s.web, w.app.db);
        s.follow = true;
    }
    ImGui::SetItemTooltip("Start everyone on a circle again and let them settle. People you placed by hand move too.");
    ImGui::SameLine();
    exportButton(w);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (!s.layout.settled()) {
        const int dots = static_cast<int>(ImGui::GetTime() * 3.0) % 4;
        ImGui::TextColored(palette().gold, "Settling%.*s", dots, "...");
    } else {
        ImGui::TextDisabled("%zu %s, %s", saved, saved == 1 ? "character" : "characters",
                            connectionsText(static_cast<int>(s.web.edges.size())).c_str());
    }

    if (w.app.db.characters.empty()) {
        ImGui::Spacing();
        note("No characters yet. Create some on the Characters tab, or import the sample sets in Settings.");
    } else if (s.web.nodes.empty()) {
        ImGui::Spacing();
        note("Nobody matches what is ticked on the left.");
    } else {
        if (s.web.edges.empty()) {
            note("No relationships between the people shown yet. Add them on a character's Relationships tab.");
        }
        canvas(w, path);
    }
    ImGui::EndChild();
}

}  // namespace lotm::window
