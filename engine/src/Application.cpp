#include "mjv/Application.hpp"

#include <cstdlib>
#include <utility>

#include "mjv/Audio.hpp"
#include "mjv/Graphics.hpp"
#include "raylib.h"

namespace mjv {

Application::Application() : Application(Config{}) {}

Application::Application(Config cfg) : m_config(std::move(cfg)) {
    unsigned int flags = 0;
    if (m_config.resizable || m_config.maximized) flags |= FLAG_WINDOW_RESIZABLE;
    if (m_config.maximized) flags |= FLAG_WINDOW_MAXIMIZED;
    if (flags) SetConfigFlags(flags);

    InitWindow(m_config.width, m_config.height, m_config.title.c_str());
    SetTargetFPS(m_config.targetFps);
    if (m_config.maximized) MaximizeWindow();
    Audio::init();
}

Application::~Application() {
    Audio::shutdown();
    if (IsWindowReady()) {
        CloseWindow();
    }
}

float Application::deltaTime() const {
    return GetFrameTime();
}

Vec2 Application::windowSize() const {
    return {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
}

void Application::run() {
    onStart();
    const char* shotPath = std::getenv("MJV_SHOT"); // capture auto si défini
    int frame = 0;
    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        Audio::update();
        onUpdate(dt);

        BeginDrawing();
        Graphics::clear(m_config.clearColor);
        onRender();
        if (shotPath && frame == 90) Graphics::screenshot(shotPath);
        EndDrawing();
        ++frame;
    }
}

} // namespace mjv
