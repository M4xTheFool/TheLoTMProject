// Characters tab: the list on the left, the character being edited on the right.
#include <algorithm>
#include <cmath>
#include <optional>

#include "organizations.hpp"
#include "presets.hpp"
#include "records.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "storage.hpp"
#include "story.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

void removeEmpty(std::vector<std::string>& items) {
    items.erase(std::remove_if(items.begin(), items.end(), [](const std::string& s) { return ui::trim(s).empty(); }),
                items.end());
}

// Organization and rank, offering your own organizations and the ranks they list. Under it, a link to
// the organization, or to write up one your characters name that isn't saved yet.
void affiliationFields(WindowState& w, Affiliation& a) {
    const Database& db = w.app.db;
    presetField("Organization", a.organization, organizationSuggestions(db));
    if (a.organization.empty()) return;
    const std::vector<std::string> ranks = rankSuggestions(db, a.organization);
    if (ranks.empty()) textField("Rank or role", a.rank);
    else presetField("Rank or role", a.rank, ranks);
    if (isIndependent(a.organization)) return;
    fieldLabel("");
    if (const Organization* o = organizationNamed(db, a.organization)) {
        statusDot(organizationColour(db, *o));
        const int id = o->id;
        if (ImGui::TextLink(("Open " + o->name + " on the Organizations tab").c_str())) openOrganization(w, id);
    } else {
        const std::string name = ui::trim(a.organization);
        if (ImGui::TextLink(("Write up " + name + " as an organization").c_str())) openNewOrganization(w, name);
        ImGui::SetItemTooltip("A page of its own on the Organizations tab: leader, headquarters, ranks and members.");
    }
}

ImVec4 threatColour(const std::string& level) {
    if (level == "Low") return palette().success;
    if (level == "Moderate") return palette().gold;
    if (level == "High") return palette().warning;
    return palette().danger;
}

void open(WindowState& w, const Character& c, bool isNew) {
    Character copy = c;
    // Linked relationships show the other character's, or the organization's, current name.
    for (auto& r : copy.relationships) {
        if (const Character* other = w.app.db.findCharacter(r.characterId)) r.name = other->name;
        else if (const Organization* o = w.app.db.findOrganization(r.organizationId)) r.name = o->name;
    }
    // Three boxes to fill in to start with; a longer honorific name keeps all its lines.
    if (copy.honorificName.size() < 3) copy.honorificName.resize(3);
    w.characters.draft.start(copy, isNew);
    w.characters.editLadder = isMixedPathway(copy);
    w.characters.switchTo.clear();
    w.characters.lastRolls = {};
    w.characters.lastSpeedRoll.clear();
    w.characters.sheetStage = -1;
    w.characters.sheetEvent = 0;
    w.characters.openStage = -1;
    w.characters.surprise.open = false;
}

void openNew(WindowState& w) {
    Character c;
    c.id = w.app.db.nextCharacterId();
    c.stats.hpIncluded = w.app.db.settings.hpMode == "always";
    open(w, c, true);
}

bool saveImpl(WindowState& w) {
    auto& draft = w.characters.draft;
    Character& c = draft.value;
    c.name = ui::trim(c.name);
    if (c.name.empty()) {
        setStatus(w, "Give the character a name before saving (Identity tab).", true);
        return false;
    }
    if (w.app.db.settings.hpMode == "always") c.stats.hpIncluded = true;
    if (w.app.db.settings.hpMode == "never") c.stats.hpIncluded = false;
    removeEmpty(c.titles);
    removeEmpty(c.aliases);
    removeEmpty(c.honorificName);
    auto& rels = c.relationships;
    rels.erase(std::remove_if(rels.begin(), rels.end(), [](const Relationship& r) { return ui::trim(r.name).empty(); }),
               rels.end());
    linkRelationshipsByName(w.app.db, c);
    auto gone = [&](int id) { return !w.app.db.findArtifact(id); };  // a Sealed Artifact may have been deleted meanwhile
    auto& held = c.artifactIds;
    held.erase(std::remove_if(held.begin(), held.end(), gone), held.end());
    for (CharacterStage& stage : c.stages) {
        removeEmpty(stage.titles);
        removeEmpty(stage.aliases);
        stage.label = ui::trim(stage.label);
        stage.artifactIds.erase(std::remove_if(stage.artifactIds.begin(), stage.artifactIds.end(), gone), stage.artifactIds.end());
    }
    try {
        const CharacterSaveReport report = saveCharacter(w.app, c, draft.isNew);
        std::string text = "Saved " + c.name + " as " + characterCode(c.id) + ".";
        for (const auto& note : report.cleanups) text += " " + note;
        for (const auto& note : report.linked) text += " " + note;
        setStatus(w, text);
        open(w, c, false);
        return true;
    } catch (const StorageError& e) {
        setStatus(w, std::string("Saving failed: ") + e.what(), true);
        return false;
    }
}

// ---------------------------------------------------------------- tabs

// The top of the editor: a medallion with the initials in the pathway's colour, the name, and chips
// for the power tier, a mixed or dual pathway, the Sefirah, state of mind, a death, threat level and
// affiliation. Someone who has died has their medallion greyed, with a mourning band.
void header(WindowState& w, const Character& c, bool isNew, const Sheet& sheet) {
    const float font = ImGui::GetFontSize();
    const float dead = glide(ImGui::GetID("headerDead"), deadNow(c, w.app.db) ? 1.0f : 0.0f, 5.0f);
    CardMarks marks;
    for (int s = 9; isMixedPathway(c) && s >= c.sequence; --s) marks.ring.push_back(pathwayColour(w.app.db, pathwayIdAt(c, s)));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 centre(at.x + font * 2.1f, at.y + font * 2.1f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    medallion(w, initials(c.name.empty() ? "?" : c.name), mourned(pathwayColour(w.app.db, c.pathwayId), dead), font * 2.1f);
    if (marks.ring.size() > 1) {  // a mixed-pathway Beyonder: a thin ring of their pathways around it
        const float part = 6.2831853f / static_cast<float>(marks.ring.size());
        for (size_t i = 0; i < marks.ring.size(); ++i) {
            const float start = -1.5707963f + part * static_cast<float>(i);
            draw->PathArcTo(centre, font * 2.1f + 4.0f, start + 0.04f, start + part - 0.04f, 16);
            draw->PathStroke(ImGui::GetColorU32(mourned(marks.ring[i], dead)), ImDrawFlags_None, 3.0f);
        }
    }
    mourningBand(draw, centre, font * 2.1f, dead);
    ImGui::SameLine(0.0f, font * 0.9f);
    ImGui::BeginGroup();
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.9f);
    const SurpriseState& surprise = w.characters.surprise;
    if (c.name.empty()) ImGui::TextUnformatted("(unnamed)");
    else if (isNew && surprise.open) ImGui::TextUnformatted(settlingName(c.name, surprise.rolledAt[0], surprise.seed).c_str());
    else ImGui::TextUnformatted(c.name.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", isNew ? "new" : characterCode(c.id).c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s", sheet.subtitle.c_str());
    ImGui::PopStyleColor();

    std::vector<std::pair<std::string, ImVec4>> chips;
    for (const Badge& badge : sheet.badges) chips.push_back({badge.text, badgeColour(w, badge)});
    if (!c.affiliation.organization.empty()) {
        chips.push_back({c.affiliation.rank.empty() ? c.affiliation.organization
                                                    : c.affiliation.organization + ", " + c.affiliation.rank,
                         palette().muted});
    }
    if (c.stats.hpIncluded) chips.push_back({"HP " + std::to_string(c.stats.hp), palette().muted});
    chipRow(chips);
    ImGui::EndGroup();
    ImGui::Spacing();
}

