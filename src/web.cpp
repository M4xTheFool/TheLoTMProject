#include "web.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <tuple>

#include "pathway_colours.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

std::string lowercase(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

std::uint64_t textHash(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

// A small fixed nudge from a key, -1 to 1, so the same web always lays out the same way.
float jitter(const std::string& key, int salt) {
    return static_cast<float>(textHash(key + "#" + std::to_string(salt)) % 2001) / 1000.0f - 1.0f;
}

// Which bond wins when two people have several relationships: the closer one.
int bondRank(Bond bond) {
    switch (bond) {
        case Bond::Love: return 0;
        case Bond::Family: return 1;
        case Bond::Teaching: return 2;
        case Bond::Conflict: return 3;
        case Bond::Friendship: return 4;
        case Bond::Allies: return 5;
        default: return 6;
    }
}

void addWord(std::string& list, const std::string& word) {
    if (word.empty()) return;
    if (list.empty()) {
        list = word;
        return;
    }
    if (lowercase(", " + list + ",").find(lowercase(", " + word + ",")) == std::string::npos) list += ", " + word;
}

}  // namespace

Bond bondOf(const std::string& type) {
    // The words of the type, in lowercase: "Sworn brother" -> sworn, brother.
    std::vector<std::string> words(1);
    for (char ch : lowercase(type)) {
        if (std::isalpha(static_cast<unsigned char>(ch))) words.back() += ch;
        else if (!words.back().empty()) words.emplace_back();
    }
    // A word that starts with one of these ("grand" finds grandmother), or is one of the exact words.
    auto has = [&](std::initializer_list<const char*> starts, std::initializer_list<const char*> exact = {}) {
        for (const std::string& word : words) {
            if (word.empty()) continue;
            for (const char* start : starts) {
                if (word.rfind(start, 0) == 0) return true;
            }
            for (const char* whole : exact) {
                if (word == whole) return true;
            }
        }
        return false;
    };
    if (has({"lover", "love", "spouse", "wife", "husband", "beloved", "fianc", "crush", "sweetheart", "girlfriend",
             "boyfriend", "consort", "romance", "romantic", "betrothed", "darling"})) {
        return Bond::Love;
    }
    if (has({"parent", "child", "sibling", "brother", "sister", "relative", "father", "mother", "daughter", "cousin",
             "family", "twin", "uncle", "aunt", "niece", "nephew", "grand", "ancestor", "descendant", "offspring",
             "kinsman", "kinswoman", "heir"},
            {"son", "sons", "kin"})) {
        return Bond::Family;
    }
    if (has({"mentor", "student", "teacher", "apprentice", "disciple", "pupil", "tutor"})) return Bond::Teaching;
    if (has({"rival", "enem", "foe", "nemesis", "hate", "opponent", "adversar", "target", "hunt", "killer",
             "murderer", "betray"})) {
        return Bond::Conflict;
    }
    if (has({"friend", "acquaintance", "companion", "confidant"})) return Bond::Friendship;
    if (has({"ally", "allies", "allied", "colleague", "superior", "subordinate", "comrade", "teammate", "partner",
             "member", "servant", "follower", "lieutenant", "employ", "boss", "leader", "patron", "client",
             "contact", "informant", "associate", "master", "guard", "bodyguard", "protector", "subject", "king",
             "queen", "captain", "crew", "handler", "agent", "believer", "worshipper", "squad"})) {
        return Bond::Allies;
    }
    return Bond::Other;
}

const char* bondName(Bond bond) {
    switch (bond) {
        case Bond::Love: return "Love";
        case Bond::Family: return "Family";
        case Bond::Friendship: return "Friends";
        case Bond::Allies: return "Allies and work";
        case Bond::Teaching: return "Teaching";
        case Bond::Conflict: return "Conflict";
        default: return "Other";
    }
}

std::string bondColour(Bond bond) {
    switch (bond) {
        case Bond::Love: return hexColour({226, 104, 150});
        case Bond::Family: return hexColour({224, 160, 72});
        case Bond::Friendship: return hexColour({104, 190, 120});
        case Bond::Allies: return hexColour({92, 156, 224});
        case Bond::Teaching: return hexColour({170, 130, 226});
        case Bond::Conflict: return hexColour({226, 84, 70});
        default: return hexColour({160, 154, 144});
    }
}

std::string WebEdge::label() const {
    if (fromB.empty() || lowercase(fromA) == lowercase(fromB)) return fromA.empty() ? fromB : fromA;
    if (fromA.empty()) return fromB;
    return fromA + " / " + fromB;
}

int Web::nodeOf(int characterId) const {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (characterId != 0 && nodes[i].characterId == characterId) return static_cast<int>(i);
    }
    return -1;
}

