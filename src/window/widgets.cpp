// Controls shared by every screen of the app window.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include "rules.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
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

// Between the warning amber and the danger red: Half mad, and a High threat.
ImVec4 orange() {
    const ImVec4 a = palette().warning, b = palette().danger;
    return ImVec4(a.x + (b.x - a.x) * 0.45f, a.y + (b.y - a.y) * 0.45f, a.z + (b.z - a.z) * 0.45f, 1.0f);
}


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

// `ring`, when given, draws the ring in those colours in equal parts, clockwise from the top.
void drawMedallion(const WindowState& w, ImVec2 centre, float radius, const ImVec4& colour, const std::string& text,
                   const std::vector<ImVec4>& ring = {}) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled(centre, radius, colourU32(colour, 0.16f));
    const float thickness = std::max(1.5f, radius * 0.08f);
    if (ring.size() > 1) {
        const float part = 6.2831853f / static_cast<float>(ring.size());
        for (size_t i = 0; i < ring.size(); ++i) {
            const float start = -1.5707963f + part * static_cast<float>(i);
            draw->PathArcTo(centre, radius, start, start + part, 12);
            draw->PathStroke(colourU32(ring[i]), ImDrawFlags_None, thickness * 1.5f);
        }
    } else {
        draw->AddCircle(centre, radius, colourU32(colour), 0, thickness);
    }
    const float size = radius * (text.size() > 2 ? 0.72f : 0.86f);
    // ImGui rounds a text size to whole pixels and stops on a size of 0 (in a debug build), which a
    // medallion still popping in would ask for.
    if (text.empty() || size < 1.0f) return;
    ImFont* font = w.fonts.bold;
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
    if (text != w.status || error != w.statusIsError) w.statusAt = ImGui::GetTime();
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

bool storyDateField(const char* text, StoryDate& date, const char* hint) {
    // What is typed in each date box, kept between frames: half-typed text doesn't read as a date yet.
    struct Typed {
        std::string text;
        bool active = false;
    };
    static std::map<ImGuiID, Typed> boxes;
    label(text);
    Typed& box = boxes[ImGui::GetID(text)];
    if (!box.active) {  // not being typed in: show the date as it is (another record may be open now)
        const auto reads = parseStoryDate(box.text);
        if (!reads || !sameStoryDate(*reads, date)) box.text = storyDateText(date);
    }
    bool changed = false;
    if (ImGui::InputTextWithHint(hidden(text).c_str(), hint, &box.text)) {
        const auto reads = parseStoryDate(box.text);
        if (reads && !sameStoryDate(*reads, date)) {
            date = *reads;
            changed = true;
        }
    }
    box.active = ImGui::IsItemActive();
    std::string problem;
    const auto reads = parseStoryDate(box.text, &problem);
    if (!reads) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelWidth());
        ImGui::TextColored(palette().danger, "%s", problem.c_str());
    } else if (box.active && !reads->empty() && ui::trim(box.text) != storyDateText(*reads)) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelWidth());
        ImGui::TextDisabled("Reads as %s", storyDateText(*reads).c_str());
    }
    return changed;
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

// An ability with only a name, or only a description, is stored as that text alone. While it is
// being edited, the box it was typed into is remembered so the text doesn't jump to the other box.
enum AbilityShape { kGuess = 0, kNameOnly = 1, kDescriptionOnly = 2 };

std::pair<std::string, std::string> splitForEditing(const std::string& ability, int shape) {
    if (ability.find(": ") == std::string::npos) {
        if (shape == kNameOnly) return {ability, ""};
        if (shape == kDescriptionOnly) return {"", ability};
        // A short phrase with no full stop is most likely a name.
        const bool looksLikeName = !ability.empty() && ability.size() <= 48 &&
                                   ability.find_first_of(".!?") == std::string::npos;
        if (looksLikeName) return {ability, ""};
    }
    return splitAbility(ability);
}

