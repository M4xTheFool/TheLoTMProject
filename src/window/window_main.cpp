// LoTM Creator Suite, app window version. Opens a window with tabs for characters,
// Sealed Artifacts, pathways and settings; everything is saved to the same data folder
// as the console program (lotm_creator), so both can be used side by side.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "storage.hpp"
#include "window.hpp"

namespace fs = std::filesystem;
using namespace lotm;
using namespace lotm::window;

namespace {

std::string glfwProblem;

void onGlfwError(int, const char* description) { glfwProblem = description ? description : "unknown error"; }

// A message that must be seen even when no window could be opened.
void showFatal(const std::string& text) {
#ifdef _WIN32
    MessageBoxA(nullptr, text.c_str(), "LoTM Creator Suite", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s\n", text.c_str());
#endif
}

ImFont* loadFirstFont(std::initializer_list<const char*> paths, float size) {
    for (const char* path : paths) {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) continue;
        if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size)) return font;
    }
    return nullptr;
}

// The computer's own fonts: Segoe UI and Georgia on Windows, their look-alikes elsewhere.
Fonts loadFonts() {
    const float size = 18.0f;
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
    fonts.heading = loadFirstFont({"C:/Windows/Fonts/georgia.ttf", "/System/Library/Fonts/Supplemental/Georgia.ttf",
                                   "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
                                   "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf"},
                                  size);
    if (!fonts.bold) fonts.bold = fonts.body;
    if (!fonts.heading) fonts.heading = fonts.body;
    ImGui::GetIO().FontDefault = fonts.body;
    return fonts;
}