std::string Web::key(size_t node) const {
    if (node >= nodes.size()) return "";
    const WebNode& n = nodes[node];
    return n.characterId != 0 ? "c" + std::to_string(n.characterId) : "n:" + lowercase(n.name);
}

Web buildWeb(const Database& db, const WebOptions& options) {
    Web web;
    std::map<std::string, size_t> index;  // Web::key -> node
    auto shown = [&](const std::string& type) { return options.bonds[static_cast<size_t>(bondOf(type))]; };
    auto member = [&](const Character& c) {
        return options.organization.empty() || lowercase(c.affiliation.organization) == lowercase(options.organization);
    };

    for (const Character& c : db.characters) {
        if (!member(c)) continue;
        WebNode node;
        node.characterId = c.id;
        node.name = c.name.empty() ? "(unnamed)" : c.name;
        node.pathwayId = c.pathwayId;
        node.badge = nameInitials(node.name);
        node.level = levelName(c);
        node.organization = c.affiliation.organization;
        index["c" + std::to_string(c.id)] = web.nodes.size();
        web.nodes.push_back(node);
    }

    // One edge per pair; each side's words for the other, and the closest bond between them.
    std::map<std::pair<size_t, size_t>, size_t> pairs;
    auto connect = [&](size_t from, size_t to, const Relationship& r) {
        if (from == to) return;
        const size_t a = std::min(from, to), b = std::max(from, to);
        auto found = pairs.find({a, b});
        if (found == pairs.end()) {
            WebEdge edge;
            edge.a = a;
            edge.b = b;
            edge.bond = bondOf(r.type);
            found = pairs.emplace(std::make_pair(a, b), web.edges.size()).first;
            web.edges.push_back(edge);
        }
        WebEdge& edge = web.edges[found->second];
        addWord(from == a ? edge.fromA : edge.fromB, r.type);
        if (bondRank(bondOf(r.type)) < bondRank(edge.bond)) edge.bond = bondOf(r.type);
        if (!r.note.empty() && edge.note.find(r.note) == std::string::npos) {
            edge.note += (edge.note.empty() ? "" : "; ") + r.note;
        }
    };
    for (const Character& c : db.characters) {
        const auto self = index.find("c" + std::to_string(c.id));
        if (self == index.end()) continue;
        for (const Relationship& r : c.relationships) {
            if (!shown(r.type)) continue;
            if (r.characterId != 0) {
                const auto other = index.find("c" + std::to_string(r.characterId));
                if (other != index.end()) connect(self->second, other->second, r);
                continue;
            }
            if (!options.unsavedPeople || r.name.empty()) continue;
            const std::string key = "n:" + lowercase(r.name);
            auto other = index.find(key);
            if (other == index.end()) {
                WebNode node;
                node.name = r.name;
                node.badge = nameInitials(r.name);
                other = index.emplace(key, web.nodes.size()).first;
                web.nodes.push_back(node);
            }
            connect(self->second, other->second, r);
        }
    }

    for (const WebEdge& e : web.edges) {
        ++web.nodes[e.a].connections;
        ++web.nodes[e.b].connections;
    }
    if (!options.loners) {  // drop characters left without a line, and renumber the edges
        std::vector<int> renumber(web.nodes.size(), -1);
        std::vector<WebNode> kept;
        for (size_t i = 0; i < web.nodes.size(); ++i) {
            if (web.nodes[i].connections == 0) continue;
            renumber[i] = static_cast<int>(kept.size());
            kept.push_back(web.nodes[i]);
        }
        for (WebEdge& e : web.edges) {
            e.a = static_cast<size_t>(renumber[e.a]);
            e.b = static_cast<size_t>(renumber[e.b]);
        }
        web.nodes = kept;
    }
    return web;
}

