# Exemple 05 : un éditeur visuel facon Unity (Dear ImGui docking).
add_executable(editor main.cpp)
target_link_libraries(editor PRIVATE mjv imgui_rl nlohmann_json::nlohmann_json)

target_compile_definitions(editor PRIVATE
    MJV_EDITOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    MJV_ASSET_DIR="${CMAKE_SOURCE_DIR}/examples/01_perso2d/assets"
    MJV_MODEL_DIR="${CMAKE_SOURCE_DIR}/examples/06_3d/assets/kenney3d/Models/GLB format")
