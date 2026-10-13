#include "sheet.hpp"

#include <algorithm>
#include <cctype>

#include "pathway_colours.hpp"
#include "rules.hpp"

namespace lotm {

void Section::field(const std::string& label, const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Field, label, text, {}, {}});
}

void Section::paragraph(const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Paragraph, "", text, {}, {}});
}

void Section::bullet(const std::string& text) { blocks.push_back({BlockKind::Bullet, "", text, {}, {}}); }

void Section::subheading(const std::string& text) { blocks.push_back({BlockKind::Subheading, "", text, {}, {}}); }

void Section::quote(const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Quote, "", text, {}, {}});
}

namespace {

std::string join(const std::vector<std::string>& items, const std::string& separator = ", ") {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += separator;
        out += items[i];
    }
    return out;
}

void addSection(Sheet& sheet, Section section) {
    if (!section.blocks.empty()) sheet.sections.push_back(std::move(section));
}

// "Black, shoulder-length, wavy": later parts start lower case unless they look like a name or acronym.
std::string describeHair(const Appearance& look) {
    std::string out;
    for (const std::string* part : {&look.hair, &look.hairLength, &look.hairStyle}) {
        if (part->empty()) continue;
        std::string text = *part;
        if (!out.empty()) {
            out += ", ";
            if (text.size() > 1 && std::islower(static_cast<unsigned char>(text[1]))) {
                text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
            }
        }
        out += text;
    }
    return out;
}

// "Seer, Apprentice and Marauder"
std::string joinAnd(const std::vector<std::string>& items) {
    if (items.size() <= 1) return join(items);
    return join(std::vector<std::string>(items.begin(), items.end() - 1)) + " and " + items.back();
}

std::string pathwayColourHex(const Database& db, const std::string& pathwayId) {
    return hexColour(pathwayRgb(db, pathwayId));
}

// The colour a threat level or state of mind is shown in, from the mildest (0) up.
std::string toneForLevel(int level) {
    static const char* kTones[] = {"good", "gold", "orange"};
    if (level < 0) return "muted";
    return level < 3 ? kTones[level] : "danger";
}

// "Spectator pathway (Visionary)"
std::string pathwayWithGod(const Database& db, const std::string& id) {
    const Pathway* p = db.findPathway(id);
    if (!p) return pathwayLabel(db, id);
    return p->name + " pathway" + (p->god.empty() ? "" : " (" + p->god + ")");
}

// The Sequence ladder, and below it every switch of pathway on the way up.
Section ladderSection(const Character& c, const Database& db) {
    Section ladder{"Sequence Ladder"};
    if (c.pathwayId.empty()) return ladder;
    ladder.blocks.push_back({BlockKind::Ladder, "", "", {}, ladderRungs(c, db)});
    const auto switches = pathwaySwitches(c, db);
    for (const PathwaySwitch& change : switches) {
        ladder.field("Switch at Sequence " + std::to_string(change.atSequence),
                     "From the " + pathwayWithGod(db, change.fromPathwayId) + " to the " +
                         pathwayWithGod(db, change.toPathwayId) + ". " +
                         (change.neighbouring ? "Neighbouring pathways, a safe switch."
                                              : "Not neighbours: a risky switch that tends to cost sanity."));
    }
    if (!switches.empty()) {
        const Pathway* now = db.findPathway(c.pathwayId);
        ladder.paragraph("Each Sequence gives the abilities of the pathway it was taken from. Stat bonuses and the "
                         "speed grade follow the pathway they are on now" +
                         (now ? std::string(", the ") + now->name + " pathway." : std::string(".")));
    }
    return ladder;
}

std::string artifactLine(const Artifact& a, const Database& db) {
    std::string line = artifactCode(a.id);
    if (!a.pathwayId.empty()) line += ", " + pathwayLabel(db, a.pathwayId) + " pathway";
    if (a.sequenceLevel != kUnknownSequence) line += ", Sequence " + std::to_string(a.sequenceLevel) + " level";
    return line;
}

}  // namespace

