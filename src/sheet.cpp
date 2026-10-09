#include "sheet.hpp"

#include "rules.hpp"

namespace lotm {

void Section::field(const std::string& label, const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Field, label, text, {}});
}

void Section::paragraph(const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Paragraph, "", text, {}});
}

void Section::bullet(const std::string& text) { blocks.push_back({BlockKind::Bullet, "", text, {}}); }

void Section::subheading(const std::string& text) { blocks.push_back({BlockKind::Subheading, "", text, {}}); }

void Section::quote(const std::string& text) {
    if (!text.empty()) blocks.push_back({BlockKind::Quote, "", text, {}});
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

std::string artifactLine(const Artifact& a, const Database& db) {
    std::string line = artifactCode(a.id);
    if (!a.pathwayId.empty()) line += ", " + pathwayLabel(db, a.pathwayId) + " pathway";
    if (a.sequenceLevel != kUnknownSequence) line += ", Sequence " + std::to_string(a.sequenceLevel) + " level";
    return line;
}

}  // namespace

Sheet buildCharacterSheet(const Character& c, const Database& db) {
    const Pathway* pathway = db.findPathway(c.pathwayId);
    Sheet sheet;
    sheet.anchor = "c-" + std::to_string(c.id);
    sheet.kind = "Character";
    sheet.title = c.name.empty() ? "(unnamed)" : c.name;
    if (c.pathwayId.empty()) {
        sheet.subtitle = "Ordinary mortal";
    } else {
        const SequenceInfo* seq = pathway ? pathway->findSequence(c.sequence) : nullptr;
        sheet.subtitle = "Sequence " + std::to_string(c.sequence) + (seq ? " " + seq->name : "") + ", " +
                         (pathway ? pathway->name + " pathway" : pathwayLabel(db, c.pathwayId));
        if (pathway && !pathway->god.empty()) sheet.subtitle += " (" + pathway->god + ")";
    }

    Section identity{"Identity", {}};
    identity.field("ID", characterCode(c.id));
    identity.field("Aliases", join(c.aliases));
    identity.field("Titles", join(c.titles));
    identity.field("Alignment", c.alignment);
    identity.field("Age", c.age);
    identity.field("Gender", c.gender);
    if (!c.affiliation.organization.empty()) {
        std::string org = c.affiliation.organization;
        if (!c.affiliation.rank.empty()) org += " (" + c.affiliation.rank + ")";
        identity.field("Affiliation", org);
    }
    addSection(sheet, identity);

    Section looks{"Appearance", {}};
    looks.field("Hair", c.appearance.hair);
    looks.field("Eyes", c.appearance.eyes);
    looks.field("Build", c.appearance.build);
    looks.field("Height", c.appearance.height);
    looks.field("Clothing", c.appearance.clothing);
    looks.field("Distinguishing mark", c.appearance.distinguishingMark);
    looks.paragraph(c.appearance.description);
    addSection(sheet, looks);

    Section about{"About", {}};
    about.paragraph(c.shortDescription);
    if (!c.backstory.empty()) {
        about.subheading("Backstory");
        about.paragraph(c.backstory);
    }
    addSection(sheet, about);

    if (!c.honorificName.empty()) {
        Section honorific{"Honorific Name", {}};
        honorific.quote(join(c.honorificName, "\n"));
        addSection(sheet, honorific);
    }

    if (c.hasUniqueness) {
        Section uniqueness{"Uniqueness", {}};
        uniqueness.paragraph(c.uniquenessForm.empty() ? "Holds a Uniqueness." : c.uniquenessForm);
        addSection(sheet, uniqueness);
    }

    // Stat block
    Section stats{"Stat Block", {}};
    Block table{BlockKind::Stats, "", "", {}};
    const auto bonus = statBonuses(c, pathway);
    const auto totals = statTotals(c, pathway);
    for (int i = 0; i < kStatCount; ++i) {
        table.stats.push_back({kStatNames[i], kStatCodes[i], c.stats.base[i], bonus[i], totals[i],
                               abilityModifier(totals[i])});
    }
    stats.blocks.push_back(table);
    if (pathway && (bonus != std::array<int, kStatCount>{})) {
        const int tierNow = tierForSequence(c.sequence);
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
    const auto modes = movementModes(c, pathway);
    stats.field("Movement", join(modes, "; "));
    if (c.stats.hpIncluded) stats.field("HP", std::to_string(c.stats.hp));
    stats.field("Spirituality", std::to_string(c.stats.spirituality));
    addSection(sheet, stats);

    if (pathway) {
        Section abilities{"Abilities", {}};
        for (const SequenceInfo* s : abilitiesUpTo(*pathway, c.sequence)) {
            abilities.subheading("Sequence " + std::to_string(s->sequence) + ": " + s->name);
            for (const auto& a : s->abilities) abilities.bullet(a);
        }
        addSection(sheet, abilities);
    }

    Section artifacts{"Sealed Artifacts", {}};
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

    Section relations{"Relationships", {}};
    for (const auto& r : c.relationships) {
        std::string line = r.type + ": " + relationshipName(db, r);
        if (!r.note.empty()) line += ", " + r.note;
        relations.bullet(line);
    }
    addSection(sheet, relations);

    Section notes{"Notes", {}};
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

    Section look{"Appearance", {}};
    look.paragraph(a.visualDescription);
    addSection(sheet, look);

    Section ability{"Ability", {}};
    ability.paragraph(a.ability);
    addSection(sheet, ability);

    Section drawback{"Drawback", {}};
    drawback.paragraph(a.drawback);
    addSection(sheet, drawback);

    Section notes{"Notes", {}};
    notes.paragraph(a.notes);
    addSection(sheet, notes);

    Section holders{"Held By", {}};
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

}  // namespace lotm
