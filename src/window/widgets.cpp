// Controls shared by every screen of the app window.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include "rules.hpp"
#include "storage.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

// Width of the label column on the left of every form.
float labelWidth() { return ImGui::GetFontSize() * 9.5f; }

// Draws the label on the left and makes the next control fill the rest of the row.
void label(const char* text) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(labelWidth());
    ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string hidden(const char* text) { return std::string("##") + text; }

ImU32 colourU32(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

// Cuts text to fit a width, ending it with "..." when something had to go.
std::string fitText(const std::string& text, ImFont* font, float size, float width) {
    if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= width) return text;
    std::string cut = text;
    while (!cut.empty()) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) cut.pop_back();
        if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, (cut + "...").c_str()).x <= width) break;
    }
    while (!cut.empty() && cut.back() == ' ') cut.pop_back();
    return cut + "...";
}

void drawMedallion(const WindowState& w, ImVec2 centre, float radius, const ImVec4& colour, const std::string& text) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled(centre, radius, colourU32(colour, 0.16f));
    draw->AddCircle(centre, radius, colourU32(colour), 0, std::max(1.5f, radius * 0.08f));
    if (text.empty()) return;
    ImFont* font = w.fonts.bold;
    const float size = radius * (text.size() > 2 ? 0.72f : 0.86f);
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
    draw->AddText(font, size, ImVec2(centre.x - textSize.x * 0.5f, centre.y - textSize.y * 0.5f), colourU32(colour),
                  text.c_str());
}

// ImGui reads "%" in a slider's text as a number placeholder, so it has to be doubled.
std::string escapePercent(const std::string& text) {
    std::string out;
    for (char c : text) {
        out += c;
        if (c == '%') out += '%';
    }
    return out;
}

}  // namespace

void setStatus(WindowState& w, const std::string& text, bool error) {
    w.status = text;
    w.statusIsError = error;
}

void fieldLabel(const char* text) { label(text); }

void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool textField(const char* text, std::string& value, const char* hint) {
    label(text);
    return ImGui::InputTextWithHint(hidden(text).c_str(), hint ? hint : "", &value);
}

bool multilineField(const char* text, std::string& value, float lines) {
    ImGui::TextUnformatted(text);
    const float height = ImGui::GetTextLineHeight() * lines + ImGui::GetStyle().FramePadding.y * 2.0f;
    return ImGui::InputTextMultiline(hidden(text).c_str(), &value, ImVec2(-FLT_MIN, height),
                                     ImGuiInputTextFlags_WordWrap);
}