std::vector<LadderRung> ladderRungs(const Character& c, const Database& db) {
    std::vector<LadderRung> rungs;
    if (c.pathwayId.empty()) return rungs;
    const auto switches = pathwaySwitches(c, db);
    for (int sequence = 9; sequence >= c.sequence; --sequence) {
        LadderRung rung;
        rung.sequence = sequence;
        rung.pathwayId = pathwayIdAt(c, sequence);
        const Pathway* p = db.findPathway(rung.pathwayId);
        rung.pathway = p ? p->name : rung.pathwayId + " (not found)";
        const SequenceInfo* info = p ? p->findSequence(sequence) : nullptr;
        rung.name = info && !info->name.empty() ? info->name : "Sequence " + std::to_string(sequence);
        rung.colour = pathwayColourHex(db, rung.pathwayId);
        rung.current = sequence == c.sequence && !c.aboveSequence;
        for (const PathwaySwitch& change : switches) {
            if (change.atSequence != sequence) continue;
            rung.switched = true;
            rung.risky = !change.neighbouring;
        }
        rungs.push_back(rung);
    }
    if (aboveTheSequence(c)) {
        const Sefirah* goo = greatOldOneSefirah(c, db);
        LadderRung rung;
        rung.sequence = 0;
        rung.above = true;
        rung.name = goo && !goo->greatOldOne.empty() ? goo->greatOldOne : "Great Old One";
        rung.pathwayId = c.pathwayId;
        const Pathway* p = db.findPathway(c.pathwayId);
        rung.pathway = p ? p->name : c.pathwayId;
        rung.colour = pathwayColourHex(db, c.pathwayId);
        rung.current = true;
        rungs.push_back(rung);
    }
    return rungs;
}

