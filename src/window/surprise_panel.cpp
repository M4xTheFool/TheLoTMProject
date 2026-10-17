// "Surprise me" on the Characters tab: the generator above a new character's tabs. Its options (pathway,
// Sequences, where the name comes from, an organization), then one row for each part with a die to roll
// it again and a line on what it came out as. A part rolled again fades in; its die tumbles first.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

#include "organizations.hpp"
#include "window.hpp"

namespace lotm::window {

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTumbleSeconds = 0.55f;
constexpr float kSettleSeconds = 0.6f;  // a new name, from flickering to still

// A number from 0 to 1, always the same for the same inputs: the faces a die flickers through.
float noise(unsigned seed, unsigned a, unsigned b = 0) {
    unsigned h = seed * 747796405u + a * 2891336453u + b * 277803737u + 0x9E3779B9u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

const std::vector<GeneratorPart> kParts = {GeneratorPart::Name, GeneratorPart::Looks, GeneratorPart::Power,
                                           GeneratorPart::Stats, GeneratorPart::Personality};

size_t partIndex(GeneratorPart part) { return static_cast<size_t>(part); }

std::string pathwayChoice(const Database& db, const GeneratorOptions& o) {
    if (o.mortal) return "An ordinary mortal";
    if (!o.pathwayId.empty()) return pathwayLabel(db, o.pathwayId);
    if (o.organizationPathways && db.findOrganization(o.organizationId)) return "The organization's pathways";
    return "Any pathway";
}

// "Any pathway", a mortal, the organization's pathways, or one pathway, in the database's groups.
bool pathwayOption(const Database& db, GeneratorOptions& o) {
    bool changed = false;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.5f);
    if (!ImGui::BeginCombo("##surprisePathway", pathwayChoice(db, o).c_str(), ImGuiComboFlags_HeightLarge)) return false;
    auto choose = [&](const char* text, bool on, bool mortal, bool organization, const std::string& id) {
        if (ImGui::Selectable(text, on)) {
            o.mortal = mortal;
            o.organizationPathways = organization;
            o.pathwayId = id;
            changed = true;
        }
    };
    choose("Any pathway", !o.mortal && o.pathwayId.empty() && !o.organizationPathways, false, false, "");
    choose("An ordinary mortal", o.mortal, true, false, "");
    if (const Organization* org = db.findOrganization(o.organizationId)) {
        choose("The organization's pathways", o.organizationPathways && o.pathwayId.empty() && !o.mortal, false, true, "");
        if (ImGui::BeginItemTooltip()) {
            if (org->pathwayIds.empty()) ImGui::TextUnformatted((org->name + " lists no pathways, so any pathway.").c_str());
            for (const std::string& id : org->pathwayIds) ImGui::BulletText("%s", pathwayLabel(db, id).c_str());
            ImGui::EndTooltip();
        }
    }
    std::string group = "\x01";
    for (const Pathway& p : db.pathways) {
        const std::string thisGroup = p.group.empty() ? "Your own pathways" : p.group;
        if (thisGroup != group) {
            group = thisGroup;
            ImGui::SeparatorText(group.c_str());
        }
        choose((pathwayLabel(db, p.id) + "##" + p.id).c_str(), p.id == o.pathwayId, false, false, p.id);
    }
    ImGui::EndCombo();
    return changed;
}

// One end of the Sequence range: 9 to 0, with the Sequence's name when one pathway is picked.
bool sequenceOption(const char* id, const Pathway* pathway, int& sequence) {
    bool changed = false;
    auto text = [&](int s) {
        const SequenceInfo* info = pathway ? pathway->findSequence(s) : nullptr;
        return std::to_string(s) + (info && !info->name.empty() ? "  " + info->name : "");
    };
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * (pathway ? 8.0f : 3.2f));
    if (ImGui::BeginCombo(id, text(sequence).c_str(), ImGuiComboFlags_HeightLarge)) {
        for (int s = 9; s >= 0; --s) {
            if (ImGui::Selectable(text(s).c_str(), s == sequence)) {
                sequence = s;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool originOption(GeneratorOptions& o) {
    static const char* kTips[] = {"Any of the three.", "The kingdom of Backlund and Tingen: Edwin Hartley, Margaret Ashford.",
                                  "The republic of Trier: Camille Laurent, Bastien Moreau.",
                                  "The cold northern empire: Anya Volkova, Leif Halvorsen."};
    bool changed = false;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.5f);
    if (ImGui::BeginCombo("##surpriseOrigin", o.origin.empty() ? "Anywhere" : o.origin.c_str())) {
        for (size_t i = 0; i <= kNameOrigins.size(); ++i) {
            const std::string origin = i == 0 ? "" : kNameOrigins[i - 1];
            if (ImGui::Selectable(i == 0 ? "Anywhere" : origin.c_str(), o.origin == origin)) {
                o.origin = origin;
                changed = true;
            }
            ImGui::SetItemTooltip("%s", kTips[i]);
        }
        ImGui::EndCombo();
    }
    return changed;
}

// None, or one of your organizations. Picking one known for some pathways picks those as well.
bool organizationOption(const Database& db, GeneratorOptions& o) {
    const Organization* now = db.findOrganization(o.organizationId);
    bool changed = false;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.5f);
    if (ImGui::BeginCombo("##surpriseOrganization", now ? now->name.c_str() : "None", ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("None", !now)) {
            o.organizationId = 0;
            o.organizationPathways = false;
            changed = true;
        }
        for (const Organization& org : db.organizations) {
            ImGui::PushID(org.id);
            statusDot(organizationColour(db, org));
            if (ImGui::Selectable(org.name.c_str(), now && now->id == org.id)) {
                o.organizationId = org.id;
                if (!org.pathwayIds.empty() && o.pathwayId.empty() && !o.mortal) o.organizationPathways = true;
                changed = true;
            }
            ImGui::PopID();
        }
        if (db.organizations.empty()) ImGui::TextDisabled("You have no organizations yet (Organizations tab).");
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("They are made a member, with any of its ranks but the highest.");
    return changed;
}

}  // namespace

std::string settlingName(const std::string& name, double since, unsigned seed) {
    if (!animations()) return name;
    const float t = static_cast<float>(ImGui::GetTime() - since) / kSettleSeconds;
    if (t >= 1.0f || t < 0.0f || name.empty()) return name;
    const unsigned step = static_cast<unsigned>(ImGui::GetTime() * 24.0);  // a new letter about 24 times a second
    std::string out = name;
    for (size_t i = 0; i < out.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(out[i]);
        if (!std::isalpha(ch)) continue;
        if (t > static_cast<float>(i + 1) / static_cast<float>(out.size())) continue;  // settled already
        const char letter = static_cast<char>('a' + static_cast<int>(noise(seed, static_cast<unsigned>(i), step) * 26.0f) % 26);
        out[i] = std::isupper(ch) ? static_cast<char>(std::toupper(static_cast<unsigned char>(letter))) : letter;
    }
    return out;
}

bool dieButton(const WindowState& w, const char* id, const std::string& label, float width, double rolledAt, int face,
               unsigned seed) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight();
    const float r = height * 0.3f;
    const float before = style.FramePadding.x + r * 2.0f + style.ItemInnerSpacing.x;
    const float natural = before + ImGui::CalcTextSize(label.c_str()).x + style.FramePadding.x;
    if (width <= 0.0f) width = natural;
    ImVec2 at = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::Button(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    at.x += std::max(0.0f, (width - natural) * style.ButtonTextAlign.x);  // centred, like a button's label
    const ImVec2 centre(at.x + style.FramePadding.x + r, at.y + height * 0.5f);
    float angle = glide(ImGui::GetID((std::string(id) + "lean").c_str()), hovered ? 0.35f : 0.0f, 12.0f);
    float scale = 1.0f;
    int shown = std::clamp(face, 1, 6);
    const float t = static_cast<float>(ImGui::GetTime() - rolledAt) / kTumbleSeconds;
    if (animations() && t >= 0.0f && t < 1.0f) {
        const float left = 1.0f - t;
        angle += (noise(seed, 1) > 0.5f ? 1.0f : -1.0f) * left * left * 3.0f * kPi;
        scale = 1.0f + 0.3f * std::sin(kPi * t);  // up off the table and back down
        if (t < 0.8f) shown = 1 + static_cast<int>(noise(seed, 2, static_cast<unsigned>(t * 14.0f)) * 6.0f) % 6;
    }
    drawDie(w, draw, centre, r * scale, angle, 6, shown, palette().gold, ImGui::GetStyle().Alpha);
    draw->AddText(ImVec2(at.x + before, at.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    return pressed;
}

// What the options say, on the button that opens them: "Any pathway, Sequence 9 to 7, names from Loen",
// "Nighthawks: their pathways, Sequence 7 to 5".
std::string optionsText(const Database& db, const GeneratorOptions& o) {
    const int weakest = std::max(o.weakest, o.strongest), strongest = std::min(o.weakest, o.strongest);
    const Organization* org = db.findOrganization(o.organizationId);
    std::string text = org ? org->name + ": " : "";
    const Pathway* one = db.findPathway(o.pathwayId);
    if (o.mortal) text += org ? "mortals" : "Ordinary mortals";
    else if (one) text += one->name;
    else if (!o.pathwayId.empty()) text += o.pathwayId;
    else text += org && o.organizationPathways ? "their pathways" : org ? "any pathway" : "Any pathway";
    if (!o.mortal) {
        text += weakest == strongest ? ", Sequence " + std::to_string(weakest)
                                     : ", Sequence " + std::to_string(weakest) + " to " + std::to_string(strongest);
    }
    if (!o.origin.empty()) text += ", names from " + o.origin;
    return text;
}

bool drawSurprisePanel(WindowState& w) {
    auto& g = w.characters.surprise;
    Character& c = w.characters.draft.value;
    const Database& db = w.app.db;
    GeneratorOptions& o = g.options;
    const float font = ImGui::GetFontSize();
    const double now = ImGui::GetTime();
    std::vector<GeneratorPart> roll;  // the parts to roll again this frame
    bool another = false;

    const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
    const ImVec4 gold = palette().gold;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(bg.x + (gold.x - bg.x) * 0.06f, bg.y + (gold.y - bg.y) * 0.06f,
                                                   bg.z + (gold.z - bg.z) * 0.06f, std::max(bg.w, 0.6f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(gold.x, gold.y, gold.z, 0.35f));
    ImGui::BeginChild("surprise", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor(2);

    // ---- the top row: re-roll everything, the options, save and make another, fold the rest away
    if (dieButton(w, "##everything", "Re-roll everything", 0.0f, g.everythingAt, g.everythingFace, g.seed + 7)) {
        roll = kParts;
        g.everythingAt = now;
        g.everythingFace = w.app.dice.roll(6);
    }
    ImGui::SetItemTooltip("A new character with the same options. Anything you typed in yourself stays.");
    ImGui::SameLine();
    static double optionsAt = -100.0;  // when the options were opened, to fade them in
    if (ImGui::Button((optionsText(db, o) + "  ...##options").c_str())) {
        ImGui::OpenPopup("surpriseOptions");
        optionsAt = now;
    }
    ImGui::SetItemTooltip("The pathway, the Sequences, where names come from, and an organization they belong to.");
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * easeIn(optionsAt, 0.18f));
    if (ImGui::BeginPopup("surpriseOptions")) {
        // Changing an option rolls what it decides again.
        if (ImGui::BeginTable("options", 2, ImGuiTableFlags_SizingFixedFit)) {
            auto row = [](const char* text) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(palette().muted, "%s", text);
                ImGui::TableNextColumn();
            };
            row("Pathway");
            if (pathwayOption(db, o)) roll.push_back(GeneratorPart::Power);
            row("Sequence");
            const Pathway* one = o.mortal ? nullptr : db.findPathway(o.pathwayId);
            ImGui::BeginDisabled(o.mortal);
            if (sequenceOption("##weakest", one, o.weakest)) {
                o.strongest = std::min(o.strongest, o.weakest);
                roll.push_back(GeneratorPart::Power);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("to");
            ImGui::SameLine();
            if (sequenceOption("##strongest", one, o.strongest)) {
                o.weakest = std::max(o.strongest, o.weakest);
                roll.push_back(GeneratorPart::Power);
            }
            ImGui::EndDisabled();
            helpMarker("9 is the weakest and 0 a god. 9 to 7 for a street gang, 7 to 5 for a Nighthawk squad.");
            row("Names from");
            if (originOption(o)) roll.push_back(GeneratorPart::Name);
            row("Organization");
            if (organizationOption(db, o)) roll.push_back(GeneratorPart::Power);
            ImGui::EndTable();
        }
        if (!o.mortal && generatorPathways(db, o).empty()) {
            ImGui::PushTextWrapPos(font * 24.0f);
            ImGui::TextColored(palette().warning, "No pathway here has a Sequence from %d to %d, so they come out as an ordinary mortal.",
                               std::max(o.weakest, o.strongest), std::min(o.weakest, o.strongest));
            ImGui::PopTextWrapPos();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    const char* fold = g.folded ? "Show" : "Fold";
    const float foldWidth = ImGui::CalcTextSize(fold).x;
    const float anotherWidth = ImGui::CalcTextSize("Save and make another").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float right = ImGui::GetContentRegionMax().x - foldWidth - anotherWidth - ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() > right) ImGui::NewLine();  // a narrow window: on a line of their own
    else ImGui::SetCursorPosX(right);
    if (ImGui::Button("Save and make another")) another = true;
    ImGui::SetItemTooltip("Saves this one and rolls the next with the same options: a Nighthawk squad in a few clicks.");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (ImGui::TextLink(fold)) g.folded = !g.folded;
    ImGui::SetItemTooltip("%s", g.folded ? "Show each part with its own die." : "Just this row, for more room below.");

    if (!g.folded) {
        // ---- one row a part: its die, then what it came out as, fading in after a roll
        const float buttonWidth = [&] {
            float widest = 0.0f;
            for (GeneratorPart part : kParts) widest = std::max(widest, ImGui::CalcTextSize(generatorPartName(part).c_str()).x);
            return widest + ImGui::GetFrameHeight() * 0.6f + ImGui::GetStyle().FramePadding.x * 2.0f +
                   ImGui::GetStyle().ItemInnerSpacing.x;
        }();
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        if (ImGui::BeginTable("parts", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("die", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
            ImGui::TableSetupColumn("what", ImGuiTableColumnFlags_WidthStretch);
            for (GeneratorPart part : kParts) {
                const size_t i = partIndex(part);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const std::string id = "##roll" + std::to_string(i);
                if (dieButton(w, id.c_str(), generatorPartName(part), buttonWidth, g.rolledAt[i], g.faces[i],
                              g.seed * 31u + static_cast<unsigned>(i))) {
                    roll.push_back(part);
                }
                ImGui::TableNextColumn();
                const float shown = easeIn(g.rolledAt[i] + kTumbleSeconds * 0.5, 0.35f);
                ImGui::AlignTextToFramePadding();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (1.0f - shown) * font * 0.8f);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * std::max(shown, 0.05f));
                if (part == GeneratorPart::Stats) {
                    // Each number counts its way to the new roll.
                    std::string text;
                    for (size_t s = 0; s < kStatCount; ++s) {
                        const float value = glide(ImGui::GetID(("stat" + std::to_string(s)).c_str()),
                                                  static_cast<float>(c.stats.base[s]), 9.0f);
                        text += std::string(kStatCodes[s]) + " " + std::to_string(static_cast<int>(std::lround(value))) + "   ";
                    }
                    const float speed = glide(ImGui::GetID("speed"), static_cast<float>(c.stats.speedBase), 9.0f);
                    text += "Speed " + std::to_string(static_cast<int>(std::lround(speed)));
                    ImGui::TextUnformatted(text.c_str());
                    ImGui::SetItemTooltip("Each one 4d6, the lowest die dropped; before the pathway's bonuses.");
                } else if (part == GeneratorPart::Name) {
                    // Only the name flickers, not the words after it.
                    const std::string summary = generatorPartSummary(c, part, db);
                    const size_t name = summary.rfind(c.name, 0) == 0 ? c.name.size() : 0;
                    ImGui::TextWrapped("%s", (settlingName(summary.substr(0, name), g.rolledAt[i], g.seed) + summary.substr(name)).c_str());
                } else {
                    ImGui::TextWrapped("%s", generatorPartSummary(c, part, db).c_str());
                }
                ImGui::PopStyleVar();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();

    for (GeneratorPart part : roll) {
        const size_t i = partIndex(part);
        const Appearance looksBefore = c.appearance;
        rerollPart(c, part, db, o, w.app.dice);
        g.rolledAt[i] = now;
        g.faces[i] = w.app.dice.roll(6);
        // A new name may be a woman's, and a beard goes with it: the looks fade in again too.
        if (part == GeneratorPart::Name && nlohmann::json(looksBefore) != nlohmann::json(c.appearance)) {
            g.rolledAt[partIndex(GeneratorPart::Looks)] = now;
        }
        if (part == GeneratorPart::Stats) {
            w.characters.lastRolls = {};
            w.characters.lastSpeedRoll.clear();
        }
    }
    if (!roll.empty()) ++g.seed;
    return another;
}

}  // namespace lotm::window
