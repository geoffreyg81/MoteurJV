#pragma once

#include <string>

#include "Color.hpp"
#include "Math.hpp"

// Point d'entrée du moteur. On dérive de Application et on surcharge
// onStart / onUpdate / onRender. La boucle de jeu, le temps (delta time)
// et la fenêtre sont gérés pour toi.
namespace mjv {

class Application {
public:
    struct Config {
        int width = 1280;
        int height = 720;
        std::string title = "MoteurJV";
        int targetFps = 60;
        Color clearColor = Colors::DarkGray;
    };

    Application();                       // configuration par défaut
    explicit Application(Config cfg);
    virtual ~Application();

    // Lance la boucle de jeu jusqu'à la fermeture de la fenêtre.
    void run();

    float deltaTime() const;   // secondes écoulées depuis la frame précédente
    Vec2 windowSize() const;

protected:
    virtual void onStart() {}            // appelé une fois avant la boucle
    virtual void onUpdate(float dt) {}   // logique du jeu, chaque frame
    virtual void onRender() {}           // dessin, chaque frame

    Config m_config;
};

} // namespace mjv