bool presetField(const char* text, std::string& value, const std::vector<std::string>& presets) {
    label(text);
    const float button = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - ImGui::GetStyle().ItemInnerSpacing.x);
    bool changed = ImGui::InputTextWithHint(hidden(text).c_str(), "type or pick", &value);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    const std::string comboId = std::string("##pick") + text;
    if (ImGui::BeginCombo(comboId.c_str(), nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft |
                                                        ImGuiComboFlags_HeightLarge)) {
        for (const auto& preset : presets) {
            if (ImGui::Selectable(preset.c_str(), preset == value)) {
                value = preset;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool listEditor(const char* id, std::vector<std::string>& items, const char* addHint) {
    static std::map<std::string, std::string> newEntries;  // the half-typed "add" text of each list
    bool changed = false;
    ImGui::PushID(id);
    const float button = ImGui::GetFrameHeight();
    for (size_t i = 0; i < items.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - ImGui::GetStyle().ItemInnerSpacing.x);
        changed |= ImGui::InputText("##item", &items[i]);
        const bool emptied = ImGui::IsItemDeactivatedAfterEdit() && items[i].empty();
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        const bool remove = ImGui::Button("x", ImVec2(button, button));
        ImGui::SetItemTooltip("Remove");
        ImGui::PopID();
        if (remove || emptied) {
            items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
            break;
        }
    }
    std::string& entry = newEntries[id];
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - ImGui::GetStyle().ItemInnerSpacing.x);
    const bool entered = ImGui::InputTextWithHint("##new", addHint, &entry, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if ((ImGui::Button("+", ImVec2(button, button)) || entered) && !entry.empty()) {
        items.push_back(entry);
        entry.clear();
        changed = true;
    }
    ImGui::SetItemTooltip("Add");
    ImGui::PopID();
    return changed;
}

bool pathwayCombo(const char* text, const Database& db, std::string& pathwayId, const char* noneLabel) {
    label(text);
    const std::string preview = pathwayId.empty() ? noneLabel : pathwayLabel(db, pathwayId);
    bool changed = false;
    if (ImGui::BeginCombo(hidden(text).c_str(), preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable(noneLabel, pathwayId.empty())) {
            pathwayId.clear();
            changed = true;
        }
        std::string group = "\x01";
        for (const auto& p : db.pathways) {
            const std::string thisGroup = p.group.empty() ? "Your own pathways" : p.group;
            if (thisGroup != group) {
                group = thisGroup;
                ImGui::SeparatorText(group.c_str());
            }
            const std::string name = pathwayLabel(db, p.id) + "##" + p.id;
            if (ImGui::Selectable(name.c_str(), p.id == pathwayId)) {
                pathwayId = p.id;
                changed = true;
            }
            if (p.id == pathwayId) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool sequenceSlider(const char* id, const Pathway* pathway, int& sequence) {
    int climbed = 9 - sequence;  // the slider runs left to right, from Sequence 9 up to Sequence 0
    const std::string shown = escapePercent(sequenceLabel(pathway, sequence));
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::SliderInt(id, &climbed, 0, 9, shown.c_str(), ImGuiSliderFlags_NoInput);
    if (changed) sequence = 9 - climbed;
    ImGui::TextDisabled("Sequence 9: weakest");
    const char* right = "Sequence 0: a god";
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(right).x);
    ImGui::TextDisabled("%s", right);
    return changed;
}

void heading(const WindowState& w, const std::string& text) {
    ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 1.1f);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().gold);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    // A small diamond, then a rule that fades out towards the right edge.
    const ImVec2 textMin = ImGui::GetItemRectMin();
    const ImVec2 textMax = ImGui::GetItemRectMax();
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float y = std::floor((textMin.y + textMax.y) * 0.5f) + 0.5f;
    const float d = ImGui::GetFontSize() * 0.22f;
    const float start = textMax.x + ImGui::GetFontSize() * 0.6f;
    if (start + d * 4.0f < right) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 gold = colourU32(palette().gold, 0.85f);
        draw->AddQuadFilled(ImVec2(start, y), ImVec2(start + d, y - d), ImVec2(start + d * 2.0f, y), ImVec2(start + d, y + d),
                            gold);
        draw->AddRectFilledMultiColor(ImVec2(start + d * 2.6f, y - 0.5f), ImVec2(right, y + 0.5f), gold,
                                      colourU32(palette().gold, 0.0f), colourU32(palette().gold, 0.0f), gold);
    }
    ImGui::Spacing();
}

std::string initials(const std::string& name) {
    // The first letter of each word (stopping at a bracket), then the first and last of those.
    std::vector<std::string> letters;
    bool wordStart = true;
    for (size_t i = 0; i < name.size();) {
        size_t end = i + 1;
        while (end < name.size() && (static_cast<unsigned char>(name[end]) & 0xC0) == 0x80) ++end;  // one UTF-8 letter
        const unsigned char ch = static_cast<unsigned char>(name[i]);
        if (ch == '(') break;
        if (ch == ' ' || ch == '-') {
            wordStart = true;
        } else {
            if (wordStart && (std::isalpha(ch) || ch >= 0x80)) {
                std::string letter = name.substr(i, end - i);
                if (letter.size() == 1) letter[0] = static_cast<char>(std::toupper(ch));
                letters.push_back(letter);
            }
            wordStart = false;
        }
        i = end;
    }
    if (letters.empty()) return "?";
    return letters.size() == 1 ? letters[0] : letters.front() + letters.back();
}

void medallion(const WindowState& w, const std::string& text, const ImVec4& colour, float radius) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    drawMedallion(w, ImVec2(at.x + radius, at.y + radius), radius, colour, text);
    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

void chip(const std::string& text, const ImVec4& colour) {
    const ImVec2 padding(ImGui::GetFontSize() * 0.55f, ImGui::GetFontSize() * 0.18f);
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 end(at.x + textSize.x + padding.x * 2.0f, at.y + textSize.y + padding.y * 2.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float rounding = (end.y - at.y) * 0.5f;
    draw->AddRectFilled(at, end, colourU32(colour, 0.14f), rounding);
    draw->AddRect(at, end, colourU32(colour, 0.55f), rounding);
    draw->AddText(ImVec2(at.x + padding.x, at.y + padding.y), colourU32(colour), text.c_str());
    ImGui::Dummy(ImVec2(end.x - at.x, end.y - at.y));
}

void statusDot(const ImVec4& colour) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float size = ImGui::GetFontSize();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + size * 0.3f, at.y + size * 0.55f), size * 0.22f,
                                                colourU32(colour));
    ImGui::Dummy(ImVec2(size * 0.6f, size));
    ImGui::SameLine();
}

