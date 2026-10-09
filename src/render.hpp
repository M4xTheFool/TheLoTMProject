// Draws sheets as plain text (for the console and .txt export), Markdown or styled HTML.
#pragma once

#include <string>
#include <vector>

#include "sheet.hpp"

namespace lotm {

std::string renderText(const Sheet& sheet);
std::string renderMarkdown(const Sheet& sheet);

// One complete HTML page. theme is "auto", "light" or "dark".
// With more than one sheet the page starts with a table of contents.
std::string renderHtmlPage(const std::string& pageTitle, const std::vector<Sheet>& sheets, const std::string& theme);

std::string escapeHtml(const std::string& text);

}  // namespace lotm