// Alive or dead: two steps like the state of mind's, Alive in green and Dead in grey; click one to pick it.
// Your own words go in the box below. For someone dead by this setting, when and how they died; for
// someone dead by a stage or a story event, where it says so.
void lifeSection(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    ImGui::SeparatorText("Alive or dead");
    const Death death = deathOf(c, db);
    const bool ownDeath = isDead(c.life);
    Character before = c;  // without this setting: what their stages and story events say
    before.life.clear();
    const Death elsewhere = ownDeath ? Death{} : deathOf(before, db);
    const int picked = death.dead ? 1 : (c.life.empty() || ui::toLower(c.life) == "alive") ? 0 : -1;

    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    const float width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    const float height = ImGui::GetFrameHeight() * 1.25f;
    const float rounding = ImGui::GetStyle().FrameRounding;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (int i = 0; i < 2; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, gap);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("life", ImVec2(width, height))) {
            // Alive for someone a stage or story event has dead means they came back: it is said in so many words.
            if (i == 0) c.life = elsewhere.dead ? "Alive" : "";
            else c.life = "Dead";
        }
        const bool hovered = ImGui::IsItemHovered();
        const float lit = glide(ImGui::GetID("lit"), picked == i ? 0.9f : hovered ? 0.25f : 0.12f, 12.0f);
        ImGui::PopID();
        const ImVec4 colour = i == 0 ? palette().success : deathColour();
        const ImVec2 end(at.x + width, at.y + height);
        draw->AddRectFilled(at, end, colourU32(colour, lit), rounding);
        draw->AddRect(at, end, colourU32(colour, picked == i ? 1.0f : 0.5f), rounding);
        const char* label = kLifeStates[static_cast<size_t>(i)].c_str();
        const ImVec2 size = ImGui::CalcTextSize(label);
        const ImU32 text = lit > 0.5f ? ImGui::GetColorU32(ImGuiCol_ChildBg) : colourU32(colour);
        draw->AddText(ImVec2(at.x + (width - size.x) * 0.5f, at.y + (height - size.y) * 0.5f), text, label);
    }
    std::string own = picked < 0 ? c.life : "";
    fieldLabel("In your own words");
    if (ImGui::InputTextWithHint("##ownLife", "or in your own words: Revived, A wandering spirit ...", &own)) c.life = own;
    if (ownDeath) {
        storyDateField("Died on", c.diedOn, "when, so the Timeline shows them alive before it");
        textField("How they died", c.lifeNote, "killed by Amon, lost at sea ...");
    } else if (death.dead) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        if (death.eventId != 0) ImGui::TextWrapped("%s, as the event's People tab says.", deathText(death, db).c_str());
        else ImGui::TextWrapped("%s, in their stage on the Stages tab.", deathText(death, db).c_str());
        ImGui::PopStyleColor();
        if (death.eventId != 0 && ImGui::TextLink("Open the event")) openEvent(w, death.eventId);
    } else {
        if (elsewhere.dead) {
            ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
            ImGui::TextWrapped("%s, and is alive now, as this says.", deathText(elsewhere, db).c_str());
            ImGui::PopStyleColor();
        }
        if (!c.life.empty()) textField("More about it", c.lifeNote, "how they came back ...");
    }
}

void identityTab(WindowState& w, Draft<Character>& draft) {
    Character& c = draft.value;
    draft.focusNameOnce();
    textField("Name", c.name, "required");
    textField("Age", c.age);
    textField("Gender", c.gender);
    lifeSection(w, c);
    ImGui::Spacing();
    multilineField("Short description", c.shortDescription, 3);
    multilineField("Backstory", c.backstory, 8);
}

void looksTab(Character& c) {
    Appearance& a = c.appearance;
    ImGui::TextDisabled("Pick from the arrow lists, or type anything you like.");
    presetField("Hair colour", a.hair, kHair);
    presetField("Hair length", a.hairLength, kHairLength);
    presetField("Hair style", a.hairStyle, kHairStyle);
    presetField("Eye colour", a.eyes, kEyes);
    presetField("Skin", a.skin, kSkin);
    presetField("Face", a.face, kFace);
    presetField("Build", a.build, kBuild);
    presetField("Height", a.height, kHeight);
    presetField("Voice", a.voice, kVoice);
    presetField("Clothing", a.clothing, kClothing);
    presetField("Distinguishing mark", a.distinguishingMark, kMarks);
    ImGui::Spacing();
    multilineField("Their look in your own words", a.description, 5);
}

// Checkboxes for the pathway's Sequence 0 powers a Uniqueness grants, plus powers typed in by hand.
void uniquenessPowers(const char* id, const Pathway* p, std::vector<std::string>& held) {
    ImGui::PushID(id);
    ImGui::TextUnformatted("Sequence 0 powers it grants");
    const SequenceInfo* god = p ? p->findSequence(0) : nullptr;
    std::vector<std::string> offered = god ? god->abilities : std::vector<std::string>{};
    for (const auto& ability : offered) {
        bool on = std::find(held.begin(), held.end(), ability) != held.end();
        const std::string name = splitAbility(ability).first;
        if (ImGui::Checkbox(((name.empty() ? ability : name) + "##" + ability).c_str(), &on)) {
            if (on) held.push_back(ability);
            else held.erase(std::remove(held.begin(), held.end(), ability), held.end());
        }
        ImGui::SetItemTooltip("%s", ability.c_str());
    }
    // Powers typed in by hand: everything picked that isn't one of the listed ones.
    std::vector<std::string> own;
    for (const auto& a : held) {
        if (std::find(offered.begin(), offered.end(), a) == offered.end()) own.push_back(a);
    }
    ImGui::TextDisabled("Your own, written as \"Name: what it does\":");
    if (listEditor("ownUniqueness", own, "add a power of your own")) {
        std::vector<std::string> picked;
        for (const auto& a : held) {
            if (std::find(offered.begin(), offered.end(), a) != offered.end()) picked.push_back(a);
        }
        picked.insert(picked.end(), own.begin(), own.end());
        held = picked;
    }
    ImGui::PopID();
}

// A Sequence 0 god's second pathway: its Sequence 1 characteristic(s) and, usually, its Uniqueness.
void secondPathwaySection(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    ImGui::SeparatorText("Second pathway (dual-pathway god)");
    if (!dualPathwayEligible(c)) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("%s", c.aboveSequence
                                     ? "Above the Sequence they hold every Uniqueness of their group, so there is no "
                                       "second pathway to add."
                                     : "A Sequence 0 god can take a second pathway: its Sequence 1 Beyonder characteristic, "
                                       "usually its Uniqueness, and every ability of that pathway. Move the slider to "
                                       "Sequence 0 to choose one.");
        ImGui::PopStyleColor();
        return;
    }
    SecondPathway& second = c.secondPathway;
    std::string picked = second.pathwayId;
    if (pathwayCombo("Second pathway", db, picked, "None (one pathway)") && picked != second.pathwayId) {
        if (picked == c.pathwayId) {
            setStatus(w, "The second pathway has to be a different one from their own.", true);
        } else {
            second = SecondPathway{};
            second.pathwayId = picked;
        }
    }
    if (second.empty()) {
        ImGui::TextDisabled("Amon, Sequence 0 Error, took the Door Uniqueness and a Key of Stars characteristic this way.");
        return;
    }
    const Pathway* other = db.findPathway(second.pathwayId);
    const auto group = db.pathwayGroup(c.pathwayId);
    if (other && std::find(group.begin(), group.end(), other) == group.end()) {
        ImGui::TextColored(palette().warning, "Not a neighbour of their pathway: canon dual-pathway gods combine "
                                              "neighbouring pathways.");
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Characteristics");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    ImGui::RadioButton("One##second", &second.beyonderCharacteristics, 1);
    ImGui::SameLine();
    ImGui::RadioButton(("Two of " + sequenceLabel(other, 1)).c_str(), &second.beyonderCharacteristics, 2);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("They have every ability of the %s pathway, Sequence 9 to 0.",
                       other ? other->name.c_str() : "second");
    ImGui::PopStyleColor();
    ImGui::Checkbox("Holds its Uniqueness too", &second.hasUniqueness);
    if (!second.hasUniqueness) return;
    ImGui::Indent();
    if (other && !other->uniqueness.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("The %s Uniqueness: %s", other->name.c_str(), other->uniqueness.c_str());
        ImGui::PopStyleColor();
        if (second.uniquenessForm != other->uniqueness && ImGui::SmallButton("Start from this description##second")) {
            second.uniquenessForm = other->uniqueness;
        }
    }
    multilineField("What that Uniqueness looks like and the shape it takes", second.uniquenessForm, 3);
    ImGui::Unindent();
}

// Five steps from Sane to Lost control, coloured green to red: click one to pick it, again to clear it.
// A state of mind in your own words goes in the box below.
void stateOfMindSection(Character& c, bool risky) {
    ImGui::SeparatorText("State of mind");
    if (risky && c.stateOfMind.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().warning);
        ImGui::TextWrapped("They switched to a pathway that isn't a neighbour, which tends to cost a Beyonder their "
                           "sanity. How are they holding up?");
        ImGui::PopStyleColor();
    }
    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    const int count = static_cast<int>(kStatesOfMind.size());
    const float width = (ImGui::GetContentRegionAvail().x - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    const float height = ImGui::GetFrameHeight() * 1.25f;
    const int level = stateOfMindLevel(c.stateOfMind);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (int i = 0; i < count; ++i) {
        const std::string& state = kStatesOfMind[static_cast<size_t>(i)];
        if (i > 0) ImGui::SameLine(0.0f, gap);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("mind", ImVec2(width, height))) c.stateOfMind = level == i ? "" : state;
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec4 colour = stateOfMindColour(state);
        const ImVec2 end(at.x + width, at.y + height);
        const bool on = level == i;
        const float rounding = ImGui::GetStyle().FrameRounding;
        draw->AddRectFilled(at, end, ImGui::GetColorU32(ImVec4(colour.x, colour.y, colour.z, on ? 0.9f : hovered ? 0.25f : 0.12f)),
                            rounding);
        draw->AddRect(at, end, ImGui::GetColorU32(ImVec4(colour.x, colour.y, colour.z, on ? 1.0f : 0.5f)), rounding);
        const ImVec2 size = ImGui::CalcTextSize(state.c_str());
        const ImU32 text = on ? ImGui::GetColorU32(ImGuiCol_ChildBg) : ImGui::GetColorU32(colour);
        ImGui::PushClipRect(at, end, true);
        draw->AddText(ImVec2(at.x + std::max(4.0f, (width - size.x) * 0.5f), at.y + (height - size.y) * 0.5f), text,
                      state.c_str());
        ImGui::PopClipRect();
    }
    std::string own = level < 0 ? c.stateOfMind : "";
    fieldLabel("In your own words");
    if (ImGui::InputTextWithHint("##ownMind", "or describe a state of mind of your own", &own)) c.stateOfMind = own;
    textField("More about it", c.stateOfMindNote, "what shook them, how it shows");
}

