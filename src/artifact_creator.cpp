#include <iostream>

#include "app.hpp"
#include "render.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace lotm {

namespace {

void stepName(Artifact& a) { a.name = ui::readRequired("Name", a.name); }

void stepVisual(Artifact& a) {
    a.visualDescription = ui::readMultiline("Short visual description (what does it look like?)", a.visualDescription);
}

void stepPathway(const App& app, Artifact& a) {
    std::string current = a.pathwayId.empty() ? "unknown" : pathwayLabel(app.db, a.pathwayId);
    int pick = choosePathway(app, "Which pathway does it belong to? [current: " + current + "]", "Unknown pathway", true);
    if (pick == ui::Choice::kKeep) return;
    a.pathwayId = pick == ui::Choice::kZero ? "" : app.db.pathways[pick].id;
}

void stepSequence(Artifact& a) {
    std::string current = a.sequenceLevel == kUnknownSequence ? "unknown" : std::to_string(a.sequenceLevel);
    while (true) {
        std::string value =
            ui::readLine("Sequence level of its power, 0-9, or U for unknown [" + current + "]: ");
        if (value.empty()) return;
        if (value == "u" || value == "U" || value == "-") {
            a.sequenceLevel = kUnknownSequence;
            return;
        }
        if (value.size() == 1 && value[0] >= '0' && value[0] <= '9') {
            a.sequenceLevel = value[0] - '0';
            return;
        }
        ui::info("Type a single digit from 0 to 9, or U.");
    }
}

void stepAbility(Artifact& a) { a.ability = ui::readMultiline("Its ability", a.ability); }
void stepDrawback(Artifact& a) { a.drawback = ui::readMultiline("Its drawback", a.drawback); }
void stepNotes(Artifact& a) { a.notes = ui::readMultiline("Notes (optional: containment, history ...)", a.notes); }

bool save(App& app, Artifact& a, bool isNew) {
    const auto backup = app.db.artifacts;
    a.updatedAt = nowTimestamp();
    if (isNew) {
        a.createdAt = a.updatedAt;
        app.db.artifacts.push_back(a);
    } else if (Artifact* existing = app.db.findArtifact(a.id)) {
        *existing = a;
    }
    try {
        app.storage.saveArtifacts(app.db);
    } catch (const StorageError& e) {
        app.db.artifacts = backup;
        ui::info(std::string("Saving failed: ") + e.what());
        return false;
    }
    ui::info("Saved " + a.name + " as " + artifactCode(a.id) + ".");
    return true;
}

}  // namespace

std::optional<int> runArtifactCreator(App& app, std::optional<int> editId) {
    Artifact a;
    bool isNew = true;
    if (editId) {
        if (const Artifact* existing = app.db.findArtifact(*editId)) {
            a = *existing;
            isNew = false;
        }
    }

    if (isNew) {
        a.id = app.db.nextArtifactId();
        ui::header("Create a Sealed Artifact");
        ui::info("Press Enter to skip optional fields. You can change everything on the review screen.");
        stepName(a);
        stepVisual(a);
        stepPathway(app, a);
        stepSequence(a);
        stepAbility(a);
        stepDrawback(a);
        stepNotes(a);
    }

    while (true) {
        ui::header(isNew ? "Review the new Sealed Artifact" : "Edit Sealed Artifact");
        std::cout << renderText(buildArtifactSheet(a, app.db));
        int pick = ui::choose("\nChange something, or save:",
                              {"Name", "Visual description", "Pathway", "Sequence level", "Ability", "Drawback",
                               "Notes", "Save"},
                              "Cancel without saving");
        switch (pick) {
            case 0: stepName(a); break;
            case 1: stepVisual(a); break;
            case 2: stepPathway(app, a); break;
            case 3: stepSequence(a); break;
            case 4: stepAbility(a); break;
            case 5: stepDrawback(a); break;
            case 6: stepNotes(a); break;
            case 7:
                if (save(app, a, isNew)) return a.id;
                break;
            default:
                if (ui::yesNo("Discard these changes?", false)) return std::nullopt;
        }
    }
}

}  // namespace lotm