bool abilityListEditor(std::vector<std::string>& list) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    bool changed = false;
    int moveUp = -1, moveDown = -1, remove = -1;
    const ImGuiID focusRow = ImGui::GetID("focusRow");
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const ImGuiID shapeId = ImGui::GetID("shape");
        auto [name, description] = splitForEditing(list[i], storage->GetInt(shapeId, kGuess));
        const float buttons = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemInnerSpacing.x * 3.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons);
        if (storage->GetInt(focusRow, -1) == static_cast<int>(i)) {
            ImGui::SetKeyboardFocusHere();
            storage->SetInt(focusRow, -1);
        }
        const bool nameChanged = ImGui::InputTextWithHint("##name", "ability name", &name);
        const ImVec2 square(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button("^", square)) moveUp = static_cast<int>(i);
        ImGui::SetItemTooltip("Move up");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button("v", square)) moveDown = static_cast<int>(i);
        ImGui::SetItemTooltip("Move down");
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button("x", square)) remove = static_cast<int>(i);
        ImGui::SetItemTooltip("Remove");
        const float lines = description.size() > 300 ? 6.0f : 3.0f;
        const bool descriptionChanged = ImGui::InputTextMultiline(
            "##what", &description,
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * lines + ImGui::GetStyle().FramePadding.y * 2.0f),
            ImGuiInputTextFlags_WordWrap);
        if (nameChanged || descriptionChanged) {
            changed = true;
            list[i] = joinAbility(name, description);
            const int shape = !description.empty() && name.empty() ? kDescriptionOnly
                              : !name.empty() && description.empty() ? kNameOnly
                                                                      : kGuess;
            storage->SetInt(shapeId, shape);
        }
        ImGui::Spacing();
        ImGui::PopID();
    }
    const size_t before = list.size();
    if (moveUp > 0) std::swap(list[moveUp], list[moveUp - 1]);
    if (moveDown >= 0 && moveDown + 1 < static_cast<int>(list.size())) std::swap(list[moveDown], list[moveDown + 1]);
    if (remove >= 0) list.erase(list.begin() + remove);
    if (ImGui::Button("+ Add an ability")) {
        list.push_back("");
        storage->SetInt(focusRow, static_cast<int>(list.size()) - 1);
    }
    if (moveUp > 0 || moveDown >= 0 || list.size() != before) {
        changed = true;
        // Rows changed places, so the remembered boxes no longer match them.
        for (size_t i = 0; i < std::max(before, list.size()); ++i) {
            ImGui::PushID(static_cast<int>(i));
            storage->SetInt(ImGui::GetID("shape"), kGuess);
            ImGui::PopID();
        }
    }
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

