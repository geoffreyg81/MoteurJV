// Exemple 04 — L'ECS du moteur exposé à Lua.
//
// Ici, Lua ne fait pas que dessiner : il crée de VRAIES entités ECS et leur
// ajoute des composants (reg:add(e, "RigidBody")), pendant que la physique C++
// (mjv.physics) tourne dessus. C'est la marche qui sépare "scripting basique"
// de "moteur vraiment scriptable" : un jeu entier peut se définir en Lua.

#include <cstdio>
#include <string>

#include "ecs_lua.hpp"     // binding sol2 (inclut sol + mjv)

#ifndef MJV_SCRIPT_DIR
#define MJV_SCRIPT_DIR "."
#endif

using namespace mjv;

class LuaEcsHost : public Application {
public:
    LuaEcsHost() : Application(makeConfig()) {}

private:
    static Config makeConfig() {
        Config c;
        c.width = 960;
        c.height = 540;
        c.title = "MoteurJV - ECS pilote en Lua";
        c.clearColor = Colors::SkyBlue;
        return c;
    }

    sol::state m_lua;
    Registry m_reg;
    bool m_ready = false;

    void onStart() override {
        mjvlua::bindEngineLua(m_lua, m_reg, m_config.width, m_config.height);

        const std::string path = std::string(MJV_SCRIPT_DIR) + "/scene.lua";
        sol::protected_function_result res = m_lua.safe_script_file(path, &sol::script_pass_on_error);
        if (!res.valid()) {
            sol::error err = res;
            std::fprintf(stderr, "[Lua] erreur de chargement: %s\n", err.what());
            return;
        }
        m_ready = true;
        if (sol::protected_function start = m_lua["start"]; start.valid()) call(start);
    }

    void onUpdate(float dt) override {
        if (!m_ready) return;
        if (sol::protected_function update = m_lua["update"]; update.valid()) call(update, dt);
    }

    void onRender() override {
        if (!m_ready) return;
        if (sol::protected_function draw = m_lua["draw"]; draw.valid()) call(draw);
    }

    template <class... Args>
    void call(sol::protected_function& fn, Args&&... args) {
        sol::protected_function_result r = fn(std::forward<Args>(args)...);
        if (!r.valid()) {
            sol::error err = r;
            std::fprintf(stderr, "[Lua] erreur d'execution: %s\n", err.what());
        }
    }
};

int main() {
    LuaEcsHost host;
    host.run();
    return 0;
}
