// Exemple 05 — Un éditeur visuel (Dear ImGui) pour MoteurJV.
//
// Étapes 1-3 du plan éditeur :
//   1. ImGui s'affiche par-dessus le rendu du moteur (backend rlImGui).
//   2. Hiérarchie : liste des entités du Registry, cliquables.
//   3. Inspecteur : édition LIVE des composants de l'entité sélectionnée
//      (position, taille, couleur, vitesse...). Bouton Play/Pause pour voir la
//      physique tourner pendant qu'on édite.

#include <algorithm>
#include <string>

#include "imgui.h"
#include "rlImGui.h"

#include "mjv/mjv.hpp"

using namespace mjv;

class Editor : public Application {
public:
    Editor() : Application(makeConfig()) {}
    ~Editor() override { rlImGuiShutdown(); }

private:
    static Config makeConfig() {
        Config c;
        c.width = 1280;
        c.height = 720;
        c.title = "MoteurJV - Editeur";
        c.clearColor = mjv::Color{45, 50, 60, 255};
        return c;
    }

    Registry m_reg;
    Entity m_selected = NullEntity;
    bool m_playing = false;

    void onStart() override {
        rlImGuiSetup(true); // true = thème sombre
        buildScene();
    }

    void buildScene() {
        m_reg.clear();
        m_selected = NullEntity;
        const float W = static_cast<float>(m_config.width);

        spawnSolid({W * 0.5f, 660.0f}, {W, 60.0f}, Colors::Green);  // sol
        spawnSolid({320.0f, 520.0f}, {180.0f, 26.0f}, mjv::Color{150, 110, 70, 255});
        spawnSolid({720.0f, 400.0f}, {180.0f, 26.0f}, mjv::Color{150, 110, 70, 255});

        // Un "joueur" + quelques caisses dynamiques.
        Entity hero = spawnBox({200.0f, 200.0f}, {44.0f, 60.0f}, Colors::Red);
        m_reg.get<RigidBody>(hero).gravityScale = 1.0f;
        for (int i = 0; i < 4; ++i)
            spawnBox({420.0f + i * 60.0f, 120.0f - i * 40.0f}, {38.0f, 38.0f}, mjv::Color{200, 150, 90, 255});

        m_selected = hero;
    }

