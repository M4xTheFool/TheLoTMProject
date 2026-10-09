#include "render.hpp"

#include <cctype>
#include <sstream>

#include "rules.hpp"

namespace lotm {

namespace {

std::string indentLines(const std::string& text, const std::string& indent) {
    std::string out = indent;
    for (char c : text) {
        out += c;
        if (c == '\n') out += indent;
    }
    return out;
}

std::string pad(const std::string& text, size_t width) {
    if (text.size() >= width) return text;
    return text + std::string(width - text.size(), ' ');
}

std::string padLeft(const std::string& text, size_t width) {
    if (text.size() >= width) return text;
    return std::string(width - text.size(), ' ') + text;
}

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

std::string escapeMarkdown(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\' || c == '`' || c == '*' || c == '_' || c == '[' || c == ']' || c == '<' || c == '>' ||
            c == '|' || c == '#') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

// Markdown needs two trailing spaces to keep a single line break.
std::string markdownLines(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\n') out += "  \n";
        else out += c;
    }
    return out;
}

// "Spirit Vision: sees auras" -> {"Spirit Vision", "sees auras"}, so the name can be shown in bold.
// Only a short name before the first ": " counts; anything else stays one piece of text.
bool splitNamed(const std::string& text, std::string& name, std::string& rest) {
    const size_t colon = text.find(": ");
    if (colon == std::string::npos || colon == 0 || colon > 48) return false;
    if (text.find(". ") < colon) return false;
    name = text.substr(0, colon);
    rest = text.substr(colon + 2);
    return true;
}

std::string htmlLines(const std::string& text) {
    std::string escaped = escapeHtml(text);
    std::string out;
    for (char c : escaped) {
        if (c == '\n') out += "<br>";
        else out += c;
    }
    return out;
}

}  // namespace

std::string escapeHtml(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

// ---------------------------------------------------------------- plain text

std::string renderText(const Sheet& sheet) {
    std::ostringstream out;
    const std::string rule(60, '=');
    out << rule << "\n " << upper(sheet.title) << "\n";
    if (!sheet.subtitle.empty()) out << " " << sheet.subtitle << "\n";
    out << rule << "\n";
    for (const auto& section : sheet.sections) {
        out << "\n-- " << section.heading << " --\n";
        for (const auto& block : section.blocks) {
            switch (block.kind) {
                case BlockKind::Field:
                    if (block.text.find('\n') != std::string::npos) {
                        out << "  " << block.label << ":\n" << indentLines(block.text, "    ") << "\n";
                    } else {
                        out << "  " << block.label << ": " << block.text << "\n";
                    }
                    break;
                case BlockKind::Paragraph: out << indentLines(block.text, "  ") << "\n"; break;
                case BlockKind::Bullet: out << "    - " << block.text << "\n"; break;
                case BlockKind::Subheading: out << "  [" << block.text << "]\n"; break;
                case BlockKind::Quote: {
                    std::istringstream lines(block.text);
                    for (std::string line; std::getline(lines, line);) out << "    \"" << line << "\"\n";
                    break;
                }
                case BlockKind::Stats:
                    out << "  Stat           Base  Bonus  Total  Mod\n";
                    for (const auto& s : block.stats) {
                        out << "  " << pad(s.name, 13) << padLeft(std::to_string(s.base), 6)
                            << padLeft(s.bonus == 0 ? "-" : signedNumber(s.bonus), 7)
                            << padLeft(std::to_string(s.total), 7) << padLeft(signedNumber(s.modifier), 5) << "\n";
                    }
                    break;
            }
        }
    }
    if (!sheet.footer.empty()) out << "\n  (" << sheet.footer << ")\n";
    return out.str();
}

// ---------------------------------------------------------------- Markdown

std::string renderMarkdown(const Sheet& sheet) {
    std::ostringstream out;
    out << "# " << escapeMarkdown(sheet.title) << "\n\n";
    if (!sheet.subtitle.empty()) out << "*" << escapeMarkdown(sheet.subtitle) << "*\n\n";
    for (const auto& section : sheet.sections) {
        out << "## " << escapeMarkdown(section.heading) << "\n\n";
        BlockKind previous = BlockKind::Paragraph;
        bool first = true;
        for (const auto& block : section.blocks) {
            // Close a run of bullets or fields with a blank line before something else.
            if (!first && (previous == BlockKind::Bullet || previous == BlockKind::Field) && block.kind != previous) {
                out << "\n";
            }
            switch (block.kind) {
                case BlockKind::Field:
                    out << "- **" << escapeMarkdown(block.label) << ":** " << markdownLines(escapeMarkdown(block.text))
                        << "\n";
                    break;
                case BlockKind::Paragraph: out << markdownLines(escapeMarkdown(block.text)) << "\n\n"; break;
                case BlockKind::Bullet: {
                    std::string name, rest;
                    if (splitNamed(block.text, name, rest)) {
                        out << "- **" << escapeMarkdown(name) << ":** " << escapeMarkdown(rest) << "\n";
                    } else {
                        out << "- " << escapeMarkdown(block.text) << "\n";
                    }
                    break;
                }
                case BlockKind::Subheading: out << "### " << escapeMarkdown(block.text) << "\n\n"; break;
                case BlockKind::Quote: {
                    std::string text = escapeMarkdown(block.text);
                    out << "> *";
                    for (char c : text) {
                        if (c == '\n') out << "*  \n> *";
                        else out << c;
                    }
                    out << "*\n\n";
                    break;
                }
                case BlockKind::Stats:
                    out << "| Stat | Base | Bonus | Total | Mod |\n| --- | ---: | ---: | ---: | ---: |\n";
                    for (const auto& s : block.stats) {
                        out << "| " << s.name << " | " << s.base << " | " << (s.bonus == 0 ? "-" : signedNumber(s.bonus))
                            << " | **" << s.total << "** | " << signedNumber(s.modifier) << " |\n";
                    }
                    out << "\n";
                    break;
            }
            previous = block.kind;
            first = false;
        }
        if (previous == BlockKind::Bullet || previous == BlockKind::Field) out << "\n";
    }
    if (!sheet.footer.empty()) out << "<sub>" << escapeMarkdown(sheet.footer) << "</sub>\n";
    return out.str();
}

// ---------------------------------------------------------------- HTML

namespace {

const char* kStyle = R"CSS(
:root {
  --bg: #efe6d2; --page: #f8f1e1; --ink: #2a2118; --muted: #6b5a45; --accent: #7a1f1f;
  --gold: #9a7b2f; --rule: #d6c6a4; --card: #fffaf0; --shadow: rgba(60, 40, 10, 0.12);
}
:root[data-theme="dark"] {
  --bg: #121110; --page: #1c1a17; --ink: #ece3cf; --muted: #b3a387; --accent: #e07a6a;
  --gold: #d1ad5b; --rule: #3b352b; --card: #24211c; --shadow: rgba(0, 0, 0, 0.4);
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #121110; --page: #1c1a17; --ink: #ece3cf; --muted: #b3a387; --accent: #e07a6a;
    --gold: #d1ad5b; --rule: #3b352b; --card: #24211c; --shadow: rgba(0, 0, 0, 0.4);
  }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink);
  font-family: Georgia, "Iowan Old Style", "Palatino Linotype", "Times New Roman", serif; line-height: 1.55; }
main { max-width: 860px; margin: 0 auto; padding: 32px 16px 64px; }
.toolbar { display: flex; justify-content: flex-end; margin-bottom: 12px; }
.toolbar button { font: inherit; font-size: 14px; color: var(--ink); background: var(--card);
  border: 1px solid var(--rule); border-radius: 6px; padding: 6px 12px; cursor: pointer; }
.contents, .sheet { background: var(--page); border: 1px solid var(--rule); border-radius: 10px;
  box-shadow: 0 2px 12px var(--shadow); padding: 32px 36px; margin-bottom: 28px; }
.contents h1 { margin-top: 0; }
.contents ul { columns: 2; padding-left: 20px; }
.contents a { color: var(--accent); }
.kind { font-variant: small-caps; letter-spacing: 0.12em; color: var(--gold); font-size: 15px; }
.sheet h1 { font-size: 34px; margin: 2px 0 4px; line-height: 1.2; }
.subtitle { font-style: italic; color: var(--muted); margin: 0 0 8px; }
.sheet h2 { font-size: 20px; color: var(--accent); margin: 28px 0 10px; padding-bottom: 4px;
  border-bottom: 2px solid var(--rule); }
.sheet h3 { font-size: 16px; margin: 16px 0 6px; color: var(--ink); }
dl { display: grid; grid-template-columns: max-content 1fr; gap: 4px 16px; margin: 8px 0; }
dt { color: var(--muted); }
dd { margin: 0; }
blockquote { margin: 8px 0; padding: 8px 16px; border-left: 3px solid var(--gold); font-style: italic; }
.stats { display: grid; grid-template-columns: repeat(6, 1fr); gap: 10px; margin: 12px 0 16px; }
.stat { background: var(--card); border: 1px solid var(--rule); border-radius: 8px; text-align: center; padding: 10px 4px; }
.stat .code { font-variant: small-caps; letter-spacing: 0.08em; color: var(--muted); font-size: 14px; }
.stat .total { font-size: 28px; font-weight: bold; line-height: 1.2; }
.stat .mod { font-size: 15px; color: var(--accent); }
.stat .parts { font-size: 12px; color: var(--muted); }
.sheet li strong { color: var(--ink); }
.dossier { position: relative; margin-top: 28px; padding: 6px 22px 14px; background: var(--card);
  border: 1px dashed var(--muted); border-radius: 4px; font-family: "Courier New", Courier, monospace; font-size: 15px; }
.dossier h2 { color: var(--ink); font-size: 17px; letter-spacing: 0.18em; text-transform: uppercase;
  border-bottom: 1px solid var(--rule); margin-top: 14px; }
.dossier .stamp { position: absolute; top: 18px; right: 18px; transform: rotate(-7deg); border: 2px solid var(--accent);
  border-radius: 4px; color: var(--accent); padding: 1px 10px; font-weight: bold; letter-spacing: 0.15em;
  text-transform: uppercase; font-size: 13px; opacity: 0.85; }
.footer { margin-top: 28px; font-size: 13px; color: var(--muted); }
@media (max-width: 600px) {
  .stats { grid-template-columns: repeat(3, 1fr); }
  .contents ul { columns: 1; }
  .contents, .sheet { padding: 20px 16px; }
}
@media print {
  body { background: #fff; }
  .toolbar { display: none; }
  .contents, .sheet { box-shadow: none; border: none; }
  .sheet { break-before: page; }
  .sheet:first-of-type { break-before: auto; }
}
)CSS";

const char* kScript = R"JS(
(function () {
  var root = document.documentElement;
  var button = document.getElementById('theme-toggle');
  function current() {
    var set = root.getAttribute('data-theme');
    if (set) return set;
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }
  function label() { button.textContent = current() === 'dark' ? 'Light theme' : 'Dark theme'; }
  button.addEventListener('click', function () {
    root.setAttribute('data-theme', current() === 'dark' ? 'light' : 'dark');
    label();
  });
  label();
})();
)JS";

void renderHtmlSheet(std::ostringstream& out, const Sheet& sheet) {
    out << "<article class=\"sheet\" id=\"" << escapeHtml(sheet.anchor) << "\">\n";
    out << "<div class=\"kind\">" << escapeHtml(sheet.kind) << "</div>\n";
    out << "<h1>" << escapeHtml(sheet.title) << "</h1>\n";
    if (!sheet.subtitle.empty()) out << "<p class=\"subtitle\">" << escapeHtml(sheet.subtitle) << "</p>\n";
    for (const auto& section : sheet.sections) {
        if (section.style == "dossier") {
            out << "<section class=\"dossier\">\n<div class=\"stamp\">Confidential</div>\n<h2>"
                << escapeHtml(section.heading) << "</h2>\n";
        } else {
            out << "<section>\n<h2>" << escapeHtml(section.heading) << "</h2>\n";
        }
        bool inList = false;
        bool inFields = false;
        for (const auto& block : section.blocks) {
            if (inList && block.kind != BlockKind::Bullet) { out << "</ul>\n"; inList = false; }
            if (inFields && block.kind != BlockKind::Field) { out << "</dl>\n"; inFields = false; }
            switch (block.kind) {
                case BlockKind::Field:
                    if (!inFields) { out << "<dl>\n"; inFields = true; }
                    out << "<dt>" << escapeHtml(block.label) << "</dt><dd>" << htmlLines(block.text) << "</dd>\n";
                    break;
                case BlockKind::Paragraph: out << "<p>" << htmlLines(block.text) << "</p>\n"; break;
                case BlockKind::Bullet:
                    if (!inList) { out << "<ul>\n"; inList = true; }
                    if (std::string name, rest; splitNamed(block.text, name, rest)) {
                        out << "<li><strong>" << escapeHtml(name) << ":</strong> " << escapeHtml(rest) << "</li>\n";
                    } else {
                        out << "<li>" << escapeHtml(block.text) << "</li>\n";
                    }
                    break;
                case BlockKind::Subheading: out << "<h3>" << escapeHtml(block.text) << "</h3>\n"; break;
                case BlockKind::Quote: out << "<blockquote>" << htmlLines(block.text) << "</blockquote>\n"; break;
                case BlockKind::Stats:
                    out << "<div class=\"stats\">\n";
                    for (const auto& s : block.stats) {
                        out << "<div class=\"stat\" title=\"" << escapeHtml(s.name) << "\">"
                            << "<div class=\"code\">" << escapeHtml(s.code) << "</div>"
                            << "<div class=\"total\">" << s.total << "</div>"
                            << "<div class=\"mod\">" << signedNumber(s.modifier) << "</div>"
                            << "<div class=\"parts\">" << s.base;
                        if (s.bonus != 0) out << " " << signedNumber(s.bonus);
                        out << "</div></div>\n";
                    }
                    out << "</div>\n";
                    break;
            }
        }
        if (inList) out << "</ul>\n";
        if (inFields) out << "</dl>\n";
        out << "</section>\n";
    }
    if (!sheet.footer.empty()) out << "<p class=\"footer\">" << escapeHtml(sheet.footer) << "</p>\n";
    out << "</article>\n";
}

}  // namespace

std::string renderHtmlPage(const std::string& pageTitle, const std::vector<Sheet>& sheets, const std::string& theme) {
    std::ostringstream out;
    out << "<!doctype html>\n<html lang=\"en\"";
    if (theme == "light" || theme == "dark") out << " data-theme=\"" << theme << "\"";
    out << ">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<title>" << escapeHtml(pageTitle) << "</title>\n<style>" << kStyle << "</style>\n</head>\n<body>\n<main>\n"
        << "<div class=\"toolbar\"><button id=\"theme-toggle\" type=\"button\">Dark theme</button></div>\n";

    if (sheets.size() > 1) {
        out << "<nav class=\"contents\">\n<div class=\"kind\">Catalogue</div>\n<h1>" << escapeHtml(pageTitle)
            << "</h1>\n";
        for (const char* kind : {"Character", "Sealed Artifact"}) {
            bool any = false;
            for (const auto& s : sheets) {
                if (s.kind != kind) continue;
                if (!any) {
                    out << "<h2>" << (std::string(kind) == "Character" ? "Characters" : "Sealed Artifacts")
                        << "</h2>\n<ul>\n";
                    any = true;
                }
                out << "<li><a href=\"#" << escapeHtml(s.anchor) << "\">" << escapeHtml(s.title) << "</a></li>\n";
            }
            if (any) out << "</ul>\n";
        }
        out << "</nav>\n";
    }
    for (const auto& sheet : sheets) renderHtmlSheet(out, sheet);
    out << "</main>\n<script>" << kScript << "</script>\n</body>\n</html>\n";
    return out.str();
}

}  // namespace lotm
