// Timeline tab: story events by date on the left; on the right, the story as a strip of dates with a
// marker to move through it (the cast below shows everyone as they were then), or the event being
// edited.
#include <algorithm>
#include <cmath>

#include "records.hpp"
#include "rules.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

ImU32 colourU32(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha)); }

std::string lower(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

std::string kindBadge(const std::string& kind) { return kind.empty() ? "?" : initials(kind).substr(0, 1); }

void open(WindowState& w, const StoryEvent& e, bool isNew) {
    StoryEvent copy = e;
    for (auto& person : copy.people) {  // linked people show the character's current name
        if (const Character* c = w.app.db.findCharacter(person.characterId)) person.name = c->name;
    }
    w.timeline.draft.start(copy, isNew);
    w.timeline.selectEditor = true;
}

bool saveImpl(WindowState& w) {
    auto& draft = w.timeline.draft;
    StoryEvent& e = draft.value;
    e.title = ui::trim(e.title);
    if (e.title.empty()) {
        setStatus(w, "Give the event a title before saving (Details tab).", true);
        return false;
    }
    for (auto& person : e.people) person.name = ui::trim(person.name);
    auto& held = e.artifactIds;  // a Sealed Artifact may have been deleted meanwhile
    held.erase(std::remove_if(held.begin(), held.end(), [&](int id) { return !w.app.db.findArtifact(id); }), held.end());
    try {
        saveEvent(w.app, e, draft.isNew);
        setStatus(w, "Saved " + e.title + " as " + eventCode(e.id) + ".");
        const bool keepEditor = w.timeline.selectEditor;
        if (const StoryEvent* saved = w.app.db.findEvent(e.id)) open(w, *saved, false);
        w.timeline.selectEditor = keepEditor;
        return true;
    } catch (const StorageError& error) {
        setStatus(w, std::string("Saving failed: ") + error.what(), true);
        return false;
    }
}

bool matches(const WindowState& w, const StoryEvent& e) {
    const TimelineScreen& s = w.timeline;
    if (s.characterId != 0 &&
        std::none_of(e.people.begin(), e.people.end(), [&](const EventPerson& p) { return p.characterId == s.characterId; })) {
        return false;
    }
    if (!s.kind.empty() && lower(e.kind) != lower(s.kind)) return false;
    const std::string search = lower(ui::trim(s.search));
    if (search.empty()) return true;
    const std::string text = lower(e.title + " " + e.place + " " + e.chapter + " " + e.kind + " " + storyDateText(e.date) +
                                   " " + eventPeopleText(w.app.db, e) + " " + eventCode(e.id));
    return text.find(search) != std::string::npos;
}

// ---------------------------------------------------------------- the list on the left

void eventList(WindowState& w) {
    TimelineScreen& s = w.timeline;
    const Database& db = w.app.db;
    auto saveThis = [&w] { return saveImpl(w); };
    if (ImGui::Button("+ New event", ImVec2(-FLT_MIN, 0))) openNewEvent(w);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search titles, places, people", &s.search);

    const Character* who = db.findCharacter(s.characterId);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (ImGui::BeginCombo("##who", who ? who->name.c_str() : "Anyone", ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("Anyone", s.characterId == 0)) s.characterId = 0;
        for (const Character& c : db.characters) {
            if (ImGui::Selectable((c.name + "##" + std::to_string(c.id)).c_str(), c.id == s.characterId)) s.characterId = c.id;
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Only the events this character was at");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##kind", s.kind.empty() ? "Any kind" : s.kind.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("Any kind", s.kind.empty())) s.kind.clear();
        std::vector<std::string> kinds = kEventKinds;
        for (const StoryEvent& e : db.events) {
            if (!e.kind.empty() && std::find(kinds.begin(), kinds.end(), e.kind) == kinds.end()) kinds.push_back(e.kind);
        }
        for (const std::string& kind : kinds) {
            statusDot(colourFromHex(eventKindColour(kind)));
            if (ImGui::Selectable(kind.c_str(), kind == s.kind)) s.kind = kind;
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    auto& draft = s.draft;
    if (draft.open && draft.isNew) {
        listCard(w, "new", true, colourFromHex(eventKindColour(draft.value.kind)), kindBadge(draft.value.kind),
                 draft.value.title.empty() ? "(new event)" : draft.value.title, "Not saved yet");
    }
    if (db.events.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("No story events yet. An event is something that happens in your story: a meeting, a "
                           "battle, a character's advancement. Give it a date and the people who were there.");
        ImGui::PopStyleColor();
        return;
    }
    std::string year = "\x01";
    size_t shown = 0;
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (!matches(w, *e)) continue;
        ++shown;
        // A heading for each year.
        std::string thisYear = e->date.empty() ? "No date" : std::to_string(e->date.year);
        if (!e->date.empty() && e->date.epoch != 5) thisYear += ", " + epochName(e->date.epoch);
        if (thisYear != year) {
            year = thisYear;
            ImGui::Spacing();
            ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 0.95f);
            ImGui::TextColored(palette().gold, "%s", year.c_str());
            ImGui::PopFont();
        }
        std::string subtitle = storyDateText(e->date);
        if (!e->place.empty()) subtitle += (subtitle.empty() ? "" : ", ") + e->place;
        if (subtitle.empty()) subtitle = e->kind.empty() ? eventCode(e->id) : e->kind;
        const bool selected = draft.open && !draft.isNew && draft.value.id == e->id;
        if (listCard(w, "e" + std::to_string(e->id), selected, colourFromHex(eventKindColour(e->kind)), kindBadge(e->kind),
                     e->title, subtitle) &&
            !selected) {
            const int id = e->id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                if (const StoryEvent* found = w.app.db.findEvent(id)) open(w, *found, false);
            });
        }
        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted((eventCode(e->id) + (e->kind.empty() ? "" : ", " + e->kind)).c_str());
            const std::string people = eventPeopleText(db, *e);
            if (!people.empty()) ImGui::TextDisabled("With %s", people.c_str());
            if (!e->chapter.empty()) ImGui::TextDisabled("%s", e->chapter.c_str());
            ImGui::EndTooltip();
        }
    }
    if (shown == 0) ImGui::TextDisabled("None match the filters.");
}

