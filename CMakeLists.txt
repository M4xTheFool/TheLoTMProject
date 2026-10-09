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
    src/artifact_creator.cpp
    src/character_creator.cpp
    src/catalogue.cpp
    src/export_menu.cpp
    src/settings_menu.cpp
)
target_include_directories(lotm_core PUBLIC src)
target_link_libraries(lotm_core PUBLIC nlohmann_json::nlohmann_json)
# Lets the program find the project's data folder even when run from inside build/.
target_compile_definitions(lotm_core PUBLIC LOTM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(MSVC)
    target_compile_options(lotm_core PUBLIC /utf-8 /W4 /permissive-)
else()
    target_compile_options(lotm_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(lotm_creator src/main.cpp)
target_link_libraries(lotm_creator PRIVATE lotm_core)

enable_testing()
add_executable(lotm_tests tests/tests.cpp)
target_link_libraries(lotm_tests PRIVATE lotm_core)
add_test(NAME lotm_tests COMMAND lotm_tests)
