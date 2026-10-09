// The window's look: two colour themes (night and parchment), the fonts, and a colour for
// every pathway group.
#include <cmath>
#include <functional>
#include <initializer_list>
#include <map>

#include "window.hpp"

namespace fs = std::filesystem;

namespace lotm::window {

namespace {

ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }

bool parchment = false;

Palette nightPalette() {
    return {rgb(217, 173, 84),
            rgb(158, 150, 135),
            rgb(220, 97, 84),
            rgb(237, 184, 92),
            rgb(135, 194, 133),
            {rgb(217, 173, 84), rgb(102, 184, 204), rgb(219, 120, 148), rgb(143, 199, 115)}};
}

Palette parchmentPalette() {
    return {rgb(138, 90, 24),
            rgb(112, 98, 80),
            rgb(168, 48, 38),
            rgb(160, 98, 14),
            rgb(52, 118, 50),
            {rgb(156, 104, 22), rgb(30, 106, 138), rgb(158, 52, 88), rgb(66, 122, 40)}};
}

Palette current = nightPalette();

void nightColours(ImVec4* c) {
    c[ImGuiCol_Text] = rgb(234, 227, 212);
    c[ImGuiCol_TextDisabled] = rgb(146, 139, 126);
    c[ImGuiCol_WindowBg] = rgb(19, 19, 24);
    c[ImGuiCol_ChildBg] = rgb(25, 25, 31);
    c[ImGuiCol_PopupBg] = rgb(30, 29, 36, 0.98f);
    c[ImGuiCol_Border] = rgb(56, 53, 58);
    c[ImGuiCol_FrameBg] = rgb(37, 36, 45);
    c[ImGuiCol_FrameBgHovered] = rgb(49, 47, 57);
    c[ImGuiCol_FrameBgActive] = rgb(60, 56, 64);
    c[ImGuiCol_TitleBg] = rgb(19, 19, 24);
    c[ImGuiCol_TitleBgActive] = rgb(30, 29, 36);
    c[ImGuiCol_MenuBarBg] = rgb(25, 25, 31);
    c[ImGuiCol_ScrollbarBg] = rgb(19, 19, 24, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = rgb(62, 59, 66);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(84, 79, 82);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(120, 98, 60);
    c[ImGuiCol_CheckMark] = rgb(217, 173, 84);
    c[ImGuiCol_SliderGrab] = rgb(190, 148, 70);
    c[ImGuiCol_SliderGrabActive] = rgb(232, 190, 100);
    c[ImGuiCol_Button] = rgb(44, 42, 52);
    c[ImGuiCol_ButtonHovered] = rgb(66, 61, 68);
    c[ImGuiCol_ButtonActive] = rgb(120, 98, 60);
    c[ImGuiCol_Header] = rgb(62, 53, 38);
    c[ImGuiCol_HeaderHovered] = rgb(78, 66, 45);
    c[ImGuiCol_HeaderActive] = rgb(98, 80, 50);
    c[ImGuiCol_Separator] = rgb(56, 53, 58);
    c[ImGuiCol_SeparatorHovered] = rgb(150, 120, 66);
    c[ImGuiCol_SeparatorActive] = rgb(217, 173, 84);
    c[ImGuiCol_ResizeGrip] = rgb(66, 56, 40, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = rgb(150, 120, 66);
    c[ImGuiCol_ResizeGripActive] = rgb(217, 173, 84);
    c[ImGuiCol_Tab] = rgb(19, 19, 24, 0.0f);
    c[ImGuiCol_TabHovered] = rgb(62, 53, 38);
    c[ImGuiCol_TabSelected] = rgb(44, 39, 32);
    c[ImGuiCol_TabSelectedOverline] = rgb(217, 173, 84);
    c[ImGuiCol_TabDimmed] = rgb(19, 19, 24, 0.0f);
    c[ImGuiCol_TabDimmedSelected] = rgb(40, 36, 31);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(150, 120, 66);
    c[ImGuiCol_TableHeaderBg] = rgb(38, 36, 44);
    c[ImGuiCol_TableBorderStrong] = rgb(56, 53, 58);
    c[ImGuiCol_TableBorderLight] = rgb(42, 41, 48);
    c[ImGuiCol_TableRowBg] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = rgb(255, 255, 255, 0.025f);
    c[ImGuiCol_TextSelectedBg] = rgb(150, 120, 66, 0.45f);
    c[ImGuiCol_NavCursor] = rgb(217, 173, 84);
    c[ImGuiCol_ModalWindowDimBg] = rgb(0, 0, 0, 0.55f);
    c[ImGuiCol_TextLink] = rgb(217, 173, 84);
}

void parchmentColours(ImVec4* c) {
    c[ImGuiCol_Text] = rgb(46, 36, 27);
    c[ImGuiCol_TextDisabled] = rgb(124, 110, 92);
    c[ImGuiCol_WindowBg] = rgb(233, 223, 201);
    c[ImGuiCol_ChildBg] = rgb(242, 235, 219);
    c[ImGuiCol_PopupBg] = rgb(247, 241, 228, 0.99f);
    c[ImGuiCol_Border] = rgb(196, 180, 150);
    c[ImGuiCol_BorderShadow] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_FrameBg] = rgb(228, 217, 193);
    c[ImGuiCol_FrameBgHovered] = rgb(220, 206, 176);
    c[ImGuiCol_FrameBgActive] = rgb(211, 193, 158);
    c[ImGuiCol_TitleBg] = rgb(233, 223, 201);
    c[ImGuiCol_TitleBgActive] = rgb(224, 211, 184);
    c[ImGuiCol_MenuBarBg] = rgb(228, 217, 193);
    c[ImGuiCol_ScrollbarBg] = rgb(233, 223, 201, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = rgb(201, 186, 157);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(181, 162, 128);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(150, 108, 46);
    c[ImGuiCol_CheckMark] = rgb(138, 90, 24);
    c[ImGuiCol_SliderGrab] = rgb(160, 112, 44);
    c[ImGuiCol_SliderGrabActive] = rgb(128, 84, 24);
    c[ImGuiCol_Button] = rgb(224, 211, 184);
    c[ImGuiCol_ButtonHovered] = rgb(212, 193, 155);
    c[ImGuiCol_ButtonActive] = rgb(194, 166, 116);
    c[ImGuiCol_Header] = rgb(222, 199, 152);
    c[ImGuiCol_HeaderHovered] = rgb(214, 189, 136);
    c[ImGuiCol_HeaderActive] = rgb(201, 172, 112);
    c[ImGuiCol_Separator] = rgb(196, 180, 150);
    c[ImGuiCol_SeparatorHovered] = rgb(160, 120, 60);
    c[ImGuiCol_SeparatorActive] = rgb(138, 90, 24);
    c[ImGuiCol_ResizeGrip] = rgb(196, 170, 120, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = rgb(160, 120, 60);
    c[ImGuiCol_ResizeGripActive] = rgb(138, 90, 24);
    c[ImGuiCol_Tab] = rgb(233, 223, 201, 0.0f);
    c[ImGuiCol_TabHovered] = rgb(222, 199, 152);
    c[ImGuiCol_TabSelected] = rgb(245, 239, 226);
    c[ImGuiCol_TabSelectedOverline] = rgb(138, 90, 24);
    c[ImGuiCol_TabDimmed] = rgb(233, 223, 201, 0.0f);
    c[ImGuiCol_TabDimmedSelected] = rgb(238, 230, 214);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(170, 130, 70);
    c[ImGuiCol_TableHeaderBg] = rgb(226, 214, 189);
    c[ImGuiCol_TableBorderStrong] = rgb(196, 180, 150);
    c[ImGuiCol_TableBorderLight] = rgb(214, 202, 177);
    c[ImGuiCol_TableRowBg] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = rgb(120, 90, 40, 0.05f);
    c[ImGuiCol_TextSelectedBg] = rgb(190, 150, 80, 0.4f);
    c[ImGuiCol_NavCursor] = rgb(138, 90, 24);
    c[ImGuiCol_ModalWindowDimBg] = rgb(60, 45, 25, 0.35f);
    c[ImGuiCol_TextLink] = rgb(138, 84, 20);
    c[ImGuiCol_PlotLines] = rgb(138, 90, 24);
    c[ImGuiCol_PlotHistogram] = rgb(138, 90, 24);
}

ImFont* loadFirstFont(std::initializer_list<fs::path> paths, float size) {
    for (const fs::path& path : paths) {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) continue;
        if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(), size)) return font;
    }
    return nullptr;
}

// assets/fonts sits next to the data folder in the project; the copy the program was built
// from is the fallback when the data folder is somewhere else.
fs::path fontsFolder(const fs::path& dataDir) {
    std::error_code ec;
    const fs::path nextToData = dataDir.parent_path() / "assets" / "fonts";
    if (fs::is_directory(nextToData, ec)) return nextToData;
#ifdef LOTM_SOURCE_DIR
    return fs::path(LOTM_SOURCE_DIR) / "assets" / "fonts";
#else
    return nextToData;
#endif
}

}  // namespace