Sheet buildCharacterSheet(const Character& c, const Database& db) {
    const Pathway* pathway = db.findPathway(c.pathwayId);
    Sheet sheet;
    sheet.anchor = "c-" + std::to_string(c.id);
    sheet.kind = "Character";
    sheet.title = c.name.empty() ? "(unnamed)" : c.name;
    const Sefirah* groupSefirah = db.sefirahForPathway(c.pathwayId);
    const Sefirah* gooSefirah = greatOldOneSefirah(c, db);
    const Pathway* second = isDualPathwayGod(c) ? db.findPathway(c.secondPathway.pathwayId) : nullptr;
    auto pathwayText = [&db](const Pathway* p, const std::string& id) {
        if (!p) return pathwayLabel(db, id);
        return p->name + " pathway" + (p->god.empty() ? "" : " (" + p->god + ")");
    };
    if (c.pathwayId.empty()) {
        sheet.subtitle = "Ordinary mortal";
    } else if (c.aboveSequence) {
        const std::string goo = gooSefirah && !gooSefirah->greatOldOne.empty() ? gooSefirah->greatOldOne
                                                                              : "a Great Old One";
        sheet.subtitle = "Above the Sequence: " + goo + ", " + pathwayText(pathway, c.pathwayId);
    } else {
        const SequenceInfo* seq = pathway ? pathway->findSequence(c.sequence) : nullptr;
        sheet.subtitle = "Sequence " + std::to_string(c.sequence) + (seq && !seq->name.empty() ? " " + seq->name : "") +
                         ", " + pathwayText(pathway, c.pathwayId);
        if (isDualPathwayGod(c)) sheet.subtitle += " and " + pathwayText(second, c.secondPathway.pathwayId);
    }
    if (const auto earlier = earlierPathwayIds(c); !earlier.empty()) {
        std::vector<std::string> names;
        for (const auto& id : earlier) {
            const Pathway* p = db.findPathway(id);
            names.push_back(p ? p->name : id);
        }
        sheet.subtitle += ", switched from the " + joinAnd(names) + (names.size() == 1 ? " pathway" : " pathways");
    }

    // Badges under the title: power tier, a mixed or dual pathway, the Sefirah, state of mind, threat.
    if (!c.pathwayId.empty()) {
        sheet.badges.push_back({speedTierOf(c).name, "pathway", c.pathwayId, pathwayColourHex(db, c.pathwayId)});
    }
    if (const auto earlier = earlierPathwayIds(c); !earlier.empty()) {
        if (riskySwitch(c, db)) sheet.badges.push_back({"Mixed pathways, risky switch", "danger", "", ""});
        else sheet.badges.push_back({"Mixed pathways", "pathway", earlier.front(), pathwayColourHex(db, earlier.front())});
    }
    if (isDualPathwayGod(c)) {
        sheet.badges.push_back({"Dual-pathway god", "pathway", c.secondPathway.pathwayId,
                                pathwayColourHex(db, c.secondPathway.pathwayId)});
    }
    if (const Sefirah* held = db.findSefirah(c.sefirahId)) sheet.badges.push_back({held->name, "gold", "", ""});
    if (!c.stateOfMind.empty()) {
        sheet.badges.push_back({c.stateOfMind,
                                toneForLevel(stateOfMindLevel(c.stateOfMind)),
                                "", ""});
    }
    {
        const std::string threat = threatLevelOf(c);
        const auto level = std::find(kThreatLevels.begin(), kThreatLevels.end(), threat);
        const int index = level == kThreatLevels.end() ? -1 : static_cast<int>(level - kThreatLevels.begin());
        sheet.badges.push_back({"Threat: " + threat, toneForLevel(index), "", ""});
    }

    Section identity{"Identity"};
    identity.field("ID", characterCode(c.id));
    identity.field("Aliases", join(c.aliases));
    identity.field("Titles", join(c.titles));
    identity.field("Alignment", c.alignment);
    if (!c.stateOfMind.empty() || !c.stateOfMindNote.empty()) {
        std::string mind = c.stateOfMind.empty() ? c.stateOfMindNote : c.stateOfMind;
        if (!c.stateOfMind.empty() && !c.stateOfMindNote.empty()) mind += ": " + c.stateOfMindNote;
        identity.field("State of mind", mind);
    }
    identity.field("Age", c.age);
    identity.field("Gender", c.gender);
    if (sequenceOneChoices(c)) identity.field("Beyonder characteristics", std::to_string(c.beyonderCharacteristics));
    if (!c.sefirahId.empty()) identity.field("Sefirah", sefirahLabel(db, c.sefirahId));
    if (!c.affiliation.organization.empty()) {
        std::string org = c.affiliation.organization;
        if (!c.affiliation.rank.empty()) org += " (" + c.affiliation.rank + ")";
        identity.field("Affiliation", org);
    }
    addSection(sheet, identity);
    addSection(sheet, ladderSection(c, db));

    Section looks{"Appearance"};
    looks.field("Hair", describeHair(c.appearance));
    looks.field("Eyes", c.appearance.eyes);
    looks.field("Skin", c.appearance.skin);
    looks.field("Face", c.appearance.face);
    looks.field("Build", c.appearance.build);
    looks.field("Height", c.appearance.height);
    looks.field("Voice", c.appearance.voice);
    looks.field("Clothing", c.appearance.clothing);
    looks.field("Distinguishing mark", c.appearance.distinguishingMark);
    looks.paragraph(c.appearance.description);
    addSection(sheet, looks);

    Section about{"About"};
    about.paragraph(c.shortDescription);
    if (!c.backstory.empty()) {
        about.subheading("Backstory");
        about.paragraph(c.backstory);
    }
    addSection(sheet, about);

    std::vector<std::string> honorificLines;  // the window shows empty boxes to fill in; they aren't lines
    for (const auto& line : c.honorificName) {
        if (!line.empty()) honorificLines.push_back(line);
    }
    if (!honorificLines.empty()) {
        Section honorific{"Honorific Name"};
        honorific.quote(join(honorificLines, "\n"));
        addSection(sheet, honorific);
    }

    if (c.hasUniqueness) {
        Section uniqueness{"Uniqueness"};
        uniqueness.paragraph(c.uniquenessForm.empty() ? "Holds a Uniqueness." : c.uniquenessForm);
        addSection(sheet, uniqueness);
    } else if (absorbedUniqueness(c)) {
        Section uniqueness{"Uniqueness"};
        uniqueness.paragraph("Has absorbed the Uniqueness of the " +
                             (pathway ? pathway->name : pathwayLabel(db, c.pathwayId)) + " pathway.");
        uniqueness.paragraph(c.uniquenessForm);
        addSection(sheet, uniqueness);
    }

    if (aboveTheSequence(c)) {
        Section above{"Above the Sequence"};
        above.paragraph("A Great Old One: past Sequence 0, holding a Sefirah and every Uniqueness of its pathway "
                        "group, and embodying an aspect of the universe.");
        if (gooSefirah) above.field("Great Old One", gooSefirah->greatOldOne);
        if (gooSefirah) above.field("Symbolism", gooSefirah->symbolism);
        std::vector<std::string> names;
        for (const Pathway* p : db.pathwayGroup(c.pathwayId)) names.push_back(p->name);
        above.field("Uniquenesses held", joinAnd(names) + (names.size() == 1 ? " pathway" : " pathways"));
        if (groupSefirah && c.sefirahId != groupSefirah->id) above.field("Their group's Sefirah", groupSefirah->name);
        addSection(sheet, above);
    }

    if (isDualPathwayGod(c)) {
        const SecondPathway& held = c.secondPathway;
        Section dual{"Second Pathway"};
        dual.paragraph("A dual-pathway god: besides their own pathway they hold a second pathway's Sequence 1 "
                       "Beyonder characteristic" + std::string(held.beyonderCharacteristics == 2 ? "s" : "") +
                       (held.hasUniqueness ? " and its Uniqueness" : "") + ", and with them every ability of "
                       "that pathway, Sequence 9 to 0.");
        dual.field("Pathway", pathwayLabel(db, held.pathwayId));
        dual.field("Beyonder characteristics", std::to_string(held.beyonderCharacteristics) + " of " +
                                                   sequenceLabel(second, 1));
        dual.field("Uniqueness", held.hasUniqueness ? "Holds it" : "Not held");
        if (held.hasUniqueness) dual.paragraph(held.uniquenessForm);
        addSection(sheet, dual);
    }

    // Stat block
    Section stats{"Stat Block"};
    Block table{BlockKind::Stats, "", "", {}, {}};
    const auto bonus = statBonuses(c, pathway);
    const auto totals = statTotals(c, pathway);
    for (int i = 0; i < kStatCount; ++i) {
        table.stats.push_back({kStatNames[i], kStatCodes[i], c.stats.base[i], bonus[i], totals[i],
                               abilityModifier(totals[i])});
    }
    stats.blocks.push_back(table);
    if (pathway && (bonus != std::array<int, kStatCount>{})) {
        const int tierNow = speedTierOf(c).tier;
        std::string text = "+" + std::to_string(primaryBonus(tierNow)) + " " + pathway->primaryStat;
        if (secondaryBonus(tierNow) > 0) text += ", +" + std::to_string(secondaryBonus(tierNow)) + " " + pathway->secondaryStat;
        stats.field("Pathway bonus", text + " (" + pathway->name + " pathway, tier " + std::to_string(tierNow) + ")");
    }
    const SpeedTier tier = speedTierOf(c);
    std::string speed = "Tier " + std::to_string(tier.tier) + " " + tier.name + ", score " +
                        std::to_string(effectiveSpeed(c, pathway));
    if (pathway && speedGradeModifier(pathway->speedGrade) != 0) {
        speed += " (" + std::to_string(c.stats.speedBase) + " " + signedNumber(speedGradeModifier(pathway->speedGrade)) +
                 " " + pathway->speedGrade + ")";
    }
    stats.field("Speed", speed);
    stats.field("Speed tier means", tier.description);
    if (pathway) stats.field("Speed note", pathway->speedNote);
    if (c.stats.hpIncluded) stats.field("HP", std::to_string(c.stats.hp));
    stats.field("Spirituality", std::to_string(c.stats.spirituality));
    addSection(sheet, stats);

    Section abilities{"Abilities"};
    for (const AbilityGroup& group : abilityGroups(c, db)) {
        abilities.subheading(group.heading);
        if (!group.summary.empty()) abilities.paragraph(group.summary + ".");
        abilities.field("Symbolism", group.symbolism);
        for (const auto& a : group.abilities) abilities.bullet(a);
        for (const auto& mode : group.movement) abilities.bullet("Movement: " + mode);
    }
    addSection(sheet, abilities);

    Section artifacts{"Sealed Artifacts"};
    for (int id : c.artifactIds) {
        const Artifact* a = db.findArtifact(id);
        if (!a) continue;
        artifacts.subheading(a->name);
        artifacts.field("Details", artifactLine(*a, db));
        artifacts.paragraph(a->visualDescription);
        artifacts.field("Ability", a->ability);
        artifacts.field("Drawback", a->drawback);
    }
    addSection(sheet, artifacts);

    Section relations{"Relationships"};
    for (const auto& r : c.relationships) {
        std::string line = r.type + ": " + relationshipName(db, r);
        if (!r.note.empty()) line += ", " + r.note;
        relations.bullet(line);
    }
    addSection(sheet, relations);

    const Dossier& file = c.dossier;
    Section dossier{"Dossier", "dossier"};
    dossier.field("File", characterCode(c.id));
    dossier.field("Filed by", file.filedBy);
    const ThreatAssessment threat = assessThreat(c);
    if (file.threatLevel.empty()) {
        dossier.field("Threat level", threat.level);
        dossier.field("Assessment", threat.explanation);
    } else {
        dossier.field("Threat level", file.threatLevel + " (set by hand)");
        dossier.field("Assessment", threat.explanation + ", which the rules call " + threat.level);
    }
    dossier.field("Recent actions", file.recentActions);
    dossier.field("What they did", file.recentActionsNote);
    dossier.field("Status", file.status);
    dossier.field("Last seen", file.lastSeen);
    dossier.field("Remarks", file.remarks);
    addSection(sheet, dossier);

    Section notes{"Notes"};
    notes.paragraph(c.notes);
    addSection(sheet, notes);

    if (!c.createdAt.empty()) {
        sheet.footer = "Created " + c.createdAt;
        if (!c.updatedAt.empty() && c.updatedAt != c.createdAt) sheet.footer += ", last edited " + c.updatedAt;
    }
    return sheet;
}