    Entity spawnSolid(Vec2 c, Vec2 size, mjv::Color col) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{c});
        m_reg.add<RectShape>(e, RectShape{size, col});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        return e;
    }
    Entity spawnBox(Vec2 c, Vec2 size, mjv::Color col) {
        Entity e = spawnSolid(c, size, col);
        m_reg.add<RigidBody>(e, RigidBody{});
        return e;
    }

    void onUpdate(float dt) override {
        if (!m_playing) return;

        // En mode Play, l'entité sélectionnée (si elle a un RigidBody) est
        // pilotable au clavier — sauf si ImGui est en train de capturer le clavier
        // (édition d'un champ).
        if (!ImGui::GetIO().WantCaptureKeyboard && m_reg.valid(m_selected) &&
            m_reg.has<RigidBody>(m_selected)) {
            RigidBody& rb = m_reg.get<RigidBody>(m_selected);
            float vx = 0.0f;
            if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) vx -= 260.0f;
            if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          vx += 260.0f;
            rb.velocity.x = vx;
            const bool jump = Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                              Input::isPressed(Key::W) || Input::isPressed(Key::Z);
            if (jump && rb.onGround) rb.velocity.y = -760.0f;
        }

        physicsStep(m_reg, std::min(dt, 0.033f), {0.0f, 1800.0f}); // dt borné = stable
    }

    void onRender() override {
        // 1) Le rendu du moteur (la scène).
        m_reg.view<Transform2D, RectShape>([&](Entity e, Transform2D& tr, RectShape& s) {
            Graphics::drawRectangleCentered(tr.position, s.size, s.color, tr.rotation);
            Graphics::drawRectangleOutlineCentered(tr.position, s.size, mjv::Color{20, 20, 28, 255}, 1.5f);
            if (e == m_selected)
                Graphics::drawRectangleOutlineCentered(tr.position, s.size + Vec2{8, 8}, Colors::Yellow, 2.5f);
        });

        // 2) L'interface de l'éditeur, par-dessus.
        rlImGuiBegin();
        drawMenuBar();
        drawHierarchy();
        drawInspector();
        rlImGuiEnd();
    }

    void drawMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::Button(m_playing ? "Pause" : "Play")) m_playing = !m_playing;
            ImGui::SameLine();
            if (ImGui::Button("Reset scene")) buildScene();
            ImGui::SameLine();
            if (ImGui::Button("Nouvelle entite")) {
                Entity e = m_reg.create();
                m_reg.add<Transform2D>(e, Transform2D{{static_cast<float>(m_config.width) * 0.5f, 300.0f}});
                m_reg.add<RectShape>(e, RectShape{{40.0f, 40.0f}, Colors::Blue});
                m_selected = e;
            }
            ImGui::SameLine();
            if (m_playing)
                ImGui::TextDisabled("  |  Play : pilote l'entite selectionnee (Fleches/ZQSD + Espace)");
            else
                ImGui::TextDisabled("  |  Selectionne une entite, puis Play pour la piloter");
            ImGui::EndMainMenuBar();
        }
    }

    void drawHierarchy() {
        ImGui::SetNextWindowPos({10, 30}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({240, 400}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Hierarchie");
        const std::vector<Entity> ents = m_reg.entities();
        ImGui::Text("%d entites", static_cast<int>(ents.size()));
        ImGui::Separator();
        for (Entity e : ents) {
            std::string label = "Entite " + std::to_string(e) + componentSummary(e);
            if (ImGui::Selectable(label.c_str(), e == m_selected)) m_selected = e;
        }
        ImGui::End();
    }

    std::string componentSummary(Entity e) {
        std::string s = "  [";
        if (m_reg.has<Transform2D>(e)) s += "T";
        if (m_reg.has<RectShape>(e))   s += "R";
        if (m_reg.has<RigidBody>(e))   s += "P";
        if (m_reg.has<AABB>(e))        s += "B";
        if (m_reg.has<Velocity>(e))    s += "V";
        s += "]";
        return s;
    }

    void drawInspector() {
        ImGui::SetNextWindowPos({static_cast<float>(m_config.width) - 310, 30}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({300, 460}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspecteur");

        if (m_selected == NullEntity || !m_reg.valid(m_selected)) {
            ImGui::TextDisabled("Aucune entite selectionnee.");
            ImGui::TextDisabled("Clique une entite dans la Hierarchie.");
            ImGui::End();
            return;
        }

        ImGui::Text("Entite %u", m_selected);
        ImGui::SameLine();
        if (ImGui::SmallButton("Supprimer")) {
            m_reg.destroy(m_selected);
            m_selected = NullEntity;
            ImGui::End();
            return;
        }
        ImGui::Separator();

        inspectTransform();
        inspectRectShape();
        inspectRigidBody();
        inspectVelocity();
        inspectAABB();

        ImGui::Separator();
        drawAddComponent();
        ImGui::End();
    }

    void inspectTransform() {
        if (!m_reg.has<Transform2D>(m_selected)) return;
        if (ImGui::CollapsingHeader("Transform2D", ImGuiTreeNodeFlags_DefaultOpen)) {
            Transform2D& t = m_reg.get<Transform2D>(m_selected);
            ImGui::DragFloat2("position", &t.position.x, 1.0f);
            ImGui::DragFloat("rotation", &t.rotation, 0.5f);
            ImGui::DragFloat2("echelle", &t.scale.x, 0.01f);
        }
    }
    void inspectRectShape() {
        if (!m_reg.has<RectShape>(m_selected)) return;
        if (ImGui::CollapsingHeader("RectShape", ImGuiTreeNodeFlags_DefaultOpen)) {
            RectShape& r = m_reg.get<RectShape>(m_selected);
            ImGui::DragFloat2("taille", &r.size.x, 1.0f, 1.0f, 4000.0f);
            float col[3] = {r.color.r / 255.0f, r.color.g / 255.0f, r.color.b / 255.0f};
            if (ImGui::ColorEdit3("couleur", col)) {
                r.color.r = static_cast<std::uint8_t>(col[0] * 255);
                r.color.g = static_cast<std::uint8_t>(col[1] * 255);
                r.color.b = static_cast<std::uint8_t>(col[2] * 255);
            }
        }
    }
    void inspectRigidBody() {
        if (!m_reg.has<RigidBody>(m_selected)) return;
        if (ImGui::CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen)) {
            RigidBody& rb = m_reg.get<RigidBody>(m_selected);
            ImGui::DragFloat2("velocite", &rb.velocity.x, 1.0f);
            ImGui::DragFloat("gravityScale", &rb.gravityScale, 0.05f);
            ImGui::BeginDisabled();
            ImGui::Checkbox("au sol", &rb.onGround);
            ImGui::EndDisabled();
        }
    }
    void inspectVelocity() {
        if (!m_reg.has<Velocity>(m_selected)) return;
        if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
            Velocity& v = m_reg.get<Velocity>(m_selected);
            ImGui::DragFloat2("valeur", &v.value.x, 1.0f);
        }
    }
    void inspectAABB() {
        if (!m_reg.has<AABB>(m_selected)) return;
        if (ImGui::CollapsingHeader("AABB", ImGuiTreeNodeFlags_DefaultOpen)) {
            AABB& a = m_reg.get<AABB>(m_selected);
            ImGui::DragFloat2("demi-taille", &a.halfSize.x, 1.0f, 1.0f, 4000.0f);
        }
    }

    void drawAddComponent() {
        ImGui::TextDisabled("Ajouter un composant :");
        if (!m_reg.has<Transform2D>(m_selected) && ImGui::Button("+ Transform2D")) m_reg.add<Transform2D>(m_selected);
        if (!m_reg.has<RectShape>(m_selected)   && ImGui::Button("+ RectShape"))   m_reg.add<RectShape>(m_selected);
        if (!m_reg.has<RigidBody>(m_selected)   && ImGui::Button("+ RigidBody"))   m_reg.add<RigidBody>(m_selected);
        if (!m_reg.has<AABB>(m_selected)        && ImGui::Button("+ AABB"))        m_reg.add<AABB>(m_selected);
        if (!m_reg.has<Velocity>(m_selected)    && ImGui::Button("+ Velocity"))    m_reg.add<Velocity>(m_selected);
    }
};

int main() {
    Editor app;
    app.run();
    return 0;
}
