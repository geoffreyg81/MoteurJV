#include "mjv/Application.hpp"

#include <utility>

#include "mjv/Audio.hpp"
#include "mjv/Graphics.hpp"
#include "raylib.h"

namespace mjv {

Application::Application() : Application(Config{}) {}

Application::Application(Config cfg) : m_config(std::move(cfg)) {
    InitWindow(m_config.width, m_config.height, m_config.title.c_str());
    SetTargetFPS(m_config.targetFps);
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
    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        Audio::update();
        onUpdate(dt);

        BeginDrawing();
        Graphics::clear(m_config.clearColor);
        onRender();
        EndDrawing();
    }
}

} // namespace mjv
