// Characters tab: the list on the left, the character being edited on the right.
#include <algorithm>

#include "presets.hpp"
#include "records.hpp"
#include "relations.hpp"
#include "rules.hpp"
#include "storage.hpp"
#include "ui.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

bool containsIgnoreCase(const std::string& text, const std::string& part) {
    return ui::toLower(text).find(ui::toLower(part)) != std::string::npos;
}

void removeEmpty(std::vector<std::string>& items) {
    items.erase(std::remove_if(items.begin(), items.end(), [](const std::string& s) { return ui::trim(s).empty(); }),
                items.end());
}

ImVec4 threatColour(const std::string& level) {
    if (level == "Low") return kSuccess;
    if (level == "Moderate") return kGold;
    if (level == "High") return kWarning;
    return kDanger;
}

void open(WindowState& w, const Character& c, bool isNew) {
    Character copy = c;
    // Linked relationships show the other character's current name.
    for (auto& r : copy.relationships) {
        if (const Character* other = w.app.db.findCharacter(r.characterId)) r.name = other->name;
    }
    copy.honorificName.resize(3);
    w.characters.draft.start(copy, isNew);
    w.characters.lastRolls = {};
    w.characters.lastSpeedRoll.clear();
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
    auto& held = c.artifactIds;  // a Sealed Artifact may have been deleted meanwhile
    held.erase(std::remove_if(held.begin(), held.end(), [&](int id) { return !w.app.db.findArtifact(id); }), held.end());
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

void identityTab(Draft<Character>& draft) {
    Character& c = draft.value;
    draft.focusNameOnce();
    textField("Name", c.name, "required");
    textField("Age", c.age);
    textField("Gender", c.gender);
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

void pathwayTab(WindowState& w, Character& c) {
    const Database& db = w.app.db;
    pathwayCombo("Pathway", db, c.pathwayId, "No pathway (ordinary mortal)");
    const Pathway* p = db.findPathway(c.pathwayId);
    if (!p) {
        ImGui::Spacing();
        ImGui::TextDisabled("Ordinary mortals have no Sequence. Choose a pathway to give them one.");
        return;
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Sequence");
    sequenceSlider("##sequence", p, c.sequence);

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

    if (const SequenceInfo* s = p->findSequence(c.sequence)) {
        ImGui::SeparatorText((sequenceLabel(p, c.sequence) + " adds").c_str());
        if (s->abilities.empty()) ImGui::TextDisabled("No abilities written for this Sequence yet.");
        for (const auto& a : s->abilities) ImGui::BulletText("%s", a.c_str());
    }
    const auto all = abilitiesUpTo(*p, c.sequence);
    size_t count = 0;
    for (const SequenceInfo* s : all) count += s->abilities.size();
    const std::string header = "Every ability from Sequence 9 down (" + std::to_string(count) + ")";
    if (ImGui::CollapsingHeader(header.c_str())) {
        for (const SequenceInfo* s : all) {
            ImGui::TextColored(kGold, "%s", sequenceLabel(p, s->sequence).c_str());
            for (const auto& a : s->abilities) ImGui::BulletText("%s", a.c_str());
        }
    }
}

void uniquenessSection(WindowState& w, Character& c) {
    const Pathway* p = w.app.db.findPathway(c.pathwayId);
    ImGui::SeparatorText("Beyonder characteristics and the Uniqueness");
    if (absorbedUniqueness(c)) {
        ImGui::TextWrapped("At Sequence 0 the Uniqueness has been absorbed, so every Sequence 0 power is theirs.");
        multilineField("How the absorbed Uniqueness looks and shows itself now (optional)", c.uniquenessForm, 4);
        return;
    }
    if (!sequenceOneChoices(c)) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
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
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("This pathway's Uniqueness: %s", p->uniqueness.c_str());
        ImGui::PopStyleColor();
        if (c.uniquenessForm != p->uniqueness && ImGui::SmallButton("Start from this description")) {
            c.uniquenessForm = p->uniqueness;
        }
    }
    multilineField("What the Uniqueness looks like and what shape it takes", c.uniquenessForm, 5);

    ImGui::Spacing();
    ImGui::TextUnformatted("Sequence 0 powers it grants");
    const SequenceInfo* god = p ? p->findSequence(0) : nullptr;
    std::vector<std::string> offered = god ? god->abilities : std::vector<std::string>{};
    for (const auto& ability : offered) {
        bool held = std::find(c.uniquenessAbilities.begin(), c.uniquenessAbilities.end(), ability) !=
                    c.uniquenessAbilities.end();
        const std::string name = splitAbility(ability).first;
        if (ImGui::Checkbox(((name.empty() ? ability : name) + "##" + ability).c_str(), &held)) {
            auto& list = c.uniquenessAbilities;
            if (held) list.push_back(ability);
            else list.erase(std::remove(list.begin(), list.end(), ability), list.end());
        }
        ImGui::SetItemTooltip("%s", ability.c_str());
    }
    // Powers typed in by hand: everything picked that isn't one of the listed ones.
    std::vector<std::string> own;
    for (const auto& a : c.uniquenessAbilities) {
        if (std::find(offered.begin(), offered.end(), a) == offered.end()) own.push_back(a);
    }
    ImGui::TextDisabled("Your own, written as \"Name: what it does\":");
    if (listEditor("ownUniqueness", own, "add a power of your own")) {
        std::vector<std::string> picked;
        for (const auto& a : c.uniquenessAbilities) {
            if (std::find(offered.begin(), offered.end(), a) != offered.end()) picked.push_back(a);
        }
        picked.insert(picked.end(), own.begin(), own.end());
        c.uniquenessAbilities = picked;
    }
    ImGui::Unindent();
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
        ImGui::TextDisabled("Three lines that, when spoken, reach the character.");
        c.honorificName.resize(3);
        textField("Line 1", c.honorificName[0], "The ... who ...");
        textField("Line 2", c.honorificName[1], "The ... of ...");
        textField("Line 3", c.honorificName[2], "The ... that ...");
    } else {
        ImGui::TextDisabled("Available from Sequence 3 upward.");
    }

    uniquenessSection(w, c);
}

void statsTab(WindowState& w, Character& c) {
    auto& s = w.characters;
    const Pathway* p = w.app.db.findPathway(c.pathwayId);
    const auto bonus = statBonuses(c, p);
    const auto totals = statTotals(c, p);

    ImGui::TextDisabled("Drag a slider, or Roll for 4d6 with the lowest die dropped. Roll again as often as you like.");
    if (ImGui::BeginTable("stats", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Stat");
        ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("");
        ImGui::TableSetupColumn("Bonus");
        ImGui::TableSetupColumn("Total");
        ImGui::TableSetupColumn("Mod");
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
            ImGui::Text("%d", totals[i]);
            ImGui::TableNextColumn();
            ImGui::TextColored(kGold, "%s", signedNumber(abilityModifier(totals[i])).c_str());
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
        ImGui::TextColored(spent > kPointBuyBudget ? kDanger : kMuted, "Point buy: %d of %d points used", spent,
                           kPointBuyBudget);
    }

    ImGui::SeparatorText("Speed");
    ImGui::TextDisabled("The Sequence sets the tier; this score ranks them inside it.");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Roll").x * 2.5f);
    ImGui::SliderInt("##speed", &c.stats.speedBase, 1, 30, "Speed %d");
    ImGui::SameLine();
    if (ImGui::Button("Roll##speed")) {
        const FourD6Roll roll = w.app.dice.roll4d6DropLowest();
        c.stats.speedBase = roll.total;
        s.lastSpeedRoll = roll.describe();
    }
    if (!s.lastSpeedRoll.empty()) ImGui::SetItemTooltip("Last roll: %s", s.lastSpeedRoll.c_str());
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

void relationshipsTab(WindowState& w, Character& c) {
    presetField("Organization", c.affiliation.organization, kOrganizations);
    if (!c.affiliation.organization.empty()) textField("Rank or role", c.affiliation.rank);

    ImGui::SeparatorText("Relationships");
    ImGui::TextWrapped("Type anyone's name. If it matches a saved character, the two are linked when you save, "
                       "and they get the relationship back.");
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
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##name", "name", &r.name);
            const auto matches = w.app.db.findCharactersByName(r.name, c.id);
            const bool willLink = matches.size() == 1 && ui::toLower(matches[0]->name) == ui::toLower(r.name);
            if (willLink) ImGui::SetItemTooltip("Linked to the saved character %s", characterCode(matches[0]->id).c_str());
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
            ImGui::InputTextWithHint("##note", willLink ? "linked to a saved character" : "note (optional)", &r.note);
            ImGui::TableNextColumn();
            if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0))) removeIndex = static_cast<int>(i);
            ImGui::SetItemTooltip("Remove (also from the other character when you save)");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removeIndex >= 0) c.relationships.erase(c.relationships.begin() + removeIndex);
    if (ImGui::Button("+ Add a relationship")) c.relationships.push_back({0, "", "Friend", ""});
}

