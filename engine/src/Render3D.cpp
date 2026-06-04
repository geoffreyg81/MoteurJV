#include "mjv/Render3D.hpp"

#include "raylib.h"

namespace {
inline ::Vector3 v3(mjv::Vec3 v) { return ::Vector3{v.x, v.y, v.z}; }
inline ::Color toRaylib(mjv::Color c) { return ::Color{c.r, c.g, c.b, c.a}; }

// --- Éclairage directionnel (shader chargé à la demande) ---------------------
::Shader g_shader{};
bool g_inited = false;
bool g_ok = false;
int g_locViewPos = -1, g_locLightDir = -1, g_locLightColor = -1, g_locAmbient = -1;

const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFS = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float ambient;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-lightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 light = vec3(ambient) + lightColor * diff;
    finalColor = vec4(texel.rgb * light, texel.a);
}
)";

::Shader& lightShader() {
    if (!g_inited) {
        g_inited = true;
        g_shader = LoadShaderFromMemory(kVS, kFS);
        g_ok = (g_shader.id > 0);
        if (g_ok) {
            g_locViewPos    = GetShaderLocation(g_shader, "viewPos");
            g_locLightDir   = GetShaderLocation(g_shader, "lightDir");
            g_locLightColor = GetShaderLocation(g_shader, "lightColor");
            g_locAmbient    = GetShaderLocation(g_shader, "ambient");
            const ::Vector3 ld{-0.5f, -1.0f, -0.4f};
            const ::Vector3 lc{1.0f, 0.97f, 0.9f};
            const float amb = 0.38f;
            SetShaderValue(g_shader, g_locLightDir, &ld, SHADER_UNIFORM_VEC3);
            SetShaderValue(g_shader, g_locLightColor, &lc, SHADER_UNIFORM_VEC3);
            SetShaderValue(g_shader, g_locAmbient, &amb, SHADER_UNIFORM_FLOAT);
        }
    }
    return g_shader;
}
} // namespace

namespace mjv {

struct Model::Impl {
    ::Model model{};
    bool loaded = false;
};

Model::~Model() { unload(); }
Model::Model(Model&& o) noexcept : m_impl(o.m_impl) { o.m_impl = nullptr; }
Model& Model::operator=(Model&& o) noexcept {
    if (this != &o) { unload(); m_impl = o.m_impl; o.m_impl = nullptr; }
    return *this;
}

bool Model::load(const std::string& path) {
    unload();
    m_impl = new Impl();
    m_impl->model = LoadModel(path.c_str());
    m_impl->loaded = (m_impl->model.meshCount > 0);
    if (m_impl->loaded) {
        ::Shader& sh = lightShader();
        if (g_ok)
            for (int i = 0; i < m_impl->model.materialCount; ++i)
                m_impl->model.materials[i].shader = sh;
    }
    return m_impl->loaded;
}

void Model::unload() {
    if (m_impl) {
        if (m_impl->loaded) UnloadModel(m_impl->model);
        delete m_impl;
        m_impl = nullptr;
    }
}

bool Model::valid() const { return m_impl && m_impl->loaded; }

void Model::draw(Vec3 p, float scale, float yawDeg, Color tint) const {
    if (!valid()) return;
    DrawModelEx(m_impl->model, v3(p), ::Vector3{0, 1, 0}, yawDeg, ::Vector3{scale, scale, scale}, toRaylib(tint));
}

namespace Graphics3D {

void begin(const Camera3D& cam) {
    ::Camera3D rc{};
    rc.position = v3(cam.position);
    rc.target = v3(cam.target);
    rc.up = v3(cam.up);
    rc.fovy = cam.fovy;
    rc.projection = CAMERA_PERSPECTIVE;
    if (g_ok) {
        const ::Vector3 vp = v3(cam.position);
        SetShaderValue(g_shader, g_locViewPos, &vp, SHADER_UNIFORM_VEC3);
    }
    BeginMode3D(rc);
}

void end() { EndMode3D(); }

void setLight(Vec3 dir, Color col, float amb) {
    lightShader();
    if (!g_ok) return;
    const ::Vector3 d = v3(dir);
    const ::Vector3 c{col.r / 255.0f, col.g / 255.0f, col.b / 255.0f};
    SetShaderValue(g_shader, g_locLightDir, &d, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_locLightColor, &c, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_locAmbient, &amb, SHADER_UNIFORM_FLOAT);
}

void drawCube(Vec3 p, Vec3 s, Color c) { DrawCubeV(v3(p), v3(s), toRaylib(c)); }
void drawCubeWires(Vec3 p, Vec3 s, Color c) { DrawCubeWiresV(v3(p), v3(s), toRaylib(c)); }
void drawSphere(Vec3 ctr, float r, Color c) { DrawSphere(v3(ctr), r, toRaylib(c)); }
void drawPlane(Vec3 ctr, float w, float d, Color c) { DrawPlane(v3(ctr), ::Vector2{w, d}, toRaylib(c)); }
void drawGrid(int slices, float spacing) { DrawGrid(slices, spacing); }

} // namespace Graphics3D
} // namespace mjv
