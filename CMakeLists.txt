cmake_minimum_required(VERSION 3.20)
project(LotmCreator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JSON library for the save files. Downloaded automatically on the first build.
include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# Everything except main() lives in a library so the tests can use it too.
add_library(lotm_core STATIC
    src/model.cpp
    src/rules.cpp
    src/relations.cpp
    src/dice.cpp
    src/storage.cpp
    src/ui.cpp
    src/sheet.cpp
    src/render.cpp
    src/pathway_picker.cpp
    src/pathway_menu.cpp
    src/records.cpp
    src/compare.cpp
    src/exporter.cpp
    src/artifact_creator.cpp
    src/character_creator.cpp
    src/catalogue.cpp
    src/export_menu.cpp
    src/settings_menu.cpp
)
target_include_directories(lotm_core PUBLIC src)
target_link_libraries(lotm_core PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(lotm_core PUBLIC shell32)  # ShellExecute opens exports in their usual program
endif()
# Lets the program find the project's data folder even when run from inside build/.
target_compile_definitions(lotm_core PUBLIC LOTM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(MSVC)
    target_compile_options(lotm_core PUBLIC /utf-8 /W4 /permissive-)
else()
    target_compile_options(lotm_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(lotm_creator src/main.cpp)
target_link_libraries(lotm_creator PRIVATE lotm_core)

# The app window (lotm_window): the same program with text boxes, dropdowns and sliders.
# It uses Dear ImGui for the controls and GLFW + OpenGL to open the window; both are downloaded
# on the first build. Configure with -DLOTM_BUILD_WINDOW=OFF to build only the console program.
option(LOTM_BUILD_WINDOW "Build the app window (lotm_window)" ON)
if(LOTM_BUILD_WINDOW)
    find_package(OpenGL REQUIRED)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)  # on Linux, X11 alone needs fewer packages
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
        URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(glfw imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
    if(MSVC)
        target_compile_options(imgui PRIVATE /utf-8)
    endif()

    add_executable(lotm_window
        src/window/window_main.cpp
        src/window/widgets.cpp
        src/window/characters_screen.cpp
        src/window/artifacts_screen.cpp
        src/window/pathways_screen.cpp
        src/window/settings_screen.cpp
        src/window/compare_screen.cpp)
    target_link_libraries(lotm_window PRIVATE lotm_core imgui)
    if(WIN32)
        # A normal Windows program: no black console window behind it.
        set_target_properties(lotm_window PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(lotm_window PRIVATE /ENTRY:mainCRTStartup)
        endif()
    endif()
endif()

enable_testing()
add_executable(lotm_tests tests/tests.cpp)
target_link_libraries(lotm_tests PRIVATE lotm_core)
add_test(NAME lotm_tests COMMAND lotm_tests)