bool listCard(const WindowState& w, const std::string& id, bool selected, const ImVec4& colour,
              const std::string& badge, const std::string& title, const std::string& subtitle) {
    const float font = ImGui::GetFontSize();
    const float pad = font * 0.4f;
    const float height = ImGui::GetTextLineHeight() * 2.0f + pad * 2.0f;
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    const bool clicked = ImGui::Selectable(("##" + id).c_str(), selected, ImGuiSelectableFlags_None, ImVec2(width, height));
    ImGui::PopStyleVar();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (selected) {
        draw->AddRectFilled(at, ImVec2(at.x + font * 0.18f, at.y + height), colourU32(palette().gold), font * 0.1f);
    }
    const float radius = height * 0.34f;
    drawMedallion(w, ImVec2(at.x + pad + radius + font * 0.2f, at.y + height * 0.5f), radius, colour, badge);
    const float textX = at.x + pad * 2.0f + radius * 2.0f + font * 0.3f;
    const float room = at.x + width - textX - pad;
    ImFont* titleFont = selected ? w.fonts.bold : ImGui::GetFont();
    draw->AddText(titleFont, font, ImVec2(textX, at.y + pad), ImGui::GetColorU32(ImGuiCol_Text),
                  fitText(title, titleFont, font, room).c_str());
    draw->AddText(ImGui::GetFont(), font * 0.9f, ImVec2(textX, at.y + pad + ImGui::GetTextLineHeight()),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), fitText(subtitle, ImGui::GetFont(), font * 0.9f, room).c_str());
    return clicked;
}

void statBoxes(const WindowState& w, const std::vector<StatRow>& stats) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float font = ImGui::GetFontSize();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int perRow = avail > font * 33.0f ? 6 : 3;  // two rows of three in a narrow window
    const float boxWidth = (avail - style.ItemSpacing.x * static_cast<float>(perRow - 1)) / static_cast<float>(perRow);
    const float boxHeight = font * 5.4f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < stats.size(); ++i) {
        const StatRow& row = stats[i];
        if (i % static_cast<size_t>(perRow) != 0) ImGui::SameLine();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const ImVec2 end(at.x + boxWidth, at.y + boxHeight);
        draw->AddRectFilled(at, end, ImGui::GetColorU32(ImGuiCol_FrameBg), style.ChildRounding);
        draw->AddRect(at, end, ImGui::GetColorU32(ImGuiCol_Border), style.ChildRounding);
        auto centred = [&](ImFont* f, float size, float y, ImU32 colour, const std::string& text) {
            const ImVec2 textSize = f->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
            draw->AddText(f, size, ImVec2(at.x + (boxWidth - textSize.x) * 0.5f, y), colour, text.c_str());
        };
        const bool nameFits =
            w.fonts.title->CalcTextSizeA(font * 0.85f, FLT_MAX, 0.0f, row.name.c_str()).x < boxWidth - font * 0.6f;
        centred(w.fonts.title, font * 0.85f, at.y + font * 0.35f, colourU32(palette().muted), nameFits ? row.name : row.code);
        centred(w.fonts.title, font * 1.75f, at.y + font * 1.3f, ImGui::GetColorU32(ImGuiCol_Text),
                std::to_string(row.total));
        centred(w.fonts.bold, font, at.y + font * 3.35f, colourU32(palette().gold), signedNumber(row.modifier));
        const std::string detail = row.bonus == 0 ? "base " + std::to_string(row.base)
                                                  : std::to_string(row.base) + " + " + std::to_string(row.bonus) + " pathway";
        centred(ImGui::GetFont(), font * 0.78f, at.y + font * 4.45f, ImGui::GetColorU32(ImGuiCol_TextDisabled), detail);
        ImGui::Dummy(ImVec2(boxWidth, boxHeight));
    }
}