void listFilterControls(const WindowState& w, RecordFilter& filter, const char* hint, const std::vector<int>& bands,
                        const std::vector<std::string>& names, size_t shown, size_t total) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", hint, &filter.search);
    const int set = (filter.letter != 0) + (filter.power != kAnyPower) + !filter.pathwayId.empty();
    const std::string button = set == 0 ? "Filter and sort" : "Filter and sort (" + std::to_string(set) + ")";
    if (ImGui::SmallButton(button.c_str())) ImGui::OpenPopup("filters");
    if (filter.active()) {
        ImGui::SameLine();
        if (ImGui::TextLink("Clear")) filter.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu of %zu", shown, total);
    }
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 27.0f, 0.0f));
    if (!ImGui::BeginPopup("filters")) return;

    label("Power");
    if (ImGui::BeginCombo("##power", powerBandName(filter.power).c_str())) {
        if (ImGui::Selectable("Any power", filter.power == kAnyPower)) filter.power = kAnyPower;
        for (int band : bands) {
            if (ImGui::Selectable(powerBandName(band).c_str(), filter.power == band)) filter.power = band;
        }
        ImGui::EndCombo();
    }

    label("First letter");
    ImGui::BeginGroup();
    const float wrap = ImGui::GetContentRegionAvail().x;
    float used = 0.0f;
    auto letterButton = [&](const std::string& text, char letter) {
        const float width = ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (used > 0.0f && used + width <= wrap) ImGui::SameLine();
        else if (used > 0.0f) used = 0.0f;
        used += width + ImGui::GetStyle().ItemSpacing.x;
        const bool on = filter.letter == letter;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton((text + "##letter").c_str())) filter.letter = letter;
        if (on) ImGui::PopStyleColor();
    };
    letterButton("Any", 0);
    for (char letter : firstLetters(names)) letterButton(std::string(1, letter), letter);
    ImGui::EndGroup();

    pathwayCombo("Pathway", w.app.db, filter.pathwayId, "Any pathway");

    static constexpr const char* kSortLabels[] = {"Name", "ID", "Power, strongest first", "Pathway"};
    label("Sort by");
    if (ImGui::BeginCombo("##sort", kSortLabels[static_cast<int>(filter.sort)])) {
        for (SortOrder order : kSortOrders) {
            if (ImGui::Selectable(kSortLabels[static_cast<int>(order)], filter.sort == order)) filter.sort = order;
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();
    if (ImGui::Button("Clear the filters")) filter.clear();
    ImGui::EndPopup();
}

bool sequenceSlider(const char* id, const Pathway* pathway, int& sequence, bool* above) {
    // The slider runs left to right, from Sequence 9 up to Sequence 0, then Above the Sequence.
    const bool isAbove = above && *above;
    int climbed = isAbove ? 10 : 9 - sequence;
    const std::string shown = isAbove ? "Above the Sequence" : escapePercent(sequenceLabel(pathway, sequence));
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::SliderInt(id, &climbed, 0, above ? 10 : 9, shown.c_str(), ImGuiSliderFlags_NoInput);
    if (changed) {
        if (above) *above = climbed == 10;
        sequence = climbed == 10 ? 0 : 9 - climbed;
    }
    ImGui::TextDisabled("Sequence 9: weakest");
    const char* right = above ? "Above the Sequence: a Great Old One" : "Sequence 0: a god";
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(right).x);
    ImGui::TextDisabled("%s", right);
    return changed;
}

