// Compare tab: tick two to four characters on the left to see them side by side, with their
// six stats drawn on one chart. It shows the saved characters, not unsaved changes.
#include <algorithm>
#include <cmath>

#include "compare.hpp"
#include "rules.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

constexpr size_t kMaxCompared = 4;

ImU32 colour(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

// The highest score shown on a stat chart or bar: 20, or more when someone goes past it.
int scaleFor(int highest) { return std::max(20, (highest + 4) / 5 * 5); }

// Chart rings every 5 points, or every 10 once the scale goes past 30.
int ringStep(int scale) { return scale > 30 ? 10 : 5; }

// Six spokes, one per stat, with one outline per character.
void statChart(const WindowState& w, const std::vector<const Character*>& characters, float size) {
    std::vector<std::array<int, kStatCount>> totals;
    int highest = 0;
    for (const Character* c : characters) {
        totals.push_back(statTotals(*c, w.app.db.findPathway(c->pathwayId)));
        highest = std::max(highest, *std::max_element(totals.back().begin(), totals.back().end()));
    }
    const int step = ringStep(scaleFor(highest));
    const int scale = (scaleFor(highest) + step - 1) / step * step;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 centre(origin.x + size * 0.5f, origin.y + size * 0.5f);
    const float labelRoom = ImGui::GetFontSize() * 1.6f;
    const float radius = size * 0.5f - labelRoom;
    auto point = [&](int axis, float fraction) {
        const float angle = -1.5707963f + static_cast<float>(axis) * 1.0471976f;  // from the top, clockwise
        return ImVec2(centre.x + std::cos(angle) * radius * fraction, centre.y + std::sin(angle) * radius * fraction);
    };

    const ImU32 grid = ImGui::GetColorU32(ImGuiCol_Border);
    for (int ring = step; ring <= scale; ring += step) {
        ImVec2 corners[kStatCount];
        for (int axis = 0; axis < kStatCount; ++axis) {
            corners[axis] = point(axis, static_cast<float>(ring) / static_cast<float>(scale));
        }
        draw->AddPolyline(corners, kStatCount, grid, ImDrawFlags_Closed, 1.0f);
    }
    for (int axis = 0; axis < kStatCount; ++axis) {
        draw->AddLine(centre, point(axis, 1.0f), grid);
        const char* code = kStatCodes[static_cast<size_t>(axis)];
        const ImVec2 textSize = ImGui::CalcTextSize(code);
        const ImVec2 at = point(axis, 1.0f + labelRoom * 0.55f / radius);
        draw->AddText(ImVec2(at.x - textSize.x * 0.5f, at.y - textSize.y * 0.5f), colour(kMuted), code);
    }
    for (size_t i = 0; i < totals.size(); ++i) {
        ImVec2 corners[kStatCount];
        for (int axis = 0; axis < kStatCount; ++axis) {
            const float fraction = static_cast<float>(totals[i][static_cast<size_t>(axis)]) / static_cast<float>(scale);
            corners[axis] = point(axis, std::clamp(fraction, 0.0f, 1.0f));
        }
        draw->AddConcavePolyFilled(corners, kStatCount, colour(kSeries[i], 0.14f));
        draw->AddPolyline(corners, kStatCount, colour(kSeries[i]), ImDrawFlags_Closed, 2.0f);
        for (const ImVec2& corner : corners) draw->AddCircleFilled(corner, 3.0f, colour(kSeries[i]));
    }
    ImGui::Dummy(ImVec2(size, size));
    ImGui::TextDisabled("Totals with pathway bonuses. A ring every %d points, up to %d.", step, scale);
}

// A thin bar under a number, filled in the character's colour.
void bar(float fraction, const ImVec4& tint) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = std::max(3.0f, ImGui::GetFontSize() * 0.22f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(at, ImVec2(at.x + width, at.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg), height);
    draw->AddRectFilled(at, ImVec2(at.x + width * std::clamp(fraction, 0.0f, 1.0f), at.y + height), colour(tint),
                        height);
    ImGui::Dummy(ImVec2(width, height));
}

bool beginColumns(const char* id, size_t count) {
    if (!ImGui::BeginTable(id, static_cast<int>(count) + 1,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) {
        return false;
    }
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.5f);
    for (size_t i = 0; i < count; ++i) {
        ImGui::TableSetupColumn(("##c" + std::to_string(i)).c_str(), ImGuiTableColumnFlags_WidthStretch);
    }
    return true;
}

void sectionTable(const WindowState& w, const CompareSection& section, const char* id) {
    if (!beginColumns(id, section.rows.empty() ? 0 : section.rows[0].values.size())) return;
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
        const int scale = isStat ? scaleFor(highestStat) : row.label == "Speed" ? scaleFor(highestSpeed) : 0;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kMuted, "%s", row.label.c_str());
        for (size_t i = 0; i < row.values.size(); ++i) {
            ImGui::TableNextColumn();
            // The highest value in a row is in bold, in that character's colour.
            const bool top = row.ranked && row.highest[i];
            if (top) {
                ImGui::PushFont(w.fonts.bold, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, kSeries[i]);
            }
            ImGui::TextWrapped("%s", row.values[i].empty() ? "-" : row.values[i].c_str());
            if (top) {
                ImGui::PopStyleColor();
                ImGui::PopFont();
                ImGui::SetItemTooltip("Highest of the characters compared");
            }
            if (scale > 0) bar(static_cast<float>(row.numbers[i]) / static_cast<float>(scale), kSeries[i]);
        }
    }
    ImGui::EndTable();
}

