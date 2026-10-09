#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>

#include "app.hpp"
#include "render.hpp"
#include "sheet.hpp"
#include "ui.hpp"

namespace fs = std::filesystem;

namespace lotm {

namespace {

enum class Format { Html, Markdown, Text };

std::optional<Format> chooseFormat() {
    int pick = ui::choose("Which format?",
                          {"HTML (styled sheet, open in a browser, print to PDF)", "Markdown (Discord, Obsidian, wikis)",
                           "Plain text"},
                          "Cancel");
    if (pick == 0) return Format::Html;
    if (pick == 1) return Format::Markdown;
    if (pick == 2) return Format::Text;
    return std::nullopt;
}

const char* extension(Format f) {
    switch (f) {
        case Format::Html: return ".html";
        case Format::Markdown: return ".md";
        default: return ".txt";
    }
}

// File-name-safe version of a name: "Klein Moretti" -> "klein-moretti".
std::string slug(const std::string& name) {
    std::string out;
    for (unsigned char c : name) {
        if (std::isalnum(c) && c < 128) out += static_cast<char>(std::tolower(c));
        else if (!out.empty() && out.back() != '-') out += '-';
        if (out.size() >= 40) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "unnamed" : out;
}

std::string compactTimestamp() {
    std::string stamp = nowTimestamp();  // "2026-10-09 14:05:33"
    std::string out;
    for (char c : stamp) {
        if (std::isdigit(static_cast<unsigned char>(c))) out += c;
        else if (c == ' ') out += '-';
    }
    return out;
}

void openFile(const fs::path& file) {
    const std::string path = file.string();
    if (path.find('"') != std::string::npos) return;
#if defined(_WIN32)
    const std::string command = "start \"\" \"" + path + "\"";
#elif defined(__APPLE__)
    const std::string command = "open \"" + path + "\"";
#else
    const std::string command = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
#endif
    if (std::system(command.c_str()) != 0) ui::info("Could not open it automatically; open the file yourself.");
}

void writeExport(App& app, const std::string& baseName, Format format, const std::vector<Sheet>& sheets,
                 const std::string& pageTitle) {
    std::error_code ec;
    fs::create_directories(app.storage.exportsDir(), ec);
    const fs::path file = app.storage.exportsDir() / (baseName + extension(format));

    std::string content;
    if (format == Format::Html) {
        content = renderHtmlPage(pageTitle, sheets, app.db.settings.exportTheme);
    } else {
        for (size_t i = 0; i < sheets.size(); ++i) {
            if (i > 0) content += format == Format::Markdown ? "\n---\n\n" : "\n\n";
            content += format == Format::Markdown ? renderMarkdown(sheets[i]) : renderText(sheets[i]);
        }
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << content;
    if (!out) {
        ui::info("Could not write " + file.string());
        return;
    }
    out.close();
    ui::info("Exported to " + fs::absolute(file, ec).string());
    if (ui::yesNo("Open it now?", format == Format::Html)) openFile(file);
}

std::vector<Sheet> allSheets(const App& app) {
    std::vector<Sheet> sheets;
    for (const auto& c : app.db.characters) sheets.push_back(buildCharacterSheet(c, app.db));
    for (const auto& a : app.db.artifacts) sheets.push_back(buildArtifactSheet(a, app.db));
    for (const auto& p : app.db.pathways) {
        if (p.custom) sheets.push_back(buildPathwaySheet(p));
    }
    return sheets;
}

}  // namespace

void exportCharacter(App& app, int id) {
    const Character* c = app.db.findCharacter(id);
    if (!c) return;
    auto format = chooseFormat();
    if (!format) return;
    writeExport(app, "character-" + std::to_string(id) + "-" + slug(c->name), *format,
                {buildCharacterSheet(*c, app.db)}, c->name);
}

void exportArtifact(App& app, int id) {
    const Artifact* a = app.db.findArtifact(id);
    if (!a) return;
    auto format = chooseFormat();
    if (!format) return;
    writeExport(app, "artifact-" + std::to_string(id) + "-" + slug(a->name), *format,
                {buildArtifactSheet(*a, app.db)}, a->name);
}

void exportPathway(App& app, const std::string& id) {
    const Pathway* p = app.db.findPathway(id);
    if (!p) return;
    auto format = chooseFormat();
    if (!format) return;
    writeExport(app, "pathway-" + slug(p->id), *format, {buildPathwaySheet(*p)}, p->name + " pathway");
}

void runExportMenu(App& app) {
    while (true) {
        ui::header("Export");
        ui::info("Files go to: " + app.storage.exportsDir().string());
        int pick = ui::choose("",
                              {"One character", "One Sealed Artifact", "One pathway",
                               "Everything (characters, Sealed Artifacts and your own pathways in one file)"},
                              "Back to the main menu");
        if (pick == 0) {
            if (app.db.characters.empty()) { ui::info("No characters yet."); continue; }
            std::vector<std::string> names;
            for (const auto& c : app.db.characters) names.push_back(c.name + " (" + characterCode(c.id) + ")");
            int which = ui::choose("Which character?", names, "Cancel");
            if (which >= 0) exportCharacter(app, app.db.characters[which].id);
        } else if (pick == 1) {
            if (app.db.artifacts.empty()) { ui::info("No Sealed Artifacts yet."); continue; }
            std::vector<std::string> names;
            for (const auto& a : app.db.artifacts) names.push_back(a.name + " (" + artifactCode(a.id) + ")");
            int which = ui::choose("Which Sealed Artifact?", names, "Cancel");
            if (which >= 0) exportArtifact(app, app.db.artifacts[which].id);
        } else if (pick == 2) {
            int which = choosePathway(app, "Which pathway?", "Cancel", false);
            if (which >= 0) exportPathway(app, app.db.pathways[which].id);
        } else if (pick == 3) {
            if (app.db.characters.empty() && app.db.artifacts.empty()) { ui::info("The catalogue is empty."); continue; }
            auto format = chooseFormat();
            if (format) writeExport(app, "catalogue-" + compactTimestamp(), *format, allSheets(app), "The Catalogue");
        } else {
            return;
        }
    }
}

}  // namespace lotm
