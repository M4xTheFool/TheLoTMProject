// Writing sheets to the exports folder. Used by the console Export menu and the app window.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app.hpp"
#include "sheet.hpp"

namespace lotm {

enum class ExportFormat { Html, Markdown, Text };

// File-name-safe version of a name: "Klein Moretti" -> "klein-moretti".
std::string fileSlug(const std::string& name);

// "character-1-klein-moretti", "artifact-2-creeping-hunger", "pathway-maestro", "catalogue-20261009-140533".
std::string exportName(const Character& c);
std::string exportName(const Artifact& a);
std::string exportName(const Pathway& p);
std::string catalogueExportName();

// Every character, every Sealed Artifact and your own pathways, for exporting everything at once.
std::vector<Sheet> catalogueSheets(const Database& db);

// Writes the sheets to <exports folder>/<baseName>.html/.md/.txt and returns the file's full path.
// Throws StorageError when the file can't be written.
std::filesystem::path writeExport(const App& app, const std::string& baseName, ExportFormat format,
                                  const std::vector<Sheet>& sheets, const std::string& pageTitle);

// Opens a file with the program the computer normally uses for it. Returns false if that failed.
bool openWithDefaultApp(const std::filesystem::path& file);

}  // namespace lotm