// Every ability each character has, by Sequence; hover a name for what it does.
void abilitiesTable(const WindowState& w, const std::vector<const Character*>& characters) {
    if (!beginColumns("abilities", characters.size())) return;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(kMuted, "By Sequence");
    for (size_t i = 0; i < characters.size(); ++i) {
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        const Character& c = *characters[i];
        const Pathway* pathway = w.app.db.findPathway(c.pathwayId);
        if (!pathway) ImGui::TextDisabled("No pathway");
        for (const SequenceInfo* seq : pathway ? abilitiesUpTo(*pathway, c.sequence) : std::vector<const SequenceInfo*>{}) {
            ImGui::TextColored(kSeries[i], "Sequence %d: %s", seq->sequence, seq->name.c_str());
            for (const std::string& ability : seq->abilities) {
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
        if (!c.uniquenessAbilities.empty()) {
            ImGui::TextColored(kSeries[i], "From the Uniqueness");
            for (const std::string& ability : c.uniquenessAbilities) {
                ImGui::Bullet();
                ImGui::TextWrapped("%s", splitAbility(ability).first.empty() ? ability.c_str()
                                                                              : splitAbility(ability).first.c_str());
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

}  // namespace

void drawCompareScreen(WindowState& w) {
    auto& s = w.compare;
    // Characters deleted since they were ticked drop out.
    s.ids.erase(std::remove_if(s.ids.begin(), s.ids.end(), [&w](int id) { return !w.app.db.findCharacter(id); }),
                s.ids.end());

    // ---- the list
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    ImGui::TextWrapped("Tick two to four characters.");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search names", &s.search);
    ImGui::BeginDisabled(s.ids.empty());
    if (ImGui::Button("Clear", ImVec2(-FLT_MIN, 0))) s.ids.clear();
    ImGui::EndDisabled();
    ImGui::Separator();
    if (w.app.db.characters.empty()) ImGui::TextDisabled("No characters yet.");
    for (const auto& c : w.app.db.characters) {
        if (!s.search.empty() && ui::toLower(c.name).find(ui::toLower(s.search)) == std::string::npos) continue;
        const auto found = std::find(s.ids.begin(), s.ids.end(), c.id);
        const bool wasTicked = found != s.ids.end();
        bool ticked = wasTicked;
        ImGui::BeginDisabled(!ticked && s.ids.size() >= kMaxCompared);
        if (wasTicked) ImGui::PushStyleColor(ImGuiCol_CheckMark, kSeries[static_cast<size_t>(found - s.ids.begin())]);
        if (ImGui::Checkbox((c.name + "##" + std::to_string(c.id)).c_str(), &ticked)) {
            if (ticked) s.ids.push_back(c.id);
            else s.ids.erase(std::remove(s.ids.begin(), s.ids.end(), c.id), s.ids.end());
        }
        if (wasTicked) ImGui::PopStyleColor();
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the comparison
    ImGui::BeginChild("comparison", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (s.ids.size() < 2) {
        ImGui::Spacing();
        if (w.app.db.characters.size() < 2) {
            ImGui::TextDisabled("Save at least two characters to compare them.");
            if (ImGui::TextLink("Add the Tarot Club or other samples")) {
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
            ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.35f);
            ImGui::PushStyleColor(ImGuiCol_Text, kSeries[i]);
            ImGui::TextWrapped("%s", characters[i]->name.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            const int id = characters[i]->id;
            if (ImGui::TextLink("Open")) openCharacter(w, id);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", characterCode(id).c_str());
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
            ImGui::BeginGroup();
            statChart(w, characters, chartSize);
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
