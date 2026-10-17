// LoTM Creator Suite, app window version. Opens a window with tabs for characters, the relationship
// web, the timeline, dice and checks, Sealed Artifacts, pathways, Sefirot and settings; everything is saved to the
// same data folder as the console program (lotm_creator), so both can be used side by side.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
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

bool saveEverything(WindowState& w) {
    bool ok = true;
    if (w.characters.draft.dirty()) ok = saveCharacterDraft(w) && ok;
    if (w.artifacts.draft.dirty()) ok = saveArtifactDraft(w) && ok;
    if (w.pathways.draft.dirty()) ok = savePathwayDraft(w) && ok;
    if (w.sefirot.draft.dirty()) ok = saveSefirahDraft(w) && ok;
    if (w.timeline.draft.dirty()) ok = saveEventDraft(w) && ok;
    return ok;
}

// Grey fog drifting slowly behind the title, as above the Sefirah Castle: wide, faint wisps.
void drawFog(float top, float height) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float left = viewport->WorkPos.x, width = viewport->WorkSize.x;
    const float time = animations() ? static_cast<float>(ImGui::GetTime()) : 0.0f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // Light grey on the night theme; on parchment a white mist, since grey there looks like a smudge.
    const ImVec4 paper = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const bool light = paper.x + paper.y + paper.z > 1.5f;
    const ImVec4 grey = light ? ImVec4(1.0f, 1.0f, 1.0f, 2.2f) : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    for (int k = 0; k < 7; ++k) {
        const float size = height * (1.3f + 0.35f * static_cast<float>(k % 3));
        const float drift = std::fmod(0.17f * static_cast<float>(k) + time * 0.006f * (1.0f + 0.4f * static_cast<float>(k % 4)), 1.0f);
        const float x = left - size * 2.0f + (width + size * 4.0f) * drift;
        const float y = top + height * (0.5f + 0.25f * std::sin(time * 0.25f + static_cast<float>(k) * 1.7f));
        for (int layer = 0; layer < 10; ++layer) {  // thinner towards the edges, in many faint layers so it has no rings
            const float scale = 1.0f - 0.075f * static_cast<float>(layer);
            draw->AddEllipseFilled(ImVec2(x, y), ImVec2(size * 2.0f * scale, size * 0.45f * scale), colourU32(grey, 0.0065f), 0.0f, 40);
        }
    }
}

// Now and then a gleam of light runs across the title.
void gleam(const char* text, ImVec2 at, ImVec2 end) {
    if (!animations()) return;
    const float phase = std::fmod(static_cast<float>(ImGui::GetTime()) + 6.0f, 11.0f) / 1.6f;
    if (phase >= 1.0f) return;
    const float x = at.x - 30.0f + (end.x - at.x + 60.0f) * phase;
    const ImVec4 gold = palette().gold;
    const ImVec4 light(gold.x + (1.0f - gold.x) * 0.65f, gold.y + (1.0f - gold.y) * 0.65f, gold.z + (1.0f - gold.z) * 0.65f, 1.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (int band = 0; band < 2; ++band) {
        const float half = band == 0 ? 22.0f : 9.0f;
        draw->PushClipRect(ImVec2(x - half, at.y), ImVec2(x + half, end.y), true);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), at, colourU32(light, band == 0 ? 0.35f : 0.55f), text);
        draw->PopClipRect();
    }
}

