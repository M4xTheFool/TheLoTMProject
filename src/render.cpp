#include "render.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <tuple>
#include <utility>

#include "pathway_colours.hpp"
#include "rules.hpp"

namespace lotm {

namespace {

// A run of Sequences in a row from the same pathway, for drawing the ladder as text.
struct LadderRun {
    std::string label;    // "Sequences 9-5", "Sequence 4", "Above the Sequence"
    std::string pathway;  // empty Above the Sequence
    std::vector<std::string> names;
    bool current = false;
};

std::vector<LadderRun> ladderRuns(const std::vector<LadderRung>& rungs) {
    std::vector<LadderRun> runs;
    std::string lastPathway = "\x01";
    int first = 0, last = 0;
    auto label = [&] {
        return first == last ? "Sequence " + std::to_string(first)
                             : "Sequences " + std::to_string(first) + "-" + std::to_string(last);
    };
    for (const LadderRung& rung : rungs) {
        if (rung.above) {
            runs.push_back({"Above the Sequence", "", {rung.name}, rung.current});
            lastPathway = "\x01";
            continue;
        }
        if (rung.pathwayId == lastPathway && !runs.empty()) {
            last = rung.sequence;
            runs.back().label = label();
            runs.back().names.push_back(rung.name);
            runs.back().current = runs.back().current || rung.current;
            continue;
        }
        first = last = rung.sequence;
        lastPathway = rung.pathwayId;
        runs.push_back({label(), rung.pathway, {rung.name}, rung.current});
    }
    return runs;
}

std::string joinNames(const std::vector<std::string>& names, const std::string& separator) {
    std::string out;
    for (const auto& name : names) out += (out.empty() ? "" : separator) + name;
    return out;
}

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
    std::tie(name, rest) = splitAbility(text);
    return !name.empty();
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
    if (!sheet.badges.empty()) {
        std::vector<std::string> badges;
        for (const Badge& badge : sheet.badges) badges.push_back(badge.text);
        out << " " << joinNames(badges, " | ") << "\n";
    }
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
                case BlockKind::Ladder:
                    for (const LadderRun& run : ladderRuns(block.rungs)) {
                        out << "  " << pad(run.label, 15);
                        if (!run.pathway.empty()) out << run.pathway << " pathway: ";
                        out << joinNames(run.names, ", ") << (run.current ? " (now)" : "") << "\n";
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
    if (!sheet.badges.empty()) {
        std::vector<std::string> badges;
        for (const Badge& badge : sheet.badges) badges.push_back("**" + escapeMarkdown(badge.text) + "**");
        out << joinNames(badges, " \xC2\xB7 ") << "\n\n";
    }
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
                case BlockKind::Ladder:
                    for (const LadderRun& run : ladderRuns(block.rungs)) {
                        out << "- **" << escapeMarkdown(run.label) << (run.pathway.empty() ? ":** " : "**, ");
                        if (!run.pathway.empty()) out << escapeMarkdown(run.pathway) << " pathway: ";
                        out << escapeMarkdown(joinNames(run.names, ", ")) << (run.current ? " *(now)*" : "") << "\n";
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
  --good: #3f7d3c; --orange: #b0561c;
}
.tone-pathway, .rung, .legend .key { --c: var(--cl); }
:root[data-theme="dark"] {
  --bg: #121110; --page: #1c1a17; --ink: #ece3cf; --muted: #b3a387; --accent: #e07a6a;
  --gold: #d1ad5b; --rule: #3b352b; --card: #24211c; --shadow: rgba(0, 0, 0, 0.4);
  --good: #87c285; --orange: #f0904e;
}
:root[data-theme="dark"] .tone-pathway, :root[data-theme="dark"] .rung, :root[data-theme="dark"] .legend .key { --c: var(--cd); }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #121110; --page: #1c1a17; --ink: #ece3cf; --muted: #b3a387; --accent: #e07a6a;
    --gold: #d1ad5b; --rule: #3b352b; --card: #24211c; --shadow: rgba(0, 0, 0, 0.4);
    --good: #87c285; --orange: #f0904e;
  }
  :root:not([data-theme="light"]) .tone-pathway, :root:not([data-theme="light"]) .rung,
  :root:not([data-theme="light"]) .legend .key { --c: var(--cd); }
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
.badges { display: flex; flex-wrap: wrap; gap: 6px; margin: 2px 0 10px; }
.badge { font-size: 13px; line-height: 1.5; padding: 1px 10px; border-radius: 999px; border: 1px solid var(--c);
  color: var(--c); background: color-mix(in srgb, var(--c) 12%, transparent); }
.tone-muted { --c: var(--muted); } .tone-gold { --c: var(--gold); } .tone-good { --c: var(--good); }
.tone-orange { --c: var(--orange); } .tone-danger { --c: var(--accent); }
.ladder { display: flex; flex-wrap: wrap; row-gap: 14px; margin: 6px 0 8px; padding-top: 8px; overflow: hidden; }
.rung { position: relative; flex: 0 0 78px; text-align: center; }
.rung + .rung::before { content: ""; position: absolute; top: 17px; right: 50%; width: 100%; height: 3px;
  background: var(--c); opacity: 0.75; }
.rung.switched::before { opacity: 1; background: repeating-linear-gradient(90deg, var(--c) 0 6px, transparent 6px 10px); }
.rung.risky::before { background: repeating-linear-gradient(90deg, var(--accent) 0 6px, transparent 6px 10px); }
.rung .dot { position: relative; z-index: 1; width: 36px; height: 36px; margin: 0 auto; border-radius: 50%;
  border: 2px solid var(--c); color: var(--c); background: var(--card); font-weight: bold; font-size: 16px; line-height: 32px; }
.rung.current .dot { box-shadow: 0 0 0 3px var(--page), 0 0 0 5px var(--gold); }
.rung.risky .dot::after { content: "!"; position: absolute; top: -7px; right: -7px; width: 16px; height: 16px;
  border-radius: 50%; background: var(--accent); color: var(--page); font-size: 11px; line-height: 16px; }
.rung .name { font-size: 12px; line-height: 1.25; color: var(--muted); margin-top: 6px; padding: 0 3px; overflow-wrap: anywhere; }
.rung.current .name { color: var(--ink); font-weight: bold; }
.legend { display: flex; flex-wrap: wrap; gap: 4px 16px; font-size: 13px; color: var(--muted); margin: 2px 0 10px; }
.legend .key::before { content: ""; display: inline-block; width: 10px; height: 10px; border-radius: 50%;
  background: var(--c); margin-right: 6px; }
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
  .rung { flex-basis: 64px; }
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

// The two colours of a pathway: as stored (for the dark theme) and darker for the light theme.
std::string colourVariables(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return "";
    Rgb colour;
    try {
        colour = {std::stoi(hex.substr(1, 2), nullptr, 16), std::stoi(hex.substr(3, 2), nullptr, 16),
                  std::stoi(hex.substr(5, 2), nullptr, 16)};
    } catch (const std::exception&) {
        return "";
    }
    return " style=\"--cd:" + hexColour(colour) + ";--cl:" + hexColour(onLightBackground(colour)) + "\"";
}

void renderHtmlLadder(std::ostringstream& out, const std::vector<LadderRung>& rungs) {
    out << "<div class=\"ladder\">\n";
    for (const LadderRung& rung : rungs) {
        std::string classes = "rung";
        if (rung.current) classes += " current";
        if (rung.switched) classes += " switched";
        if (rung.risky) classes += " risky";
        std::string tip = rung.above ? "Above the Sequence: " + rung.name
                                     : "Sequence " + std::to_string(rung.sequence) + ": " + rung.name + ", " +
                                           rung.pathway + " pathway";
        if (rung.switched) tip += rung.risky ? " (switched here from a pathway that isn't a neighbour)" : " (switched here)";
        out << "<div class=\"" << classes << "\"" << colourVariables(rung.colour) << " title=\"" << escapeHtml(tip)
            << "\"><div class=\"dot\">" << (rung.above ? std::string("&infin;") : std::to_string(rung.sequence))
            << "</div><div class=\"name\">" << escapeHtml(rung.name) << "</div></div>\n";
    }
    out << "</div>\n<div class=\"legend\">";
    std::vector<std::string> shown;
    bool switched = false, risky = false;
    for (const LadderRung& rung : rungs) {
        switched = switched || rung.switched;
        risky = risky || rung.risky;
        if (rung.above || std::find(shown.begin(), shown.end(), rung.pathwayId) != shown.end()) continue;
        shown.push_back(rung.pathwayId);
        out << "<span class=\"key\"" << colourVariables(rung.colour) << ">" << escapeHtml(rung.pathway)
            << " pathway</span>";
    }
    if (switched) out << "<span>Dashed line: a switch of pathway</span>";
    if (risky) out << "<span>Red: to a pathway that isn't a neighbour</span>";
    out << "</div>\n";
}

void renderHtmlSheet(std::ostringstream& out, const Sheet& sheet) {
    out << "<article class=\"sheet\" id=\"" << escapeHtml(sheet.anchor) << "\">\n";
    out << "<div class=\"kind\">" << escapeHtml(sheet.kind) << "</div>\n";
    out << "<h1>" << escapeHtml(sheet.title) << "</h1>\n";
    if (!sheet.subtitle.empty()) out << "<p class=\"subtitle\">" << escapeHtml(sheet.subtitle) << "</p>\n";
    if (!sheet.badges.empty()) {
        out << "<div class=\"badges\">";
        for (const Badge& badge : sheet.badges) {
            out << "<span class=\"badge tone-" << escapeHtml(badge.tone) << "\""
                << (badge.tone == "pathway" ? colourVariables(badge.colour) : "") << ">" << escapeHtml(badge.text)
                << "</span>";
        }
        out << "</div>\n";
    }
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
                case BlockKind::Ladder: renderHtmlLadder(out, block.rungs); break;
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
        const std::pair<const char*, const char*> kinds[] = {
            {"Character", "Characters"}, {"Sealed Artifact", "Sealed Artifacts"}, {"Pathway", "Pathways"},
            {"Sefirah", "Sefirot"}};
        for (const auto& [kind, plural] : kinds) {
            bool any = false;
            for (const auto& s : sheets) {
                if (s.kind != kind) continue;
                if (!any) {
                    out << "<h2>" << plural << "</h2>\n<ul>\n";
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
