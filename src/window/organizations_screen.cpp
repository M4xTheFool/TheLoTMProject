// Organizations tab: your organizations on the left, the one open on the right. Its members sit around
// its emblem as at the Tarot Club's long table above the grey fog, and gather there when it is opened.
// Organizations your characters name that aren't written up yet are listed too, ready to fill in.
#include <algorithm>
#include <cmath>

#include "organizations.hpp"
#include "records.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

constexpr float kPi = 3.14159265f;

std::string countOf(size_t n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

ImVec4 threatColour(const std::string& level) {
    if (level == "Low") return palette().success;
    if (level == "Moderate") return palette().gold;
    if (level == "High") return palette().warning;
    return palette().danger;
}

ImVec4 faded(const ImVec4& c, float alpha) { return ImVec4(c.x, c.y, c.z, c.w * alpha); }

// Cuts text to fit a width, ending it with "..." when something had to go.
std::string fitted(const std::string& text, float size, float width) {
    ImFont* font = ImGui::GetFont();
    if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= width) return text;
    std::string cut = text;
    while (!cut.empty() && font->CalcTextSizeA(size, FLT_MAX, 0.0f, (cut + "...").c_str()).x > width) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) cut.pop_back();  // a whole letter
    }
    return cut + "...";
}

void open(WindowState& w, const Organization& o, bool isNew) {
    auto& s = w.organizations;
    const bool another = !s.draft.open || s.draft.isNew != isNew || s.draft.value.id != o.id;
    Organization copy = o;
    linkLeaderByName(w.app.db, copy);  // a leader renamed since shows their name now
    s.draft.start(copy, isNew);
    if (another) {
        s.openedAt = ImGui::GetTime();
        s.seatedAt.clear();
        s.hovered = 0;
        s.addPathway.clear();
        s.newRank.clear();
    }
}

void openNew(WindowState& w, const std::string& name) {
    Organization o = name.empty() ? Organization{} : organizationFromMembers(w.app.db, name);
    o.id = w.app.db.nextOrganizationId();
    open(w, o, true);
}

bool saveImpl(WindowState& w) {
    auto& draft = w.organizations.draft;
    Organization& o = draft.value;
    o.name = ui::trim(o.name);
    if (o.name.empty()) {
        setStatus(w, "Give the organization a name before saving.", true);
        return false;
    }
    for (const Organization& other : w.app.db.organizations) {
        if ((draft.isNew || other.id != o.id) && ui::toLower(ui::trim(other.name)) == ui::toLower(o.name)) {
            setStatus(w, "Another organization is already called " + other.name + ". Give this one another name.", true);
            return false;
        }
    }
    const Organization* saved = draft.isNew ? nullptr : w.app.db.findOrganization(o.id);
    const std::string oldName = saved ? saved->name : "";
    try {
        const int renamed = saveOrganization(w.app, o, draft.isNew);
        std::string text = "Saved " + o.name + " as " + organizationCode(o.id) + ".";
        if (renamed > 0) {
            text += " Changed " + oldName + " to the new name on " +
                    (renamed == 1 ? std::string("1 character's page") : std::to_string(renamed) + " characters' pages") + " too.";
        }
        setStatus(w, text);
        open(w, o, false);
        if (renamed > 0) reloadCharacterDraft(w);
        return true;
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
        return false;
    }
}

// The organization as its members are found while it is being edited: the draft, which still answers
// to its saved name until a new one is saved (saving renames it for them).
Organization membershipView(const Organization& draft, const Organization* saved) {
    Organization view = draft;
    if (saved) {
        view.alsoKnownAs.push_back(saved->name);
        for (const auto& other : saved->alsoKnownAs) view.alsoKnownAs.push_back(other);
    }
    return view;
}

// ---------------------------------------------------------------- the list