// ---------------------------------------------------------------- the story

struct StripDate {
    std::vector<const StoryEvent*> events;
    std::vector<std::pair<const Character*, const CharacterStage*>> stages;
};

// The strip: every date where something happens, evenly spaced, events as dots above the line and
// characters' new stages as diamonds below it, with the marker at the date being looked at.
void storyStrip(WindowState& w, const std::vector<StoryDate>& dates, const std::vector<StripDate>& on) {
    TimelineScreen& s = w.timeline;
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = font * 7.2f;
    const size_t slots = dates.size() + 1;  // the last one is now
    const float step = width / static_cast<float>(slots);
    const float railY = origin.y + height * 0.5f;
    auto slotX = [&](float slot) { return origin.x + step * (slot + 0.5f); };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("strip", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    int hoveredSlot = -1;
    if (hovered) {
        hoveredSlot = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - origin.x) / step), 0, static_cast<int>(slots) - 1);
    }
    if (ImGui::IsItemActive() && hoveredSlot >= 0) {  // click or drag along it to move the marker
        const int to = hoveredSlot == static_cast<int>(dates.size()) ? -1 : hoveredSlot;
        if (to != s.asOf) {
            s.asOf = to;
            s.asOfChangedAt = ImGui::GetTime();
            s.playing = false;
        }
    }

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg),
                        ImGui::GetStyle().ChildRounding);
    // The marker glides; the story before it is lit.
    const float target = s.asOf < 0 ? static_cast<float>(dates.size()) : static_cast<float>(s.asOf);
    if (s.shownAsOf < 0.0f) s.shownAsOf = target;
    s.shownAsOf += (target - s.shownAsOf) * (1.0f - std::exp(-10.0f * frameSeconds()));
    const float markerX = slotX(s.shownAsOf);
    const ImVec4 gold = palette().gold;
    draw->AddRectFilledMultiColor(ImVec2(origin.x, origin.y), ImVec2(markerX, origin.y + height), colourU32(gold, 0.0f),
                                  colourU32(gold, 0.10f), colourU32(gold, 0.10f), colourU32(gold, 0.0f));
    draw->AddLine(ImVec2(origin.x + step * 0.25f, railY), ImVec2(origin.x + width - step * 0.25f, railY),
                  ImGui::GetColorU32(ImGuiCol_Border), 2.0f);
    draw->AddLine(ImVec2(origin.x + step * 0.25f, railY), ImVec2(markerX, railY), colourU32(gold, 0.7f), 2.0f);
    if (hoveredSlot >= 0) {
        draw->AddRectFilled(ImVec2(slotX(static_cast<float>(hoveredSlot)) - step * 0.5f, origin.y),
                            ImVec2(slotX(static_cast<float>(hoveredSlot)) + step * 0.5f, origin.y + height),
                            colourU32(gold, 0.08f));
    }

    // Each date: events above, stages below, the year under the first date of each year.
    const float dotRadius = std::clamp(step * 0.22f, 3.0f, font * 0.42f);
    float lastLabelEnd = -1e9f;
    for (size_t i = 0; i < dates.size(); ++i) {
        const float x = slotX(static_cast<float>(i));
        const bool passed = static_cast<float>(i) <= s.shownAsOf + 0.01f;
        const float fade = passed ? 1.0f : 0.45f;
        draw->AddLine(ImVec2(x, railY - 3.0f), ImVec2(x, railY + 3.0f), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
        const auto& events = on[i].events;
        for (size_t k = 0; k < events.size() && k < 3; ++k) {
            const StoryEvent* e = events[k];
            const bool dimmed = (s.characterId != 0 || !s.kind.empty()) && !matches(w, *e);
            const ImVec2 at(x, railY - dotRadius * 2.0f - static_cast<float>(k) * dotRadius * 2.4f);
            draw->AddCircleFilled(at, dotRadius, colourU32(colourFromHex(eventKindColour(e->kind)), fade * (dimmed ? 0.25f : 1.0f)));
        }
        if (events.size() > 3) {
            const std::string more = "+" + std::to_string(events.size() - 3);
            const ImVec2 size = ImGui::CalcTextSize(more.c_str());
            draw->AddText(ImVec2(x - size.x * 0.5f, railY - dotRadius * 9.6f - size.y), colourU32(palette().muted), more.c_str());
        }
        const auto& stages = on[i].stages;
        for (size_t k = 0; k < stages.size() && k < 3; ++k) {
            const float d = dotRadius * 0.9f;
            const ImVec2 at(x, railY + d * 2.2f + static_cast<float>(k) * d * 2.4f);
            const ImU32 colour = colourU32(pathwayColour(db, stages[k].second->pathwayId), fade);
            draw->AddQuadFilled(ImVec2(at.x, at.y - d), ImVec2(at.x + d, at.y), ImVec2(at.x, at.y + d), ImVec2(at.x - d, at.y), colour);
        }
        // The year, when there is room for it.
        if (i == 0 || dates[i].year != dates[i - 1].year || dates[i].epoch != dates[i - 1].epoch) {
            const std::string year = std::to_string(dates[i].year);
            const ImVec2 size = ImGui::CalcTextSize(year.c_str());
            if (x - size.x * 0.5f > lastLabelEnd + font * 0.5f) {
                draw->AddText(ImVec2(x - size.x * 0.5f, origin.y + height - size.y - font * 0.25f),
                              colourU32(palette().muted, fade), year.c_str());
                lastLabelEnd = x + size.x * 0.5f;
            }
        }
    }
    // Now, at the end.
    const float nowX = slotX(static_cast<float>(dates.size()));
    draw->AddCircle(ImVec2(nowX, railY), dotRadius * 1.2f, colourU32(gold), 0, 2.0f);
    const ImVec2 nowSize = ImGui::CalcTextSize("Now");
    draw->AddText(ImVec2(std::min(nowX - nowSize.x * 0.5f, origin.x + width - nowSize.x - 4.0f),
                         origin.y + height - nowSize.y - font * 0.25f),
                  colourU32(gold), "Now");
    // The marker: a gold line with a small diamond on top.
    draw->AddLine(ImVec2(markerX, origin.y + font * 0.5f), ImVec2(markerX, origin.y + height - font * 1.4f), colourU32(gold), 2.0f);
    const float d = font * 0.3f;
    draw->AddQuadFilled(ImVec2(markerX, origin.y + font * 0.5f - d), ImVec2(markerX + d, origin.y + font * 0.5f),
                        ImVec2(markerX, origin.y + font * 0.5f + d), ImVec2(markerX - d, origin.y + font * 0.5f), colourU32(gold));

    if (hoveredSlot >= 0 && ImGui::BeginTooltip()) {
        if (hoveredSlot == static_cast<int>(dates.size())) {
            ImGui::TextUnformatted("Now: everyone as they are now");
        } else {
            const size_t i = static_cast<size_t>(hoveredSlot);
            ImGui::TextColored(gold, "%s", storyDateText(dates[i]).c_str());
            for (const StoryEvent* e : on[i].events) {
                statusDot(colourFromHex(eventKindColour(e->kind)));
                ImGui::TextUnformatted(e->title.c_str());
            }
            for (const auto& [c, stage] : on[i].stages) {
                ImGui::TextDisabled("%s: %s", c->name.c_str(), stageLevelText(*stage, db).c_str());
            }
        }
        ImGui::TextDisabled("Click to see everyone as they were then.");
        ImGui::EndTooltip();
    }
}

