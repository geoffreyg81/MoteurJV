# Exemple 05 : un éditeur visuel (Dear ImGui) — hiérarchie + inspecteur + viewport + JSON.
add_executable(editor main.cpp)
target_link_libraries(editor PRIVATE mjv imgui_rl nlohmann_json::nlohmann_json)

# Dossier où l'éditeur enregistre/charge scene.json.
target_compile_definitions(editor PRIVATE
    MJV_EDITOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