void listPanel(WindowState& w) {
    auto& s = w.organizations;
    auto& draft = s.draft;
    const Database& db = w.app.db;
    auto saveThis = [&w] { return saveImpl(w); };
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (ImGui::Button("+ New organization", ImVec2(-FLT_MIN, 0))) {
        whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w, ""); });
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search names", &s.search);
    ImGui::Separator();
    auto shows = [&](const std::string& name, const std::vector<std::string>& others) {
        if (s.search.empty()) return true;
        const std::string wanted = ui::toLower(s.search);
        if (ui::toLower(name).find(wanted) != std::string::npos) return true;
        return std::any_of(others.begin(), others.end(),
                           [&](const std::string& o) { return ui::toLower(o).find(wanted) != std::string::npos; });
    };

    if (draft.open && draft.isNew) {
        const Organization& o = draft.value;
        listCard(w, "new", true, organizationColour(db, o), o.name.empty() ? "?" : initials(o.name),
                 o.name.empty() ? "(new organization)" : o.name, "Not saved yet");
    }
    const std::vector<const Organization*> saved = organizationsByName(db);
    size_t shown = 0;
    for (const Organization* o : saved) {
        if (!shows(o->name, o->alsoKnownAs)) continue;
        ++shown;
        const bool selected = draft.open && !draft.isNew && draft.value.id == o->id;
        // The living, as its page counts them.
        size_t members = o->otherMembers.size();
        for (const Member& m : membersOf(*o, db)) members += deadNow(*m.character, db) ? 0 : 1;
        const std::string subtitle = countOf(members, "member", "members") + "  \xC2\xB7  " +
                                     (o->kind.empty() ? organizationCode(o->id) : o->kind);
        if (listCard(w, "o" + std::to_string(o->id), selected, organizationColour(db, *o), initials(o->name), o->name,
                     subtitle) &&
            !selected) {
            const int id = o->id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                if (const Organization* found = w.app.db.findOrganization(id)) open(w, *found, false);
            });
        }
    }
    if (saved.empty() && !(draft.open && draft.isNew)) ImGui::TextDisabled("No organizations yet.");
    else if (!saved.empty() && shown == 0) ImGui::TextDisabled("None by that name.");

    // Names your characters use that have no page yet: one click starts it, with their ranks.
    const std::vector<UnsavedOrganization> unsaved = organizationsNotSaved(db);
    bool headed = false;
    for (const UnsavedOrganization& u : unsaved) {
        if (!shows(u.name, {})) continue;
        if (draft.open && draft.isNew && ui::toLower(ui::trim(draft.value.name)) == ui::toLower(u.name)) continue;  // being written up
        if (!headed) {
            ImGui::Spacing();
            ImGui::SeparatorText("Named by your characters");
            ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
            ImGui::TextWrapped("Not written up yet. Pick one to give it a page.");
            ImGui::PopStyleColor();
            headed = true;
        }
        const std::string subtitle = u.members == 0 ? std::string("only in earlier stages")
                                                    : countOf(static_cast<size_t>(u.members), "member", "members") + ", no page yet";
        if (listCard(w, "u" + u.name, false, palette().muted, "+", u.name, subtitle)) {
            const std::string name = u.name;
            whenSaved(w, draft.dirty(), saveThis, [&w, name] { openNew(w, name); });
        }
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------- Details

// Ranks, highest first: a box each, with buttons to move it up or down and to remove it.
void rankList(std::vector<std::string>& ranks, std::string& entry) {
    const float button = ImGui::GetFrameHeight();
    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushID("ranks");
    for (size_t i = 0; i < ranks.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        int move = 0;
        ImGui::BeginDisabled(i == 0);
        if (ImGui::ArrowButton("up", ImGuiDir_Up)) move = -1;
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Higher");
        ImGui::SameLine(0.0f, gap);
        ImGui::BeginDisabled(i + 1 == ranks.size());
        if (ImGui::ArrowButton("down", ImGuiDir_Down)) move = 1;
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Lower");
        ImGui::SameLine(0.0f, gap);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - gap);
        ImGui::InputText("##rank", &ranks[i]);
        const bool emptied = ImGui::IsItemDeactivatedAfterEdit() && ranks[i].empty();
        ImGui::SameLine(0.0f, gap);
        const bool remove = ImGui::Button("x", ImVec2(button, button));
        ImGui::SetItemTooltip("Remove");
        ImGui::PopID();
        if (remove || emptied) {
            ranks.erase(ranks.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
        if (move != 0) {
            std::swap(ranks[i], ranks[move == 1 ? i + 1 : i - 1]);
            break;
        }
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - gap);
    const bool entered = ImGui::InputTextWithHint("##newRank", "add a rank, e.g. Captain", &entry, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0.0f, gap);
    if ((ImGui::Button("+", ImVec2(button, button)) || entered) && !ui::trim(entry).empty()) {
        ranks.push_back(ui::trim(entry));
        entry.clear();
    }
    ImGui::SetItemTooltip("Add");
    ImGui::PopID();
}

void detailsTab(WindowState& w, Organization& o, const std::vector<Member>& members) {
    auto& s = w.organizations;
    const Database& db = w.app.db;
    s.draft.focusNameOnce();
    textField("Name", o.name, "required");
    fieldLabel("Also known as");
    ImGui::TextDisabled("Other names for it. A character whose affiliation uses one belongs to it too.");
    ImGui::Indent(ImGui::GetFontSize() * 9.5f);
    listEditor("aliases", o.alsoKnownAs, "add a name, e.g. Nighthawks");
    ImGui::Unindent(ImGui::GetFontSize() * 9.5f);
    presetField("Kind", o.kind, kOrganizationKinds);
    presetField("Status", o.status, kOrganizationStatuses);

    // The leader: typed, or picked from the members and then everyone else. A name that is one saved
    // character's is linked to them when it is saved.
    std::vector<std::string> people;
    for (const Member& m : members) people.push_back(m.character->name);
    for (const Character& c : db.characters) {
        if (std::find(people.begin(), people.end(), c.name) == people.end()) people.push_back(c.name);
    }
    if (presetField("Leader", o.leader, people)) o.leaderId = 0;
    {
        const Character* leader = o.leaderId != 0 ? db.findCharacter(o.leaderId) : nullptr;
        if (!leader && !ui::trim(o.leader).empty()) {
            const auto found = db.findCharactersByName(ui::trim(o.leader), 0);
            if (found.size() == 1 && ui::toLower(found[0]->name) == ui::toLower(ui::trim(o.leader))) leader = found[0];
        }
        if (leader) {
            fieldLabel("");
            statusDot(pathwayColour(db, leader->pathwayId));
            const int id = leader->id;
            if (ImGui::TextLink((leader->name + ", a saved character").c_str())) openCharacter(w, id);
        }
    }
    textField("Headquarters", o.headquarters, "e.g. the Sefirah Castle");
    storyDateField("Founded", o.founded, "e.g. 1349, or 3 July 1349");
    textField("Symbol", o.symbol, "its emblem, or how members know each other");

    // Pathways it is known for; the first gives its colour.
    for (size_t i = 0; i < o.pathwayIds.size(); ++i) {
        fieldLabel(i == 0 ? "Pathways" : "");
        statusDot(pathwayColour(db, o.pathwayIds[i]));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(pathwayLabel(db, o.pathwayIds[i]).c_str());
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("Remove")) {
            o.pathwayIds.erase(o.pathwayIds.begin() + static_cast<std::ptrdiff_t>(i));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (pathwayCombo(o.pathwayIds.empty() ? "Pathways" : "Add a pathway", db, s.addPathway, "Add a pathway it is known for") &&
        !s.addPathway.empty()) {
        if (std::find(o.pathwayIds.begin(), o.pathwayIds.end(), s.addPathway) == o.pathwayIds.end()) {
            o.pathwayIds.push_back(s.addPathway);
        }
        s.addPathway.clear();
    }
    if (o.pathwayIds.empty()) {
        fieldLabel("");
        ImGui::TextDisabled("Its colour comes from its first pathway, else its leader's, else its name.");
    }

    multilineField("What it is", o.description, 4);
    ImGui::SeparatorText("Ranks, highest first");
    ImGui::TextDisabled("Members pick from these, and the Members tab sits them in this order.");
    rankList(o.ranks, s.newRank);
    ImGui::SeparatorText("Members who aren't saved characters");
    listEditor("others", o.otherMembers, "add someone, e.g. Dunn Smith, Captain");
    multilineField("Notes", o.notes, 3);
}

// ---------------------------------------------------------------- Members

// Where n seats go round an oval, from the top and clockwise: spaced evenly along its edge rather than by
// angle, which would bunch them up at its narrow ends.
std::vector<float> seatAngles(size_t n, float rx, float ry) {
    constexpr int kSteps = 360;
    std::vector<float> lengths(kSteps + 1, 0.0f);
    auto at = [&](int step) {
        const float angle = -kPi * 0.5f + 2.0f * kPi * static_cast<float>(step) / static_cast<float>(kSteps);
        return ImVec2(std::cos(angle) * rx, std::sin(angle) * ry);
    };
    for (int step = 1; step <= kSteps; ++step) {
        const ImVec2 a = at(step - 1), b = at(step);
        lengths[static_cast<size_t>(step)] = lengths[static_cast<size_t>(step - 1)] + std::hypot(b.x - a.x, b.y - a.y);
    }
    std::vector<float> angles;
    int step = 0;
    for (size_t i = 0; i < n; ++i) {
        const float wanted = lengths.back() * static_cast<float>(i) / static_cast<float>(n);
        while (step < kSteps && lengths[static_cast<size_t>(step + 1)] < wanted) ++step;
        const float span = lengths[static_cast<size_t>(step + 1)] - lengths[static_cast<size_t>(step)];
        const float part = span > 0.0f ? (wanted - lengths[static_cast<size_t>(step)]) / span : 0.0f;
        angles.push_back(-kPi * 0.5f + 2.0f * kPi * (static_cast<float>(step) + part) / static_cast<float>(kSteps));
    }
    return angles;
}

// Someone at the table: a saved member, or one of the members who aren't saved characters.
struct Seat {
    int key = 0;  // the character's id; for the others -1, -2 ... in the order they are listed
    std::string name;
    std::string role;
    ImVec4 colour;
    bool leader = false;
    const Character* character = nullptr;  // nullptr for the others
    std::string death;                     // "Died 1349 ..." for a member who has died
};

// The leader, then its saved members as membersOf orders them, then the others. "Dunn Smith, Captain" is
// Dunn Smith with the rank of Captain.
std::vector<Seat> seatsAt(const Database& db, const Organization& o, const std::vector<Member>& members) {
    std::vector<Seat> seats;
    for (const Member& m : members) {
        seats.push_back({m.character->id, m.character->name, memberRole(m), pathwayColour(db, m.character->pathwayId), m.leader,
                         m.character, deathText(deathOf(*m.character, db), db)});
    }
    const std::string leader = ui::toLower(ui::trim(leaderName(o, db)));
    int next = -1;
    for (const auto& line : o.otherMembers) {
        const std::string text = ui::trim(line);
        if (text.empty()) continue;
        const auto comma = text.find(',');
        Seat seat;
        seat.key = next--;
        seat.name = ui::trim(text.substr(0, comma));
        seat.role = comma == std::string::npos ? "" : ui::trim(text.substr(comma + 1));
        seat.colour = palette().muted;
        seat.leader = !leader.empty() && ui::toLower(seat.name) == leader;
        if (seat.leader) seat.role = seat.role.empty() ? "Leader" : seat.role + ", leader";
        seats.push_back(seat);
    }
    std::stable_partition(seats.begin(), seats.end(), [](const Seat& seat) { return seat.leader; });  // at the head
    return seats;
}

// Its members around a table, the leader at the head and the rest by rank, gathering from the emblem
// when the organization is opened; members who aren't saved characters sit at the foot, in grey. A saved
// member's medallion opens their page.
void roundTable(WindowState& w, const Organization& o, const std::vector<Member>& members) {
    auto& s = w.organizations;
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    const float width = std::max(ImGui::GetContentRegionAvail().x, font * 10.0f);
    const float height = std::clamp(width * 0.46f, font * 15.0f, font * 23.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("table", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, ImVec2(origin.x + width, origin.y + height), true);

    const std::vector<Seat> seats = seatsAt(db, o, members);
    const size_t n = seats.size();
    const bool crowded = n > 14;
    const float seat = font * (crowded ? 1.05f : 1.3f);
    const float nameSize = font * (crowded ? 0.78f : 0.86f);
    const ImVec2 centre(origin.x + width * 0.5f, origin.y + height * 0.5f - font * 0.4f);
    const float rx = std::max(width * 0.5f - font * 4.5f, font * 5.0f);
    const float ry = std::max(height * 0.5f - seat - font * 2.0f, font * 3.5f);
    const ImVec4 colour = organizationColour(db, o);
    const float time = animations() ? static_cast<float>(ImGui::GetTime()) : 0.0f;

    // The table: a soft oval that spreads out first.
    const float spread = easeIn(s.openedAt, 0.7f);
    if (spread > 0.01f) {
        const ImVec2 table(rx * 0.74f * spread, ry * 0.6f * spread);
        draw->AddEllipseFilled(centre, table, colourU32(colour, 0.06f), 0.0f, 64);
        draw->AddEllipse(centre, table, colourU32(colour, 0.3f), 0.0f, 64, std::max(1.0f, font * 0.07f));
        draw->AddEllipse(centre, ImVec2(table.x * 1.04f, table.y * 1.06f), colourU32(colour, 0.12f), 0.0f, 64, 1.0f);
    }

    // Each seat, round from the head of the table; a member slides out from the emblem to it.
    const double stagger = n > 0 ? std::min(0.07, 0.9 / static_cast<double>(n)) : 0.0;
    const double now = ImGui::GetTime();
    int under = 0;
    struct Placed {
        ImVec2 at;
        float shown;
        const Seat* seat;
    };
    std::vector<Placed> placed;
    const std::vector<float> angles = seatAngles(n, rx, ry);
    for (size_t i = 0; i < n; ++i) {
        const ImVec2 seatAt(centre.x + std::cos(angles[i]) * rx, centre.y + std::sin(angles[i]) * ry);
        // Everyone gathers one after another when it is opened; someone who joins later walks in alone.
        const double seated =
            s.seatedAt.try_emplace(seats[i].key, now - s.openedAt < 1.0 ? s.openedAt + 0.2 + stagger * static_cast<double>(i) : now)
                .first->second;
        const float t = easeIn(seated, 0.6f);
        const ImVec2 at(centre.x + (seatAt.x - centre.x) * t, centre.y + (seatAt.y - centre.y) * t);
        placed.push_back({at, t, &seats[i]});
        if (hovered && t > 0.5f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const float dx = mouse.x - at.x, dy = mouse.y - at.y;
            if (dx * dx + dy * dy < seat * seat * 1.3f) under = seats[i].key;
        }
    }
    // Faint lines from the emblem to each seat, brighter for the one under the pointer.
    for (const Placed& p : placed) {
        const bool lit = p.seat->key == s.hovered;
        draw->AddLine(centre, p.at, colourU32(colour, (lit ? 0.45f : 0.12f) * p.shown), lit ? 2.0f : 1.0f);
    }

    // The emblem: the organization's initials, with a slow pulse of light around it.
    const float emblem = font * 2.5f * std::max(popIn(s.openedAt, 0.6f), 0.0f);
    if (emblem > 1.0f) {
        const float pulse = 0.5f + 0.5f * std::sin(time * 1.3f);
        draw->AddCircle(centre, emblem + font * (0.3f + 0.35f * pulse), colourU32(colour, 0.28f * (1.0f - pulse)), 0,
                        std::max(1.0f, font * 0.1f));
        medallionAt(w, centre, emblem, colour, o.name.empty() ? "?" : initials(o.name));
    }

    // Room for a name: the gap between seats along the edge, up to a limit.
    const float edge = 2.0f * kPi * std::sqrt((rx * rx + ry * ry) * 0.5f);
    const float seatWidth = n > 0 ? std::clamp(edge / static_cast<float>(n) * 0.95f, font * 3.0f, font * 8.0f) : font * 8.0f;
    for (const Placed& p : placed) {
        const Seat& who = *p.seat;
        const bool lit = who.key == s.hovered;
        const float grow = glide(ImGui::GetID(("seat " + std::to_string(who.key)).c_str()), lit ? 1.15f : 1.0f, 14.0f);
        const float r = seat * grow * std::max(p.shown, 0.15f) * (who.character ? 1.0f : 0.85f);
        if (who.leader) {
            draw->AddCircle(p.at, r + font * 0.28f, colourU32(palette().gold, 0.9f * p.shown), 0, std::max(1.5f, font * 0.1f));
        }
        // A member who has died: greyed, with a mourning band.
        const float dead = glide(ImGui::GetID(("seatDead " + std::to_string(who.key)).c_str()), who.death.empty() ? 0.0f : 1.0f, 5.0f);
        medallionAt(w, p.at, r, faded(mourned(who.colour, dead), p.shown), initials(who.name));
        mourningBand(draw, p.at, r, dead * p.shown);
        // The name and rank below, once they have arrived.
        const float words = std::clamp((p.shown - 0.6f) / 0.4f, 0.0f, 1.0f);
        if (words <= 0.0f) continue;
        const std::string name = fitted(who.name, nameSize, seatWidth);
        const ImVec2 nameSizePx = ImGui::GetFont()->CalcTextSizeA(nameSize, FLT_MAX, 0.0f, name.c_str());
        float y = p.at.y + seat * 1.15f + font * 0.15f;
        const ImVec4 nameColour = lit ? palette().gold : who.character ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : palette().muted;
        draw->AddText(ImGui::GetFont(), nameSize, ImVec2(p.at.x - nameSizePx.x * 0.5f, y), colourU32(nameColour, words),
                      name.c_str());
        y += nameSizePx.y;
        if (!who.role.empty()) {
            const std::string shownRank = fitted(who.role, nameSize * 0.9f, seatWidth);
            const ImVec2 rankSize = ImGui::GetFont()->CalcTextSizeA(nameSize * 0.9f, FLT_MAX, 0.0f, shownRank.c_str());
            draw->AddText(ImGui::GetFont(), nameSize * 0.9f, ImVec2(p.at.x - rankSize.x * 0.5f, y),
                          colourU32(who.leader ? palette().gold : palette().muted, words), shownRank.c_str());
        }
    }
    if (n == 0 && spread > 0.5f) {
        const char* text = "Nobody sits here yet";
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(centre.x - size.x * 0.5f, centre.y + font * 3.4f), colourU32(palette().muted, spread), text);
    }
    draw->PopClipRect();

    s.hovered = under;
    const auto it = std::find_if(seats.begin(), seats.end(), [&](const Seat& seat) { return seat.key == under; });
    if (under == 0 || it == seats.end()) return;
    if (it->character) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::BeginTooltip()) {
        ImGui::TextUnformatted(it->name.c_str());
        if (!it->role.empty()) ImGui::TextColored(it->leader ? palette().gold : palette().muted, "%s", it->role.c_str());
        if (it->character) {
            ImGui::TextDisabled("%s", levelText(*it->character, db).c_str());
            if (!it->death.empty()) ImGui::TextColored(deathColour(), "%s", it->death.c_str());
            ImGui::TextDisabled("Click to open their page.");
        } else {
            ImGui::TextDisabled("Not a saved character. Change them on the Details tab.");
        }
        ImGui::EndTooltip();
    }
    if (clicked && it->character) openCharacter(w, it->character->id);
}

// Characters who aren't members yet, to pick one to join.
void addMemberPopup(WindowState& w, const Organization& saved) {
    auto& s = w.organizations;
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    if (!ImGui::BeginPopup("add member")) return;
    ImGui::TextUnformatted(("Who joins " + saved.name + "?").c_str());
    const std::vector<std::string> ranks = rankSuggestions(db, saved.name);
    ImGui::SetNextItemWidth(font * 20.0f);
    ImGui::InputTextWithHint("##rank", "their rank or role (optional)", &s.newMemberRank);
    if (!ranks.empty()) {
        ImGui::SameLine();
        if (ImGui::BeginCombo("##ranks", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
            for (const auto& rank : ranks) {
                if (ImGui::Selectable(rank.c_str(), rank == s.newMemberRank)) s.newMemberRank = rank;
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SetNextItemWidth(font * 20.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##who", "Search names", &s.memberSearch);
    std::vector<const Character*> shown;
    for (const Character& c : db.characters) {
        if (belongsTo(db, saved, c.affiliation.organization)) continue;
        if (!s.memberSearch.empty() && ui::toLower(c.name).find(ui::toLower(s.memberSearch)) == std::string::npos) continue;
        shown.push_back(&c);
    }
    if (shown.empty()) ImGui::TextDisabled(s.memberSearch.empty() ? "Every saved character belongs to it already." : "Nobody by that name.");
    const float card = ImGui::GetTextLineHeight() * 2.0f + font * 0.8f + ImGui::GetStyle().ItemSpacing.y;
    int picked = 0;
    if (!shown.empty()) {
        ImGui::BeginChild("cards", ImVec2(font * 20.0f, card * static_cast<float>(std::min<size_t>(shown.size(), 7)) +
                                                            ImGui::GetStyle().WindowPadding.y));
        for (const Character* c : shown) {
            const std::string now = isIndependent(c->affiliation.organization) ? levelName(*c) : "in " + c->affiliation.organization;
            ImGui::PushID(c->id);
            if (listCard(w, "pick", false, pathwayColour(db, c->pathwayId), initials(c->name), c->name, now)) picked = c->id;
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    if (picked != 0) {
        const Character* c = db.findCharacter(picked);
        if (w.characters.draft.dirty() && w.characters.draft.value.id == picked) {
            setStatus(w, "Save or undo the changes to " + c->name + " on the Characters tab first.", true);
        } else {
            const std::string name = c->name;
            const std::string before = isIndependent(c->affiliation.organization) ? "" : ui::trim(c->affiliation.organization);
            try {
                joinOrganization(w.app, picked, saved, s.newMemberRank);
                // A line naming them among the members who aren't saved characters is gone now.
                if (!s.draft.dirty()) open(w, saved, false);
                reloadCharacterDraft(w);
                setStatus(w, name + " joined " + saved.name + (before.empty() ? "." : ", leaving " + before + "."));
                s.newMemberRank.clear();
                s.memberSearch.clear();
                ImGui::CloseCurrentPopup();
            } catch (const StorageError& e) {
                setStatus(w, std::string("Saving failed: ") + e.what(), true);
            }
        }
    }
    ImGui::EndPopup();
}

void membersTab(WindowState& w, const Organization& o, const Organization& view, const std::vector<Member>& members) {
    auto& s = w.organizations;
    const Database& db = w.app.db;
    const Organization* saved = s.draft.isNew ? nullptr : db.findOrganization(o.id);
    roundTable(w, o, members);

    ImGui::BeginDisabled(!saved);
    if (ImGui::Button("+ Add a member")) {
        s.memberSearch.clear();
        ImGui::OpenPopup("add member");
    }
    ImGui::EndDisabled();
    if (!saved) ImGui::SetItemTooltip("Save the organization first.");
    if (saved) addMemberPopup(w, *saved);
    ImGui::SameLine();
    ImGui::TextDisabled("A saved character is a member when their affiliation names it.");

    if (!members.empty() && ImGui::BeginTable("members", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                                                                ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.4f);
        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("##leave", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        int leave = 0;
        for (const Member& m : members) {
            const Character& c = *m.character;
            ImGui::PushID(c.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const Death death = deathOf(c, db);
            statusDot(death.dead ? deathColour() : pathwayColour(db, c.pathwayId));
            if (ImGui::TextLink(c.name.c_str())) openCharacter(w, c.id);
            ImGui::TableNextColumn();
            if (m.leader) ImGui::TextColored(palette().gold, "%s", memberRole(m).c_str());
            else if (m.rank.empty()) ImGui::TextDisabled("-");
            else ImGui::TextUnformatted(m.rank.c_str());
            ImGui::TableNextColumn();
            if (death.dead) {
                ImGui::PushStyleColor(ImGuiCol_Text, deathColour());
                ImGui::TextWrapped("%s. %s", levelText(c, db).c_str(), deathText(death, db).c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextWrapped("%s", levelText(c, db).c_str());
            }
            ImGui::TableNextColumn();
            const bool editing = w.characters.draft.dirty() && w.characters.draft.value.id == c.id;
            ImGui::BeginDisabled(editing);
            if (ImGui::SmallButton("Take out")) leave = c.id;
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("%s", editing ? "They have unsaved changes on the Characters tab." : "Clears their affiliation.");
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (leave != 0) {
            const std::string name = db.findCharacter(leave)->name;
            try {
                leaveOrganization(w.app, leave);
                reloadCharacterDraft(w);
                setStatus(w, name + " no longer belongs to " + view.name + ".");
            } catch (const StorageError& e) {
                setStatus(w, std::string("Saving failed: ") + e.what(), true);
            }
        }
    }

    std::vector<std::string> others;
    for (const auto& line : o.otherMembers) {
        if (!ui::trim(line).empty()) others.push_back(ui::trim(line));
    }
    if (!others.empty()) {
        ImGui::Spacing();
        heading(w, "Not saved as characters");
        int join = 0;
        std::string joinRank;
        for (const auto& line : others) {
            ImGui::PushID(line.c_str());
            ImGui::Bullet();
            ImGui::TextWrapped("%s", line.c_str());
            // Someone saved since with that name: one click makes them a member, with the line's role as their rank.
            const auto comma = line.find(',');
            const std::string name = ui::trim(line.substr(0, comma));
            const auto found = db.findCharactersByName(name, 0);
            const Character* c = found.size() == 1 && ui::toLower(found[0]->name) == ui::toLower(name) ? found[0] : nullptr;
            if (c && saved) {
                ImGui::SameLine();
                const std::string where = isIndependent(c->affiliation.organization)
                                              ? "is a saved character now"
                                              : "is a saved character, in " + c->affiliation.organization;
                ImGui::TextDisabled("%s", where.c_str());
                ImGui::SameLine();
                const bool editing = w.characters.draft.dirty() && w.characters.draft.value.id == c->id;
                ImGui::BeginDisabled(editing || s.draft.dirty());
                if (ImGui::SmallButton("Make them a member")) {
                    join = c->id;
                    joinRank = comma == std::string::npos ? "" : ui::trim(line.substr(comma + 1));
                }
                ImGui::EndDisabled();
                ImGui::SetItemTooltip("%s", editing           ? "They have unsaved changes on the Characters tab."
                                            : s.draft.dirty() ? "Save the changes here first."
                                                              : "Their affiliation becomes this organization, and the line here goes.");
            }
            ImGui::PopID();
        }
        ImGui::TextDisabled("Change these on the Details tab. Anyone here saved as a character later joins by themselves.");
        if (join != 0 && saved) {
            const std::string name = db.findCharacter(join)->name;
            const std::string organization = saved->name;
            try {
                joinOrganization(w.app, join, *saved, joinRank);  // which takes the line away too
                if (const Organization* now = db.findOrganization(o.id)) open(w, *now, false);
                reloadCharacterDraft(w);
                setStatus(w, name + " joined " + organization + ".");
            } catch (const StorageError& e) {
                setStatus(w, std::string("Saving failed: ") + e.what(), true);
            }
        }
    }

    // Relationships your characters have with it as a whole, which reach every member.
    const std::vector<OrganizationRelationship> ties = saved ? relationshipsWithOrganization(db, saved->id)
                                                             : std::vector<OrganizationRelationship>{};
    if (!ties.empty()) {
        ImGui::Spacing();
        heading(w, "Relationships with it");
        ImGui::TextDisabled("Each reaches every member, unless a member has a relationship of their own with them.");
        int openId = 0;
        for (const OrganizationRelationship& r : ties) {
            ImGui::PushID(r.character->id);
            ImGui::PushID(r.relationship);
            const float shown = glideFrom(ImGui::GetID("shown"), 0.0f, 1.0f, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * shown);
            const bool dead = deadNow(*r.character, db);
            statusDot(dead ? deathColour() : pathwayColour(db, r.character->pathwayId));
            if (ImGui::TextLink(r.character->name.c_str())) openId = r.character->id;
            ImGui::SameLine();
            // As the organization sees them: someone who calls it their Mentor is its Student.
            ImGui::TextUnformatted(reciprocalType(r.relationship->type).c_str());
            if (!r.relationship->note.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", r.relationship->note.c_str());
            }
            ImGui::PopStyleVar();
            ImGui::PopID();
            ImGui::PopID();
        }
        ImGui::TextDisabled("Add or change them on a character's Relationships tab.");
        if (openId != 0) openCharacter(w, openId);
    }

    const std::vector<FormerMember> former = formerMembersOf(view, db);
    if (!former.empty()) {
        ImGui::Spacing();
        heading(w, "Former members");
        ImGui::TextDisabled("They belonged to it in an earlier stage, or after a story event.");
        for (const FormerMember& m : former) {
            ImGui::PushID(m.character->id);
            statusDot(pathwayColour(db, m.character->pathwayId));
            if (ImGui::TextLink(m.character->name.c_str())) openCharacter(w, m.character->id);
            std::string about = ui::trim(m.rank);
            const std::string dates = formerMemberDates(m);
            if (!dates.empty()) about += (about.empty() ? "" : ", ") + dates;
            if (!about.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", about.c_str());
            }
            ImGui::PopID();
        }
    }
}

// ---------------------------------------------------------------- Strength

// A row of the strength bars: a label, a bar that grows to its share, and the number.
void strengthBar(const std::string& label, int value, int most, const ImVec4& colour, const std::string& key) {
    const float font = ImGui::GetFontSize();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(value > 0 ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : palette().muted, "%s", label.c_str());
    ImGui::SameLine(font * 13.0f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = std::max(ImGui::GetContentRegionAvail().x - font * 3.0f, font * 4.0f);
    const float height = ImGui::GetFrameHeight() * 0.55f;
    const float target = most > 0 ? static_cast<float>(value) / static_cast<float>(most) : 0.0f;
    const float shown = glideFrom(ImGui::GetID(key.c_str()), 0.0f, target, 6.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float top = at.y + (ImGui::GetFrameHeight() - height) * 0.5f;
    draw->AddRectFilled(ImVec2(at.x, top), ImVec2(at.x + width, top + height), ImGui::GetColorU32(ImGuiCol_FrameBg), height * 0.5f);
    if (shown > 0.002f) {
        draw->AddRectFilled(ImVec2(at.x, top), ImVec2(at.x + std::max(width * shown, height), top + height), colourU32(colour, 0.85f),
                            height * 0.5f);
    }
    ImGui::Dummy(ImVec2(width, ImGui::GetFrameHeight()));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (value > 0) ImGui::TextUnformatted(std::to_string(value).c_str());
    else ImGui::TextDisabled("0");
}

// A large number that counts up to its value, with a label under it.
void bigNumber(const WindowState& w, const char* label, int value, const std::string& key) {
    ImGui::BeginGroup();
    const float shown = glideFrom(ImGui::GetID(key.c_str()), 0.0f, static_cast<float>(value), 5.0f);
    ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 2.1f);  // Cinzel: its figures read better large
    ImGui::PushStyleColor(ImGuiCol_Text, palette().gold);
    ImGui::TextUnformatted(std::to_string(static_cast<int>(std::lround(shown))).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::TextDisabled("%s", label);
    ImGui::EndGroup();
}

void strengthTab(WindowState& w, const Organization& o, const Organization& view) {
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    const OrganizationStrength st = organizationStrength(view, db);
    if (st.members == 0) {
        ImGui::Spacing();
        ImGui::TextWrapped("Its strength comes from its living members who are saved characters: their Sequences, threat "
                           "levels and Sealed Artifacts. Add some on the Members tab.");
        if (st.dead > 0) ImGui::TextColored(deathColour(), "%d of its members died.", st.dead);
        return;
    }
    const std::string id = std::to_string(o.id);
    bigNumber(w, st.others > 0 ? ("members (and " + std::to_string(st.others) + " not saved)").c_str() : "members",
              st.members, "members " + id);
    ImGui::SameLine(0.0f, font * 3.0f);
    bigNumber(w, "combined threat", st.threatScore, "threat " + id);
    ImGui::SetItemTooltip("Every member's threat score added up: their Sequence, recent actions and Sealed Artifacts.");
    ImGui::SameLine(0.0f, font * 3.0f);
    bigNumber(w, "Sealed Artifacts held", st.artifacts, "artifacts " + id);
    if (st.dead > 0) {
        ImGui::TextColored(deathColour(), "Not counting the %d %s who died: nothing on this tab does.", st.dead,
                           st.dead == 1 ? "member" : "members");
    }

    ImGui::Spacing();
    auto person = [&](const char* title, const Character* c, const std::string& extra, const ImVec4& extraColour) {
        if (!c) return;
        ImGui::PushID(title);
        medallion(w, initials(c->name), pathwayColour(db, c->pathwayId), font * 1.2f);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("%s", title);
        if (ImGui::TextLink(c->name.c_str())) openCharacter(w, c->id);
        ImGui::SameLine();
        ImGui::TextColored(extraColour, "%s", extra.c_str());
        ImGui::EndGroup();
        ImGui::PopID();
    };
    person("Strongest", st.strongest, st.strongest ? levelText(*st.strongest, db) : "", palette().muted);
    person("Most dangerous", st.mostDangerous, st.threatLevel + " threat", threatColour(st.threatLevel));

    ImGui::Spacing();
    heading(w, "Power");
    static const std::array<const char*, 7> kTierLabels = {"Mortals", "Sequence 9 to 8", "Sequence 7 to 5", "Saints (4 to 3)",
                                                           "Angels (2 to 1)", "Gods (Sequence 0)", "Great Old Ones"};
    const int mostTier = *std::max_element(st.tiers.begin(), st.tiers.end());
    const ImVec4 colour = organizationColour(db, view);
    for (int tier = kAboveSequenceTier; tier >= 0; --tier) {
        strengthBar(kTierLabels[static_cast<size_t>(tier)], st.tiers[static_cast<size_t>(tier)], mostTier, colour,
                    "tier " + std::to_string(tier));
    }
    ImGui::Spacing();
    heading(w, "Threat");
    const int mostThreat = *std::max_element(st.threats.begin(), st.threats.end());
    for (size_t level = kThreatLevels.size(); level-- > 0;) {
        strengthBar(kThreatLevels[level], st.threats[level], mostThreat, threatColour(kThreatLevels[level]),
                    "threat level " + std::to_string(level));
    }

    ImGui::Spacing();
    if (!w.organizations.draft.isNew && db.organizations.size() >= 2 && ImGui::Button("Compare with other organizations")) {
        auto& ids = w.compare.organizationIds;
        if (std::find(ids.begin(), ids.end(), o.id) == ids.end()) {
            if (ids.size() >= 4) ids.pop_back();
            ids.insert(ids.begin(), o.id);
        }
        w.compare.organizations = true;
        w.switchTo = Tab::Compare;
    }
}

}  // namespace

bool saveOrganizationDraft(WindowState& w) { return saveImpl(w); }

void openOrganization(WindowState& w, int id) {
    w.switchTo = Tab::Organizations;
    auto& draft = w.organizations.draft;
    if (draft.open && !draft.isNew && draft.value.id == id) return;
    whenSaved(w, draft.dirty(), [&w] { return saveImpl(w); }, [&w, id] {
        if (const Organization* found = w.app.db.findOrganization(id)) open(w, *found, false);
    });
}

void openNewOrganization(WindowState& w, const std::string& name) {
    w.switchTo = Tab::Organizations;
    whenSaved(w, w.organizations.draft.dirty(), [&w] { return saveImpl(w); }, [&w, name] { openNew(w, name); });
}

void drawOrganizationsScreen(WindowState& w) {
    auto& s = w.organizations;
    auto& draft = s.draft;
    const Database& db = w.app.db;
    // One deleted elsewhere (Import all never deletes, but be safe) closes.
    if (draft.open && !draft.isNew && !db.findOrganization(draft.value.id)) draft.close();

    listPanel(w);
    ImGui::SameLine();

    ImGui::BeginChild("editor", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!draft.open) {
        ImGui::Spacing();
        ImGui::TextDisabled("Pick an organization on the left, or create a new one.");
        const size_t unsaved = organizationsNotSaved(db).size();
        if (unsaved > 0) {
            ImGui::TextDisabled("Your characters already name %s without a page yet: they are under "
                                "\"Named by your characters\".",
                                countOf(unsaved, "organization", "organizations").c_str());
        }
        ImGui::EndChild();
        return;
    }
    Organization& o = draft.value;
    const Organization* saved = draft.isNew ? nullptr : db.findOrganization(o.id);
    const Organization view = membershipView(o, saved);
    const std::vector<Member> members = membersOf(view, db);
    const OrganizationStrength strength = organizationStrength(view, db);
    const Sheet sheet = buildOrganizationSheet(o, db);
    const ImVec4 colour = organizationColour(db, o);
    const float font = ImGui::GetFontSize();

    // ---- header
    medallion(w, o.name.empty() ? "?" : initials(o.name), colour, font * 2.1f);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextUnformatted(o.name.empty() ? "(unnamed)" : o.name.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", draft.isNew ? "new" : organizationCode(o.id).c_str());
    ImGui::TextColored(palette().muted, "%s", sheet.subtitle.c_str());
    std::vector<std::pair<std::string, ImVec4>> chips;
    if (!o.status.empty()) chips.push_back({o.status, palette().gold});
    chips.push_back({countOf(static_cast<size_t>(strength.members + strength.others), "member", "members"), colour});
    if (strength.dead > 0) chips.push_back({countOf(static_cast<size_t>(strength.dead), "member died", "members died"), deathColour()});
    if (!strength.threatLevel.empty()) chips.push_back({"Threat: " + strength.threatLevel, threatColour(strength.threatLevel)});
    if (const std::string leader = leaderName(o, db); !leader.empty()) chips.push_back({"Led by " + leader, palette().muted});
    chipRow(chips);
    ImGui::EndGroup();
    ImGui::Spacing();

    EditorButtons buttons;
    buttons.dirty = draft.dirty();
    buttons.isNew = draft.isNew;
    switch (drawEditorButtons(buttons)) {
        case EditorAction::Save: saveImpl(w); break;
        case EditorAction::Revert:
            if (draft.isNew) draft.close();
            else if (saved) open(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, exportName(o), ExportFormat::Html, {sheet}, o.name); break;
        case EditorAction::ExportMarkdown: exportAndReport(w, exportName(o), ExportFormat::Markdown, {sheet}, o.name); break;
        case EditorAction::ExportText: exportAndReport(w, exportName(o), ExportFormat::Text, {sheet}, o.name); break;
        case EditorAction::Delete:
            try {
                const std::string name = o.name;
                deleteOrganization(w.app, o.id);
                draft.close();
                setStatus(w, "Deleted " + name + ". Its members keep it as their affiliation, as typed text.");
            } catch (const StorageError& e) {
                setStatus(w, std::string("Deleting failed: ") + e.what(), true);
            }
            break;
        default: break;
    }
    if (!draft.open) {
        ImGui::EndChild();
        return;
    }

    ImGui::Spacing();
    if (ImGui::BeginTabBar("organizationTabs")) {
        if (ImGui::BeginTabItem("Members")) {
            ImGui::BeginChild("tab");
            membersTab(w, o, view, members);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Details", nullptr, draft.firstTabFlags())) {
            ImGui::BeginChild("tab");
            detailsTab(w, o, members);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Strength")) {
            ImGui::BeginChild("tab");
            strengthTab(w, o, view);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sheet")) {
            ImGui::BeginChild("tab");
            drawSheet(w, sheet);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

}  // namespace lotm::window