const Palette& palette() { return current; }

void applyTheme(const std::string& theme, float dpiScale) {
    parchment = theme == "parchment";
    current = parchment ? parchmentPalette() : nightPalette();

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    if (parchment) ImGui::StyleColorsLight(&style);
    else ImGui::StyleColorsDark(&style);
    style.FontSizeBase = 18.0f;
    style.WindowPadding = ImVec2(16, 12);
    style.FramePadding = ImVec2(9, 5);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.CellPadding = ImVec2(6, 4);
    style.IndentSpacing = 20;
    style.ScrollbarSize = 12;
    style.GrabMinSize = 14;
    style.WindowRounding = 0;
    style.ChildRounding = 8;
    style.FrameRounding = 5;
    style.PopupRounding = 8;
    style.GrabRounding = 4;
    style.TabRounding = 6;
    style.ScrollbarRounding = 6;
    style.ChildBorderSize = 1;
    style.TabBarBorderSize = 1;
    style.TabBarOverlineSize = 2;
    style.SeparatorTextBorderSize = 1;
    style.SeparatorTextPadding = ImVec2(0, 6);
    if (parchment) parchmentColours(style.Colors);
    else nightColours(style.Colors);
    style.ScaleAllSizes(dpiScale);
    style.FontScaleDpi = dpiScale;
}