bool sefirahCombo(const char* text, const Database& db, std::string& sefirahId, const char* noneLabel) {
    label(text);
    const std::string preview = sefirahId.empty() ? noneLabel : sefirahLabel(db, sefirahId);
    bool changed = false;
    if (ImGui::BeginCombo(hidden(text).c_str(), preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable(noneLabel, sefirahId.empty())) {
            sefirahId.clear();
            changed = true;
        }
        for (const auto& s : db.sefirot) {
            const std::string name = sefirahLabel(db, s.id) + "##" + s.id;
            if (ImGui::Selectable(name.c_str(), s.id == sefirahId)) {
                sefirahId = s.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

int savedCharacterList(const WindowState& w, std::string& search, int excludeId) {
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    ImGui::SetNextItemWidth(font * 20.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##search", "Search names", &search);
    auto lower = [](std::string text) {
        for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return text;
    };
    std::vector<const Character*> shown;
    for (const auto& c : db.characters) {
        if (c.id != excludeId && lower(c.name).find(lower(search)) != std::string::npos) shown.push_back(&c);
    }
    if (shown.empty()) {
        ImGui::TextDisabled(db.characters.size() <= 1 ? "No other saved characters yet." : "Nobody by that name.");
        return 0;
    }
    // Up to eight cards show at once; the list scrolls past that.
    const float card = ImGui::GetTextLineHeight() * 2.0f + font * 0.8f + ImGui::GetStyle().ItemSpacing.y;
    const float height = card * static_cast<float>(std::min<size_t>(shown.size(), 8)) + ImGui::GetStyle().WindowPadding.y;
    int picked = 0;
    ImGui::BeginChild("cards", ImVec2(font * 20.0f, height));
    for (const Character* c : shown) {
        const Pathway* p = db.findPathway(c->pathwayId);
        const std::string subtitle = (c->pathwayId.empty() ? "Mortal" : levelName(*c) + (p ? ", " + p->name : "")) +
                                     "  \xC2\xB7  " + characterCode(c->id);
        ImGui::PushID(c->id);
        if (listCard(w, "pick", false, pathwayColour(db, c->pathwayId), initials(c->name), c->name, subtitle)) {
            picked = c->id;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    return picked;
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
        // Each time the screen is shown the rule runs out to the right.
        const float ruleStart = start + d * 2.6f;
        const float ruleEnd = ruleStart + (right - ruleStart) * easeIn(w.tabShownAt + 0.05, 0.7f);
        if (ruleEnd > ruleStart + 1.0f) {
            draw->AddRectFilledMultiColor(ImVec2(ruleStart, y - 0.5f), ImVec2(ruleEnd, y + 0.5f), gold,
                                          colourU32(palette().gold, 0.0f), colourU32(palette().gold, 0.0f), gold);
        }
    }
    ImGui::Spacing();
}

std::string initials(const std::string& name) { return nameInitials(name); }

void medallionAt(const WindowState& w, ImVec2 centre, float radius, const ImVec4& colour, const std::string& text) {
    drawMedallion(w, centre, radius, colour, text);
}

namespace {
bool animationsOn = true;
}  // namespace

void setAnimations(bool on) { animationsOn = on; }
bool animations() { return animationsOn; }

float frameSeconds() { return std::min(ImGui::GetIO().DeltaTime, 0.1f); }

float glideFrom(ImGuiID id, float start, float target, float speed) {
    float* value = ImGui::GetStateStorage()->GetFloatRef(id, animationsOn ? start : target);
    if (!animationsOn) return *value = target;
    *value += (target - *value) * (1.0f - std::exp(-speed * frameSeconds()));
    if (std::fabs(target - *value) < 1e-3f * std::max(1.0f, std::fabs(target))) *value = target;
    return *value;
}

float glide(ImGuiID id, float target, float speed) { return glideFrom(id, target, target, speed); }

float easeIn(double since, float seconds) {
    if (!animationsOn) return 1.0f;
    const float t = std::clamp(static_cast<float>((ImGui::GetTime() - since) / seconds), 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

float popIn(double since, float seconds) {
    if (!animationsOn) return 1.0f;
    const float t = std::clamp(static_cast<float>((ImGui::GetTime() - since) / seconds), 0.0f, 1.0f);
    const float back = 1.70158f, u = t - 1.0f;  // ease out, overshooting a little before settling
    return 1.0f + (back + 1.0f) * u * u * u + back * u * u;
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
              const std::string& badge, const std::string& title, const std::string& subtitle,
              const CardMarks& marks) {
    const float font = ImGui::GetFontSize();
    const float pad = font * 0.4f;
    const float height = ImGui::GetTextLineHeight() * 2.0f + pad * 2.0f;
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
    const bool clicked = ImGui::Selectable(("##" + id).c_str(), selected, ImGuiSelectableFlags_None, ImVec2(width, height));
    ImGui::PopStyleVar();

    // Hovering nudges the card to the right; the gold bar of the one picked grows from its middle.
    const float hover = glide(ImGui::GetID(("hover " + id).c_str()), ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    const float picked = glide(ImGui::GetID(("picked " + id).c_str()), selected ? 1.0f : 0.0f, 12.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (picked > 0.01f) {
        const float half = height * 0.5f * picked;
        draw->AddRectFilled(ImVec2(at.x, at.y + height * 0.5f - half), ImVec2(at.x + font * 0.18f, at.y + height * 0.5f + half),
                            colourU32(palette().gold, std::min(1.0f, picked * 1.5f)), font * 0.1f);
    } else if (hover > 0.01f) {
        draw->AddRectFilled(ImVec2(at.x, at.y + height * 0.25f), ImVec2(at.x + font * 0.12f, at.y + height * 0.75f),
                            colourU32(palette().gold, 0.4f * hover), font * 0.06f);
    }
    const float nudge = font * 0.22f * hover;
    const float radius = height * 0.34f * (1.0f + 0.06f * hover);
    const ImVec2 centre(at.x + pad + height * 0.34f + font * 0.2f + nudge, at.y + height * 0.5f);
    drawMedallion(w, centre, radius, colour, badge, marks.ring);
    if (marks.dot) {
        const ImVec2 dot(centre.x + radius * 0.74f, centre.y - radius * 0.74f);
        draw->AddCircleFilled(dot, radius * 0.3f, ImGui::GetColorU32(ImGuiCol_ChildBg));
        draw->AddCircleFilled(dot, radius * 0.22f, colourU32(marks.dotColour));
    }
    const float textX = at.x + pad * 2.0f + height * 0.68f + font * 0.3f + nudge;
    const float room = at.x + width - textX - pad;
    ImFont* titleFont = selected ? w.fonts.bold : ImGui::GetFont();
    draw->AddText(titleFont, font, ImVec2(textX, at.y + pad), ImGui::GetColorU32(ImGuiCol_Text),
                  fitText(title, titleFont, font, room).c_str());
    draw->AddText(ImGui::GetFont(), font * 0.9f, ImVec2(textX, at.y + pad + ImGui::GetTextLineHeight()),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), fitText(subtitle, ImGui::GetFont(), font * 0.9f, room).c_str());
    return clicked;
}

void chipRow(const std::vector<std::pair<std::string, ImVec4>>& chips) {
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x * 0.6f;
    for (size_t i = 0; i < chips.size(); ++i) {
        const float width = ImGui::CalcTextSize(chips[i].first.c_str()).x + ImGui::GetFontSize() * 1.1f;
        if (i > 0) {
            ImGui::SameLine(0.0f, gap);
            if (ImGui::GetCursorScreenPos().x + width > right) ImGui::NewLine();
        }
        chip(chips[i].first, chips[i].second);
    }
}

ImVec4 badgeColour(const WindowState& w, const Badge& badge) {
    if (badge.tone == "pathway") return pathwayColour(w.app.db, badge.pathwayId);
    if (badge.tone == "colour") return colourFromHex(badge.colour);
    if (badge.tone == "gold") return palette().gold;
    if (badge.tone == "good") return palette().success;
    if (badge.tone == "orange") return orange();
    if (badge.tone == "danger") return palette().danger;
    return palette().muted;
}

void badgeRow(const WindowState& w, const std::vector<Badge>& badges) {
    std::vector<std::pair<std::string, ImVec4>> chips;
    for (const Badge& badge : badges) chips.push_back({badge.text, badgeColour(w, badge)});
    chipRow(chips);
}

ImVec4 stateOfMindColour(const std::string& state) {
    switch (stateOfMindLevel(state)) {
        case 0: return palette().success;
        case 1: return palette().gold;
        case 2: return orange();
        case 3: return palette().danger;
        case 4: {  // Lost control: deeper than Mad
            const ImVec4 d = palette().danger;
            return ImVec4(d.x * 0.75f, d.y * 0.45f, d.z * 0.85f + 0.1f, d.w);
        }
        default: return palette().muted;
    }
}

void sequenceLadder(const WindowState& w, const std::vector<LadderRung>& rungs) {
    if (rungs.empty()) return;
    const float font = ImGui::GetFontSize();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int count = static_cast<int>(rungs.size());
    const int perRow = std::clamp(static_cast<int>(avail / (font * 4.4f)), 1, count);
    const float step = std::min(avail / static_cast<float>(perRow), font * 6.5f);
    const float radius = font * 0.95f;
    const float rowHeight = radius * 2.0f + font * 2.1f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const LadderRung* hovered = nullptr;
    // When the ladder changes (another character, a new Sequence) its rungs climb in one after another.
    static std::map<ImGuiID, std::pair<std::string, double>> shownSince;
    std::string signature;
    for (const LadderRung& rung : rungs) signature += rung.pathwayId + ":" + std::to_string(rung.sequence) + (rung.current ? "*" : "") + ";";
    auto& since = shownSince[ImGui::GetID("ladder")];
    if (since.first != signature) since = {signature, ImGui::GetTime()};
    for (int i = 0; i < count; ++i) {
        const LadderRung& rung = rungs[static_cast<size_t>(i)];
        const int row = i / perRow, column = i % perRow;
        const ImVec2 centre(origin.x + step * (static_cast<float>(column) + 0.5f),
                            origin.y + radius + font * 0.3f + rowHeight * static_cast<float>(row));
        const ImVec4 colour = pathwayColour(w.app.db, rung.pathwayId);
        const double appearsAt = since.second + 0.05 * static_cast<double>(i);
        const float grown = std::max(0.0f, popIn(appearsAt, 0.4f));
        if (column > 0) {  // the line from the Sequence before, drawn out as this one appears
            const ImVec2 from(centre.x - step + radius + 3.0f, centre.y);
            const ImVec2 to(from.x + (centre.x - radius - 3.0f - from.x) * easeIn(appearsAt - 0.05, 0.25f), centre.y);
            if (!rung.switched) {
                draw->AddLine(from, to, colourU32(colour, 0.7f), 2.5f);
            } else {
                const ImVec4 dash = rung.risky ? palette().danger : colour;
                for (float x = from.x; x < to.x; x += 9.0f) {
                    draw->AddLine(ImVec2(x, centre.y), ImVec2(std::min(x + 5.0f, to.x), centre.y), colourU32(dash), 2.5f);
                }
                const ImVec2 mid((from.x + to.x) * 0.5f, centre.y);
                const float d = font * 0.28f;
                draw->AddQuadFilled(ImVec2(mid.x, mid.y - d), ImVec2(mid.x + d, mid.y), ImVec2(mid.x, mid.y + d),
                                    ImVec2(mid.x - d, mid.y), colourU32(rung.risky ? palette().danger : palette().gold));
            }
        }
        if (rung.current) draw->AddCircle(centre, (radius + 4.0f) * grown, colourU32(palette().gold), 0, 2.0f);
        drawMedallion(w, centre, radius * grown, colour, rung.above ? "\xE2\x88\x9E" : std::to_string(rung.sequence));
        if (rung.risky) {  // a red "!" on a switch to a pathway that isn't a neighbour
            const ImVec2 mark(centre.x + radius * 0.78f, centre.y - radius * 0.78f);
            draw->AddCircleFilled(mark, font * 0.36f, colourU32(palette().danger));
            const ImVec2 size = w.fonts.bold->CalcTextSizeA(font * 0.62f, FLT_MAX, 0.0f, "!");
            draw->AddText(w.fonts.bold, font * 0.62f, ImVec2(mark.x - size.x * 0.5f, mark.y - size.y * 0.5f),
                          ImGui::GetColorU32(ImGuiCol_ChildBg), "!");
        }
        ImFont* nameFont = rung.current ? w.fonts.bold : ImGui::GetFont();
        const std::string name = fitText(rung.name, nameFont, font * 0.8f, step - 4.0f);
        const ImVec2 nameSize = nameFont->CalcTextSizeA(font * 0.8f, FLT_MAX, 0.0f, name.c_str());
        draw->AddText(nameFont, font * 0.8f, ImVec2(centre.x - nameSize.x * 0.5f, centre.y + radius + font * 0.3f),
                      ImGui::GetColorU32(rung.current ? ImGuiCol_Text : ImGuiCol_TextDisabled), name.c_str());
        const ImVec2 cellMin(centre.x - step * 0.5f, centre.y - radius), cellMax(centre.x + step * 0.5f, centre.y + radius + font * 1.4f);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(cellMin, cellMax)) hovered = &rung;
    }
    const int rows = (count + perRow - 1) / perRow;
    ImGui::Dummy(ImVec2(avail, rowHeight * static_cast<float>(rows)));
    if (hovered && ImGui::BeginTooltip()) {
        if (hovered->above) ImGui::Text("Above the Sequence: %s", hovered->name.c_str());
        else ImGui::Text("Sequence %d: %s", hovered->sequence, hovered->name.c_str());
        ImGui::TextDisabled("%s pathway%s", hovered->pathway.c_str(), hovered->current ? ", where they are now" : "");
        if (hovered->switched) {
            ImGui::TextColored(hovered->risky ? palette().danger : palette().gold, "%s",
                               hovered->risky ? "Switched here from a pathway that isn't a neighbour"
                                              : "Switched here from a neighbouring pathway");
        }
        ImGui::EndTooltip();
    }
    // The pathways on the ladder, and what the switch marks mean.
    std::vector<std::pair<std::string, ImVec4>> keys;
    std::vector<std::string> seen;
    bool switched = false, risky = false;
    for (const LadderRung& rung : rungs) {
        switched = switched || rung.switched;
        risky = risky || rung.risky;
        if (rung.above || std::find(seen.begin(), seen.end(), rung.pathwayId) != seen.end()) continue;
        seen.push_back(rung.pathwayId);
        keys.push_back({rung.pathway + " pathway", pathwayColour(w.app.db, rung.pathwayId)});
    }
    if (switched) keys.push_back({"dashed: a switch of pathway", palette().gold});
    if (risky) keys.push_back({"red: not a neighbour", palette().danger});
    if (keys.size() > 1 || switched) chipRow(keys);
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
        const float shown = glide(ImGui::GetID(("stat box " + row.code).c_str()), static_cast<float>(row.total), 9.0f);
        centred(w.fonts.title, font * 1.75f, at.y + font * 1.3f, ImGui::GetColorU32(ImGuiCol_Text),
                std::to_string(static_cast<int>(std::lround(shown))));
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
    ImGui::PushID("stat chart");
    for (size_t i = 0; i < totals.size() && i < palette().series.size(); ++i) {
        const ImVec4& tint = palette().series[i];
        ImVec2 corners[kStatCount];
        for (int axis = 0; axis < kStatCount; ++axis) {
            const float fraction = static_cast<float>(totals[i][static_cast<size_t>(axis)]) / static_cast<float>(scale);
            // Each outline grows out of the middle, and glides when the totals or the scale change.
            const ImGuiID id = ImGui::GetID(static_cast<int>(i) * kStatCount + axis);
            corners[axis] = point(axis, glideFrom(id, 0.0f, std::clamp(fraction, 0.0f, 1.0f), 7.0f));
        }
        draw->AddConcavePolyFilled(corners, kStatCount, colourU32(tint, 0.14f));
        draw->AddPolyline(corners, kStatCount, colourU32(tint), ImDrawFlags_Closed, 2.0f);
        for (const ImVec2& corner : corners) draw->AddCircleFilled(corner, 3.0f, colourU32(tint));
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(size, size));
    ImGui::TextDisabled("Totals with pathway bonuses. A ring every %d points, up to %d.", step, scale);
}

// Story events down a line, each with a dot in its kind's colour: the date, the title, where and who.
void timelineList(const WindowState& w, const std::vector<TimelineEntry>& entries) {
    const float font = ImGui::GetFontSize();
    const float dateWidth = font * 7.5f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < entries.size(); ++i) {
        const TimelineEntry& e = entries[i];
        ImGui::PushID(static_cast<int>(i));
        const ImVec2 top = ImGui::GetCursorScreenPos();
        const float x = ImGui::GetCursorPosX();
        const ImVec4 colour = colourFromHex(e.colour);
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::PushTextWrapPos(x + dateWidth - font * 0.6f);
        ImGui::TextWrapped("%s", e.date.empty() ? "No date" : e.date.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(top.x + dateWidth + font * 1.2f, top.y));
        ImGui::BeginGroup();
        ImGui::PushFont(w.fonts.bold, 0.0f);
        ImGui::TextWrapped("%s", e.title.c_str());
        ImGui::PopFont();
        if (!e.kind.empty()) {
            ImGui::SameLine();
            chip(e.kind, colour);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        std::string where = e.place;
        if (!e.chapter.empty()) where += (where.empty() ? "" : ", ") + e.chapter;
        if (!where.empty()) ImGui::TextWrapped("%s", where.c_str());
        if (!e.people.empty()) ImGui::TextWrapped("With %s", e.people.c_str());
        if (!e.artifacts.empty()) ImGui::TextWrapped("Sealed Artifacts: %s", e.artifacts.c_str());
        ImGui::PopStyleColor();
        if (!e.description.empty()) ImGui::TextWrapped("%s", e.description.c_str());
        ImGui::EndGroup();
        const float bottom = ImGui::GetItemRectMax().y;
        // The line down the side, and the event's dot on it.
        const float railX = top.x + dateWidth + font * 0.45f;
        if (i + 1 < entries.size()) {
            draw->AddLine(ImVec2(railX, top.y + font * 0.6f), ImVec2(railX, bottom + font * 1.0f),
                          ImGui::GetColorU32(ImGuiCol_Border), 2.0f);
        }
        draw->AddCircleFilled(ImVec2(railX, top.y + font * 0.6f), font * 0.32f, ImGui::GetColorU32(ImGuiCol_ChildBg));
        draw->AddCircleFilled(ImVec2(railX, top.y + font * 0.6f), font * 0.24f, colourU32(colour));
        ImGui::Dummy(ImVec2(0.0f, font * 0.4f));
        ImGui::PopID();
    }
}

// The relationship web as a still picture, scaled to the width there is.
void webPicture(const WindowState& w, const WebFigure& web) {
    if (web.nodes.empty()) return;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float scale = std::min(1.0f, avail / web.width);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(at.x + web.width * scale, at.y + web.height * scale);
    draw->AddRectFilled(at, end, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().ChildRounding);
    auto point = [&](const FigureNode& n) { return ImVec2(at.x + n.x * scale, at.y + n.y * scale); };
    for (const FigureEdge& e : web.edges) {
        if (e.a >= web.nodes.size() || e.b >= web.nodes.size()) continue;
        draw->AddLine(point(web.nodes[e.a]), point(web.nodes[e.b]), colourU32(colourFromHex(e.colour), 0.6f), 1.5f);
    }
    for (const FigureNode& n : web.nodes) {
        const ImVec4 colour = n.saved ? colourFromHex(n.colour) : palette().muted;
        drawMedallion(w, point(n), n.radius * scale * 0.9f, colour, n.badge);
    }
    ImGui::Dummy(ImVec2(end.x - at.x, end.y - at.y));
    std::vector<std::pair<std::string, ImVec4>> keys;
    for (const auto& [name, colour] : web.legend) keys.push_back({name, colourFromHex(colour)});
    chipRow(keys);
}

// A sheet drawn with the window's own controls: the same content as the exports.
void drawSheet(const WindowState& w, const Sheet& sheet) {
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.7f);
    ImGui::TextWrapped("%s", sheet.title.c_str());
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s", sheet.subtitle.c_str());
    ImGui::PopStyleColor();
    badgeRow(w, sheet.badges);

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
                case BlockKind::Ladder:
                    sequenceLadder(w, block.rungs);
                    break;
                case BlockKind::Timeline:
                    timelineList(w, block.entries);
                    break;
                case BlockKind::Web:
                    webPicture(w, block.web);
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
    return w.characters.draft.dirty() || w.artifacts.draft.dirty() || w.pathways.draft.dirty() ||
           w.sefirot.draft.dirty() || w.timeline.draft.dirty();
}

}  // namespace lotm::window
