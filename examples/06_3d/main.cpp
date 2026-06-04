// Exemple 06 — Démo 3D : un collectathon avec de vrais modèles 3D (Kenney CC0).
//
// Un personnage 3D marche sur un sol d'herbe, ramasse des pièces dorées et
// atteint le drapeau. Éclairage directionnel, caméra 3e personne (clic droit
// pour tourner, molette pour zoomer). HUD 2D par-dessus la 3D.
//
// Les modèles viennent du pack "Platformer Kit" de Kenney (domaine public CC0),
// récupéré par tools/fetch_kenney3d.sh.

#include <cmath>
#include <string>
#include <vector>

#include "mjv/mjv.hpp"

#ifndef MJV_MODEL_DIR
#define MJV_MODEL_DIR "."
#endif

using namespace mjv;

class Demo3D : public Application {
public:
    Demo3D() : Application(makeConfig()) {}

private:
    static Config makeConfig() {
        Config c;
        c.width = 1280; c.height = 720;
        c.title = "MoteurJV - Demo 3D (collectathon)";
        c.clearColor = Color{135, 206, 235, 255};
        return c;
    }

    static constexpr float kGround = 1.0f; // hauteur du dessus du sol

    Model m_char, m_coin, m_flag, m_tree, m_block;
    bool m_ready = false;

    Vec3 m_player{0.0f, kGround, 0.0f};
    float m_vy = 0.0f, m_charYaw = 0.0f;
    float m_camYaw = 0.0f, m_camPitch = 0.55f, m_camDist = 12.0f;
    Camera3D m_cam;

    struct Coin { Vec3 pos; bool taken = false; };
    std::vector<Coin> m_coins;
    Vec3 m_flagPos{10.0f, kGround, -8.0f};
    int m_score = 0;
    bool m_won = false;
    float m_spin = 0.0f;

    std::string model(const char* name) { return std::string(MJV_MODEL_DIR) + "/" + name; }

    void onStart() override {
        Graphics3D::setLight({-0.5f, -1.0f, -0.4f}, Color{255, 247, 230, 255}, 0.40f);
        m_ready = m_char.load(model("character-oobi.glb"));
        m_coin.load(model("coin-gold.glb"));
        m_flag.load(model("flag.glb"));
        m_tree.load(model("tree.glb"));
        m_block.load(model("block-grass.glb"));

        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) / 8.0f * 6.2832f;
            m_coins.push_back({Vec3{std::cos(a) * 6.0f, kGround + 0.9f, std::sin(a) * 6.0f}, false});
        }
        m_coins.push_back({Vec3{0.0f, kGround + 0.9f, -4.0f}, false});
    }

    void onUpdate(float dt) override {
        m_spin += dt * 90.0f;

        // Caméra orbitale.
        if (Input::mouseDown(Mouse::Right)) {
            const Vec2 d = Input::mouseDelta();
            m_camYaw -= d.x * 0.005f;
            m_camPitch = std::min(1.45f, std::max(0.15f, m_camPitch + d.y * 0.005f));
        }
        m_camDist = std::min(30.0f, std::max(5.0f, m_camDist - Input::mouseWheel() * 1.5f));

        // Déplacement (axes du monde).
        Vec3 dir{0, 0, 0};
        if (Input::isDown(Key::Up)    || Input::isDown(Key::W) || Input::isDown(Key::Z)) dir.z -= 1.0f;
        if (Input::isDown(Key::Down)  || Input::isDown(Key::S))                          dir.z += 1.0f;
        if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) dir.x -= 1.0f;
        if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          dir.x += 1.0f;
        const float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        if (len > 0.0f) {
            const float sp = 7.0f * dt;
            m_player.x += dir.x / len * sp;
            m_player.z += dir.z / len * sp;
            m_charYaw = std::atan2(dir.x, dir.z) * 57.2958f;
        }
        // Saut + gravité.
        const bool grounded = m_player.y <= kGround + 1e-3f;
        if (grounded && (Input::isPressed(Key::Space) || Input::isPressed(Key::Enter))) m_vy = 10.0f;
        m_vy -= 24.0f * dt;
        m_player.y += m_vy * dt;
        if (m_player.y < kGround) { m_player.y = kGround; m_vy = 0.0f; }

        // Ramassage des pièces / objectif.
        if (!m_won) {
            for (Coin& c : m_coins) {
                if (c.taken) continue;
                const float dx = c.pos.x - m_player.x, dz = c.pos.z - m_player.z;
                if (dx * dx + dz * dz < 1.6f) { c.taken = true; m_score += 100; }
            }
            const float fx = m_flagPos.x - m_player.x, fz = m_flagPos.z - m_player.z;
            if (fx * fx + fz * fz < 2.2f) m_won = true;
        }

        // Caméra qui suit.
        m_cam.target = m_player + Vec3{0.0f, 1.2f, 0.0f};
        m_cam.position = m_cam.target + Vec3{std::cos(m_camPitch) * std::sin(m_camYaw),
                                             std::sin(m_camPitch),
                                             std::cos(m_camPitch) * std::cos(m_camYaw)} * m_camDist;
    }

    void onRender() override {
        Graphics3D::begin(m_cam);

        // Sol en blocs d'herbe.
        if (m_block.valid()) {
            for (int x = -7; x <= 7; ++x)
                for (int z = -7; z <= 7; ++z)
                    m_block.draw({static_cast<float>(x), 0.0f, static_cast<float>(z)}, 1.0f, 0.0f);
        } else {
            Graphics3D::drawPlane({0, 0, 0}, 30, 30, Color{90, 170, 90, 255});
        }

        // Arbres décoratifs aux coins.
        if (m_tree.valid()) {
            const Vec3 trees[4] = {{-6, kGround, -6}, {6, kGround, -6}, {-6, kGround, 6}, {6, kGround, 6}};
            for (const Vec3& t : trees) m_tree.draw(t, 1.0f, 0.0f);
        }

        // Pièces qui tournent.
        for (const Coin& c : m_coins)
            if (!c.taken) m_coin.draw(c.pos, 1.0f, m_spin);

        // Drapeau (objectif).
        m_flag.draw(m_flagPos, 1.0f, 0.0f);

        // Le personnage (ou un cube de repli).
        if (m_char.valid()) m_char.draw(m_player, 1.0f, m_charYaw);
        else Graphics3D::drawCube({m_player.x, m_player.y + 0.5f, m_player.z}, {1, 1, 1}, Colors::Red);

        Graphics3D::end();

        // HUD.
        Graphics::drawText("MoteurJV 3D", {16.0f, 14.0f}, 28, Colors::White);
        Graphics::drawText("Score : " + std::to_string(m_score), {16.0f, 50.0f}, 24, Colors::Yellow);
        Graphics::drawText("ZQSD/WASD : bouger   Espace : sauter   Clic droit : camera", {16.0f, 84.0f}, 18, Colors::White);
        if (!m_ready)
            Graphics::drawText("(modeles 3D introuvables : lance tools/fetch_kenney3d.sh)", {16.0f, 112.0f}, 18, mjv::Color{255, 150, 150, 255});
        if (m_won) {
            Graphics::drawRectangle({0, 0}, {1280, 720}, mjv::Color{10, 12, 18, 150});
            Graphics::drawText("VICTOIRE !", {1280 * 0.5f - 150.0f, 300.0f}, 80, mjv::Color{120, 230, 120, 255});
            Graphics::drawText("Score : " + std::to_string(m_score), {1280 * 0.5f - 90.0f, 400.0f}, 36, Colors::White);
        }
    }
};

int main() {
    Demo3D demo;
    demo.run();
    return 0;
}
