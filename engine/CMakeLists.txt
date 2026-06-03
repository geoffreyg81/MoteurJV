# Bibliothèque du moteur MoteurJV.
add_library(mjv
    src/Application.cpp
    src/Audio.cpp
    src/Graphics.cpp
    src/Input.cpp
    src/Texture.cpp
)

target_include_directories(mjv PUBLIC include)
target_link_libraries(mjv PUBLIC raylib)
target_compile_features(mjv PUBLIC cxx_std_17)
