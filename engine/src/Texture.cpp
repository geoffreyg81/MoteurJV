#include "mjv/Texture.hpp"

#include <cmath>
#include <utility>

#include "raylib.h"

namespace mjv {

struct Texture::Impl {
    ::Texture2D tex{};
    bool loaded = false;
};

Texture::~Texture() {
    unload();
}

Texture::Texture(Texture&& o) noexcept : m_impl(o.m_impl) {
    o.m_impl = nullptr;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        unload();
        m_impl = o.m_impl;
        o.m_impl = nullptr;
    }
    return *this;
}

bool Texture::load(const std::string& path) {
    unload();
    m_impl = new Impl();
    m_impl->tex = LoadTexture(path.c_str());
    m_impl->loaded = (m_impl->tex.id != 0);
    return m_impl->loaded;
}

void Texture::unload() {
    if (m_impl) {
        if (m_impl->loaded) {
            UnloadTexture(m_impl->tex);
        }
        delete m_impl;
        m_impl = nullptr;
    }
}

bool Texture::valid() const {
    return m_impl && m_impl->loaded;
}

int Texture::width() const {
    return valid() ? m_impl->tex.width : 0;
}

int Texture::height() const {
    return valid() ? m_impl->tex.height : 0;
}

void Texture::draw(const Transform2D& t, Color tint) const {
    if (!valid()) {
        return;
    }
    const float texW = static_cast<float>(m_impl->tex.width);
    const float texH = static_cast<float>(m_impl->tex.height);
    // Une échelle négative en X retourne le sprite horizontalement (orientation).
    const float flipX = (t.scale.x < 0.0f) ? -1.0f : 1.0f;
    const ::Rectangle src{0.0f, 0.0f, flipX * texW, texH};
    const ::Rectangle dst{t.position.x, t.position.y,
                          texW * std::fabs(t.scale.x),
                          texH * std::fabs(t.scale.y)};
    const ::Vector2 origin{dst.width * 0.5f, dst.height * 0.5f};
    DrawTexturePro(m_impl->tex, src, dst, origin, t.rotation,
                   ::Color{tint.r, tint.g, tint.b, tint.a});
}

void Texture::drawRegion(const Transform2D& t, Rect source, Color tint) const {
    if (!valid()) {
        return;
    }
    const float flipX = (t.scale.x < 0.0f) ? -1.0f : 1.0f;
    const ::Rectangle src{source.x, source.y, flipX * source.w, source.h};
    const ::Rectangle dst{t.position.x, t.position.y,
                          source.w * std::fabs(t.scale.x),
                          source.h * std::fabs(t.scale.y)};
    const ::Vector2 origin{dst.width * 0.5f, dst.height * 0.5f};
    DrawTexturePro(m_impl->tex, src, dst, origin, t.rotation,
                   ::Color{tint.r, tint.g, tint.b, tint.a});
}

} // namespace mjv