Sheet buildArtifactSheet(const Artifact& a, const Database& db) {
    Sheet sheet;
    sheet.anchor = "a-" + std::to_string(a.id);
    sheet.kind = "Sealed Artifact";
    sheet.title = a.name.empty() ? "(unnamed)" : a.name;
    sheet.subtitle = "Sealed Artifact " + artifactCode(a.id);
    sheet.subtitle += a.pathwayId.empty() ? ", unknown pathway" : ", " + pathwayLabel(db, a.pathwayId) + " pathway";
    if (a.sequenceLevel != kUnknownSequence) sheet.subtitle += ", Sequence " + std::to_string(a.sequenceLevel) + " level";

    Section look{"Appearance"};
    look.paragraph(a.visualDescription);
    addSection(sheet, look);

    Section ability{"Ability"};
    ability.paragraph(a.ability);
    addSection(sheet, ability);

    Section drawback{"Drawback"};
    drawback.paragraph(a.drawback);
    addSection(sheet, drawback);

    Section notes{"Notes"};
    notes.paragraph(a.notes);
    addSection(sheet, notes);

    Section holders{"Held By"};
    for (const auto& c : db.characters) {
        for (int id : c.artifactIds) {
            if (id == a.id) holders.bullet(c.name + " (" + characterCode(c.id) + ")");
        }
    }
    addSection(sheet, holders);

    if (!a.createdAt.empty()) {
        sheet.footer = "Created " + a.createdAt;
        if (!a.updatedAt.empty() && a.updatedAt != a.createdAt) sheet.footer += ", last edited " + a.updatedAt;
    }
    return sheet;
}

