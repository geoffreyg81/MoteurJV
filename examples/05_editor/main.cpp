// Exemple 05 — Un éditeur visuel facon Unity pour MoteurJV (Dear ImGui docking).
//
//   - thème sombre personnalisé
//   - layout ancré 3 zones (DockSpace) : Hiérarchie à gauche, Inspecteur à
//     droite, Assets en bas, Viewport au centre
//   - le jeu est rendu dans une RenderTexture affichée dans la fenêtre Viewport
//   - barre de menus Fichier / Édition / Affichage
//   - clic dans le viewport pour sélectionner, glisser pour déplacer
//   - sauvegarde / chargement de scène en JSON

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "imgui.h"
#include "imgui_internal.h" // DockBuilder
#include "rlImGui.h"

#include "mjv/mjv.hpp"

#ifndef MJV_EDITOR_DIR
#define MJV_EDITOR_DIR "."
#endif
#ifndef MJV_ASSET_DIR
#define MJV_ASSET_DIR "."
#endif

using namespace mjv;
using json = nlohmann::json;
namespace fs = std::filesystem;

// Composant propre à l'éditeur : une entité qui affiche une image (asset).
struct SpriteAsset {
    std::string path;
    Vec2 size;
};

class Editor : public Application {
public:
    Editor() : Application(makeConfig()) {}
    ~Editor() override {
        for (auto& [name, tex] : m_thumbs) UnloadTexture(tex);
        UnloadRenderTexture(m_target);
        rlImGuiShutdown();
    }

private:
    static Config makeConfig() {
        Config c;
        c.width = 1380;
        c.height = 820;
        c.title = "MoteurJV - Editeur";
        c.clearColor = mjv::Color{30, 32, 38, 255};
        c.maximized = true;   // ouvrir en grand (plein écran fenêtré)
        return c;
    }

    static constexpr int kWorldW = 1280;
    static constexpr int kWorldH = 720;

    Registry m_reg;
    Entity m_selected = NullEntity;
    bool m_playing = false;
    bool m_dragging = false;
    Vec2 m_dragOffset;
    std::string m_status;
    float m_statusTimer = 0.0f;

    ::RenderTexture2D m_target{};
    bool m_layoutInit = false;
    std::unordered_map<std::string, ::Texture2D> m_thumbs; // miniatures d'images (cache)
    mjv::Sound m_preview;                                  // son écouté au clic

    // Rectangle écran où le viewport est affiché (pour mapper la souris).
    Vec2 m_vpPos, m_vpSize;
    bool m_vpHovered = false;

    void onStart() override {
        rlImGuiSetup(true);
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        applyDarkTheme();
        m_target = LoadRenderTexture(kWorldW, kWorldH);
        buildScene();
    }