// The marker's controls: back, play, forward, and a slider with the date.
void markerControls(WindowState& w, const std::vector<StoryDate>& dates) {
    TimelineScreen& s = w.timeline;
    const int last = static_cast<int>(dates.size());  // now
    int at = s.asOf < 0 ? last : s.asOf;
    const int before = at;
    if (ImGui::ArrowButton("##back", ImGuiDir_Left)) at = std::max(0, at - 1);
    ImGui::SetItemTooltip("The date before");
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if (ImGui::Button(s.playing ? "Pause" : "Play", ImVec2(ImGui::GetFontSize() * 4.0f, 0.0f))) {
        s.playing = !s.playing;
        if (s.playing && at == last) at = 0;  // from the start again
        s.playedAt = ImGui::GetTime();
    }
    ImGui::SetItemTooltip("Step through the story date by date");
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    if (ImGui::ArrowButton("##forward", ImGuiDir_Right)) at = std::min(last, at + 1);
    ImGui::SetItemTooltip("The date after");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const std::string label = at == last ? "Now" : "As of " + storyDateText(dates[static_cast<size_t>(at)]);
    ImGui::SliderInt("##asOf", &at, 0, last, label.c_str(), ImGuiSliderFlags_NoInput);
    if (s.playing && ImGui::GetTime() - s.playedAt > 1.2) {
        s.playedAt = ImGui::GetTime();
        at = std::min(last, at + 1);
        if (at == last) s.playing = false;
    }
    if (at != before) {
        s.asOf = at == last ? -1 : at;
        s.asOfChangedAt = ImGui::GetTime();
    }
}

