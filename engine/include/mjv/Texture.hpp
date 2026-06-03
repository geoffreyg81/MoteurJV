#pragma once

#include <string>

#include "Color.hpp"
#include "Math.hpp"

// Texture 2D chargée depuis un fichier (PNG, etc.).
// Implémentation cachée (PIMPL) pour ne pas exposer la lib graphique.
// C'est la première brique du futur "système d'assets".
namespace mjv {

class Texture {
public:
    Texture() = default;
    ~Texture();

    // Non copiable (possède une ressource GPU), mais déplaçable.
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;

    bool load(const std::string& path);
    void unload();
    bool valid() const;

    int width() const;
    int height() const;

    // Dessine la texture entière, centrée sur transform.position.
    void draw(const Transform2D& t, Color tint = Colors::White) const;

    // Dessine une sous-région (en pixels) de la texture — base de l'animation
    // par spritesheet. Échelle X négative = retournement horizontal.
    void drawRegion(const Transform2D& t, Rect source, Color tint = Colors::White) const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace mjv
