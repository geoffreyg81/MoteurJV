# Démo 01 : personnage 2D déplaçable au clavier.
add_executable(perso2d main.cpp)
target_link_libraries(perso2d PRIVATE mjv)

# Chemin absolu du dossier d'assets, pour charger les images indépendamment
# du répertoire courant à l'exécution.
target_compile_definitions(perso2d PRIVATE
    MJV_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
