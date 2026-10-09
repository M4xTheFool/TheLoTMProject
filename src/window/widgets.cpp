// Controls shared by every screen of the app window.
#include <algorithm>
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
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.25f);
    ImGui::PushStyleColor(ImGuiCol_Text, kGold);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Separator();
}

// A sheet drawn with the window's own controls: the same content as the exports.
void drawSheet(const WindowState& w, const Sheet& sheet) {
    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.7f);
    ImGui::TextWrapped("%s", sheet.title.c_str());
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
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
                    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
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
                    ImGui::PushStyleColor(ImGuiCol_Text, kGold);
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
                    if (ImGui::BeginTable("stats", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                        for (const char* column : {"Stat", "Base", "Bonus", "Total", "Mod"}) {
                            ImGui::TableSetupColumn(column);
                        }
                        ImGui::TableHeadersRow();
                        for (const auto& row : block.stats) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(row.name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%d", row.base);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(row.bonus == 0 ? "-" : signedNumber(row.bonus).c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%d", row.total);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(signedNumber(row.modifier).c_str());
                        }
                        ImGui::EndTable();
                    }
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
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
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
        ImGui::TextColored(kWarning, options.isNew ? "Not saved yet" : "Unsaved changes");
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