void statChart(const std::vector<std::array<int, kStatCount>>& totals, float size) {
    int highest = 0;
    for (const auto& set : totals) highest = std::max(highest, *std::max_element(set.begin(), set.end()));
    // Rings every 5 points up to 20 or the highest score, every 10 past 30.
    const int roughScale = std::max(20, (highest + 4) / 5 * 5);
    const int step = roughScale > 30 ? 10 : 5;
    const int scale = (roughScale + step - 1) / step * step;

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
        draw->AddText(ImVec2(at.x - textSize.x * 0.5f, at.y - textSize.y * 0.5f), colourU32(palette().muted), code);
    }
    for (size_t i = 0; i < totals.size() && i < palette().series.size(); ++i) {
        const ImVec4& tint = palette().series[i];
        ImVec2 corners[kStatCount];
        for (int axis = 0; axis < kStatCount; ++axis) {
            const float fraction = static_cast<float>(totals[i][static_cast<size_t>(axis)]) / static_cast<float>(scale);
            corners[axis] = point(axis, std::clamp(fraction, 0.0f, 1.0f));
        }
        draw->AddConcavePolyFilled(corners, kStatCount, colourU32(tint, 0.14f));
        draw->AddPolyline(corners, kStatCount, colourU32(tint), ImDrawFlags_Closed, 2.0f);
        for (const ImVec2& corner : corners) draw->AddCircleFilled(corner, 3.0f, colourU32(tint));
    }
    ImGui::Dummy(ImVec2(size, size));
    ImGui::TextDisabled("Totals with pathway bonuses. A ring every %d points, up to %d.", step, scale);
}

// A sheet drawn with the window's own controls: the same content as the exports.
void drawSheet(const WindowState& w, const Sheet& sheet) {
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.7f);
    ImGui::TextWrapped("%s", sheet.title.c_str());
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s", sheet.subtitle.c_str());
    ImGui::PopStyleColor();

    for (const auto& section : sheet.sections) {
        ImGui::Spacing();
        ImGui::Spacing();
        heading(w, section.style == "dossier" ? section.heading + "  (confidential)" : section.heading);
        float valueColumn = labelWidth();  // wide enough for the longest label in this section
        for (const auto& block : section.blocks) {
            if (block.kind != BlockKind::Field) continue;
            valueColumn = std::max(valueColumn, ImGui::CalcTextSize(block.label.c_str()).x + ImGui::GetFontSize());
        }
        for (const auto& block : section.blocks) {
            switch (block.kind) {
                case BlockKind::Field:
                    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
                    ImGui::TextUnformatted(block.label.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine(valueColumn);
                    ImGui::TextWrapped("%s", block.text.c_str());
                    break;
                case BlockKind::Paragraph:
                    ImGui::TextWrapped("%s", block.text.c_str());
                    break;
                case BlockKind::Quote:
                    ImGui::Indent();
                    ImGui::PushStyleColor(ImGuiCol_Text, palette().gold);
                    ImGui::TextWrapped("\"%s\"", block.text.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                    break;
                case BlockKind::Subheading:
                    ImGui::Spacing();
                    ImGui::PushFont(w.fonts.bold, 0.0f);
                    ImGui::TextWrapped("%s", block.text.c_str());
                    ImGui::PopFont();
                    break;
                case BlockKind::Bullet: {
                    // "Name: what it does" puts the name in bold on its own line.
                    const auto [name, rest] = splitAbility(block.text);
                    ImGui::Bullet();
                    if (!name.empty()) {
                        ImGui::PushFont(w.fonts.bold, 0.0f);
                        ImGui::TextWrapped("%s", name.c_str());
                        ImGui::PopFont();
                        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                        ImGui::TextWrapped("%s", rest.c_str());
                        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    } else {
                        ImGui::TextWrapped("%s", block.text.c_str());
                    }
                    break;
                }
                case BlockKind::Stats:
                    statBoxes(w, block.stats);
                    break;
            }
        }
    }
    if (!sheet.footer.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", sheet.footer.c_str());
    }
}

EditorAction drawEditorButtons(const EditorButtons& options) {
    EditorAction action = EditorAction::None;
    if (options.dirty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.43f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.66f, 0.52f, 0.22f, 1.0f));
    }
    if (ImGui::Button(options.isNew ? "Save new" : "Save")) action = EditorAction::Save;
    if (options.dirty) ImGui::PopStyleColor(2);
    ImGui::SetItemTooltip("Ctrl+S");
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) action = EditorAction::Save;

    ImGui::SameLine();
    ImGui::BeginDisabled(!options.dirty);
    if (ImGui::Button(options.isNew ? "Discard" : "Undo changes")) action = EditorAction::Revert;
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Export...")) ImGui::OpenPopup("export");
    if (ImGui::BeginPopup("export")) {
        if (ImGui::Selectable("HTML (opens in your browser, prints to PDF)")) action = EditorAction::ExportHtml;
        if (ImGui::Selectable("Markdown (Discord, Obsidian, wikis)")) action = EditorAction::ExportMarkdown;
        if (ImGui::Selectable("Plain text")) action = EditorAction::ExportText;
        ImGui::EndPopup();
    }

    if (options.canDuplicate) {
        ImGui::SameLine();
        ImGui::BeginDisabled(options.isNew);
        if (ImGui::Button("Duplicate")) action = EditorAction::Duplicate;
        ImGui::EndDisabled();
    }

    if (!options.isNew && options.canDelete) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, palette().danger);
        if (ImGui::Button("Delete")) ImGui::OpenPopup("###delete");
        ImGui::PopStyleColor();
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        const bool blocked = !options.deleteBlockedReason.empty();
        if (ImGui::BeginPopupModal(blocked ? "Can't delete it yet###delete" : "Delete?###delete", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape);
            if (blocked) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                ImGui::TextUnformatted(options.deleteBlockedReason.c_str());
                ImGui::PopTextWrapPos();
                if (ImGui::Button("OK") || escape) ImGui::CloseCurrentPopup();
            } else {
                ImGui::TextUnformatted("Delete it for good?");
                if (ImGui::Button("Delete")) {
                    action = EditorAction::Delete;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep it") || escape) ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (options.dirty) {
        ImGui::SameLine();
        ImGui::TextColored(palette().warning, options.isNew ? "Not saved yet" : "Unsaved changes");
    }
    return action;
}