// Everyone as they were on the marker's date, as cards; a card lights up when they change.
void castGrid(WindowState& w, const StoryDate& date) {
    TimelineScreen& s = w.timeline;
    const Database& db = w.app.db;
    const float font = ImGui::GetFontSize();
    const float cardWidth = font * 14.5f, cardHeight = font * 3.6f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int perRow = std::max(1, static_cast<int>((avail + ImGui::GetStyle().ItemSpacing.x) / (cardWidth + ImGui::GetStyle().ItemSpacing.x)));
    const float width = (avail - ImGui::GetStyle().ItemSpacing.x * static_cast<float>(perRow - 1)) / static_cast<float>(perRow);
    const double now = ImGui::GetTime();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    int column = 0;
    for (const Character& c : db.characters) {
        if (s.characterId != 0 && c.id != s.characterId) continue;
        const AsOf then = characterAsOf(c, date, db);
        const Character& t = then.character;
        const Pathway* p = db.findPathway(t.pathwayId);
        std::string line = t.pathwayId.empty() ? "Mortal" : levelName(t) + (p ? ", " + p->name : "");
        const std::string key = line + "|" + t.stateOfMind + "|" + t.affiliation.organization;
        auto& seen = s.castLines[c.id];
        if (!seen.empty() && seen != key) s.castChangedAt[c.id] = now;
        seen = key;
        const auto changed = s.castChangedAt.find(c.id);
        const float glow = changed == s.castChangedAt.end() ? 0.0f
                                                            : std::clamp(1.0f - static_cast<float>(now - changed->second) / 1.2f, 0.0f, 1.0f);

        if (column > 0) ImGui::SameLine();
        ImGui::PushID(c.id);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("card", ImVec2(width, cardHeight))) {
            s.viewingCharacter = c.id;
            ImGui::OpenPopup("asOfSheet");
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec2 end(at.x + width, at.y + cardHeight);
        const float rounding = ImGui::GetStyle().ChildRounding;
        draw->AddRectFilled(at, end, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), rounding);
        if (glow > 0.0f) {
            draw->AddRectFilled(at, end, colourU32(palette().gold, 0.22f * glow), rounding);
            draw->AddRect(at, end, colourU32(palette().gold, glow), rounding, 0, 2.0f);
        }
        const ImVec4 colour = pathwayColour(db, t.pathwayId);
        const float r = cardHeight * 0.32f;
        const std::string badge = t.pathwayId.empty() ? "-" : t.aboveSequence ? "\xE2\x88\x9E" : std::to_string(t.sequence);
        medallionAt(w, ImVec2(at.x + font * 0.5f + r, at.y + cardHeight * 0.5f), r * (1.0f + 0.12f * glow), colour, badge);
        const float textX = at.x + font * 1.0f + r * 2.0f;
        ImGui::PushClipRect(at, ImVec2(end.x - font * 0.3f, end.y), true);
        draw->AddText(w.fonts.bold, font, ImVec2(textX, at.y + font * 0.35f), ImGui::GetColorU32(ImGuiCol_Text), c.name.c_str());
        draw->AddText(ImGui::GetFont(), font * 0.86f, ImVec2(textX, at.y + font * 1.4f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      line.c_str());
        std::string extra = then.beforeKnown ? "Before anything recorded" : t.stateOfMind;
        if (extra.empty()) extra = t.affiliation.organization;
        const ImVec4 extraColour = then.beforeKnown || t.stateOfMind.empty() ? palette().muted : stateOfMindColour(t.stateOfMind);
        draw->AddText(ImGui::GetFont(), font * 0.8f, ImVec2(textX, at.y + font * 2.35f), colourU32(extraColour), extra.c_str());
        ImGui::PopClipRect();
        if (hovered) {
            ImGui::SetItemTooltip("%s", then.stageIndex >= 0
                                            ? stageSummary(c.stages[static_cast<size_t>(then.stageIndex)], db).c_str()
                                            : "As they are now. Click to see their sheet as of this date.");
        }
        column = (column + 1) % perRow;
    }

    // The sheet of the card clicked, as of the date.
    ImGui::SetNextWindowSize(ImVec2(font * 42.0f, ImGui::GetMainViewport()->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (ImGui::BeginPopupModal("asOfSheet", &open)) {
        const Character* c = db.findCharacter(s.viewingCharacter);
        if (!c) {
            ImGui::CloseCurrentPopup();
        } else {
            const Sheet sheet = buildCharacterSheetAsOf(*c, date, db);
            const std::string name = exportName(*c) + (date.empty() ? "" : "-" + lower(storyDateText(date)));
            if (ImGui::Button("Export as HTML")) exportAndReport(w, name, ExportFormat::Html, {sheet}, c->name);
            ImGui::SameLine();
            if (ImGui::Button("Open them on the Characters tab")) {
                ImGui::CloseCurrentPopup();
                openCharacter(w, c->id);
            }
            ImGui::SameLine();
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
            ImGui::Separator();
            ImGui::BeginChild("sheet");
            drawSheet(w, sheet);
            ImGui::EndChild();
        }
        ImGui::EndPopup();
    }
}

void exportTimeline(WindowState& w) {
    if (ImGui::Button("Export the timeline...")) ImGui::OpenPopup("exportTimeline");
    if (!ImGui::BeginPopup("exportTimeline")) return;
    const int who = w.timeline.characterId;
    const Character* c = w.app.db.findCharacter(who);
    if (c) ImGui::TextDisabled("%s's events only (pick Anyone on the left for all)", c->name.c_str());
    const Sheet sheet = buildTimelineSheet(w.app.db, c ? who : 0);
    const std::string name = c ? "timeline-" + exportName(*c) : "timeline";
    if (ImGui::Selectable("HTML (opens in your browser, prints to PDF)")) exportAndReport(w, name, ExportFormat::Html, {sheet}, sheet.title);
    if (ImGui::Selectable("Markdown (Discord, Obsidian, wikis)")) {
        exportAndReport(w, name, ExportFormat::Markdown, {sheet}, sheet.title);
    }
    if (ImGui::Selectable("Plain text")) exportAndReport(w, name, ExportFormat::Text, {sheet}, sheet.title);
    ImGui::EndPopup();
}

void storyView(WindowState& w) {
    TimelineScreen& s = w.timeline;
    const Database& db = w.app.db;
    const std::vector<StoryDate> dates = storyDates(db);
    if (s.asOf >= static_cast<int>(dates.size())) s.asOf = -1;
    std::vector<StripDate> on(dates.size());
    auto slotOf = [&](const StoryDate& date) {
        for (size_t i = 0; i < dates.size(); ++i) {
            if (sameStoryDate(dates[i], date)) return static_cast<int>(i);
        }
        return -1;
    };
    size_t stages = 0;
    for (const StoryEvent* e : eventsInOrder(db)) {
        if (const int i = slotOf(e->date); i >= 0) on[static_cast<size_t>(i)].events.push_back(e);
    }
    for (const Character& c : db.characters) {
        for (const CharacterStage& stage : c.stages) {
            if (const int i = slotOf(stage.date); i >= 0) {
                on[static_cast<size_t>(i)].stages.push_back({&c, &stage});
                ++stages;
            }
        }
    }

    exportTimeline(w);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%zu %s, %zu %s", db.events.size(), db.events.size() == 1 ? "event" : "events", stages,
                        stages == 1 ? "stage dated" : "stages dated");
    ImGui::Spacing();
    if (dates.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("Nothing has a date yet. Give your events dates (\"3 July 1360\", or just \"1360\") and they line "
                           "up here. Give characters stages on their Stages tab (what they were like from a date on), and "
                           "you can move through the story and see everyone as they were.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (ImGui::Button("+ New event")) openNewEvent(w);
        return;
    }
    storyStrip(w, dates, on);
    markerControls(w, dates);

    const StoryDate date = s.asOf < 0 ? StoryDate{} : dates[static_cast<size_t>(s.asOf)];
    const float appear = std::clamp(static_cast<float>(ImGui::GetTime() - s.asOfChangedAt) / 0.35f, 0.0f, 1.0f);
    ImGui::Spacing();
    if (s.asOf >= 0) {
        heading(w, "On " + storyDateText(date));
        const auto& events = on[static_cast<size_t>(s.asOf)].events;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * appear);
        if (events.empty()) ImGui::TextDisabled("No events on this date; a character's stage begins.");
        for (const StoryEvent* e : events) {
            ImGui::PushID(e->id);
            statusDot(colourFromHex(eventKindColour(e->kind)));
            if (ImGui::TextLink(e->title.c_str())) openEvent(w, e->id);
            if (!e->kind.empty()) {
                ImGui::SameLine();
                chip(e->kind, colourFromHex(eventKindColour(e->kind)));
            }
            const std::string people = eventPeopleText(db, *e);
            ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
            std::string where = e->place;
            if (!e->chapter.empty()) where += (where.empty() ? "" : ", ") + e->chapter;
            if (!where.empty()) ImGui::TextWrapped("%s", where.c_str());
            if (!people.empty()) ImGui::TextWrapped("With %s", people.c_str());
            ImGui::PopStyleColor();
            if (!e->description.empty()) ImGui::TextWrapped("%s", e->description.c_str());
            ImGui::Spacing();
            ImGui::PopID();
        }
        for (const auto& [c, stage] : on[static_cast<size_t>(s.asOf)].stages) {
            statusDot(pathwayColour(db, stage->pathwayId));
            ImGui::TextWrapped("%s: %s%s", c->name.c_str(), stageLevelText(*stage, db).c_str(),
                               stage->label.empty() ? "" : (". " + stage->label).c_str());
        }
        ImGui::PopStyleVar();
    }
    ImGui::Spacing();
    heading(w, s.asOf < 0 ? "The Cast Now" : "The Cast as of " + storyDateText(date));
    if (db.characters.empty()) {
        ImGui::TextDisabled("No characters yet.");
        return;
    }
    if (stages == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("Everyone is shown as they are now. Give a character stages on their Stages tab to see them "
                           "change as you move through the story.");
        ImGui::PopStyleColor();
    }
    castGrid(w, date);
}

// ---------------------------------------------------------------- the event editor

void detailsTab(WindowState& w, Draft<StoryEvent>& draft) {
    StoryEvent& e = draft.value;
    draft.focusNameOnce();
    textField("Title", e.title, "required: what happened, in a few words");
    storyDateField("Date", e.date);
    presetField("Kind", e.kind, kEventKinds);
    textField("Place", e.place, "Backlund, the Sea of Fog ...");
    textField("Chapter", e.chapter, "where it is in your book (optional)");
    ImGui::Spacing();
    multilineField("What happened", e.description, 7);
    multilineField("Notes (for you)", e.notes, 4);
    (void)w;
}

void peopleTab(WindowState& w, StoryEvent& e) {
    const Database& db = w.app.db;
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("Who was there. Pick saved characters, or type anyone's name. Their sheet lists the event, and "
                       "the event shows each of them as they were on its date.");
    ImGui::PopStyleColor();
    int removeIndex = -1;
    if (!e.people.empty() && ImGui::BeginTable("people", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Then", ImGuiTableColumnFlags_WidthStretch, 2.5f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < e.people.size(); ++i) {
            EventPerson& person = e.people[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::InputTextWithHint("##name", "name", &person.name)) {
                // Typed by hand: linked again by name when saved.
                const Character* linked = db.findCharacter(person.characterId);
                if (linked && lower(linked->name) != lower(person.name)) person.characterId = 0;
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::BeginCombo("##pick", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft |
                                                         ImGuiComboFlags_HeightLarge)) {
                for (const Character& c : db.characters) {
                    if (ImGui::Selectable((c.name + "  (" + characterCode(c.id) + ")##" + std::to_string(c.id)).c_str(),
                                          c.id == person.characterId)) {
                        person.characterId = c.id;
                        person.name = c.name;
                    }
                }
                if (db.characters.empty()) ImGui::TextDisabled("No saved characters yet.");
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("Pick one of your saved characters");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##role", "role (optional)", &person.role);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if (const Character* c = db.findCharacter(person.characterId)) {
                const AsOf then = characterAsOf(*c, e.date, db);
                const Pathway* p = db.findPathway(then.character.pathwayId);
                const std::string level = then.character.pathwayId.empty()
                                              ? "Mortal"
                                              : levelName(then.character) + (p ? ", " + p->name : "");
                ImGui::TextColored(pathwayColour(db, then.character.pathwayId), "%s", level.c_str());
                if (then.stageIndex >= 0) ImGui::SetItemTooltip("From their stage: %s", stageSummary(c->stages[static_cast<size_t>(then.stageIndex)], db).c_str());
            } else {
                ImGui::TextDisabled("not a saved character");
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0))) removeIndex = static_cast<int>(i);
            ImGui::SetItemTooltip("Remove");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removeIndex >= 0) e.people.erase(e.people.begin() + removeIndex);
    if (ImGui::Button("+ Add a saved character")) ImGui::OpenPopup("pickPerson");
    if (ImGui::BeginPopup("pickPerson")) {
        static std::string search;
        if (const int id = savedCharacterList(w, search, 0)) {
            const bool already = std::any_of(e.people.begin(), e.people.end(), [&](const EventPerson& p) { return p.characterId == id; });
            if (const Character* c = db.findCharacter(id); c && !already) e.people.push_back({id, c->name, ""});
            search.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add someone by name")) e.people.push_back({0, "", ""});
}

void eventArtifactsTab(WindowState& w, StoryEvent& e) {
    if (w.app.db.artifacts.empty()) {
        ImGui::TextDisabled("No Sealed Artifacts exist yet. Create them in the Sealed Artifacts tab.");
        return;
    }
    ImGui::TextDisabled("Tick the Sealed Artifacts that played a part.");
    for (const auto& a : w.app.db.artifacts) {
        bool used = std::find(e.artifactIds.begin(), e.artifactIds.end(), a.id) != e.artifactIds.end();
        if (ImGui::Checkbox((a.name + "  (" + artifactCode(a.id) + ")##" + std::to_string(a.id)).c_str(), &used)) {
            if (used) e.artifactIds.push_back(a.id);
            else e.artifactIds.erase(std::remove(e.artifactIds.begin(), e.artifactIds.end(), a.id), e.artifactIds.end());
        }
        if (!a.ability.empty()) ImGui::SetItemTooltip("%s", a.ability.c_str());
    }
}

void eventEditor(WindowState& w) {
    TimelineScreen& s = w.timeline;
    auto& draft = s.draft;
    StoryEvent& e = draft.value;
    const Database& db = w.app.db;
    const Sheet sheet = buildEventSheet(e, db);

    // The top: a medallion in the kind's colour, the title, and the date and kind.
    const float font = ImGui::GetFontSize();
    const ImVec4 colour = colourFromHex(eventKindColour(e.kind));
    medallion(w, kindBadge(e.kind), colour, font * 2.1f);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    ImGui::TextUnformatted(e.title.empty() ? "(untitled event)" : e.title.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", draft.isNew ? "new" : eventCode(e.id).c_str());
    std::vector<std::pair<std::string, ImVec4>> chips;
    chips.push_back({e.date.empty() ? "No date yet" : storyDateText(e.date), e.date.empty() ? palette().muted : palette().gold});
    if (!e.kind.empty()) chips.push_back({e.kind, colour});
    if (!e.place.empty()) chips.push_back({e.place, palette().muted});
    if (!e.people.empty()) {
        chips.push_back({std::to_string(e.people.size()) + (e.people.size() == 1 ? " person" : " people"), palette().muted});
    }
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
            else if (const StoryEvent* saved = db.findEvent(e.id)) open(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, "event-" + lower(eventCode(e.id)), ExportFormat::Html, {sheet}, e.title); break;
        case EditorAction::ExportMarkdown:
            exportAndReport(w, "event-" + lower(eventCode(e.id)), ExportFormat::Markdown, {sheet}, e.title);
            break;
        case EditorAction::ExportText: exportAndReport(w, "event-" + lower(eventCode(e.id)), ExportFormat::Text, {sheet}, e.title); break;
        case EditorAction::Delete:
            try {
                const std::string title = e.title;
                deleteEvent(w.app, e.id);
                draft.close();
                setStatus(w, "Deleted " + title + ".");
            } catch (const StorageError& error) {
                setStatus(w, std::string("Deleting failed: ") + error.what(), true);
            }
            break;
        default: break;
    }
    if (!draft.open) return;

    ImGui::Spacing();
    if (ImGui::BeginTabBar("eventTabs")) {
        const char* tabs[] = {"Details", "People", "Sealed Artifacts", "Sheet"};
        for (int index = 0; index < 4; ++index) {
            if (!ImGui::BeginTabItem(tabs[index], nullptr, index == 0 ? draft.firstTabFlags() : 0)) continue;
            ImGui::BeginChild("tab", ImVec2(0, 0), ImGuiChildFlags_None);
            switch (index) {
                case 0: detailsTab(w, draft); break;
                case 1: peopleTab(w, e); break;
                case 2: eventArtifactsTab(w, e); break;
                default: drawSheet(w, sheet); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

}  // namespace

bool saveEventDraft(WindowState& w) { return saveImpl(w); }

void openEvent(WindowState& w, int id) {
    whenSaved(w, w.timeline.draft.dirty(), [&w] { return saveImpl(w); }, [&w, id] {
        if (const StoryEvent* found = w.app.db.findEvent(id)) open(w, *found, false);
        w.switchTo = Tab::Timeline;
    });
}

void openNewEvent(WindowState& w, const std::vector<int>& characterIds) {
    whenSaved(w, w.timeline.draft.dirty(), [&w] { return saveImpl(w); }, [&w, characterIds] {
        StoryEvent e;
        e.id = w.app.db.nextEventId();
        for (int id : characterIds) {
            if (const Character* c = w.app.db.findCharacter(id)) e.people.push_back({id, c->name, ""});
        }
        open(w, e, true);
        w.switchTo = Tab::Timeline;
    });
}

void drawTimelineScreen(WindowState& w) {
    TimelineScreen& s = w.timeline;
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    eventList(w);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("story", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (ImGui::BeginTabBar("timelineViews")) {
        if (ImGui::BeginTabItem("The Story")) {
            ImGui::BeginChild("storyView");
            storyView(w);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (s.draft.open) {
            const ImGuiTabItemFlags flags = s.selectEditor ? ImGuiTabItemFlags_SetSelected : 0;
            s.selectEditor = false;
            const std::string title = s.draft.value.title.empty() ? "New event" : s.draft.value.title;
            if (ImGui::BeginTabItem((title + "###event").c_str(), nullptr, flags)) {
                ImGui::BeginChild("eventEditor");
                eventEditor(w);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

}  // namespace lotm::window
