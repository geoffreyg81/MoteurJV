cmake_minimum_required(VERSION 3.20)
project(MoteurJV LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dépendance graphique : raylib ---------------------------------------
# Récupérée et compilée automatiquement par CMake (FetchContent). Native ARM64.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
)
FetchContent_MakeAvailable(raylib)

# --- Scripting : Lua + sol2 ----------------------------------------------
# Lua : installé via le système (liblua5.4-dev). sol2 : binding C++ header-only.
find_package(Lua 5.4 REQUIRED)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(sol2)

# --- Éditeur : Dear ImGui + backend raylib (rlImGui) ---------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.1
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(rlimgui)

# Petite bibliothèque qui compile ImGui + le backend raylib.
add_library(imgui_rl STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(imgui_rl PUBLIC ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
target_link_libraries(imgui_rl PUBLIC raylib)

# --- Le moteur + les exemples --------------------------------------------
add_subdirectory(engine)
add_subdirectory(examples/01_perso2d)
add_subdirectory(examples/02_jeu)
add_subdirectory(examples/03_lua)
add_subdirectory(examples/04_lua_ecs)
add_subdirectory(examples/05_editor)