std::vector<size_t> shortestPath(const Web& web, size_t from, size_t to) {
    if (from >= web.nodes.size() || to >= web.nodes.size()) return {};
    if (from == to) return {from};
    std::vector<std::vector<size_t>> next(web.nodes.size());
    for (const WebEdge& e : web.edges) {
        next[e.a].push_back(e.b);
        next[e.b].push_back(e.a);
    }
    std::vector<int> cameFrom(web.nodes.size(), -1);
    std::queue<size_t> queue;
    queue.push(from);
    cameFrom[from] = static_cast<int>(from);
    while (!queue.empty()) {
        const size_t at = queue.front();
        queue.pop();
        if (at == to) break;
        for (size_t n : next[at]) {
            if (cameFrom[n] >= 0) continue;
            cameFrom[n] = static_cast<int>(at);
            queue.push(n);
        }
    }
    if (cameFrom[to] < 0) return {};
    std::vector<size_t> path;
    for (size_t at = to; at != from; at = static_cast<size_t>(cameFrom[at])) path.push_back(at);
    path.push_back(from);
    std::reverse(path.begin(), path.end());
    return path;
}

const WebEdge* edgeBetween(const Web& web, size_t a, size_t b) {
    for (const WebEdge& e : web.edges) {
        if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) return &e;
    }
    return nullptr;
}

std::string describePath(const Web& web, const std::vector<size_t>& path) {
    std::string text;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] >= web.nodes.size()) return "";
        if (i == 0) {
            text = web.nodes[path[i]].name;
            continue;
        }
        // How the one before lists them: "Klein Moretti (Friend)".
        const WebEdge* edge = edgeBetween(web, path[i - 1], path[i]);
        std::string how;
        if (edge) how = edge->a == path[i - 1] ? edge->fromA : edge->fromB;
        if (edge && how.empty()) how = edge->label();
        text += " \xE2\x86\x92 " + web.nodes[path[i]].name + (how.empty() ? "" : " (" + how + ")");
    }
    return text;
}

// ---------------------------------------------------------------- layout

void WebLayout::start(const Web& web, const Database& db) {
    const size_t n = web.nodes.size();
    x.assign(n, 0.0f);
    y.assign(n, 0.0f);
    pinned.assign(n, false);
    // Around a circle, neighbouring pathways next to each other.
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) order[i] = i;
    auto group = [&](size_t i) {
        const Pathway* p = db.findPathway(web.nodes[i].pathwayId);
        return std::make_tuple(web.nodes[i].characterId == 0, p ? p->group : std::string("~"),
                               web.nodes[i].pathwayId, lowercase(web.nodes[i].name));
    };
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return group(a) < group(b); });
    const float radius = std::max(1.0f, std::sqrt(static_cast<float>(n)) * 0.9f);
    for (size_t rank = 0; rank < n; ++rank) {
        const size_t i = order[rank];
        const float angle = 6.2831853f * static_cast<float>(rank) / static_cast<float>(std::max<size_t>(n, 1));
        x[i] = std::cos(angle) * radius + jitter(web.key(i), 1) * 0.05f;
        y[i] = std::sin(angle) * radius + jitter(web.key(i), 2) * 0.05f;
    }
    temperature = std::max(0.4f, std::sqrt(static_cast<float>(n)) * 0.35f);
}

