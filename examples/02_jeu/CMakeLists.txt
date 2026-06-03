# Exemple 02 : un petit jeu complet (platformer 3 niveaux).
add_executable(jeu main.cpp)
target_link_libraries(jeu PRIVATE mjv)

# Réutilise les assets de la démo 01 (sprite du héros + sons).
target_compile_definitions(jeu PRIVATE
    MJV_ASSET_DIR="${CMAKE_SOURCE_DIR}/examples/01_perso2d/assets")