Sheet buildSefirahSheet(const Sefirah& s, const Database& db) {
    Sheet sheet;
    sheet.anchor = "s-" + s.id;
    sheet.kind = "Sefirah";
    sheet.title = s.name;
    sheet.subtitle = s.custom ? "Your own Sefirah" : "Sefirah";

    Section overview{"Overview"};
    overview.field("Also known as", join(s.alsoKnownAs));
    overview.field("Great Old One", s.greatOldOne);
    std::vector<std::string> pathways;
    for (const auto& id : s.pathwayIds) pathways.push_back(pathwayLabel(db, id));
    overview.field("Pathways", join(pathways));
    overview.paragraph(s.description);
    addSection(sheet, overview);

    Section above{"Above the Sequence"};
    above.field("Symbolism", s.symbolism);
    for (const auto& a : s.abilities) above.bullet(a);
    if (!s.symbolism.empty() || !s.abilities.empty()) addSection(sheet, above);

    Section canon{"Known Holders"};
    for (const auto& line : s.knownHolders) canon.bullet(line);
    addSection(sheet, canon);

    Section holders{"Held By"};
    for (const Character* c : db.sefirahHolders(s.id)) {
        holders.bullet(c->name + " (" + characterCode(c->id) + "), " + levelName(*c));
    }
    addSection(sheet, holders);

    Section notes{"Notes"};
    notes.paragraph(s.notes);
    addSection(sheet, notes);

    sheet.footer = s.custom ? "Stored in custom_sefirot.json" : "Stored in sefirot.json";
    return sheet;
}