void WebLayout::adapt(const Web& oldWeb, const Web& newWeb, const Database& db) {
    std::map<std::string, std::pair<float, float>> was;
    std::map<std::string, bool> held;
    for (size_t i = 0; i < oldWeb.nodes.size() && i < x.size(); ++i) {
        was[oldWeb.key(i)] = {x[i], y[i]};
        held[oldWeb.key(i)] = i < pinned.size() && pinned[i];
    }
    if (was.empty()) {
        start(newWeb, db);
        return;
    }
    const size_t n = newWeb.nodes.size();
    std::vector<float> nx(n, 0.0f), ny(n, 0.0f);
    std::vector<bool> placed(n, false), np(n, false);
    for (size_t i = 0; i < n; ++i) {
        const auto it = was.find(newWeb.key(i));
        if (it == was.end()) continue;
        nx[i] = it->second.first;
        ny[i] = it->second.second;
        np[i] = held[newWeb.key(i)];
        placed[i] = true;
    }
    // Newcomers start next to someone they know, or at the edge.
    const float radius = std::max(1.0f, std::sqrt(static_cast<float>(n)) * 0.9f);
    for (size_t i = 0; i < n; ++i) {
        if (placed[i]) continue;
        const std::string key = newWeb.key(i);
        bool nearSomeone = false;
        for (const WebEdge& e : newWeb.edges) {
            const size_t other = e.a == i ? e.b : e.b == i ? e.a : n;
            if (other >= n || !placed[other]) continue;
            nx[i] = nx[other] + jitter(key, 3) * 0.6f;
            ny[i] = ny[other] + jitter(key, 4) * 0.6f;
            nearSomeone = true;
            break;
        }
        if (!nearSomeone) {
            const float angle = (jitter(key, 5) + 1.0f) * 3.14159265f;
            nx[i] = std::cos(angle) * radius;
            ny[i] = std::sin(angle) * radius;
        }
        placed[i] = true;
    }
    x = nx;
    y = ny;
    pinned = np;
    temperature = std::max(temperature, 0.35f);
}

bool WebLayout::step(const Web& web) {
    const size_t n = web.nodes.size();
    if (temperature <= 0.0f || n == 0 || x.size() != n) {
        temperature = 0.0f;
        return false;
    }
    const float spacing = 1.0f;  // the closest two people may be, so they don't overlap
    std::vector<float> dx(n, 0.0f), dy(n, 0.0f);
    std::vector<bool> loner(n, false);
    for (size_t i = 0; i < n; ++i) loner[i] = web.nodes[i].connections == 0;
    // Everyone with a line pushes everyone else away.
    for (size_t i = 0; i < n; ++i) {
        if (loner[i]) continue;
        for (size_t j = i + 1; j < n; ++j) {
            if (loner[j]) continue;
            float ox = x[i] - x[j], oy = y[i] - y[j];
            float d2 = ox * ox + oy * oy;
            if (d2 < 1e-6f) {  // on top of each other: push apart in a fixed direction
                ox = jitter(web.key(i), static_cast<int>(j)) * 0.01f + 0.01f;
                oy = jitter(web.key(j), static_cast<int>(i)) * 0.01f;
                d2 = ox * ox + oy * oy;
            }
            // 2 / d^2: like the classic k^2 / d close up, but fading faster with distance, so small
            // separate groups stay near the rest.
            const float d = std::sqrt(d2);
            float push = 2.0f / (d2 * d);
            if (d < spacing) push += (spacing - d) * 3.0f / d;
            dx[i] += ox * push;
            dy[i] += oy * push;
            dx[j] -= ox * push;
            dy[j] -= oy * push;
        }
    }
    // Lines pull the two people they join together (d^2).
    for (const WebEdge& e : web.edges) {
        const float ox = x[e.a] - x[e.b], oy = y[e.a] - y[e.b];
        const float pull = std::sqrt(ox * ox + oy * oy);
        dx[e.a] -= ox * pull;
        dy[e.a] -= oy * pull;
        dx[e.b] += ox * pull;
        dy[e.b] += oy * pull;
    }
    // People without a line wait in a ring around the rest, in the same order every time.
    float cx = 0.0f, cy = 0.0f, linked = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        if (loner[i]) continue;
        cx += x[i];
        cy += y[i];
        linked += 1.0f;
    }
    if (linked > 0.0f) {
        cx /= linked;
        cy /= linked;
    }
    float reach = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        if (!loner[i]) reach = std::max(reach, std::hypot(x[i] - cx, y[i] - cy));
    }
    std::vector<size_t> waiting;
    for (size_t i = 0; i < n; ++i) {
        if (loner[i]) waiting.push_back(i);
    }
    float ring = linked > 0.0f ? reach + spacing * 1.3f : 0.0f;
    for (size_t placed = 0; placed < waiting.size();) {
        const size_t fits = ring <= 0.0f ? 1 : std::max<size_t>(6, static_cast<size_t>(6.2831853f * ring / spacing));
        const size_t count = std::min(fits, waiting.size() - placed);
        for (size_t k = 0; k < count; ++k) {
            const size_t i = waiting[placed + k];
            // From the top, clockwise, spread over the whole ring.
            const float angle = -1.5707963f + 6.2831853f * (static_cast<float>(k) + 0.5f) / static_cast<float>(count);
            dx[i] = cx + std::cos(angle) * ring - x[i];
            dy[i] = cy + std::sin(angle) * ring - y[i];
        }
        placed += count;
        ring = ring <= 0.0f ? spacing : ring + spacing;
    }
    bool moving = false;
    for (size_t i = 0; i < n; ++i) {
        // A gentle pull to the middle keeps separate groups in view.
        if (!loner[i]) {
            dx[i] -= x[i] * 0.12f;
            dy[i] -= y[i] * 0.12f;
        }
        if (pinned.size() == n && pinned[i]) continue;
        const float length = std::sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
        if (length < 1e-6f) continue;
        const float move = std::min(length, std::max(temperature, loner[i] ? 0.08f : 0.0f));
        x[i] += dx[i] / length * move;
        y[i] += dy[i] / length * move;
        moving = moving || move > 0.002f;
    }
    // Then anyone still overlapping someone is moved apart, half the overlap each.
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const float ox = x[i] - x[j], oy = y[i] - y[j];
            const float d = std::sqrt(ox * ox + oy * oy);
            if (d >= spacing || d < 1e-6f) continue;
            const bool holdI = pinned.size() == n && pinned[i], holdJ = pinned.size() == n && pinned[j];
            if (holdI && holdJ) continue;
            const float shift = (spacing - d) / d * (holdI || holdJ ? 1.0f : 0.5f);
            if (!holdI) {
                x[i] += ox * shift;
                y[i] += oy * shift;
            }
            if (!holdJ) {
                x[j] -= ox * shift;
                y[j] -= oy * shift;
            }
        }
    }
    temperature *= 0.965f;
    if (temperature < 0.004f || !moving) temperature = 0.0f;
    return temperature > 0.0f;
}