void drawWindow(WindowState& w) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("LoTM Creator Suite", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 1.45f);
    drawFog(ImGui::GetCursorScreenPos().y, ImGui::GetFontSize() * 1.2f);
    ImGui::TextColored(palette().gold, "LoTM Creator Suite");
    gleam("LoTM Creator Suite", ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGui::PopFont();
    ImGui::SameLine();
    auto count = [](size_t n, const char* one, const char* many) {
        return std::to_string(n) + " " + (n == 1 ? one : many);
    };
    const std::string counts = count(w.app.db.characters.size(), "character", "characters") + "  \xC2\xB7  " +
                               count(w.app.db.artifacts.size(), "Sealed Artifact", "Sealed Artifacts") +
                               "  \xC2\xB7  " + count(w.app.db.pathways.size(), "pathway", "pathways");
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(counts.c_str()).x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", counts.c_str());

    const float statusHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    // The open tab's gold line is drawn here instead of by ImGui, so it can glide from tab to tab.
    ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelectedOverline, ImVec4(0, 0, 0, 0));
    ImVec2 openTabMin(0, 0), openTabMax(0, 0);
    if (ImGui::BeginTabBar("screens")) {
        const std::pair<Tab, const char*> tabs[] = {{Tab::Characters, "Characters"},
                                                    {Tab::Compare, "Compare"},
                                                    {Tab::Web, "Web"},
                                                    {Tab::Timeline, "Timeline"},
                                                    {Tab::Dice, "Dice"},
                                                    {Tab::Artifacts, "Sealed Artifacts"},
                                                    {Tab::Pathways, "Pathways"},
                                                    {Tab::Sefirot, "Sefirot"},
                                                    {Tab::Settings, "Settings"}};
        for (const auto& [tab, name] : tabs) {
            const ImGuiTabItemFlags flags = w.switchTo == tab ? ImGuiTabItemFlags_SetSelected : 0;
            // Tab names in the title font, with a little more room around them.
            ImGui::PushFont(w.fonts.title, ImGui::GetStyle().FontSizeBase * 1.05f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetFontSize() * 0.8f, ImGui::GetFontSize() * 0.4f));
            const bool open = ImGui::BeginTabItem(name, nullptr, flags);
            ImGui::PopStyleVar();
            ImGui::PopFont();
            if (!open) continue;
            openTabMin = ImGui::GetItemRectMin();
            openTabMax = ImGui::GetItemRectMax();
            if (w.tab != tab) w.tabShownAt = ImGui::GetTime();
            w.tab = tab;
            // A new screen fades in, rising a little into place.
            const float shown = easeIn(w.tabShownAt, 0.3f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.1f + 0.9f * shown));
            ImGui::BeginChild("screen", ImVec2(0, -statusHeight));
            if (shown < 1.0f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - shown) * ImGui::GetFontSize() * 0.7f);
            switch (tab) {
                case Tab::Characters: drawCharactersScreen(w); break;
                case Tab::Compare: drawCompareScreen(w); break;
                case Tab::Web: drawWebScreen(w); break;
                case Tab::Timeline: drawTimelineScreen(w); break;
                case Tab::Dice: drawDiceScreen(w); break;
                case Tab::Artifacts: drawArtifactsScreen(w); break;
                case Tab::Pathways: drawPathwaysScreen(w); break;
                case Tab::Sefirot: drawSefirotScreen(w); break;
                case Tab::Settings: drawSettingsScreen(w); break;
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        w.switchTo.reset();
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(2);
    if (openTabMax.x > openTabMin.x) {  // the gold line along the top of the open tab, gliding to the next one
        const float inset = ImGui::GetStyle().TabRounding * 0.6f;
        const float from = glide(ImGui::GetID("tab line from"), openTabMin.x + inset, 16.0f);
        const float to = glide(ImGui::GetID("tab line to"), openTabMax.x - inset, 16.0f);
        const float y = openTabMin.y;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float thick = std::max(2.0f, ImGui::GetFontSize() * 0.14f);
        draw->AddRectFilledMultiColor(ImVec2(from, y + thick), ImVec2(to, y + thick + ImGui::GetFontSize() * 0.6f),
                                      colourU32(palette().gold, 0.16f), colourU32(palette().gold, 0.16f),
                                      colourU32(palette().gold, 0.0f), colourU32(palette().gold, 0.0f));
        draw->AddRectFilled(ImVec2(from, y), ImVec2(to, y + thick), colourU32(palette().gold), thick * 0.5f);
    }

    ImGui::Separator();
    if (w.status.empty()) {
        ImGui::TextDisabled("Saved in %s", w.app.storage.dataDir().string().c_str());
    } else {
        // A new message slides in from the left as it fades in.
        const float shown = easeIn(w.statusAt, 0.4f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * shown);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (1.0f - shown) * ImGui::GetFontSize() * 1.5f);
        const ImVec4 colour = w.statusIsError ? palette().danger : palette().success;
        statusDot(colour);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(colour, "%s", w.status.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleVar();
    }

    drawUnsavedQuestion(w);
    ImGui::End();
}

void drawLoadError(const std::string& problem, bool& quit) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("problem", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(palette().danger, "The program could not load its data.");
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
    w.fonts = loadFonts(w.app.storage.dataDir());
    std::string appliedTheme;

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

        if (appliedTheme != w.app.db.settings.windowTheme) {  // between frames, never halfway through one
            appliedTheme = w.app.db.settings.windowTheme;
            applyTheme(appliedTheme, scale);
        }
        ImGui::GetStyle().FontScaleMain = w.app.db.settings.windowTextSize / 100.0f;
        setAnimations(w.app.db.settings.windowAnimations);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (loadProblem.empty()) drawWindow(w);
        else drawLoadError(loadProblem, quit);
        ImGui::Render();

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        const ImVec4 background = backgroundColour();
        glClearColor(background.x, background.y, background.z, 1.0f);
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