void dossierTab(Character& c) {
    Dossier& d = c.dossier;
    ImGui::TextDisabled("An official file on the character, as the Nighthawks or another agency would keep it.");
    std::vector<std::string> keepers = kOrganizations;
    keepers.erase(std::remove(keepers.begin(), keepers.end(), "Independent"), keepers.end());
    presetField("Filed by", d.filedBy, keepers);

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

bool saveCharacterDraft(WindowState& w) { return saveImpl(w); }

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
    if (ImGui::Button("+ New character", ImVec2(-FLT_MIN, 0))) {
        whenSaved(w, draft.dirty(), saveThis, [&w] { openNew(w); });
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search names", &s.search);
    ImGui::Separator();
    if (draft.open && draft.isNew) ImGui::Selectable("(new character)", true);
    if (w.app.db.characters.empty()) ImGui::TextDisabled("No characters yet.");
    for (const auto& c : w.app.db.characters) {
        if (!s.search.empty() && !containsIgnoreCase(c.name, s.search)) continue;
        const bool selected = draft.open && !draft.isNew && draft.value.id == c.id;
        const std::string label = c.name + "##" + std::to_string(c.id);
        if (ImGui::Selectable(label.c_str(), selected) && !selected) {
            const int id = c.id;
            whenSaved(w, draft.dirty(), saveThis, [&w, id] {
                if (const Character* found = w.app.db.findCharacter(id)) open(w, *found, false);
            });
        }
        ImGui::Indent();
        ImGui::TextDisabled("%s, %s", characterCode(c.id).c_str(),
                            c.pathwayId.empty() ? "mortal"
                                                : sequenceLabel(w.app.db.findPathway(c.pathwayId), c.sequence).c_str());
        ImGui::Unindent();
    }
    ImGui::Spacing();
    if (ImGui::TextLink("Add the Tarot Club or other samples")) w.switchTo = Tab::Settings;
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
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.6f);
    ImGui::TextUnformatted(c.name.empty() ? "(unnamed)" : c.name.c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", draft.isNew ? "new" : characterCode(c.id).c_str());
    ImGui::TextColored(kMuted, "%s", sheet.subtitle.c_str());

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

    ImGui::Spacing();
    if (ImGui::BeginTabBar("characterTabs")) {
        const std::pair<const char*, int> tabs[] = {{"Identity", 0},      {"Looks", 1},    {"Pathway", 2},
                                                     {"Customization", 3}, {"Stats", 4},    {"Sealed Artifacts", 5},
                                                     {"Relationships", 6}, {"Dossier", 7},  {"Notes", 8},
                                                     {"Sheet", 9}};
        for (const auto& [name, index] : tabs) {
            if (!ImGui::BeginTabItem(name, nullptr, index == 0 ? draft.firstTabFlags() : 0)) continue;
            ImGui::BeginChild("tab", ImVec2(0, 0), ImGuiChildFlags_None);
            switch (index) {
                case 0: identityTab(draft); break;
                case 1: looksTab(c); break;
                case 2: pathwayTab(w, c); break;
                case 3: customizationTab(w, c); break;
                case 4: statsTab(w, c); break;
                case 5: artifactsTab(w, c); break;
                case 6: relationshipsTab(w, c); break;
                case 7: dossierTab(c); break;
                case 8: multilineField("Anything else worth remembering", c.notes, 14); break;
                default: drawSheet(w, sheet); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

}  // namespace lotm::window