// Dark ink and parchment with gold accents, like the dark theme of the HTML exports.
void applyStyle(float scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.FontSizeBase = 18.0f;
    style.WindowPadding = ImVec2(14, 12);
    style.FramePadding = ImVec2(9, 5);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.IndentSpacing = 20;
    style.ScrollbarSize = 14;
    style.GrabMinSize = 14;
    style.WindowRounding = 0;
    style.ChildRounding = 6;
    style.FrameRounding = 5;
    style.PopupRounding = 6;
    style.GrabRounding = 4;
    style.TabRounding = 5;
    style.ScrollbarRounding = 6;
    style.SeparatorTextBorderSize = 1;
    style.SeparatorTextPadding = ImVec2(0, 6);

    auto rgb = [](int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = rgb(232, 226, 212);
    c[ImGuiCol_TextDisabled] = rgb(150, 143, 130);
    c[ImGuiCol_WindowBg] = rgb(22, 22, 27);
    c[ImGuiCol_ChildBg] = rgb(27, 27, 33);
    c[ImGuiCol_PopupBg] = rgb(30, 30, 37, 0.98f);
    c[ImGuiCol_Border] = rgb(58, 56, 62);
    c[ImGuiCol_FrameBg] = rgb(38, 38, 47);
    c[ImGuiCol_FrameBgHovered] = rgb(50, 49, 60);
    c[ImGuiCol_FrameBgActive] = rgb(60, 57, 66);
    c[ImGuiCol_TitleBg] = rgb(22, 22, 27);
    c[ImGuiCol_TitleBgActive] = rgb(30, 29, 36);
    c[ImGuiCol_MenuBarBg] = rgb(27, 27, 33);
    c[ImGuiCol_ScrollbarBg] = rgb(22, 22, 27);
    c[ImGuiCol_ScrollbarGrab] = rgb(64, 61, 68);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(84, 79, 82);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(120, 98, 60);
    c[ImGuiCol_CheckMark] = rgb(217, 173, 84);
    c[ImGuiCol_SliderGrab] = rgb(190, 148, 70);
    c[ImGuiCol_SliderGrabActive] = rgb(232, 190, 100);
    c[ImGuiCol_Button] = rgb(46, 45, 56);
    c[ImGuiCol_ButtonHovered] = rgb(66, 62, 70);
    c[ImGuiCol_ButtonActive] = rgb(120, 98, 60);
    c[ImGuiCol_Header] = rgb(66, 56, 40);
    c[ImGuiCol_HeaderHovered] = rgb(80, 68, 46);
    c[ImGuiCol_HeaderActive] = rgb(100, 82, 52);
    c[ImGuiCol_Separator] = rgb(58, 56, 62);
    c[ImGuiCol_SeparatorHovered] = rgb(150, 120, 66);
    c[ImGuiCol_SeparatorActive] = rgb(217, 173, 84);
    c[ImGuiCol_ResizeGrip] = rgb(66, 56, 40, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = rgb(150, 120, 66);
    c[ImGuiCol_ResizeGripActive] = rgb(217, 173, 84);
    c[ImGuiCol_Tab] = rgb(36, 35, 43);
    c[ImGuiCol_TabHovered] = rgb(80, 68, 46);
    c[ImGuiCol_TabSelected] = rgb(58, 50, 38);
    c[ImGuiCol_TabSelectedOverline] = rgb(217, 173, 84);
    c[ImGuiCol_TabDimmed] = rgb(30, 30, 36);
    c[ImGuiCol_TabDimmedSelected] = rgb(48, 43, 36);
    c[ImGuiCol_TableHeaderBg] = rgb(40, 38, 46);
    c[ImGuiCol_TableBorderStrong] = rgb(58, 56, 62);
    c[ImGuiCol_TableBorderLight] = rgb(46, 45, 52);
    c[ImGuiCol_TableRowBgAlt] = rgb(255, 255, 255, 0.025f);
    c[ImGuiCol_TextSelectedBg] = rgb(150, 120, 66, 0.45f);
    c[ImGuiCol_NavCursor] = rgb(217, 173, 84);
    c[ImGuiCol_ModalWindowDimBg] = rgb(0, 0, 0, 0.55f);
    c[ImGuiCol_TextLink] = rgb(217, 173, 84);

    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

bool saveEverything(WindowState& w) {
    bool ok = true;
    if (w.characters.draft.dirty()) ok = saveCharacterDraft(w) && ok;
    if (w.artifacts.draft.dirty()) ok = saveArtifactDraft(w) && ok;
    if (w.pathways.draft.dirty()) ok = savePathwayDraft(w) && ok;
    return ok;
}

void drawWindow(WindowState& w) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("LoTM Creator Suite", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushFont(w.fonts.heading, ImGui::GetStyle().FontSizeBase * 1.35f);
    ImGui::TextColored(kGold, "LoTM Creator Suite");
    ImGui::PopFont();
    ImGui::SameLine();
    const std::string counts = std::to_string(w.app.db.characters.size()) + " characters   " +
                               std::to_string(w.app.db.artifacts.size()) + " Sealed Artifacts   " +
                               std::to_string(w.app.db.pathways.size()) + " pathways";
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(counts.c_str()).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", counts.c_str());

    const float statusHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginTabBar("screens")) {
        const std::pair<Tab, const char*> tabs[] = {{Tab::Characters, "Characters"},
                                                    {Tab::Compare, "Compare"},
                                                    {Tab::Artifacts, "Sealed Artifacts"},
                                                    {Tab::Pathways, "Pathways"},
                                                    {Tab::Settings, "Settings"}};
        for (const auto& [tab, name] : tabs) {
            const ImGuiTabItemFlags flags = w.switchTo == tab ? ImGuiTabItemFlags_SetSelected : 0;
            if (!ImGui::BeginTabItem(name, nullptr, flags)) continue;
            w.tab = tab;
            ImGui::BeginChild("screen", ImVec2(0, -statusHeight));
            switch (tab) {
                case Tab::Characters: drawCharactersScreen(w); break;
                case Tab::Compare: drawCompareScreen(w); break;
                case Tab::Artifacts: drawArtifactsScreen(w); break;
                case Tab::Pathways: drawPathwaysScreen(w); break;
                case Tab::Settings: drawSettingsScreen(w); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        w.switchTo.reset();
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (w.status.empty()) {
        ImGui::TextDisabled("Saved in %s", w.app.storage.dataDir().string().c_str());
    } else {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(w.statusIsError ? kDanger : kSuccess, "%s", w.status.c_str());
        ImGui::PopTextWrapPos();
    }

    drawUnsavedQuestion(w);
    ImGui::End();
}

void drawLoadError(const std::string& problem, bool& quit) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("problem", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(kDanger, "The program could not load its data.");
    ImGui::Spacing();
    ImGui::TextWrapped("%s", problem.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Close")) quit = true;
    ImGui::End();
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    glfwSetErrorCallback(onGlfwError);
    if (!glfwInit()) {
        showFatal("Could not start the window system: " + glfwProblem);
        return 1;
    }
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const float scale = monitor ? ImGui_ImplGlfw_GetContentScaleForMonitor(monitor) : 1.0f;
    int windowWidth = static_cast<int>(1320 * scale), windowHeight = static_cast<int>(860 * scale);
    if (monitor) {  // never bigger than the screen, leaving room for the taskbar and title bar
        int x = 0, y = 0, areaWidth = 0, areaHeight = 0;
        glfwGetMonitorWorkarea(monitor, &x, &y, &areaWidth, &areaHeight);
        if (areaWidth > 0) windowWidth = std::min(windowWidth, areaWidth * 9 / 10);
        if (areaHeight > 0) windowHeight = std::min(windowHeight, areaHeight * 9 / 10);
    }
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "LoTM Creator Suite", nullptr, nullptr);
    if (!window) {
        showFatal("Could not open the window: " + glfwProblem +
                  "\nThe console version (lotm_creator) still works.");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini next to the program
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    WindowState w;
    std::string loadProblem;
    try {
        w.app.storage = Storage{findDataDir(argv[0])};
        w.app.storage.loadAll(w.app.db);
    } catch (const std::exception& e) {
        loadProblem = e.what();
    }
    applyStyle(scale);
    w.fonts = loadFonts();

    bool quit = false;
    while (!quit) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            if (!loadProblem.empty() || !hasUnsavedChanges(w)) {
                quit = true;
            } else if (!w.quitRequested) {
                w.quitRequested = true;
                whenSaved(w, true, [&w] { return saveEverything(w); }, [&quit] { quit = true; });
            }
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui::GetStyle().FontScaleMain = w.app.db.settings.windowTextSize / 100.0f;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (loadProblem.empty()) drawWindow(w);
        else drawLoadError(loadProblem, quit);
        ImGui::Render();

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.086f, 0.086f, 0.106f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
