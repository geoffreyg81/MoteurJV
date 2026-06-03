# Exemple 03 : le jeu est écrit en Lua (game.lua), pas en C++.
add_executable(lua_demo main.cpp)
target_link_libraries(lua_demo PRIVATE mjv sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(lua_demo PRIVATE ${LUA_INCLUDE_DIR})

# Chemin du script : on le charge à l'exécution → modifiable SANS recompiler.
target_compile_definitions(lua_demo PRIVATE
    MJV_SCRIPT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
