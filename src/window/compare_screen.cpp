// Compare tab: tick two to four characters on the left to see them side by side, with their
// six stats drawn on one chart; or two to four organizations, with how strong their members make them.
// It shows what is saved, not unsaved changes.
#include <algorithm>
#include <cmath>

#include "compare.hpp"
#include "organizations.hpp"
#include "rules.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

constexpr size_t kMaxCompared = 4;
constexpr float kOrganizationLabels = 12.0f;  // room for "Great Old Ones and gods"

ImU32 colour(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

// The highest score shown on a stat chart or bar: 20, or more when someone goes past it.
int scaleFor(int highest) { return std::max(20, (highest + 4) / 5 * 5); }

// A thin bar under a number, filled in the character's colour. It grows to its length when first shown,
// and glides when the number changes.
void bar(const std::string& key, float fraction, const ImVec4& tint) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = std::max(3.0f, ImGui::GetFontSize() * 0.22f);
    const float shown = glideFrom(ImGui::GetID(key.c_str()), 0.0f, std::clamp(fraction, 0.0f, 1.0f), 7.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(at, ImVec2(at.x + width, at.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg), height);
    if (shown > 0.002f) draw->AddRectFilled(at, ImVec2(at.x + width * shown, at.y + height), colour(tint), height);
    ImGui::Dummy(ImVec2(width, height));
}

// A table with a column of labels (`labelWidth` letters wide, roughly) and one column per character or
// organization.
bool beginColumns(const char* id, size_t count, float labelWidth = 8.5f) {
    if (!ImGui::BeginTable(id, static_cast<int>(count) + 1,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) {
        return false;
    }
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * labelWidth);
    for (size_t i = 0; i < count; ++i) {
        ImGui::TableSetupColumn(("##c" + std::to_string(i)).c_str(), ImGuiTableColumnFlags_WidthStretch);
    }
    return true;
}

// The counts in an organization comparison drawn as bars, each row on its own scale.
bool countedRow(const CompareRow& row) {
    return row.ranked && row.label != "Strongest" && row.label != "Most dangerous";
}

void sectionTable(const WindowState& w, const CompareSection& section, const char* id, bool organizations = false) {
    if (!beginColumns(id, section.rows.empty() ? 0 : section.rows[0].values.size(), organizations ? kOrganizationLabels : 8.5f)) {
        return;
    }
    // Bars for the six stats share one scale, Speed has its own.
    int highestStat = 0;
    int highestSpeed = 0;
    for (const auto& row : section.rows) {
        for (int n : row.numbers) {
            if (row.label == "Speed") highestSpeed = std::max(highestSpeed, n);
            else if (std::find(kStatNames.begin(), kStatNames.end(), row.label) != kStatNames.end())
                highestStat = std::max(highestStat, n);
        }
    }
    for (const auto& row : section.rows) {
        const bool isStat = std::find(kStatNames.begin(), kStatNames.end(), row.label) != kStatNames.end();
        int scale = isStat ? scaleFor(highestStat) : row.label == "Speed" ? scaleFor(highestSpeed) : 0;
        if (organizations && countedRow(row)) {
            scale = 0;
            for (int n : row.numbers) scale = std::max(scale, n);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(palette().muted, "%s", row.label.c_str());
        for (size_t i = 0; i < row.values.size(); ++i) {
            ImGui::TableNextColumn();
            // The highest value in a row is in bold, in that character's colour.
            const bool top = row.ranked && row.highest[i];
            if (top) {
                ImGui::PushFont(w.fonts.bold, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, palette().series[i]);
            }
            ImGui::TextWrapped("%s", row.values[i].empty() ? "-" : row.values[i].c_str());
            if (top) {
                ImGui::PopStyleColor();
                ImGui::PopFont();
                ImGui::SetItemTooltip(organizations ? "Highest of the organizations compared" : "Highest of the characters compared");
            }
            if (scale > 0) {
                bar(row.label + "##bar" + std::to_string(i), static_cast<float>(std::max(row.numbers[i], 0)) / static_cast<float>(scale),
                    palette().series[i]);
            }
        }
    }
    ImGui::EndTable();
}

// Every ability each character has, grouped as on their sheet; hover a name for what it does.
void abilitiesTable(const WindowState& w, const std::vector<const Character*>& characters) {
    if (!beginColumns("abilities", characters.size())) return;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(palette().muted, "By Sequence");
    for (size_t i = 0; i < characters.size(); ++i) {
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        const Character& c = *characters[i];
        const auto groups = abilityGroups(c, w.app.db);
        if (groups.empty()) ImGui::TextDisabled("No pathway");
        for (const AbilityGroup& group : groups) {
            ImGui::PushStyleColor(ImGuiCol_Text, palette().series[i]);
            ImGui::TextWrapped("%s", group.heading.c_str());
            ImGui::PopStyleColor();
            if (!group.summary.empty()) ImGui::TextWrapped("%s", group.summary.c_str());
            for (const std::string& ability : group.abilities) {
                const auto [name, description] = splitAbility(ability);
                ImGui::Bullet();
                ImGui::TextWrapped("%s", name.empty() ? description.c_str() : name.c_str());
                if (!name.empty() && !description.empty() && ImGui::BeginItemTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                    ImGui::TextUnformatted(description.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

// Each organization's members as a row of small medallions in their pathway colours, the leader ringed
// in gold, appearing one after another. Hover one for who it is; click to open their page.
void memberDots(WindowState& w, const Organization& o, size_t column) {
    const Database& db = w.app.db;
    const std::vector<Member> members = membersOf(o, db);
    const float font = ImGui::GetFontSize();
    const float r = font * 0.42f;
    const float step = r * 2.0f + font * 0.25f;
    const float width = std::max(ImGui::GetContentRegionAvail().x, step);
    const size_t perLine = std::max<size_t>(1, static_cast<size_t>(width / step));
    const size_t shown = std::min<size_t>(members.size(), perLine * 2);
    if (members.empty()) {
        ImGui::TextDisabled(o.otherMembers.empty() ? "No members yet" : "None saved as characters");
        return;
    }
    const size_t lines = (shown + perLine - 1) / perLine;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(("members" + std::to_string(column)).c_str(), ImVec2(width, step * static_cast<float>(lines)));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    int under = -1;
    for (size_t i = 0; i < shown; ++i) {
        const Character& c = *members[i].character;
        const ImVec2 at(origin.x + r + static_cast<float>(i % perLine) * step, origin.y + r + static_cast<float>(i / perLine) * step);
        // Later ones grow more slowly, so they appear one after another.
        const float grow = glideFrom(ImGui::GetID(("dot" + std::to_string(column) + "-" + std::to_string(c.id)).c_str()), 0.0f, 1.0f,
                                     9.0f - std::min(6.0f, static_cast<float>(i) * 0.4f));
        if (grow < 0.05f) continue;
        if (members[i].leader) draw->AddCircle(at, r * grow + font * 0.14f, colourU32(palette().gold, grow), 0, std::max(1.2f, font * 0.08f));
        const bool dead = deadNow(c, db);  // greyed, with a mourning band
        draw->AddCircleFilled(at, r * grow, colourU32(dead ? mourned(pathwayColour(db, c.pathwayId), 1.0f) : pathwayColour(db, c.pathwayId),
                                                      0.9f * grow));
        if (dead) mourningBand(draw, at, r * grow, 1.0f, grow);
        if (hovered) {
            const ImVec2 mouse = ImGui::GetMousePos();
            if (std::abs(mouse.x - at.x) <= step * 0.5f && std::abs(mouse.y - at.y) <= step * 0.5f) under = static_cast<int>(i);
        }
    }
    if (under >= 0) {
        const Member& m = members[static_cast<size_t>(under)];
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::BeginTooltip()) {
            ImGui::TextUnformatted(m.character->name.c_str());
            const std::string rank = memberRole(m);
            if (!rank.empty()) ImGui::TextColored(m.leader ? palette().gold : palette().muted, "%s", rank.c_str());
            ImGui::TextDisabled("%s", levelText(*m.character, db).c_str());
            const std::string death = deathText(deathOf(*m.character, db), db);
            if (!death.empty()) ImGui::TextColored(deathColour(), "%s", death.c_str());
            ImGui::EndTooltip();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) openCharacter(w, m.character->id);
    }
    if (members.size() > shown) ImGui::TextDisabled("and %zu more", members.size() - shown);
}

// Every member of each, with their rank: one column per organization.
void membersTable(WindowState& w, const std::vector<const Organization*>& organizations) {
    if (!beginColumns("members", organizations.size(), kOrganizationLabels)) return;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(palette().muted, "Leader first");
    for (size_t i = 0; i < organizations.size(); ++i) {
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        const std::vector<Member> members = membersOf(*organizations[i], w.app.db);
        if (members.empty()) ImGui::TextDisabled("Nobody saved");
        for (const Member& m : members) {
            ImGui::PushID(m.character->id);
            statusDot(pathwayColour(w.app.db, m.character->pathwayId));
            if (ImGui::TextLink(m.character->name.c_str())) openCharacter(w, m.character->id);
            const std::string rank = memberRole(m);
            if (!rank.empty()) {
                ImGui::Indent(ImGui::GetFontSize() * 1.1f);
                ImGui::TextColored(m.leader ? palette().gold : palette().muted, "%s", rank.c_str());
                ImGui::Unindent(ImGui::GetFontSize() * 1.1f);
            }
            ImGui::PopID();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        for (const auto& other : organizations[i]->otherMembers) {
            if (!ui::trim(other).empty()) ImGui::TextWrapped("%s", ui::trim(other).c_str());
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void drawOrganizationComparison(WindowState& w, const std::vector<int>& ids) {
    std::vector<const Organization*> organizations;
    for (int id : ids) organizations.push_back(w.app.db.findOrganization(id));
    const Comparison comparison = buildOrganizationComparison(organizations, w.app.db);

    // Names along the top, in each one's column colour, with its members below.
    if (beginColumns("names", organizations.size(), kOrganizationLabels)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        for (size_t i = 0; i < organizations.size(); ++i) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            const Organization& o = *organizations[i];
            medallion(w, initials(o.name), palette().series[i], ImGui::GetFontSize() * 1.25f);
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.45f);
            ImGui::PushStyleColor(ImGuiCol_Text, palette().series[i]);
            ImGui::TextWrapped("%s", o.name.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (ImGui::TextLink("Open")) openOrganization(w, o.id);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", organizationCode(o.id).c_str());
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(palette().muted, "Members");
        for (size_t i = 0; i < organizations.size(); ++i) {
            ImGui::TableNextColumn();
            memberDots(w, *organizations[i], i);
        }
        ImGui::EndTable();
    }

    for (const CompareSection& section : comparison.sections) {
        ImGui::Spacing();
        heading(w, section.heading);
        sectionTable(w, section, section.heading.c_str(), true);
    }

    ImGui::Spacing();
    heading(w, "Between them");
    if (comparison.between.empty()) ImGui::TextDisabled("No member of one lists a member of another as a relationship.");
    for (const std::string& line : comparison.between) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", line.c_str());
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Members side by side")) membersTable(w, organizations);
}

}  // namespace

void drawCompareScreen(WindowState& w) {
    auto& s = w.compare;
    const Database& db = w.app.db;
    // Characters and organizations deleted since they were ticked drop out.
    s.ids.erase(std::remove_if(s.ids.begin(), s.ids.end(), [&db](int id) { return !db.findCharacter(id); }), s.ids.end());
    s.organizationIds.erase(std::remove_if(s.organizationIds.begin(), s.organizationIds.end(),
                                           [&db](int id) { return !db.findOrganization(id); }),
                            s.organizationIds.end());
    std::vector<int>& ticks = s.organizations ? s.organizationIds : s.ids;

    // ---- the list
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (const int pick = segmented("what", {"Characters", "Organizations"}, s.organizations ? 1 : 0); pick >= 0) {
        s.organizations = pick == 1;
    }
    ImGui::TextWrapped(s.organizations ? "Tick two to four organizations." : "Tick two to four characters.");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search names", &s.search);
    ImGui::BeginDisabled(ticks.empty());
    if (ImGui::Button("Clear", ImVec2(-FLT_MIN, 0))) ticks.clear();
    ImGui::EndDisabled();
    ImGui::Separator();
    // A box for each, ticked in the colour of its column on the right.
    auto tickBox = [&](const std::string& name, int id) {
        if (!s.search.empty() && ui::toLower(name).find(ui::toLower(s.search)) == std::string::npos) return;
        const auto found = std::find(ticks.begin(), ticks.end(), id);
        const bool wasTicked = found != ticks.end();
        bool ticked = wasTicked;
        ImGui::BeginDisabled(!ticked && ticks.size() >= kMaxCompared);
        if (wasTicked) ImGui::PushStyleColor(ImGuiCol_CheckMark, palette().series[static_cast<size_t>(found - ticks.begin())]);
        if (ImGui::Checkbox((name + "##" + std::to_string(id)).c_str(), &ticked)) {
            if (ticked) ticks.push_back(id);
            else ticks.erase(std::remove(ticks.begin(), ticks.end(), id), ticks.end());
        }
        if (wasTicked) ImGui::PopStyleColor();
        ImGui::EndDisabled();
    };
    if (s.organizations) {
        if (db.organizations.empty()) ImGui::TextDisabled("No organizations yet.");
        for (const Organization* o : organizationsByName(db)) tickBox(o->name, o->id);
    } else {
        if (db.characters.empty()) ImGui::TextDisabled("No characters yet.");
        for (const auto& c : db.characters) tickBox(c.name, c.id);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the comparison
    ImGui::BeginChild("comparison", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (s.organizations) {
        if (s.organizationIds.size() < 2) {
            ImGui::Spacing();
            if (db.organizations.size() < 2) {
                ImGui::TextDisabled("Save at least two organizations to compare them.");
                if (ImGui::TextLink("Go to the Organizations tab")) w.switchTo = Tab::Organizations;
            } else {
                ImGui::TextDisabled("Tick two to four organizations on the left to see them side by side.");
            }
        } else {
            drawOrganizationComparison(w, s.organizationIds);
        }
        ImGui::EndChild();
        return;
    }
    if (s.ids.size() < 2) {
        ImGui::Spacing();
        if (db.characters.size() < 2) {
            ImGui::TextDisabled("Save at least two characters to compare them.");
            if (ImGui::TextLink("Import the sample sets (the Tarot Club and more)")) {
                w.switchTo = Tab::Settings;
                w.showSampleSets = true;
            }
        } else {
            ImGui::TextDisabled("Tick two to four characters on the left to see them side by side.");
        }
        ImGui::EndChild();
        return;
    }
    std::vector<const Character*> characters;
    for (int id : s.ids) characters.push_back(w.app.db.findCharacter(id));
    const Comparison comparison = buildComparison(characters, w.app.db);

    // Names along the top, in each character's colour.
    if (beginColumns("names", characters.size())) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        for (size_t i = 0; i < characters.size(); ++i) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            const Character& c = *characters[i];
            medallion(w, initials(c.name), palette().series[i], ImGui::GetFontSize() * 1.25f);
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.45f);
            ImGui::PushStyleColor(ImGuiCol_Text, palette().series[i]);
            ImGui::TextWrapped("%s", c.name.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (ImGui::TextLink("Open")) openCharacter(w, c.id);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", characterCode(c.id).c_str());
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (const CompareSection& section : comparison.sections) {
        ImGui::Spacing();
        heading(w, section.heading);
        if (section.heading == "Stats") {
            // The chart sits beside the table when there is room, and above it when there isn't.
            const float chartSize = std::min(ImGui::GetFontSize() * 17.0f, ImGui::GetContentRegionAvail().x);
            const float tableWidth = ImGui::GetFontSize() * (10.0f + 7.0f * static_cast<float>(characters.size()));
            const bool beside = ImGui::GetContentRegionAvail().x > chartSize + tableWidth;
            std::vector<std::array<int, kStatCount>> totals;
            for (const Character* c : characters) totals.push_back(statTotals(*c, w.app.db.findPathway(c->pathwayId)));
            ImGui::BeginGroup();
            statChart(totals, chartSize);
            ImGui::EndGroup();
            if (beside) ImGui::SameLine(0.0f, ImGui::GetFontSize());
            ImGui::BeginGroup();
            sectionTable(w, section, "stats");
            ImGui::EndGroup();
        } else {
            sectionTable(w, section, section.heading.c_str());
        }
    }

    ImGui::Spacing();
    heading(w, "Between them");
    if (comparison.between.empty()) ImGui::TextDisabled("None of them lists another as a relationship.");
    for (const std::string& line : comparison.between) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", line.c_str());
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Abilities side by side")) abilitiesTable(w, characters);
    ImGui::EndChild();
}

}  // namespace lotm::window