void WebLayout::settle(const Web& web, int steps) {
    for (int i = 0; i < steps && step(web); ++i) {
    }
    temperature = 0.0f;
}

// ---------------------------------------------------------------- the picture and the sheet

WebFigure webFigure(const Web& web, const WebLayout& layout, const Database& db, bool links) {
    WebFigure figure;
    const size_t n = web.nodes.size();
    if (n == 0 || layout.x.size() != n) return figure;
    float minX = layout.x[0], maxX = layout.x[0], minY = layout.y[0], maxY = layout.y[0];
    for (size_t i = 1; i < n; ++i) {
        minX = std::min(minX, layout.x[i]);
        maxX = std::max(maxX, layout.x[i]);
        minY = std::min(minY, layout.y[i]);
        maxY = std::max(maxY, layout.y[i]);
    }
    const float spanX = std::max(maxX - minX, 0.5f), spanY = std::max(maxY - minY, 0.5f);
    const float padX = 70.0f, padY = 44.0f;
    figure.width = 860.0f;
    figure.height = std::clamp((figure.width - padX * 2.0f) * spanY / spanX + padY * 2.0f, 320.0f, 900.0f);
    const float scale = std::min((figure.width - padX * 2.0f) / spanX, (figure.height - padY * 2.0f - 16.0f) / spanY);
    const float offsetX = (figure.width - spanX * scale) * 0.5f, offsetY = (figure.height - 16.0f - spanY * scale) * 0.5f;

    for (size_t i = 0; i < n; ++i) {
        const WebNode& node = web.nodes[i];
        FigureNode f;
        f.x = offsetX + (layout.x[i] - minX) * scale;
        f.y = offsetY + (layout.y[i] - minY) * scale;
        f.saved = node.characterId != 0;
        f.radius = f.saved ? 16.0f + static_cast<float>(std::min(node.connections, 12)) * 0.6f : 11.0f;
        f.badge = node.badge;
        f.name = node.name;
        f.colour = hexColour(f.saved ? pathwayRgb(db, node.pathwayId) : Rgb{});
        f.tip = node.name;
        if (f.saved) {
            const Pathway* p = db.findPathway(node.pathwayId);
            f.tip += ", " + node.level + (p ? ", " + p->name + " pathway" : "");
            if (!node.organization.empty()) f.tip += ", " + node.organization;
            f.tip += ". " + std::to_string(node.connections) + (node.connections == 1 ? " connection" : " connections");
            if (links) f.link = "#c-" + std::to_string(node.characterId);
        } else {
            f.tip += " (not a saved character)";
        }
        figure.nodes.push_back(f);
    }
    std::vector<bool> used(kBondCount, false);
    for (const WebEdge& e : web.edges) {
        FigureEdge f;
        f.a = e.a;
        f.b = e.b;
        f.label = e.label();
        f.colour = bondColour(e.bond);
        f.tip = web.nodes[e.a].name + " and " + web.nodes[e.b].name + ": " + e.label() + (e.note.empty() ? "" : ". " + e.note);
        used[static_cast<size_t>(e.bond)] = true;
        figure.edges.push_back(f);
    }
    for (int b = 0; b < kBondCount; ++b) {
        if (used[static_cast<size_t>(b)]) figure.legend.push_back({bondName(static_cast<Bond>(b)), bondColour(static_cast<Bond>(b))});
    }
    return figure;
}

