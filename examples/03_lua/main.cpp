// Exemple 03 — Le jeu est écrit en Lua, pas en C++.
//
// Ce programme C++ est un simple "hôte" : il expose une partie du moteur à Lua
// (Graphics, Input) puis délègue toute la logique au fichier game.lua, chargé à
// l'exécution. Conséquence : on peut modifier le jeu et relancer SANS recompiler
// le C++ — c'est le passage de "moteur pour soi" à "moteur pour les autres".

#include <cstdio>
#include <string>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "mjv/mjv.hpp"

#ifndef MJV_SCRIPT_DIR
#define MJV_SCRIPT_DIR "."
#endif

using namespace mjv;

namespace {
// Traduit un nom de touche Lua ("left", "space"...) en touches physiques.
bool keyDown(const std::string& k) {
    if (k == "left")  return Input::isDown(Key::Left) || Input::isDown(Key::A) || Input::isDown(Key::Q);
    if (k == "right") return Input::isDown(Key::Right) || Input::isDown(Key::D);
    if (k == "up")    return Input::isDown(Key::Up) || Input::isDown(Key::W) || Input::isDown(Key::Z);
    if (k == "down")  return Input::isDown(Key::Down) || Input::isDown(Key::S);
    if (k == "space") return Input::isDown(Key::Space);
    return false;
}

bool keyPressed(const std::string& k) {
    if (k == "space") return Input::isPressed(Key::Space);
    if (k == "enter") return Input::isPressed(Key::Enter);
    return false;
}

inline Color rgb(int r, int g, int b) {
    return Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 255};
}
} // namespace

class LuaHost : public Application {
public:
    LuaHost() : Application(makeConfig()) {}

private:
    static Config makeConfig() {
        Config c;
        c.width = 960;
        c.height = 540;
        c.title = "MoteurJV - Jeu en Lua";
        c.clearColor = Color{20, 20, 30, 255};
        return c;
    }

    sol::state m_lua;
    bool m_ready = false;

    void bindEngine() {
        m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

        // Graphics : dessin 2D.
        auto g = m_lua.create_named_table("Graphics");
        g.set_function("rect", [](float x, float y, float w, float h, int r, int gg, int b) {
            Graphics::drawRectangle({x, y}, {w, h}, rgb(r, gg, b));
        });
        g.set_function("circle", [](float x, float y, float rad, int r, int gg, int b) {
            Graphics::drawCircle({x, y}, rad, rgb(r, gg, b));
        });
        g.set_function("text", [](const std::string& s, float x, float y, int size, int r, int gg, int b) {
            Graphics::drawText(s, {x, y}, size, rgb(r, gg, b));
        });

        // Input : clavier.
        auto in = m_lua.create_named_table("Input");
        in.set_function("down", &keyDown);
        in.set_function("pressed", &keyPressed);

        // Quelques constantes utiles.
        m_lua["SCREEN_W"] = static_cast<float>(m_config.width);
        m_lua["SCREEN_H"] = static_cast<float>(m_config.height);
    }

    void onStart() override {
        bindEngine();
        const std::string path = std::string(MJV_SCRIPT_DIR) + "/game.lua";
        sol::protected_function_result res =
            m_lua.safe_script_file(path, &sol::script_pass_on_error);
        if (!res.valid()) {
            sol::error err = res;
            std::fprintf(stderr, "[Lua] erreur de chargement: %s\n", err.what());
            return;
        }
        m_ready = true;
        if (sol::protected_function start = m_lua["start"]; start.valid()) {
            callLua(start);
        }
    }

    void onUpdate(float dt) override {
        if (!m_ready) return;
        if (sol::protected_function update = m_lua["update"]; update.valid()) {
            callLua(update, dt);
        }
    }

    void onRender() override {
        if (!m_ready) return;
        if (sol::protected_function draw = m_lua["draw"]; draw.valid()) {
            callLua(draw);
        }
    }

    // Appelle une fonction Lua et journalise proprement les erreurs d'exécution.
    template <class... Args>
    void callLua(sol::protected_function& fn, Args&&... args) {
        sol::protected_function_result r = fn(std::forward<Args>(args)...);
        if (!r.valid()) {
            sol::error err = r;
            std::fprintf(stderr, "[Lua] erreur d'execution: %s\n", err.what());
        }
    }
};

int main() {
    LuaHost host;
    host.run();
    return 0;
}
