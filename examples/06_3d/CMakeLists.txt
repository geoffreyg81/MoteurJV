# Exemple 06 : démo 3D (modèles Kenney CC0, éclairage, collectathon).
add_executable(demo3d main.cpp)
target_link_libraries(demo3d PRIVATE mjv)

# Dossier des modèles 3D (pack Kenney récupéré par tools/fetch_kenney3d.sh).
target_compile_definitions(demo3d PRIVATE
    MJV_MODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney3d/Models/GLB format")