Sheet buildPathwaySheet(const Pathway& p, const Database& db) {
    Sheet sheet;
    sheet.anchor = "p-" + p.id;
    sheet.kind = "Pathway";
    sheet.title = p.name + " pathway";
    sheet.subtitle = p.god.empty() ? "Sequence 9 to 0" : "Sequence 9 to 0, " + p.god;
    if (p.custom) sheet.subtitle += ", one of your own pathways";

    auto statName = [](const std::string& code) {
        const int i = statIndex(code);
        return i < 0 ? code : std::string(kStatNames[i]) + " (" + kStatCodes[i] + ")";
    };
    Section overview{"Overview"};
    overview.field("Pathway id", p.id);
    overview.field("God", p.god);
    overview.field("Neighbouring pathways", p.group);
    overview.field("Primary stat", p.primaryStat.empty() ? "" : statName(p.primaryStat));
    overview.field("Secondary stat", p.secondaryStat.empty() ? "" : statName(p.secondaryStat));
    std::string speed = p.speedGrade;
    if (speedGradeModifier(p.speedGrade) != 0) speed += " (" + signedNumber(speedGradeModifier(p.speedGrade)) + " Speed)";
    overview.field("Speed grade", speed);
    overview.field("Speed note", p.speedNote);
    overview.paragraph(p.description);
    addSection(sheet, overview);

    Section uniqueness{"Uniqueness"};
    uniqueness.paragraph(p.uniqueness);
    addSection(sheet, uniqueness);

    Section sequences{"Sequences"};
    for (const auto& s : p.sequences) {
        sequences.subheading("Sequence " + std::to_string(s.sequence) + (s.name.empty() ? "" : ": " + s.name));
        if (s.abilities.empty()) sequences.paragraph("No abilities written yet.");
        for (const auto& a : s.abilities) sequences.bullet(a);
    }
    // One more step past Sequence 0, written on the Sefirah that gathers the pathway.
    const Sefirah* sefirah = db.sefirahForPathway(p.id);
    sequences.subheading(aboveSequenceLabel(sefirah));
    if (!sefirah) {
        sequences.paragraph("No Sefirah gathers this pathway yet, so it has no Great Old One.");
    } else {
        sequences.paragraph("A Great Old One holds " + sefirah->name + " and the Uniqueness of every pathway it gathers.");
        sequences.field("Symbolism", sefirah->symbolism);
        for (const auto& a : sefirah->abilities) sequences.bullet(a);
    }
    addSection(sheet, sequences);

    sheet.footer = p.custom ? "Stored in custom_pathways.json" : "Stored in pathways.json";
    return sheet;
}

}  // namespace lotm