    // ---------------------------------------------------------------- thème
    void applyDarkTheme() {
        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 5.0f;
        s.FrameRounding = 4.0f;
        s.GrabRounding = 4.0f;
        s.TabRounding = 4.0f;
        s.WindowPadding = ImVec2(10, 10);
        s.FramePadding = ImVec2(8, 4);
        s.ItemSpacing = ImVec2(8, 6);
        s.WindowBorderSize = 0.0f;

        ImVec4* c = s.Colors;
        const ImVec4 bg = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        const ImVec4 panel = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
        const ImVec4 accent = ImVec4(0.20f, 0.50f, 0.90f, 1.00f);
        c[ImGuiCol_WindowBg] = bg;
        c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
        c[ImGuiCol_Header] = panel;
        c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.27f, 0.32f, 1.00f);
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Button] = panel;
        c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.75f, 1.00f);
        c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
        c[ImGuiCol_Tab] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.45f, 0.75f, 1.00f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SeparatorHovered] = accent;
    }

    // --------------------------------------------------------------- scène
    void buildScene() {
        m_reg.clear();
        m_selected = NullEntity;
        spawnSolid({kWorldW * 0.5f, 690.0f}, {kWorldW, 60.0f}, Colors::Green);
        spawnSolid({340.0f, 540.0f}, {200.0f, 26.0f}, mjv::Color{150, 110, 70, 255});
        spawnSolid({820.0f, 410.0f}, {200.0f, 26.0f}, mjv::Color{150, 110, 70, 255});
        Entity hero = spawnBox({240.0f, 200.0f}, {44.0f, 60.0f}, Colors::Red);
        for (int i = 0; i < 4; ++i)
            spawnBox({480.0f + i * 70.0f, 120.0f - i * 40.0f}, {40.0f, 40.0f}, mjv::Color{200, 150, 90, 255});
        m_selected = hero;
    }
    Entity spawnSolid(Vec2 cc, Vec2 size, mjv::Color col) {
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{cc});
        m_reg.add<RectShape>(e, RectShape{size, col});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        return e;
    }
    Entity spawnBox(Vec2 cc, Vec2 size, mjv::Color col) {
        Entity e = spawnSolid(cc, size, col);
        m_reg.add<RigidBody>(e, RigidBody{});
        return e;
    }

    // Crée une entité-image à partir d'un asset (clic ou glisser depuis Assets).
    Entity spawnSprite(const std::string& path, Vec2 pos) {
        ::Texture2D& t = thumbnail(path);
        const Vec2 size{static_cast<float>(t.width), static_cast<float>(t.height)};
        Entity e = m_reg.create();
        m_reg.add<Transform2D>(e, Transform2D{pos});
        m_reg.add<AABB>(e, AABB{size * 0.5f});
        m_reg.add<SpriteAsset>(e, SpriteAsset{path, size});
        m_selected = e;
        setStatus("Image ajoutee a la scene");
        return e;
    }

    // Demi-tailles d'une entité (pour le clic et le surlignage).
    Vec2 halfExtents(Entity e) {
        if (m_reg.has<RectShape>(e)) return m_reg.get<RectShape>(e).size * 0.5f;
        if (m_reg.has<AABB>(e))      return m_reg.get<AABB>(e).halfSize;
        return {20.0f, 20.0f};
    }

    // --------------------------------------------------------------- update
    void onUpdate(float dt) override {
        if (m_statusTimer > 0.0f) m_statusTimer -= dt;
        handleViewportInteraction(); // utilise le rect viewport de la frame précédente

        if (m_playing) {
            if (!ImGui::GetIO().WantCaptureKeyboard && m_reg.valid(m_selected) && m_reg.has<RigidBody>(m_selected)) {
                RigidBody& rb = m_reg.get<RigidBody>(m_selected);
                float vx = 0.0f;
                if (Input::isDown(Key::Left)  || Input::isDown(Key::A) || Input::isDown(Key::Q)) vx -= 260.0f;
                if (Input::isDown(Key::Right) || Input::isDown(Key::D))                          vx += 260.0f;
                rb.velocity.x = vx;
                const bool jump = Input::isPressed(Key::Space) || Input::isPressed(Key::Up) ||
                                  Input::isPressed(Key::W) || Input::isPressed(Key::Z);
                if (jump && rb.onGround) rb.velocity.y = -760.0f;
            }
            physicsStep(m_reg, std::min(dt, 0.033f), {0.0f, 1800.0f});
        }

        renderSceneToTexture();
    }

    // Convertit une position écran en coordonnées "monde" (dans la RenderTexture).
    Vec2 screenToWorld(Vec2 s) const {
        if (m_vpSize.x <= 0.0f || m_vpSize.y <= 0.0f) return s;
        return {(s.x - m_vpPos.x) * (kWorldW / m_vpSize.x),
                (s.y - m_vpPos.y) * (kWorldH / m_vpSize.y)};
    }

    Entity pickEntity(Vec2 p) {
        Entity hit = NullEntity;
        m_reg.view<Transform2D>([&](Entity e, Transform2D& tr) {
            const Vec2 h = halfExtents(e);
            if (std::abs(p.x - tr.position.x) <= h.x && std::abs(p.y - tr.position.y) <= h.y) hit = e;
        });
        return hit;
    }

    void handleViewportInteraction() {
        if (!m_vpHovered) { if (Input::mouseReleased(Mouse::Left)) m_dragging = false; return; }
        const Vec2 w = screenToWorld(Input::mousePosition());

        if (Input::mousePressed(Mouse::Left)) {
            const Entity hit = pickEntity(w);
            if (hit != NullEntity) {
                m_selected = hit;
                m_dragging = true;
                m_dragOffset = m_reg.get<Transform2D>(hit).position - w;
            }
        }
        if (m_dragging && Input::mouseDown(Mouse::Left) && m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            m_reg.get<Transform2D>(m_selected).position = w + m_dragOffset;
            if (m_reg.has<RigidBody>(m_selected)) m_reg.get<RigidBody>(m_selected).velocity = {0.0f, 0.0f};
        }
        if (Input::mouseReleased(Mouse::Left)) m_dragging = false;
    }

    void renderSceneToTexture() {
        BeginTextureMode(m_target);
        Graphics::clear(Colors::SkyBlue);
        // Formes (sol, plateformes, caisses).
        m_reg.view<Transform2D, RectShape>([&](Entity, Transform2D& tr, RectShape& s) {
            Graphics::drawRectangleCentered(tr.position, s.size, s.color, tr.rotation);
            Graphics::drawRectangleOutlineCentered(tr.position, s.size, mjv::Color{20, 20, 28, 255}, 1.5f);
        });
        // Images (assets déposés dans la scène).
        m_reg.view<Transform2D, SpriteAsset>([&](Entity, Transform2D& tr, SpriteAsset& sp) {
            ::Texture2D& t = thumbnail(sp.path);
            const ::Rectangle src{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)};
            const ::Rectangle dst{tr.position.x, tr.position.y, sp.size.x * tr.scale.x, sp.size.y * tr.scale.y};
            DrawTexturePro(t, src, dst, ::Vector2{dst.width * 0.5f, dst.height * 0.5f}, tr.rotation, ::WHITE);
        });
        // Surlignage de la sélection.
        if (m_reg.valid(m_selected) && m_reg.has<Transform2D>(m_selected)) {
            const Vec2 h = halfExtents(m_selected);
            Graphics::drawRectangleOutlineCentered(m_reg.get<Transform2D>(m_selected).position,
                                                   h * 2.0f + Vec2{8, 8}, Colors::Yellow, 2.5f);
        }
        EndTextureMode();
    }

    // --------------------------------------------------------------- render
    void onRender() override {
        rlImGuiBegin();
        drawMenuBar();
        setupDockspace();
        drawHierarchy();
        drawInspector();
        drawViewport();
        drawAssets();
        rlImGuiEnd();
    }

    void setupDockspace() {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImGuiID dockId = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_None);
        if (!m_layoutInit) {
            m_layoutInit = true;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, vp->Size);
            ImGuiID center = dockId;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Hierarchie", left);
            ImGui::DockBuilderDockWindow("Inspecteur", right);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockId);
        }
    }

    void drawMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Fichier")) {
                if (ImGui::MenuItem("Nouveau")) buildScene();
                if (ImGui::MenuItem("Ouvrir")) loadScene();
                if (ImGui::MenuItem("Sauvegarder")) saveScene();
                ImGui::Separator();
                if (ImGui::MenuItem("Quitter")) std::exit(0);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edition")) {
                if (ImGui::MenuItem("Nouvelle entite")) {
                    Entity e = m_reg.create();
                    m_reg.add<Transform2D>(e, Transform2D{{kWorldW * 0.5f, 300.0f}});
                    m_reg.add<RectShape>(e, RectShape{{40.0f, 40.0f}, Colors::Blue});
                    m_selected = e;
                }
                if (ImGui::MenuItem("Supprimer la selection", nullptr, false, m_reg.valid(m_selected))) {
                    m_reg.destroy(m_selected);
                    m_selected = NullEntity;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Affichage")) {
                ImGui::MenuItem("Lecture (physique)", nullptr, &m_playing);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::Button(m_playing ? " Pause " : " Play ")) m_playing = !m_playing;
            if (m_statusTimer > 0.0f) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", m_status.c_str());
            }
            ImGui::EndMainMenuBar();
        }
    }

    void drawViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        rlImGuiImageRenderTextureFit(&m_target, true);
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        m_vpPos = {mn.x, mn.y};
        m_vpSize = {mx.x - mn.x, mx.y - mn.y};
        m_vpHovered = ImGui::IsItemHovered();
        // Déposer une image depuis le panneau Assets pour l'ajouter à la scène.
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_IMG")) {
                const std::string path(static_cast<const char*>(pl->Data));
                spawnSprite(path, screenToWorld(Input::mousePosition()));
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void drawHierarchy() {
        ImGui::Begin("Hierarchie");
        const std::vector<Entity> ents = m_reg.entities();
        ImGui::TextDisabled("%d entites", static_cast<int>(ents.size()));
        ImGui::Separator();
        for (Entity e : ents) {
            const std::string label = "Entite " + std::to_string(e) + componentSummary(e);
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
        if (m_reg.has<SpriteAsset>(e)) s += "S";
        return s + "]";
    }

    void drawInspector() {
        ImGui::Begin("Inspecteur");
        if (m_selected == NullEntity || !m_reg.valid(m_selected)) {
            ImGui::TextDisabled("Aucune entite selectionnee.");
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

        if (m_reg.has<Transform2D>(m_selected) && ImGui::CollapsingHeader("Transform2D", ImGuiTreeNodeFlags_DefaultOpen)) {
            Transform2D& t = m_reg.get<Transform2D>(m_selected);
            ImGui::DragFloat2("position", &t.position.x, 1.0f);
            ImGui::DragFloat("rotation", &t.rotation, 0.5f);
            ImGui::DragFloat2("echelle", &t.scale.x, 0.01f);
        }
        if (m_reg.has<RectShape>(m_selected) && ImGui::CollapsingHeader("RectShape", ImGuiTreeNodeFlags_DefaultOpen)) {
            RectShape& r = m_reg.get<RectShape>(m_selected);
            ImGui::DragFloat2("taille", &r.size.x, 1.0f, 1.0f, 4000.0f);
            float col[3] = {r.color.r / 255.0f, r.color.g / 255.0f, r.color.b / 255.0f};
            if (ImGui::ColorEdit3("couleur", col)) {
                r.color.r = static_cast<std::uint8_t>(col[0] * 255);
                r.color.g = static_cast<std::uint8_t>(col[1] * 255);
                r.color.b = static_cast<std::uint8_t>(col[2] * 255);
            }
        }
        if (m_reg.has<RigidBody>(m_selected) && ImGui::CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen)) {
            RigidBody& rb = m_reg.get<RigidBody>(m_selected);
            ImGui::DragFloat2("velocite", &rb.velocity.x, 1.0f);
            ImGui::DragFloat("gravityScale", &rb.gravityScale, 0.05f);
        }
        if (m_reg.has<AABB>(m_selected) && ImGui::CollapsingHeader("AABB", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat2("demi-taille", &m_reg.get<AABB>(m_selected).halfSize.x, 1.0f, 1.0f, 4000.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Ajouter un composant :");
        if (!m_reg.has<RigidBody>(m_selected) && ImGui::Button("+ RigidBody")) m_reg.add<RigidBody>(m_selected);
        if (!m_reg.has<AABB>(m_selected)      && ImGui::Button("+ AABB"))      m_reg.add<AABB>(m_selected);
        if (!m_reg.has<Velocity>(m_selected)  && ImGui::Button("+ Velocity"))  m_reg.add<Velocity>(m_selected);
        ImGui::End();
    }

    // Charge (et met en cache) la miniature d'une image.
    ::Texture2D& thumbnail(const std::string& path) {
        auto it = m_thumbs.find(path);
        if (it != m_thumbs.end()) return it->second;
        ::Texture2D tex = LoadTexture(path.c_str());
        return m_thumbs.emplace(path, tex).first->second;
    }

    void drawAssets() {
        ImGui::Begin("Assets");
        ImGui::TextDisabled("Dossier : %s   (clique un son pour l'ecouter)", MJV_ASSET_DIR);
        ImGui::Separator();
        std::error_code ec;
        const fs::path dir(MJV_ASSET_DIR);
        if (!fs::exists(dir, ec)) { ImGui::TextDisabled("(dossier introuvable)"); ImGui::End(); return; }

        const float cell = 92.0f; // largeur d'une tuile d'asset
        float avail = ImGui::GetContentRegionAvail().x;
        int perRow = std::max(1, static_cast<int>(avail / cell));
        int i = 0;
        for (const fs::directory_entry& f : fs::directory_iterator(dir, ec)) {
            if (!f.is_regular_file()) continue;
            const std::string path = f.path().string();
            const std::string name = f.path().filename().string();
            const std::string ext = f.path().extension().string();

            ImGui::BeginGroup();
            if (ext == ".png" || ext == ".jpg") {
                ::Texture2D& t = thumbnail(path);
                if (rlImGuiImageButtonSize(name.c_str(), &t, ::Vector2{64, 64}))
                    spawnSprite(path, {kWorldW * 0.5f, kWorldH * 0.5f}); // clic = ajoute au centre
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("ASSET_IMG", path.c_str(), path.size() + 1);
                    rlImGuiImageSize(&t, 48, 48);
                    ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }
            } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
                if (ImGui::Button((">##" + name).c_str(), ImVec2(64, 64))) {
                    if (m_preview.load(path)) m_preview.play(); // écoute
                }
            } else if (ext == ".lua") {
                ImGui::Button(("Lua##" + name).c_str(), ImVec2(64, 64));
            } else {
                ImGui::Button(("?##" + name).c_str(), ImVec2(64, 64));
            }
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::EndGroup();

            if (++i % perRow != 0) ImGui::SameLine();
        }
        ImGui::End();
    }

    // ------------------------------------------------------------- JSON I/O
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
            if (m_reg.has<SpriteAsset>(e)) {
                SpriteAsset& sp = m_reg.get<SpriteAsset>(e);
                je["SpriteAsset"] = {{"path", sp.path}, {"size", vecToJ(sp.size)}};
            }
            doc["entities"].push_back(je);
        }
        std::ofstream f(scenePath());
        if (f) { f << doc.dump(2); setStatus("Scene sauvegardee"); } else setStatus("Echec sauvegarde");
    }

    void loadScene() {
        std::ifstream f(scenePath());
        if (!f) { setStatus("Aucun scene.json"); return; }
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
            if (je.contains("SpriteAsset")) {
                const json& j = je["SpriteAsset"];
                m_reg.add<SpriteAsset>(e, SpriteAsset{j["path"].get<std::string>(), jToVec(j["size"])});
            }
        }
        setStatus("Scene chargee");
    }

    void setStatus(const std::string& s) { m_status = s; m_statusTimer = 3.0f; }
};

int main() {
    Editor app;
    app.run();
    return 0;
}
