#include "exporter.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>

#include "render.hpp"
#include "storage.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace lotm {

namespace {

const char* extension(ExportFormat f) {
    switch (f) {
        case ExportFormat::Html: return ".html";
        case ExportFormat::Markdown: return ".md";
        default: return ".txt";
    }
}

std::string compactTimestamp() {
    std::string out;
    for (char c : nowTimestamp()) {  // "2026-10-09 14:05:33" -> "20261009-140533"
        if (std::isdigit(static_cast<unsigned char>(c))) out += c;
        else if (c == ' ') out += '-';
    }
    return out;
}

}  // namespace

std::string fileSlug(const std::string& name) {
    std::string out;
    for (unsigned char c : name) {
        if (std::isalnum(c) && c < 128) out += static_cast<char>(std::tolower(c));
        else if (!out.empty() && out.back() != '-') out += '-';
        if (out.size() >= 40) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "unnamed" : out;
}

std::string exportName(const Character& c) { return "character-" + std::to_string(c.id) + "-" + fileSlug(c.name); }
std::string exportName(const Artifact& a) { return "artifact-" + std::to_string(a.id) + "-" + fileSlug(a.name); }
std::string exportName(const Pathway& p) { return "pathway-" + fileSlug(p.id); }
std::string catalogueExportName() { return "catalogue-" + compactTimestamp(); }

std::vector<Sheet> catalogueSheets(const Database& db) {
    std::vector<Sheet> sheets;
    for (const auto& c : db.characters) sheets.push_back(buildCharacterSheet(c, db));
    for (const auto& a : db.artifacts) sheets.push_back(buildArtifactSheet(a, db));
    for (const auto& p : db.pathways) {
        if (p.custom) sheets.push_back(buildPathwaySheet(p));
    }
    return sheets;
}

fs::path writeExport(const App& app, const std::string& baseName, ExportFormat format, const std::vector<Sheet>& sheets,
                     const std::string& pageTitle) {
    std::error_code ec;
    fs::create_directories(app.storage.exportsDir(), ec);
    const fs::path file = app.storage.exportsDir() / (baseName + extension(format));

    std::string content;
    if (format == ExportFormat::Html) {
        content = renderHtmlPage(pageTitle, sheets, app.db.settings.exportTheme);
    } else {
        for (size_t i = 0; i < sheets.size(); ++i) {
            if (i > 0) content += format == ExportFormat::Markdown ? "\n---\n\n" : "\n\n";
            content += format == ExportFormat::Markdown ? renderMarkdown(sheets[i]) : renderText(sheets[i]);
        }
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    if (!out) throw StorageError("Could not write " + file.string());
    return fs::absolute(file, ec);
}

bool openWithDefaultApp(const fs::path& file) {
#if defined(_WIN32)
    // ShellExecute handles any letters in the path and doesn't flash a console window.
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", file.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
#else
    const std::string path = file.string();
    if (path.find('"') != std::string::npos) return false;
#if defined(__APPLE__)
    const std::string command = "open \"" + path + "\"";
#else
    const std::string command = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
#endif
    return std::system(command.c_str()) == 0;
#endif
}

}  // namespace lotm