// Which pathway each Sequence came from: the switches so far, "Switch to another pathway", and a
// pathway box for every Sequence they hold.
void switchesSection(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    auto& screen = w.characters;
    ImGui::SeparatorText("Pathway switches");
    const auto switches = pathwaySwitches(c, db);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    if (switches.empty()) {
        ImGui::TextWrapped("Every Sequence came from the %s pathway. A Beyonder can switch on the way up: to a "
                           "neighbouring pathway safely, to any other at a cost to their sanity.",
                           db.findPathway(c.pathwayId) ? db.findPathway(c.pathwayId)->name.c_str() : "same");
    }
    ImGui::PopStyleColor();
    for (const PathwaySwitch& change : switches) {
        statusDot(change.neighbouring ? palette().success : palette().danger);
        ImGui::TextWrapped("Sequence %d: from %s to %s. %s", change.atSequence,
                           pathwayLabel(db, change.fromPathwayId).c_str(), pathwayLabel(db, change.toPathwayId).c_str(),
                           change.neighbouring ? "Neighbouring pathways, a safe switch."
                                               : "Not neighbours: a risky switch that tends to cost sanity.");
    }
    if (!switches.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("Each Sequence gives the abilities of its own pathway; stat bonuses and the speed grade "
                           "follow the pathway they are on now.");
        ImGui::PopStyleColor();
    }

    const bool canSwitch = c.sequence > 0 && !c.aboveSequence;
    ImGui::BeginDisabled(!canSwitch);
    if (ImGui::Button("Switch to another pathway...")) {
        screen.switchTo.clear();
        ImGui::OpenPopup("switchPathway");
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("%s", canSwitch ? "They drink the next Sequence's potion of another pathway."
                                          : "A Sequence 0 god takes a second pathway instead (below).");
    ImGui::SameLine();
    if (ImGui::Button(screen.editLadder ? "Hide the pathway of each Sequence" : "Choose the pathway of each Sequence")) {
        screen.editLadder = !screen.editLadder;
    }
    if (isMixedPathway(c)) {
        ImGui::SameLine();
        if (ImGui::Button("Make them one pathway again")) c.mixedSequences.clear();
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, 0.0f));
    if (ImGui::BeginPopup("switchPathway")) {
        ImGui::TextWrapped("They drink the Sequence %d potion of another pathway. Sequences 9 to %d keep the pathways "
                           "they came from.",
                           c.sequence - 1, c.sequence);
        pathwayCombo("New pathway", db, screen.switchTo, "Pick a pathway");
        const bool same = screen.switchTo == c.pathwayId;
        if (same) {
            ImGui::TextColored(palette().muted, "That is their pathway already.");
        } else if (!screen.switchTo.empty()) {
            const Pathway* next = db.findPathway(screen.switchTo);
            const SequenceInfo* info = next ? next->findSequence(c.sequence - 1) : nullptr;
            ImGui::TextWrapped("They become Sequence %d%s%s of the %s pathway.", c.sequence - 1, info ? " " : "",
                               info ? info->name.c_str() : "", next ? next->name.c_str() : "new");
            if (!neighbouringPathways(db, c.pathwayId, screen.switchTo)) {
                ImGui::TextColored(palette().danger, "Not a neighbour: a risky switch that tends to cost sanity.");
            } else {
                ImGui::TextColored(palette().success, "A neighbouring pathway: a safe switch.");
            }
        }
        ImGui::BeginDisabled(screen.switchTo.empty() || same);
        if (ImGui::Button("Switch")) {
            switchPathway(c, screen.switchTo);
            screen.switchTo.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!screen.editLadder) return;
    ImGui::Spacing();
    if (c.sequence >= 9) {
        ImGui::TextDisabled("Sequence 9 is their only Sequence, and it comes from the pathway they are on now.");
        return;
    }
    ImGui::TextDisabled("Sequences weaker than theirs can come from another pathway.");
    for (int sequence = 9; sequence > c.sequence; --sequence) {
        std::string picked = pathwayIdAt(c, sequence);
        const std::string label = "Sequence " + std::to_string(sequence);
        if (pathwayCombo(label.c_str(), db, picked, "Their pathway now")) {
            auto& steps = c.mixedSequences;
            steps.erase(std::remove_if(steps.begin(), steps.end(),
                                       [&](const SequenceStep& step) { return step.sequence == sequence; }),
                        steps.end());
            if (!picked.empty() && picked != c.pathwayId) steps.push_back({sequence, picked});
            std::sort(steps.begin(), steps.end(),
                      [](const SequenceStep& a, const SequenceStep& b) { return a.sequence > b.sequence; });
        }
    }
    fieldLabel(("Sequence " + std::to_string(c.sequence)).c_str());
    ImGui::TextDisabled("%s, their pathway now (change it at the top)", pathwayLabel(db, c.pathwayId).c_str());
}

void pathwayTab(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    pathwayCombo("Pathway", db, c.pathwayId, "No pathway (ordinary mortal)");
    const Pathway* p = db.findPathway(c.pathwayId);
    if (!p) {
        ImGui::Spacing();
        ImGui::TextDisabled("Ordinary mortals have no Sequence. Choose a pathway to give them one.");
        stateOfMindSection(c, false);
        return;
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Sequence");
    const bool wasAbove = c.aboveSequence;
    sequenceSlider("##sequence", p, c.sequence, &c.aboveSequence);
    const Sefirah* groupSefirah = db.sefirahForPathway(c.pathwayId);
    if (c.aboveSequence && !wasAbove && c.sefirahId.empty() && groupSefirah) c.sefirahId = groupSefirah->id;

    ImGui::Spacing();
    const auto bonus = statBonuses(c, p);
    std::string bonuses;
    for (int i = 0; i < kStatCount; ++i) {
        if (bonus[i] == 0) continue;
        if (!bonuses.empty()) bonuses += ", ";
        bonuses += std::string(kStatCodes[i]) + " " + signedNumber(bonus[i]);
    }
    const SpeedTier tier = speedTierOf(c);
    ImGui::TextWrapped("Speed tier %d, %s. Speed grade %s. Stat bonuses at this Sequence: %s.", tier.tier,
                       tier.name.c_str(), p->speedGrade.c_str(), bonuses.empty() ? "none" : bonuses.c_str());

    ImGui::SeparatorText("Sequence ladder");
    sequenceLadder(w, ladderRungs(c, db));

    if (c.aboveSequence) {
        ImGui::SeparatorText("Above the Sequence");
        const auto group = db.pathwayGroup(c.pathwayId);
        std::string names;
        for (size_t i = 0; i < group.size(); ++i) {
            names += (i == 0 ? "" : i + 1 == group.size() ? " and " : ", ") + group[i]->name;
        }
        ImGui::TextWrapped("A Great Old One: past Sequence 0, holding %s and every Uniqueness of the %s %s, with "
                           "every ability of those pathways.",
                           groupSefirah ? groupSefirah->name.c_str() : "their group's Sefirah", names.c_str(),
                           group.size() == 1 ? "pathway" : "pathways");
        const Sefirah* goo = greatOldOneSefirah(c, db);
        if (goo && !goo->greatOldOne.empty()) {
            ImGui::TextColored(palette().gold, "%s", goo->greatOldOne.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("the Great Old One of %s", goo->name.c_str());
        }
        if (goo && c.sefirahId != goo->id) {
            ImGui::TextColored(palette().warning, "They don't hold %s.", goo->name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Give them " + goo->name).c_str())) c.sefirahId = goo->id;
        }
        ImGui::SeparatorText((aboveSequenceLabel(goo) + " adds").c_str());
        if (!goo) {
            ImGui::TextDisabled("No Sefirah gathers this pathway yet, so it has no Great Old One.");
            ImGui::SameLine();
            if (ImGui::TextLink("Make one")) openNewSefirah(w, c.pathwayId);
        } else {
            if (!goo->symbolism.empty()) {
                ImGui::TextColored(palette().muted, "Symbolism");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", goo->symbolism.c_str());
            }
            if (goo->abilities.empty()) ImGui::TextDisabled("No Great Old One abilities written yet.");
            for (const auto& a : goo->abilities) {
                ImGui::Bullet();
                ImGui::TextWrapped("%s", a.c_str());
            }
            ImGui::TextDisabled("%s", goo->custom ? "They are written on the Sefirah, in the Sefirot tab."
                                                  : "Copy the Sefirah into one of your own to write your own.");
            ImGui::SameLine();
            if (ImGui::TextLink(("Open " + goo->name).c_str())) openSefirah(w, goo->id);
        }
    } else if (const SequenceInfo* s = p->findSequence(c.sequence)) {
        ImGui::SeparatorText((sequenceLabel(p, c.sequence) + " adds").c_str());
        if (s->abilities.empty()) ImGui::TextDisabled("No abilities written for this Sequence yet.");
        for (const auto& a : s->abilities) ImGui::BulletText("%s", a.c_str());
    }
    switchesSection(w, c);
    stateOfMindSection(c, riskySwitch(c, db));

    ImGui::Spacing();
    const std::string header = "Every ability they have (" + std::to_string(abilityCount(c, db)) + ")";
    if (ImGui::CollapsingHeader(header.c_str())) {
        const bool mixed = isMixedPathway(c);
        for (const AbilityGroup& group : abilityGroups(c, db)) {
            // In the pathway's colour when more than one pathway is involved.
            const bool tinted = (mixed || !group.summary.empty()) && !group.pathwayId.empty();
            ImGui::TextColored(tinted ? pathwayColour(db, group.pathwayId) : palette().gold, "%s", group.heading.c_str());
            if (!group.summary.empty()) ImGui::TextWrapped("%s.", group.summary.c_str());
            if (!group.symbolism.empty()) ImGui::TextWrapped("Symbolism: %s", group.symbolism.c_str());
            for (const auto& a : group.abilities) ImGui::BulletText("%s", a.c_str());
        }
    }

    secondPathwaySection(w, c);
}

void uniquenessSection(WindowState& w, Character& c) {
    const Pathway* p = w.app.db.findPathway(c.pathwayId);
    ImGui::SeparatorText("Beyonder characteristics and the Uniqueness");
    if (absorbedUniqueness(c)) {
        ImGui::TextWrapped("%s", c.aboveSequence
                                     ? "Above the Sequence they have absorbed every Uniqueness of their pathway group."
                                     : "At Sequence 0 the Uniqueness has been absorbed, so every Sequence 0 power is "
                                       "theirs.");
        multilineField("How the absorbed Uniqueness looks and shows itself now (optional)", c.uniquenessForm, 4);
        return;
    }
    if (!sequenceOneChoices(c)) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("A second Beyonder characteristic and the Uniqueness (what it looks like and the powers it "
                           "grants) come at Sequence 1. Move the Sequence slider in the Pathway tab to 1 to describe "
                           "them.");
        ImGui::PopStyleColor();
        return;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Characteristics");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    ImGui::RadioButton("One##characteristics", &c.beyonderCharacteristics, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Two Beyonder characteristics", &c.beyonderCharacteristics, 2);
    ImGui::Checkbox("Holds the pathway's Uniqueness", &c.hasUniqueness);
    if (!c.hasUniqueness) return;

    ImGui::Indent();
    if (p && !p->uniqueness.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("This pathway's Uniqueness: %s", p->uniqueness.c_str());
        ImGui::PopStyleColor();
        if (c.uniquenessForm != p->uniqueness && ImGui::SmallButton("Start from this description")) {
            c.uniquenessForm = p->uniqueness;
        }
    }
    multilineField("What the Uniqueness looks like and what shape it takes", c.uniquenessForm, 5);

    ImGui::Spacing();
    uniquenessPowers("own", p, c.uniquenessAbilities);
    ImGui::Unindent();
}

// The honorific name's lines: a box each with buttons to move it up and remove it, and "+ Add a line".
void honorificEditor(std::vector<std::string>& lines) {
    static constexpr const char* kHints[] = {"The ... who ...", "The ... of ...", "The ... that ..."};
    int moveUp = -1, remove = -1;
    const ImVec2 square(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
    const float inner = ImGui::GetStyle().ItemInnerSpacing.x;
    for (size_t i = 0; i < lines.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        fieldLabel(("Line " + std::to_string(i + 1)).c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - square.x * 2.0f - inner * 2.0f);
        ImGui::InputTextWithHint("##line", i < 3 ? kHints[i] : "The ...", &lines[i]);
        ImGui::SameLine(0.0f, inner);
        ImGui::BeginDisabled(i == 0);
        if (ImGui::Button("^", square)) moveUp = static_cast<int>(i);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Move up");
        ImGui::SameLine(0.0f, inner);
        if (ImGui::Button("x", square)) remove = static_cast<int>(i);
        ImGui::SetItemTooltip("Remove this line");
        ImGui::PopID();
    }
    if (moveUp > 0) std::swap(lines[static_cast<size_t>(moveUp)], lines[static_cast<size_t>(moveUp) - 1]);
    if (remove >= 0) lines.erase(lines.begin() + remove);
    if (ImGui::Button("+ Add a line")) lines.push_back("");
    const auto written = std::count_if(lines.begin(), lines.end(), [](const std::string& l) { return !ui::trim(l).empty(); });
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%d %s written", static_cast<int>(written), written == 1 ? "line" : "lines");
}

void customizationTab(WindowState& w, Character& c) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Alignment");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##alignment", c.alignment.empty() ? "Unaligned" : c.alignment.c_str())) {
        if (ImGui::Selectable("Unaligned", c.alignment.empty())) c.alignment.clear();
        for (const auto& a : kAlignments) {
            if (ImGui::Selectable(a.c_str(), a == c.alignment)) c.alignment = a;
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Titles");
    listEditor("titles", c.titles, "add a title");
    ImGui::SeparatorText("Aliases");
    listEditor("aliases", c.aliases, "add an alias");

    ImGui::SeparatorText("Honorific name");
    if (honorificEligible(c)) {
        ImGui::TextDisabled("Lines that, when spoken, reach the character. Usually three, but it can be longer.");
        honorificEditor(c.honorificName);
    } else {
        ImGui::TextDisabled("Available from Sequence 3 upward.");
    }

    uniquenessSection(w, c);

    ImGui::SeparatorText("Sefirah");
    sefirahCombo("Holds", w.app.db, c.sefirahId, "None");
    ImGui::TextDisabled("A source of power that gathers a pathway group's Uniquenesses. See the Sefirot tab.");
}

void statsTab(WindowState& w, Character& c) {
    auto& s = w.characters;
    const Pathway* p = w.app.db.findPathway(c.pathwayId);
    const auto bonus = statBonuses(c, p);
    const auto totals = statTotals(c, p);

    ImGui::TextDisabled("Drag a slider, or Roll for 4d6 with the lowest die dropped. Roll again as often as you like.");
    // Check rolls a d20 check with the stat on the Dice tab, as they are here (unsaved changes too).
    const bool canCheck = !w.characters.draft.isNew;
    auto checkButton = [&](int kind, const char* id) {
        ImGui::BeginDisabled(!canCheck);
        if (ImGui::Button(id)) openCheck(w, c.id, kind);
        ImGui::EndDisabled();
        if (!canCheck) {
            ImGui::SetItemTooltip("Save them first, then roll checks for them on the Dice tab.");
        } else {
            CheckSetup setup;
            setup.kind = kind;
            ImGui::SetItemTooltip("Roll a %s check on the Dice tab: d20 %s", checkName(kind).c_str(),
                                  signedNumber(checkBonus(c, w.app.db, setup)).c_str());
        }
    };
    if (ImGui::BeginTable("stats", 7, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Stat");
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("");
        ImGui::TableSetupColumn("Bonus");
        ImGui::TableSetupColumn("Total");
        ImGui::TableSetupColumn("Mod");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (int i = 0; i < kStatCount; ++i) {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(kStatNames[i]);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##base", &c.stats.base[i], 1, 30);
            if (!s.lastRolls[i].empty()) ImGui::SetItemTooltip("Last roll: %s", s.lastRolls[i].c_str());
            ImGui::TableNextColumn();
            if (ImGui::Button("Roll")) {
                const FourD6Roll roll = w.app.dice.roll4d6DropLowest();
                c.stats.base[i] = roll.total;
                s.lastRolls[i] = roll.describe();
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(bonus[i] == 0 ? "-" : signedNumber(bonus[i]).c_str());
            ImGui::TableNextColumn();
            // The total counts its way to a new roll instead of jumping.
            ImGui::Text("%d", static_cast<int>(std::lround(glide(ImGui::GetID("total"), static_cast<float>(totals[i]), 9.0f))));
            ImGui::TableNextColumn();
            ImGui::TextColored(palette().gold, "%s", signedNumber(abilityModifier(totals[i])).c_str());
            ImGui::TableNextColumn();
            checkButton(i, "Check");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Roll all six")) {
        for (int i = 0; i < kStatCount; ++i) {
            const FourD6Roll roll = w.app.dice.roll4d6DropLowest();
            c.stats.base[i] = roll.total;
            s.lastRolls[i] = roll.describe();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Point buy: all to 8")) c.stats.base = {8, 8, 8, 8, 8, 8};
    int spent = 0;
    bool pointBuy = true;
    for (int score : c.stats.base) {
        const int cost = pointBuyCost(score);
        if (cost < 0) pointBuy = false;
        spent += cost;
    }
    if (pointBuy) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(spent > kPointBuyBudget ? palette().danger : palette().muted, "Point buy: %d of %d points used", spent,
                           kPointBuyBudget);
    }

    ImGui::SeparatorText("Speed");
    ImGui::TextDisabled("The Sequence sets the tier; this score ranks them inside it.");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Roll Check").x -
                            ImGui::GetStyle().FramePadding.x * 4.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f);
    ImGui::SliderInt("##speed", &c.stats.speedBase, 1, 30, "Speed %d");
    ImGui::SameLine();
    if (ImGui::Button("Roll##speed")) {
        const FourD6Roll roll = w.app.dice.roll4d6DropLowest();
        c.stats.speedBase = roll.total;
        s.lastSpeedRoll = roll.describe();
    }
    if (!s.lastSpeedRoll.empty()) ImGui::SetItemTooltip("Last roll: %s", s.lastSpeedRoll.c_str());
    ImGui::SameLine();
    checkButton(kSpeedCheck, "Check##speed");
    const SpeedTier tier = speedTierOf(c);
    ImGui::TextWrapped("Tier %d %s, score %d. %s", tier.tier, tier.name.c_str(), effectiveSpeed(c, p),
                       tier.description.c_str());

    ImGui::SeparatorText("HP and Spirituality");
    const std::string& mode = w.app.db.settings.hpMode;
    if (mode == "ask") {
        ImGui::Checkbox("Include HP for this character", &c.stats.hpIncluded);
    } else {
        c.stats.hpIncluded = mode == "always";
        ImGui::TextDisabled(mode == "always" ? "HP is always included (change it in Settings)."
                                            : "HP is turned off (change it in Settings).");
    }
    if (c.stats.hpIncluded) {
        const int suggested = suggestedHp(c, p);
        fieldLabel("HP");
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
        if (ImGui::InputInt("##hp", &c.stats.hp)) c.stats.hp = std::clamp(c.stats.hp, 0, 999999);
        ImGui::SameLine();
        if (ImGui::Button(("Use suggested " + std::to_string(suggested)).c_str())) c.stats.hp = suggested;
    }
    const int suggestedSp = suggestedSpirituality(c, p);
    fieldLabel("Spirituality");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    if (ImGui::InputInt("##spirituality", &c.stats.spirituality)) {
        c.stats.spirituality = std::clamp(c.stats.spirituality, 0, 999999);
    }
    ImGui::SameLine();
    if (ImGui::Button(("Use suggested " + std::to_string(suggestedSp) + "##sp").c_str())) {
        c.stats.spirituality = suggestedSp;
    }
    helpMarker("The suggestion grows with the speed tier, plus the Wisdom modifier for every Sequence climbed.");
}

void artifactsTab(WindowState& w, Character& c) {
    if (w.app.db.artifacts.empty()) {
        ImGui::TextDisabled("No Sealed Artifacts exist yet. Create them in the Sealed Artifacts tab.");
        return;
    }
    ImGui::TextDisabled("Tick the Sealed Artifacts this character holds.");
    for (const auto& a : w.app.db.artifacts) {
        bool held = std::find(c.artifactIds.begin(), c.artifactIds.end(), a.id) != c.artifactIds.end();
        std::string text = a.name + "  (" + artifactCode(a.id);
        if (!a.pathwayId.empty()) text += ", " + pathwayLabel(w.app.db, a.pathwayId);
        text += ")##" + std::to_string(a.id);
        if (ImGui::Checkbox(text.c_str(), &held)) {
            if (held) c.artifactIds.push_back(a.id);
            else c.artifactIds.erase(std::remove(c.artifactIds.begin(), c.artifactIds.end(), a.id), c.artifactIds.end());
        }
        if (!a.ability.empty()) ImGui::SetItemTooltip("%s", a.ability.c_str());
    }
}

// One of your organizations to pick, under a search box: a dot in its colour and its member count.
// Returns its id, or 0.
int organizationList(const WindowState& w, std::string& search) {
    const Database& db = w.app.db;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##searchOrganizations", "search", &search);
    int picked = 0;
    for (const Organization* o : organizationsByName(db)) {
        if (!search.empty() && ui::toLower(o->name).find(ui::toLower(search)) == std::string::npos) continue;
        statusDot(organizationColour(db, *o));
        const size_t members = membersOf(*o, db).size();
        const std::string item = o->name + "  (" + std::to_string(members) + (members == 1 ? " member)" : " members)") +
                                 "##org" + std::to_string(o->id);
        if (ImGui::Selectable(item.c_str())) picked = o->id;
    }
    if (db.organizations.empty()) ImGui::TextDisabled("No organizations yet. Write one up on the Organizations tab.");
    return picked;
}

// Relationships that reach them through an organization, either way round: theirs with a whole
// organization (to each member), and others' with an organization they belong to. Each one can be given a
// relationship of its own instead.
void tiesSection(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    const std::vector<Tie> ties = tiesThroughOrganizations(db, c);
    if (ties.empty()) return;
    ImGui::SeparatorText("Through organizations");
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("A relationship with a whole organization reaches each of its members. To change it for one "
                       "of them, give the two of them a relationship of their own, which counts instead.");
    ImGui::PopStyleColor();
    int openId = 0;
    std::optional<Relationship> add;
    const Organization* group = nullptr;
    bool groupOutgoing = false;
    for (const Tie& tie : ties) {
        if (tie.organization != group || tie.outgoing != groupOutgoing) {
            group = tie.organization;
            groupOutgoing = tie.outgoing;
            ImGui::Spacing();
            statusDot(organizationColour(db, *group));
            ImGui::PushFont(w.fonts.bold, 0.0f);
            if (tie.outgoing) ImGui::TextWrapped("Their relationship with the %s reaches its members", group->name.c_str());
            else ImGui::TextWrapped("Through the %s, which they belong to", group->name.c_str());
            ImGui::PopFont();
        }
        const std::string key = "tie" + std::to_string(tie.organization->id) + (tie.outgoing ? "o" : "i") +
                                std::to_string(tie.other->id);
        ImGui::PushID(key.c_str());
        // Each one fades in the first time it is shown.
        const float shown = glideFrom(ImGui::GetID("shown"), 0.0f, 1.0f, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * shown);
        ImGui::Indent();
        const bool dead = deadNow(*tie.other, db);
        statusDot(dead ? deathColour() : pathwayColour(db, tie.other->pathwayId));
        if (ImGui::TextLink(tie.other->name.c_str())) openId = tie.other->id;
        if (dead) ImGui::SetItemTooltip("%s", deathText(deathOf(*tie.other, db), db).c_str());
        ImGui::SameLine();
        if (tie.overridden) {
            // What counts instead: their own relationship, as this character sees them.
            std::string own;
            for (const Relationship& r : c.relationships) {
                if (r.characterId == tie.other->id) own = r.type;
            }
            for (const Relationship& r : tie.other->relationships) {
                if (own.empty() && r.characterId == c.id) own = reciprocalType(r.type);
            }
            if (ui::toLower(own) == ui::toLower(tie.type)) ImGui::TextDisabled("%s, by a relationship of their own", own.c_str());
            else ImGui::TextDisabled("%s, by a relationship of their own, instead of %s", own.c_str(), tie.type.c_str());
        } else {
            ImGui::TextUnformatted(tie.type.c_str());
            if (!tie.note.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", tie.note.c_str());
            }
            ImGui::SameLine();
            const std::string first = tie.other->name.substr(0, tie.other->name.find(' '));
            if (ImGui::SmallButton(("Change for " + first).c_str())) {
                add = Relationship{tie.other->id, tie.other->name, tie.type, "", 0};
            }
            ImGui::SetItemTooltip("Gives the two of them a relationship of their own, starting as %s. Change it in the "
                                  "list above; it counts instead of the one through the %s.",
                                  tie.type.c_str(), tie.organization->name.c_str());
        }
        ImGui::Unindent();
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
    if (add) c.relationships.push_back(*add);
    if (openId != 0) openCharacter(w, openId);
}

void relationshipsTab(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    affiliationFields(w, c.affiliation);

    ImGui::SeparatorText("Relationships");
    ImGui::TextWrapped("Pick one of your saved characters or organizations, or type anyone's name. Saved characters are "
                       "linked when you save, and they get the relationship back. A relationship with an organization "
                       "reaches all of its members.");
    int removeIndex = -1;
    if (!c.relationships.empty() &&
        ImGui::BeginTable("relationships", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Relationship", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < c.relationships.size(); ++i) {
            Relationship& r = c.relationships[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // An organization, picked or named: a dot in its colour in front.
            const Organization* organization = db.findOrganization(r.organizationId);
            const auto matches = db.findCharactersByName(r.name, c.id);
            const bool willLink = matches.size() == 1 && ui::toLower(matches[0]->name) == ui::toLower(r.name);
            if (!organization && !willLink && r.characterId == 0) organization = organizationNamed(db, r.name);
            if (organization) statusDot(organizationColour(db, *organization));
            const float pickWidth = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - pickWidth - ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::InputTextWithHint("##name", "name", &r.name) && organization && !namesOrganization(*organization, r.name)) {
                r.organizationId = 0;  // typed someone else: linked again by name when saved
            }
            if (organization) {
                const size_t members = membersOf(*organization, db).size();
                ImGui::SetItemTooltip("The organization %s: this reaches its %zu %s", organizationCode(organization->id).c_str(),
                                      members, members == 1 ? "member" : "members");
            } else if (willLink) {
                ImGui::SetItemTooltip("Linked to the saved character %s", characterCode(matches[0]->id).c_str());
            }
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::BeginCombo("##pick", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft |
                                                         ImGuiComboFlags_HeightLarge)) {
                for (const auto& other : db.characters) {
                    if (other.id == c.id) continue;
                    const std::string item = other.name + "  (" + characterCode(other.id) + ")##" + std::to_string(other.id);
                    if (ImGui::Selectable(item.c_str(), other.id == r.characterId)) {
                        r.characterId = other.id;
                        r.organizationId = 0;
                        r.name = other.name;
                    }
                }
                if (db.characters.size() <= 1) ImGui::TextDisabled("No other saved characters yet.");
                if (!db.organizations.empty()) {
                    ImGui::SeparatorText("Organizations");
                    for (const Organization* o : organizationsByName(db)) {
                        statusDot(organizationColour(db, *o));
                        const std::string item = o->name + "  (" + organizationCode(o->id) + ")##org" + std::to_string(o->id);
                        if (ImGui::Selectable(item.c_str(), o->id == r.organizationId)) {
                            r.characterId = 0;
                            r.organizationId = o->id;
                            r.name = o->name;
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("Pick one of your saved characters or organizations");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##type", r.type.c_str(), ImGuiComboFlags_HeightLarge)) {
                for (const auto& type : kRelationshipTypes) {
                    if (ImGui::Selectable(type.c_str(), type == r.type)) r.type = type;
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##note", organization ? "every member" : willLink ? "linked to a saved character" : "note (optional)",
                                     &r.note);
            ImGui::TableNextColumn();
            if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0))) removeIndex = static_cast<int>(i);
            ImGui::SetItemTooltip("Remove (also from the other character when you save)");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removeIndex >= 0) c.relationships.erase(c.relationships.begin() + removeIndex);
    if (ImGui::Button("+ Add a saved character")) ImGui::OpenPopup("pickCharacter");
    if (ImGui::BeginPopup("pickCharacter")) {
        static std::string search;
        if (const int id = savedCharacterList(w, search, c.id)) {
            if (const Character* other = db.findCharacter(id)) c.relationships.push_back({id, other->name, "Friend", "", 0});
            search.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add an organization")) ImGui::OpenPopup("pickOrganization");
    ImGui::SetItemTooltip("A relationship with the whole organization: an enemy of it is an enemy of every member.");
    if (ImGui::BeginPopup("pickOrganization")) {
        static std::string search;
        if (const int id = organizationList(w, search)) {
            if (const Organization* o = db.findOrganization(id)) c.relationships.push_back({0, o->name, "Enemy", "", id});
            search.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add someone by name")) c.relationships.push_back({0, "", "Friend", "", 0});
    tiesSection(w, c);
}

void dossierTab(const WindowState& w, Character& c) {
    Dossier& d = c.dossier;
    ImGui::TextDisabled("An official file on the character, as the Nighthawks or another agency would keep it.");
    presetField("Filed by", d.filedBy, keeperSuggestions(w.app.db));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Recent actions");
    ImGui::SameLine(ImGui::GetFontSize() * 9.5f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##recent", d.recentActions.empty() ? "Nothing on file" : d.recentActions.c_str())) {
        if (ImGui::Selectable("Nothing on file", d.recentActions.empty())) d.recentActions.clear();
        for (const auto& a : kRecentActions) {
            if (ImGui::Selectable(a.c_str(), a == d.recentActions)) d.recentActions = a;
        }
        ImGui::EndCombo();
    }
    textField("What they did", d.recentActionsNote);

    ImGui::SeparatorText("Threat level");
    const ThreatAssessment threat = assessThreat(c);
    ImGui::TextColored(threatColour(threat.level), "%s", threat.level.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", threat.explanation.c_str());
    bool byHand = !d.threatLevel.empty();
    if (ImGui::RadioButton("Automatic", !byHand)) d.threatLevel.clear();
    ImGui::SameLine();
    if (ImGui::RadioButton("Set by hand", byHand) && !byHand) d.threatLevel = threat.level;
    if (!d.threatLevel.empty()) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        if (ImGui::BeginCombo("##threat", d.threatLevel.c_str())) {
            for (const auto& level : kThreatLevels) {
                if (ImGui::Selectable(level.c_str(), level == d.threatLevel)) d.threatLevel = level;
            }
            ImGui::EndCombo();
        }
    }
    helpMarker("Sequence: mortal 0, Seq 9-8 1, 7-5 3, 4-3 5, 2-1 7, Seq 0 10. Recent actions: peaceful -1 up to "
               "mass casualties +2. Sealed Artifacts: one or two +1, three or more +2. Up to 2 is Low, 3-4 Moderate, "
               "5-6 High, 7-8 Extreme, 9+ Catastrophic.");

    ImGui::Spacing();
    presetField("Status", d.status, kStatuses);
    textField("Last seen", d.lastSeen);
    multilineField("Remarks", d.remarks, 4);
}

}  // namespace

void stageFields(WindowState& w, const Character& c, CharacterStage& stage, const std::string& key) {
    const Database& db = w.app.db;
    textField("What changed", stage.label, "joins the Tarot Club, advances to Sequence 7 ...");
    pathwayCombo("Pathway", db, stage.pathwayId, "No pathway (ordinary mortal)");
    if (const Pathway* p = db.findPathway(stage.pathwayId)) {
        ImGui::TextUnformatted("Sequence");
        sequenceSlider("##stageSequence", p, stage.sequence, &stage.aboveSequence);
    }
    // At Sequence 1 the characteristics and the Uniqueness (a King of Angels), at Sequence 0 the absorbed
    // Uniqueness and a second pathway: the Pathway tab's own sections, on the stage.
    Character level = c;
    putStageLevel(level, stage);
    if (sequenceOneChoices(level) || absorbedUniqueness(level)) uniquenessSection(w, level);
    if (dualPathwayEligible(level)) secondPathwaySection(w, level);
    takeStageLevel(stage, level);
    if (isKingOfAngels(level)) ImGui::TextColored(palette().gold, "A King of Angels then.");

    ImGui::SeparatorText("Then");
    presetField("State of mind", stage.stateOfMind, kStatesOfMind);
    presetField("Alive or dead", stage.life, kLifeStates);
    if (isDead(stage.life)) ImGui::SetItemTooltip("From this stage's date on they are dead, until a later stage says otherwise.");
    affiliationFields(w, stage.affiliation);
    presetField("Status", stage.status, kStatuses);
    sefirahCombo("Sefirah", db, stage.sefirahId, "None");
    ImGui::SeparatorText("Titles then");
    listEditor(("stageTitles" + key).c_str(), stage.titles, "add a title");
    ImGui::SeparatorText("Aliases then");
    listEditor(("stageAliases" + key).c_str(), stage.aliases, "add an alias");
    if (!db.artifacts.empty()) {
        ImGui::SeparatorText("Sealed Artifacts held then");
        for (const auto& a : db.artifacts) {
            bool held = std::find(stage.artifactIds.begin(), stage.artifactIds.end(), a.id) != stage.artifactIds.end();
            if (ImGui::Checkbox((a.name + "##stageArtifact" + std::to_string(a.id)).c_str(), &held)) {
                auto& ids = stage.artifactIds;
                if (held) ids.push_back(a.id);
                else ids.erase(std::remove(ids.begin(), ids.end(), a.id), ids.end());
            }
        }
    }
    ImGui::Spacing();
    textField("Note", stage.note, "anything else about this time");
}

namespace {

// Their death, on the Stages tab: the same as Alive or dead on the Identity tab, with when and how; or
// where a stage or story event says it.
void deathBox(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    ImGui::SeparatorText("Their death");
    const Death death = deathOf(c, db);
    // The box opens out a little when they die.
    const float open = glide(ImGui::GetID("deathOpen"), death.dead ? 1.0f : 0.0f, 9.0f);
    if (isDead(c.life)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.35f + 0.65f * open));
        statusDot(deathColour());
        ImGui::TextColored(deathColour(), "%s", deathText(death, db).c_str());
        storyDateField("Died on", c.diedOn, "when, so the Timeline shows them alive before it");
        textField("How they died", c.lifeNote, "killed by Amon, lost at sea ...");
        ImGui::PopStyleVar();
        if (ImGui::SmallButton("They didn't die after all")) {
            c.life.clear();
            c.diedOn = StoryDate{};
        }
        return;
    }
    if (death.dead) {
        statusDot(deathColour());
        ImGui::TextColored(deathColour(), "%s", deathText(death, db).c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped(death.eventId != 0 ? "As the event's People tab says." : "As their stage above says.");
        ImGui::PopStyleColor();
        if (death.eventId != 0 && ImGui::SmallButton("Open the event")) openEvent(w, death.eventId);
        if (death.stageIndex >= 0) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Show the stage")) w.characters.openStage = death.stageIndex;
        }
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("%s. When they die, mark it here or on the Identity tab, or at a story event with + Dies here on "
                       "its People tab.", c.life.empty() ? "Alive" : c.life.c_str());
    ImGui::PopStyleColor();
    if (ImGui::Button("+ They die")) c.life = "Dead";
    ImGui::SetItemTooltip("Marks them dead now. Give the date they died, so the Timeline and their sheet as of an "
                          "earlier date show them alive.");
}

// Stages: how they were earlier in the story, each from a date on. Everything a stage doesn't hold is
// as they are now.
void stagesTab(WindowState& w, Draft<Character>& draft) {
    auto& screen = w.characters;
    const Database& db = w.app.db;
    Character& c = draft.value;
    ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
    ImGui::TextWrapped("A stage is how they were earlier in your story, from a date on: their pathway and Sequence, "
                       "state of mind, organization, status, titles, aliases and Sealed Artifacts. Everything else is "
                       "as they are now. The Timeline tab and their sheet use stages to show them as they were on any "
                       "date.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    storyDateField("Now since", c.nowSince, "optional: the date they became as the other tabs describe them");
    ImGui::SetItemTooltip("From this date on, they are as the other tabs describe them. Leave it empty when now is "
                          "simply the end of your story.");
    ImGui::Spacing();
    if (ImGui::Button("+ Add a stage from how they are now")) {
        c.stages.push_back(stageFromNow(c));
        screen.openStage = static_cast<int>(c.stages.size()) - 1;
    }
    ImGui::SetItemTooltip("Copies their Sequence, organization and the rest. Then give it the date it began and change "
                          "what was different back then.");

    int remove = -1;
    for (size_t i = 0; i < c.stages.size(); ++i) {
        CharacterStage& stage = c.stages[i];
        ImGui::PushID(static_cast<int>(i));
        if (screen.openStage == static_cast<int>(i)) {
            ImGui::SetNextItemOpen(true);
            screen.openStage = -1;
        }
        const std::string title = (stage.date.empty() ? std::string("No date yet") : "From " + storyDateText(stage.date)) +
                                  ": " + stageLevelText(stage, db) + (stage.label.empty() ? "" : ". " + stage.label) +
                                  "###stage";
        ImGui::PushStyleColor(ImGuiCol_Text, isDead(stage.life) ? deathColour() : pathwayColour(db, stage.pathwayId));
        const bool open = ImGui::CollapsingHeader(title.c_str());
        ImGui::PopStyleColor();
        if (open) {
            ImGui::Indent();
            storyDateField("From", stage.date);
            if (stage.date.empty()) {
                ImGui::TextColored(palette().warning, "Give the stage the date it began; without one it isn't used.");
            }
            stageFields(w, c, stage, "own" + std::to_string(i));
            if (ImGui::Button("See their sheet as of then")) {
                screen.sheetStage = static_cast<int>(i);
                screen.sheetEvent = 0;
                screen.selectSheet = true;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, palette().danger);
            if (ImGui::Button("Remove this stage")) remove = static_cast<int>(i);
            ImGui::PopStyleColor();
            ImGui::Unindent();
            ImGui::Spacing();
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        c.stages.erase(c.stages.begin() + remove);
        if (screen.sheetStage == remove) screen.sheetStage = -1;
        else if (screen.sheetStage > remove) --screen.sheetStage;
    }

    deathBox(w, c);

    // What changed for them at story events: made and changed on the event's People tab.
    std::vector<StageRef> atEvents;
    if (!draft.isNew) {
        for (StageRef& ref : stagesOf(c, db)) {
            if (ref.eventId != 0) atEvents.push_back(std::move(ref));
        }
    }
    if (!atEvents.empty()) {
        ImGui::SeparatorText("Changes at story events");
        ImGui::PushStyleColor(ImGuiCol_Text, palette().muted);
        ImGui::TextWrapped("These come from the People tab of each event, where you change them. They count like stages, "
                           "from the event's date on.");
        ImGui::PopStyleColor();
        for (const StageRef& ref : atEvents) {
            ImGui::PushID(ref.eventId);
            statusDot(pathwayColour(db, ref.stage.pathwayId));
            ImGui::TextWrapped("%s", stageSummary(ref.stage, db).c_str());
            if (ImGui::SmallButton("Open the event")) openEvent(w, ref.eventId);
            ImGui::SameLine();
            if (ImGui::SmallButton("See their sheet as of then")) {
                screen.sheetStage = -1;
                screen.sheetEvent = ref.eventId;
                screen.selectSheet = true;
            }
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Their story events");
    if (draft.isNew) {
        ImGui::TextDisabled("Save them first; then they can be added to story events on the Timeline tab.");
        return;
    }
    const auto events = eventsOf(db, c.id);
    if (events.empty()) ImGui::TextDisabled("None yet.");
    for (const StoryEvent* e : events) {
        ImGui::PushID(e->id);
        statusDot(colourFromHex(eventKindColour(e->kind)));
        if (ImGui::TextLink(e->title.c_str())) openEvent(w, e->id);
        const std::string summary = eventSummary(*e);
        if (!summary.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", summary.c_str());
        }
        ImGui::PopID();
    }
    if (ImGui::Button("+ New story event with them")) openNewEvent(w, {c.id});
}

// The sheet, as they are now or as they were in one of their stages (their own, or a change at a story
// event).
void sheetTab(WindowState& w, const Character& c, const Sheet& now) {
    auto& screen = w.characters;
    const Database& db = w.app.db;
    const std::vector<StageRef> stages = stagesOf(c, db);
    auto isShown = [&](const StageRef& ref) {
        return ref.eventId != 0 ? ref.eventId == screen.sheetEvent : ref.ownIndex == screen.sheetStage;
    };
    const auto shown = std::find_if(stages.begin(), stages.end(), isShown);
    if (shown == stages.end()) {
        screen.sheetStage = -1;
        screen.sheetEvent = 0;
    }
    if (!stages.empty()) {
        fieldLabel("Show them");
        const std::string preview = shown == stages.end() ? std::string("As they are now") : stageSummary(shown->stage, db);
        if (ImGui::BeginCombo("##sheetStage", preview.c_str())) {
            if (ImGui::Selectable("As they are now", shown == stages.end())) {
                screen.sheetStage = -1;
                screen.sheetEvent = 0;
            }
            for (size_t i = 0; i < stages.size(); ++i) {
                const std::string label = stageSummary(stages[i].stage, db) + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), isShown(stages[i]))) {
                    screen.sheetStage = stages[i].ownIndex;
                    screen.sheetEvent = stages[i].eventId;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing();
    }
    if (shown == stages.end()) {
        drawSheet(w, now);
        return;
    }
    const CharacterStage& stage = shown->stage;
    drawSheet(w, stage.date.empty() ? buildCharacterSheet(characterInStage(c, stage, db), db)
                                    : buildCharacterSheetAsOf(c, stage.date, db));
}

}  // namespace

bool saveCharacterDraft(WindowState& w) { return saveImpl(w); }

void openSurprise(WindowState& w, bool another) {
    auto& g = w.characters.surprise;
    Character c = generateCharacter(w.app.db, g.options, w.app.dice);
    c.id = w.app.db.nextCharacterId();
    c.stats.hpIncluded = w.app.db.settings.hpMode == "always";
    if (c.stats.hpIncluded) c.stats.hp = suggestedHp(c, w.app.db.findPathway(c.pathwayId));
    open(w, c, true);
    w.characters.draft.focusName = false;  // the Sheet tab, to see them whole
    w.characters.selectSheet = true;
    g.open = true;
    g.made = another ? g.made + 1 : 0;
    // Each part's line fades in a moment after the one above it.
    const double now = ImGui::GetTime();
    for (size_t i = 0; i < kGeneratorPartCount; ++i) {
        g.rolledAt[i] = now + 0.07 * static_cast<double>(i);
        g.faces[i] = w.app.dice.roll(6);
    }
    g.everythingAt = now;
    g.everythingFace = w.app.dice.roll(6);
    ++g.seed;
}

void openCharacter(WindowState& w, int id) {
    whenSaved(w, w.characters.draft.dirty(), [&w] { return saveImpl(w); }, [&w, id] {
        if (const Character* found = w.app.db.findCharacter(id)) open(w, *found, false);
        w.switchTo = Tab::Characters;
    });
}

void reloadCharacterDraft(WindowState& w) {
    const auto& draft = w.characters.draft;
    if (!draft.open || draft.isNew || draft.dirty()) return;
    if (const Character* saved = w.app.db.findCharacter(draft.value.id)) open(w, *saved, false);
}

void drawCharactersScreen(WindowState& w) {
    auto& s = w.characters;
    auto& draft = s.draft;
    auto saveThis = [&w] { return saveImpl(w); };

    // ---- the list
    ImGui::BeginChild("list", ImVec2(ImGui::GetFontSize() * 17.0f, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    {
        // "+ New character" and "Surprise me" side by side, or one above the other in a narrow list.
        const ImGuiStyle& style = ImGui::GetStyle();
        const float room = ImGui::GetContentRegionAvail().x;
        const float plain = ImGui::CalcTextSize("+ New character").x + style.FramePadding.x * 2.0f;
        const float surprise = ImGui::CalcTextSize("Surprise me").x + style.FramePadding.x * 2.0f +
                               ImGui::GetFrameHeight() * 0.6f + style.ItemInnerSpacing.x;
        const bool together = plain + surprise + style.ItemSpacing.x <= room;
        const float extra = together ? (room - plain - surprise - style.ItemSpacing.x) * 0.5f : 0.0f;
        if (ImGui::Button("+ New character", ImVec2(together ? plain + extra : -FLT_MIN, 0))) {
            whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w); });
        }
        if (together) ImGui::SameLine();
        if (dieButton(w, "##surpriseMe", "Surprise me", together ? surprise + extra : room, s.surprise.everythingAt,
                      s.surprise.everythingFace, s.surprise.seed)) {
            whenSaved(w, draft.dirty(), saveThis, [&w] { openSurprise(w); });
        }
        ImGui::SetItemTooltip("A random Beyonder in one click: pathway, Sequence, stats, looks, personality and a name.\n"
                              "Roll any part again, then save them like any other character.");
    }
    const auto rows = filterCharacters(w.app.db, s.filter);
    {
        std::vector<std::string> names;
        for (const auto& c : w.app.db.characters) names.push_back(c.name);
        listFilterControls(w, s.filter, "Search names, aliases or IDs", characterPowerBands(), names, rows.size(), w.app.db.characters.size());
    }
    ImGui::Separator();
    if (draft.open && draft.isNew) {
        listCard(w, "new", true, pathwayColour(w.app.db, draft.value.pathwayId), initials(draft.value.name),
                 draft.value.name.empty() ? "(new character)" : draft.value.name, "Not saved yet");
    }
    if (w.app.db.characters.empty()) ImGui::TextDisabled("No characters yet.");
    else if (rows.empty()) ImGui::TextDisabled("None match the filters.");
    for (const Character* row : rows) {
        const Character& c = *row;
        const bool selected = draft.open && !draft.isNew && draft.value.id == c.id;
        const Pathway* pathway = w.app.db.findPathway(c.pathwayId);
        // The medallion shows the Sequence; the line below names it and the pathway.
        std::string subtitle = "Mortal";
        if (c.aboveSequence && !c.pathwayId.empty()) {
            subtitle = "Above the Sequence" + (pathway ? ", " + pathway->name : "");
        } else if (!c.pathwayId.empty()) {
            const SequenceInfo* seq = pathway ? pathway->findSequence(c.sequence) : nullptr;
            subtitle = (seq && !seq->name.empty() ? seq->name : "Sequence " + std::to_string(c.sequence)) +
                       (pathway ? ", " + pathway->name : "");
        }
        // A mixed-pathway Beyonder's ring shows each Sequence's pathway; a dot shows a shaken state of mind.
        CardMarks marks;
        const auto earlier = earlierPathwayIds(c);
        if (!earlier.empty()) {
            const Pathway* first = w.app.db.findPathway(earlier.front());
            subtitle += " (from " + (first ? first->name : earlier.front()) + (earlier.size() > 1 ? " and more)" : ")");
            for (int s = 9; s >= c.sequence; --s) marks.ring.push_back(pathwayColour(w.app.db, pathwayIdAt(c, s)));
        }
        if (!c.stateOfMind.empty() && stateOfMindLevel(c.stateOfMind) != 0) {
            marks.dot = true;
            marks.dotColour = stateOfMindColour(c.stateOfMind);
        }
        const Death death = deathOf(c, w.app.db);
        if (death.dead) {
            marks.dead = true;
            subtitle = (death.date.empty() ? std::string("Dead") : "Died " + storyDateText(death.date)) + " \xC2\xB7 " + subtitle;
        }
        // The badge is the Sequence; Above the Sequence it is an infinity sign.
        const std::string badge =
            c.pathwayId.empty() ? "-" : c.aboveSequence ? "\xE2\x88\x9E" : std::to_string(c.sequence);
        if (listCard(w, "c" + std::to_string(c.id), selected, pathwayColour(w.app.db, c.pathwayId), badge, c.name,
                     subtitle, marks) &&
            !selected) {
            const int id = c.id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                if (const Character* found = w.app.db.findCharacter(id)) open(w, *found, false);
            });
        }
        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted((characterCode(c.id) + ", " +
                                    (c.pathwayId.empty() ? std::string("ordinary mortal")
                                     : c.aboveSequence   ? std::string("Above the Sequence")
                                                         : sequenceLabel(pathway, c.sequence)))
                                       .c_str());
            for (const PathwaySwitch& change : pathwaySwitches(c, w.app.db)) {
                ImGui::TextColored(change.neighbouring ? palette().muted : palette().danger, "Switched at Sequence %d from %s",
                                   change.atSequence, pathwayLabel(w.app.db, change.fromPathwayId).c_str());
            }
            if (!c.stateOfMind.empty()) ImGui::TextColored(stateOfMindColour(c.stateOfMind), "%s", c.stateOfMind.c_str());
            if (death.dead) ImGui::TextColored(deathColour(), "%s", deathText(death, w.app.db).c_str());
            if (!c.shortDescription.empty()) ImGui::TextDisabled("%s", c.shortDescription.c_str());
            ImGui::EndTooltip();
        }
    }
    ImGui::Spacing();
    if (ImGui::TextLink("Import the sample sets (the Tarot Club and more)")) {
        w.switchTo = Tab::Settings;
        w.showSampleSets = true;
    }
    ImGui::SetItemTooltip("Ready-made characters and Sealed Artifacts, under Sample sets in Settings.");
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- the editor
    ImGui::BeginChild("editor", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (!draft.open) {
        ImGui::Spacing();
        ImGui::TextDisabled("Pick a character on the left, or create a new one.");
        ImGui::EndChild();
        return;
    }
    Character& c = draft.value;
    const Sheet sheet = buildCharacterSheet(c, w.app.db);
    header(w, c, draft.isNew, sheet);
    EditorButtons buttons;
    buttons.dirty = draft.dirty();
    buttons.isNew = draft.isNew;
    buttons.canDuplicate = true;
    switch (drawEditorButtons(buttons)) {
        case EditorAction::Save: saveImpl(w); break;
        case EditorAction::Revert:
            if (draft.isNew) draft.close();
            else if (const Character* saved = w.app.db.findCharacter(c.id)) open(w, *saved, false);
            break;
        case EditorAction::ExportHtml: exportAndReport(w, exportName(c), ExportFormat::Html, {sheet}, c.name); break;
        case EditorAction::ExportMarkdown:
            exportAndReport(w, exportName(c), ExportFormat::Markdown, {sheet}, c.name);
            break;
        case EditorAction::ExportText: exportAndReport(w, exportName(c), ExportFormat::Text, {sheet}, c.name); break;
        case EditorAction::Duplicate: {
            const int id = c.id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                try {
                    const int copyId = duplicateCharacter(w.app, id);
                    if (const Character* copy = w.app.db.findCharacter(copyId)) open(w, *copy, false);
                    setStatus(w, "Saved a copy as " + characterCode(copyId) + ".");
                } catch (const StorageError& e) {
                    setStatus(w, std::string("Copying failed: ") + e.what(), true);
                }
            });
            break;
        }
        case EditorAction::Delete:
            try {
                const std::string name = c.name;
                deleteCharacter(w.app, c.id);
                draft.close();
                setStatus(w, "Deleted " + name + ".");
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
    if (draft.isNew && s.surprise.open) {
        ImGui::Spacing();
        if (drawSurprisePanel(w) && saveImpl(w)) {
            openSurprise(w, true);
            if (s.surprise.made > 1) setStatus(w, w.status + " " + std::to_string(s.surprise.made) + " saved in a row.");
        }
    }

    ImGui::Spacing();
    // Eleven tabs: a little less room around each name, so they fit without being cut short. The tab
    // bar is laid out in BeginTabBar and EndTabBar as well as in each tab, so all of them get it.
    const ImVec2 tabPadding(ImGui::GetFontSize() * 0.32f, ImGui::GetStyle().FramePadding.y);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, tabPadding);
    const bool tabBar = ImGui::BeginTabBar("characterTabs");
    ImGui::PopStyleVar();
    if (tabBar) {
        const std::pair<const char*, int> tabs[] = {{"Identity", 0},      {"Looks", 1},    {"Pathway", 2},
                                                     {"Customization", 3}, {"Stats", 4},    {"Artifacts", 5},
                                                     {"Relationships", 6}, {"Stages", 10},  {"Dossier", 7},
                                                     {"Notes", 8},         {"Sheet", 9}};
        for (const auto& [name, index] : tabs) {
            ImGuiTabItemFlags flags = index == 0 ? draft.firstTabFlags() : 0;
            if (index == 9 && s.selectSheet) {
                flags |= ImGuiTabItemFlags_SetSelected;
                s.selectSheet = false;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, tabPadding);
            const bool open = ImGui::BeginTabItem(name, nullptr, flags);
            ImGui::PopStyleVar();
            if (!open) continue;
            ImGui::BeginChild("tab", ImVec2(0, 0), ImGuiChildFlags_None);
            switch (index) {
                case 0: identityTab(w, draft); break;
                case 1: looksTab(c); break;
                case 2: pathwayTab(w, c); break;
                case 3: customizationTab(w, c); break;
                case 4: statsTab(w, c); break;
                case 5: artifactsTab(w, c); break;
                case 6: relationshipsTab(w, c); break;
                case 7: dossierTab(w, c); break;
                case 8: multilineField("Anything else worth remembering", c.notes, 14); break;
                case 10: stagesTab(w, draft); break;
                default: sheetTab(w, c, sheet); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, tabPadding);
        ImGui::EndTabBar();
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
}

}  // namespace lotm::window