ImVec4 backgroundColour() { return ImGui::GetStyle().Colors[ImGuiCol_WindowBg]; }

ImVec4 pathwayColour(const Database& db, const std::string& pathwayId) {
    static const std::map<std::string, ImVec4> kGroups = {
        {"Fool / Door / Error", rgb(160, 128, 220)},
        {"Visionary / Sun / Tyrant / White Tower / Hanged Man", rgb(236, 160, 78)},
        {"Darkness / Death / Twilight Giant", rgb(112, 140, 220)},
        {"Red Priest / Demoness", rgb(222, 90, 90)},
        {"Mother / Moon", rgb(122, 196, 118)},
        {"Abyss / Chained", rgb(196, 98, 150)},
        {"Black Emperor / Justiciar", rgb(160, 174, 192)},
        {"Hermit / Paragon", rgb(84, 190, 190)},
        {"Wheel of Fortune", rgb(214, 200, 96)},
    };
    ImVec4 colour = rgb(150, 145, 138);  // mortals and unknown pathways
    if (const Pathway* p = db.findPathway(pathwayId)) {
        if (auto it = kGroups.find(p->group); it != kGroups.end()) {
            colour = it->second;
        } else {
            // Your own pathways: a hue picked from the group's name (or the pathway's), the same every time.
            const std::string key = p->group.empty() ? p->id : p->group;
            const float hue = static_cast<float>(std::hash<std::string>{}(key) % 360) / 360.0f;
            ImGui::ColorConvertHSVtoRGB(hue, 0.5f, 0.86f, colour.x, colour.y, colour.z);
        }
    }
    if (parchment) {  // darker, so it reads on the light background
        colour.x *= 0.62f;
        colour.y *= 0.62f;
        colour.z *= 0.62f;
    }
    return colour;
}

Fonts loadFonts(const fs::path& dataDir) {
    const float size = 18.0f;
    const fs::path assets = fontsFolder(dataDir);
    Fonts fonts;
    fonts.body = loadFirstFont({"C:/Windows/Fonts/segoeui.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
                                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"},
                               size);
    if (!fonts.body) fonts.body = ImGui::GetIO().Fonts->AddFontDefaultVector();
    fonts.bold = loadFirstFont({"C:/Windows/Fonts/segoeuib.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                                "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
                               size);
    fonts.heading = loadFirstFont({assets / "CormorantGaramond-SemiBold.ttf", "C:/Windows/Fonts/georgia.ttf",
                                   "/System/Library/Fonts/Supplemental/Georgia.ttf",
                                   "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"},
                                  size);
    fonts.title = loadFirstFont({assets / "Cinzel-SemiBold.ttf"}, size);
    if (!fonts.bold) fonts.bold = fonts.body;
    if (!fonts.heading) fonts.heading = fonts.body;
    if (!fonts.title) fonts.title = fonts.heading;
    ImGui::GetIO().FontDefault = fonts.body;
    return fonts;
}

}  // namespace lotm::window