void exportAndReport(WindowState& w, const std::string& baseName, ExportFormat format,
                     const std::vector<Sheet>& sheets, const std::string& title) {
    try {
        const auto file = writeExport(w.app, baseName, format, sheets, title);
        std::string text = "Exported to " + file.string();
        if (format == ExportFormat::Html && !openWithDefaultApp(file)) text += " (open it yourself)";
        setStatus(w, text);
    } catch (const StorageError& e) {
        setStatus(w, e.what(), true);
    }
}

// ---------------------------------------------------------------- unsaved changes

void whenSaved(WindowState& w, bool dirty, std::function<bool()> save, std::function<void()> action) {
    if (!dirty) {
        action();
        return;
    }
    w.askUnsaved = true;
    w.saveCurrent = std::move(save);
    w.afterwards = std::move(action);
}

void drawUnsavedQuestion(WindowState& w) {
    if (w.askUnsaved) {
        ImGui::OpenPopup("Unsaved changes");
        w.askUnsaved = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("You have changes that aren't saved yet.");
        ImGui::Spacing();
        bool done = false;
        if (ImGui::Button("Save them")) {
            if (!w.saveCurrent || w.saveCurrent()) {
                done = true;
            } else {  // saving failed: stay where we are, the status line says why
                w.afterwards = nullptr;
                w.quitRequested = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Throw them away")) done = true;
        ImGui::SameLine();
        if (ImGui::Button("Keep editing") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            w.afterwards = nullptr;
            w.quitRequested = false;
            ImGui::CloseCurrentPopup();
        }
        if (done) {
            auto action = std::move(w.afterwards);
            w.afterwards = nullptr;
            ImGui::CloseCurrentPopup();
            if (action) action();
        }
        ImGui::EndPopup();
    }
}

bool hasUnsavedChanges(const WindowState& w) {
    return w.characters.draft.dirty() || w.artifacts.draft.dirty() || w.pathways.draft.dirty();
}

}  // namespace lotm::window
