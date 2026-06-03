# Exemple 04 : l'ECS du moteur piloté depuis Lua.
add_executable(lua_ecs main.cpp)
target_link_libraries(lua_ecs PRIVATE mjv sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(lua_ecs PRIVATE ${LUA_INCLUDE_DIR})

target_compile_definitions(lua_ecs PRIVATE
    MJV_SCRIPT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
