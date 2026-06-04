// Exemple 06 — Première démo 3D du moteur MoteurJV.
//
// Un cube joueur déplaçable (ZQSD/WASD), qui saute (Espace), sur un sol, entouré
// de cubes colorés. Caméra orbitale : clic droit pour tourner, molette pour zoomer.
// Le HUD 2D se dessine par-dessus la 3D (même boucle de rendu).

#include <cmath>

#include "mjv/mjv.hpp"

using namespace mjv;

class Demo3D : public Application {
public:
    Demo3D() : Application(makeConfig()) {}

private:
    static Config makeConfig() {
        Config c;
        c.width = 1280;
        c.height = 720;
        c.title = "MoteurJV - Demo 3D";
        c.clearColor = Colors::SkyBlue;
        return c;
    }

    Vec3 m_player{0.0f, 1.0f, 0.0f};
    float m_vy = 0.0f;          // vitesse verticale (saut/gravité)
    float m_yaw = 0.0f;         // orientation caméra
    float m_pitch = 0.6f;       // inclinaison caméra
    float m_dist = 16.0f;       // distance caméra
    Camera3D m_cam;

    void onUpdate(float dt) override {
        // --- Caméra orbitale (clic droit pour tourner, molette pour zoomer) ---
        if (Input::mouseDown(Mouse::Right)) {
            const Vec2 d = Input::mouseDelta();
            m_yaw   -= d.x * 0.005f;
            m_pitch += d.y * 0.005f;
            m_pitch = std::min(1.45f, std::max(0.15f, m_pitch));
        }
        m_dist -= Input::mouseWheel() * 1.5f;
        m_dist = std::min(40.0f, std::max(6.0f, m_dist));

        // --- Déplacement du joueur (axes du monde) ---
        const float sp = 8.0f * dt;
        if (Input::isDown(Key::Up)    || Input::isDown(Key::W) || Input::isDown(Key::Z)) m_player.z -= sp;
        if (Input::isDown(Key::Down)  || Input::isDown(Key::S))                          m_player.z += sp;
        if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) m_player.x -= sp;
        if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          m_player.x += sp;

        // --- Saut + gravité (sur l'axe Y) ---
        const bool grounded = m_player.y <= 1.0f + 1e-3f;
        if (grounded && (Input::isPressed(Key::Space) || Input::isPressed(Key::Enter))) m_vy = 11.0f;
        m_vy -= 26.0f * dt;
        m_player.y += m_vy * dt;
        if (m_player.y < 1.0f) { m_player.y = 1.0f; m_vy = 0.0f; }

        // --- Caméra qui suit le joueur ---
        m_cam.target = m_player + Vec3{0.0f, 1.0f, 0.0f};
        m_cam.position = m_cam.target + Vec3{std::cos(m_pitch) * std::sin(m_yaw),
                                             std::sin(m_pitch),
                                             std::cos(m_pitch) * std::cos(m_yaw)} * m_dist;
    }

    void onRender() override {
        Graphics3D::begin(m_cam);

        Graphics3D::drawPlane({0.0f, 0.0f, 0.0f}, 60.0f, 60.0f, Color{70, 90, 80, 255}); // sol
        Graphics3D::drawGrid(30, 2.0f);

        // Anneau de cubes colorés.
        const mjv::Color palette[6] = {Colors::Red, Colors::Blue, Colors::Yellow,
                                       Colors::Green, mjv::Color{200, 120, 60, 255}, mjv::Color{150, 90, 200, 255}};
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) / 8.0f * 6.2832f;
            const Vec3 p{std::cos(a) * 10.0f, 1.5f, std::sin(a) * 10.0f};
            Graphics3D::drawCube(p, {3.0f, 3.0f, 3.0f}, palette[i % 6]);
            Graphics3D::drawCubeWires(p, {3.0f, 3.0f, 3.0f}, Colors::Black);
        }

        // Le joueur.
        Graphics3D::drawCube(m_player, {1.4f, 2.0f, 1.4f}, mjv::Color{240, 80, 80, 255});
        Graphics3D::drawCubeWires(m_player, {1.4f, 2.0f, 1.4f}, Colors::Black);
        // Ombre simple au sol.
        Graphics3D::drawCube({m_player.x, 0.02f, m_player.z}, {1.6f, 0.02f, 1.6f}, Color{0, 0, 0, 90});

        Graphics3D::end();

        // HUD 2D par-dessus la 3D.
        Graphics::drawText("MoteurJV - Demo 3D", {16.0f, 16.0f}, 26, Colors::White);
        Graphics::drawText("ZQSD / WASD : bouger   Espace : sauter", {16.0f, 48.0f}, 20, Colors::White);
        Graphics::drawText("Clic droit : tourner la camera   Molette : zoom", {16.0f, 74.0f}, 20, Colors::White);
    }
};

int main() {
    Demo3D demo;
    demo.run();
    return 0;
}
