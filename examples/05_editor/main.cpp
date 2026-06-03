// Exemple 05 — Un éditeur visuel (Dear ImGui) pour MoteurJV.
//
// Étapes 1-3 du plan éditeur :
//   1. ImGui s'affiche par-dessus le rendu du moteur (backend rlImGui).
//   2. Hiérarchie : liste des entités du Registry, cliquables.
//   3. Inspecteur : édition LIVE des composants de l'entité sélectionnée
//      (position, taille, couleur, vitesse...). Bouton Play/Pause pour voir la
//      physique tourner pendant qu'on édite.

#include <algorithm>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "imgui.h"
#include "rlImGui.h"

#include "mjv/mjv.hpp"

#ifndef MJV_EDITOR_DIR
#define MJV_EDITOR_DIR "."
#endif

using namespace mjv;
using json = nlohmann::json;

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
    bool m_dragging = false;
    Vec2 m_dragOffset;
    std::string m_status;
    float m_statusTimer = 0.0f;

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
        if (m_playing) {
            // En mode Play, l'entité sélectionnée (si RigidBody) est pilotable —
            // sauf si ImGui capture le clavier (édition d'un champ).
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

        handleMouse(); // sélection au clic + glisser, en édition comme en Play
        if (m_statusTimer > 0.0f) m_statusTimer -= dt;
    }

    // L'entité (avec Transform + RectShape) dont la boîte contient le point ;
    // on garde la dernière trouvée = celle dessinée au-dessus.
    Entity pickEntity(Vec2 p) {
        Entity hit = NullEntity;
        m_reg.view<Transform2D, RectShape>([&](Entity e, Transform2D& tr, RectShape& s) {
            if (std::abs(p.x - tr.position.x) <= s.size.x * 0.5f &&
                std::abs(p.y - tr.position.y) <= s.size.y * 0.5f) {
                hit = e;
            }
        });
        return hit;
    }

    void handleMouse() {
        if (ImGui::GetIO().WantCaptureMouse) { m_dragging = false; return; } // souris sur un panneau
        const Vec2 m = Input::mousePosition();

        if (Input::mousePressed(Mouse::Left)) {
            const Entity hit = pickEntity(m);
            if (hit != NullEntity) {
                m_selected = hit;
                m_dragging = true;
                m_dragOffset = m_reg.get<Transform2D>(hit).position - m;
            }
        }
        if (m_dragging && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected) &&
            m_reg.has<Transform2D>(m_selected)) {
            m_reg.get<Transform2D>(m_selected).position = m + m_dragOffset;
            if (m_reg.has<RigidBody>(m_selected)) m_reg.get<RigidBody>(m_selected).velocity = {0.0f, 0.0f};
        }
        if (Input::mouseReleased(Mouse::Left)) m_dragging = false;
    }

    // ----- Sérialisation JSON -----
    static json vecToJ(Vec2 v) { return json::array({v.x, v.y}); }
    static Vec2 jToVec(const json& j) { return {j[0].get<float>(), j[1].get<float>()}; }

    std::string scenePath() const { return std::string(MJV_EDITOR_DIR) + "/scene.json"; }

    void saveScene() {
        json doc;
        doc["entities"] = json::array();
        for (Entity e : m_reg.entities()) {
            json je;
            if (m_reg.has<Transform2D>(e)) {
                Transform2D& t = m_reg.get<Transform2D>(e);
                je["Transform2D"] = {{"position", vecToJ(t.position)}, {"rotation", t.rotation}, {"scale", vecToJ(t.scale)}};
            }
            if (m_reg.has<RectShape>(e)) {
                RectShape& r = m_reg.get<RectShape>(e);
                je["RectShape"] = {{"size", vecToJ(r.size)}, {"color", json::array({r.color.r, r.color.g, r.color.b})}};
            }
            if (m_reg.has<RigidBody>(e)) je["RigidBody"] = {{"gravityScale", m_reg.get<RigidBody>(e).gravityScale}};
            if (m_reg.has<AABB>(e))      je["AABB"] = {{"halfSize", vecToJ(m_reg.get<AABB>(e).halfSize)}};
            if (m_reg.has<Velocity>(e))  je["Velocity"] = {{"value", vecToJ(m_reg.get<Velocity>(e).value)}};
            doc["entities"].push_back(je);
        }
        std::ofstream f(scenePath());
        if (f) { f << doc.dump(2); setStatus("Scene sauvegardee -> scene.json"); }
        else setStatus("Echec de la sauvegarde");
    }

    void loadScene() {
        std::ifstream f(scenePath());
        if (!f) { setStatus("Aucun scene.json a charger"); return; }
        json doc;
        try { f >> doc; } catch (...) { setStatus("scene.json invalide"); return; }

        m_reg.clear();
        m_selected = NullEntity;
        for (const json& je : doc["entities"]) {
            Entity e = m_reg.create();
            if (je.contains("Transform2D")) {
                const json& j = je["Transform2D"];
                m_reg.add<Transform2D>(e, Transform2D{jToVec(j["position"]), j["rotation"].get<float>(), jToVec(j["scale"])});
            }
            if (je.contains("RectShape")) {
                const json& j = je["RectShape"];
                const json& c = j["color"];
                m_reg.add<RectShape>(e, RectShape{jToVec(j["size"]),
                    mjv::Color{c[0].get<std::uint8_t>(), c[1].get<std::uint8_t>(), c[2].get<std::uint8_t>(), 255}});
            }
            if (je.contains("RigidBody")) { RigidBody rb; rb.gravityScale = je["RigidBody"]["gravityScale"].get<float>(); m_reg.add<RigidBody>(e, rb); }
            if (je.contains("AABB"))      m_reg.add<AABB>(e, AABB{jToVec(je["AABB"]["halfSize"])});
            if (je.contains("Velocity"))  m_reg.add<Velocity>(e, Velocity{jToVec(je["Velocity"]["value"])});
        }
        setStatus("Scene chargee depuis scene.json");
    }

    void setStatus(const std::string& s) { m_status = s; m_statusTimer = 3.0f; }

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
            if (ImGui::Button("Sauver")) saveScene();
            ImGui::SameLine();
            if (ImGui::Button("Charger")) loadScene();
            ImGui::SameLine();
            if (m_statusTimer > 0.0f) ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", m_status.c_str());
            else ImGui::TextDisabled("  |  Clic = selectionner, glisser = deplacer | Play pour piloter");
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