Sheet buildWebSheet(const Database& db, bool links) {
    const Web web = buildWeb(db);
    WebLayout layout;
    layout.start(web, db);
    layout.settle(web);
    return buildWebSheet(web, layout, db, links);
}

Sheet buildWebSheet(const Web& web, const WebLayout& layout, const Database& db, bool links) {
    Sheet sheet;
    sheet.anchor = "web";
    sheet.kind = "Relationship web";
    sheet.title = "Who Knows Whom";
    size_t saved = 0;
    for (const WebNode& n : web.nodes) saved += n.characterId != 0 ? 1 : 0;
    sheet.subtitle = std::to_string(saved) + (saved == 1 ? " character, " : " characters, ") +
                     std::to_string(web.edges.size()) + (web.edges.size() == 1 ? " connection" : " connections");
    if (saved != web.nodes.size()) {
        const size_t others = web.nodes.size() - saved;
        sheet.subtitle += ", and " + std::to_string(others) + (others == 1 ? " other person" : " other people");
    }

    Section picture{"The Web"};
    Block figure{BlockKind::Web, "", "", {}, {}, {}, {}};
    figure.web = webFigure(web, layout, db, links);
    if (!figure.web.nodes.empty()) picture.blocks.push_back(figure);
    picture.paragraph("Each circle is a character in their pathway's colour; each line is a relationship, coloured by "
                      "its kind. Rest the pointer on someone to see only their connections.");
    if (!web.nodes.empty()) sheet.sections.push_back(picture);

    // Who has the most.
    std::vector<size_t> order;
    for (size_t i = 0; i < web.nodes.size(); ++i) {
        if (web.nodes[i].connections > 0) order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return web.nodes[a].connections > web.nodes[b].connections; });
    Section most{"Most Connected"};
    for (size_t i = 0; i < order.size() && i < 5; ++i) {
        const WebNode& n = web.nodes[order[i]];
        most.bullet(n.name + ": " + std::to_string(n.connections) + (n.connections == 1 ? " connection" : " connections"));
    }
    if (!most.blocks.empty()) sheet.sections.push_back(most);

    for (int b = 0; b < kBondCount; ++b) {
        Section section{bondName(static_cast<Bond>(b))};
        for (const WebEdge& e : web.edges) {
            if (static_cast<int>(e.bond) != b) continue;
            std::string line = web.nodes[e.a].name + " and " + web.nodes[e.b].name + ": " + e.label();
            if (!e.note.empty()) line += ". " + e.note;
            section.bullet(line);
        }
        if (!section.blocks.empty()) sheet.sections.push_back(section);
    }
    if (web.nodes.empty()) sheet.subtitle = "Nobody to show";
    return sheet;
}

}  // namespace lotm
