#include "mjv/Audio.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "raylib.h" // définit ::Sound, ::Music et les fonctions audio

namespace mjv {

// --- Implémentations cachées -------------------------------------------------
struct Sound::Impl {
    ::Sound snd{};
    bool loaded = false;
};

struct Music::Impl {
    ::Music mus{};
    bool loaded = false;
};

namespace {
bool g_ready = false;
// Musiques en cours : leur flux doit être rafraîchi chaque frame. On stocke des
// pointeurs vers le ::Music raylib (objet sur le tas, stable même si le mjv::Music
// est déplacé) — pas besoin d'accéder au type privé Music::Impl.
std::vector<::Music*> g_activeMusic;
} // namespace

// --- Audio (périphérique) ----------------------------------------------------
namespace Audio {

void init() {
    if (g_ready) return;
    InitAudioDevice();
    g_ready = IsAudioDeviceReady();
}

void shutdown() {
    if (!g_ready) return;
    g_activeMusic.clear();
    CloseAudioDevice();
    g_ready = false;
}

void update() {
    if (!g_ready) return;
    for (::Music* m : g_activeMusic) {
        UpdateMusicStream(*m);
    }
}

bool ready() { return g_ready; }

void setMasterVolume(float v) { SetMasterVolume(v); }

} // namespace Audio

// --- Sound -------------------------------------------------------------------
Sound::~Sound() { unload(); }

Sound::Sound(Sound&& o) noexcept : m_impl(o.m_impl) { o.m_impl = nullptr; }

Sound& Sound::operator=(Sound&& o) noexcept {
    if (this != &o) {
        unload();
        m_impl = o.m_impl;
        o.m_impl = nullptr;
    }
    return *this;
}

bool Sound::load(const std::string& path) {
    unload();
    if (!g_ready) return false;
    m_impl = new Impl();
    m_impl->snd = LoadSound(path.c_str());
    m_impl->loaded = (m_impl->snd.frameCount > 0);
    return m_impl->loaded;
}

void Sound::unload() {
    if (m_impl) {
        if (m_impl->loaded) UnloadSound(m_impl->snd);
        delete m_impl;
        m_impl = nullptr;
    }
}

bool Sound::valid() const { return m_impl && m_impl->loaded; }

void Sound::play() const {
    if (valid()) PlaySound(m_impl->snd);
}

void Sound::setVolume(float v) {
    if (valid()) SetSoundVolume(m_impl->snd, v);
}

// --- Music -------------------------------------------------------------------
Music::~Music() { unload(); }

Music::Music(Music&& o) noexcept : m_impl(o.m_impl) { o.m_impl = nullptr; }

Music& Music::operator=(Music&& o) noexcept {
    if (this != &o) {
        unload();
        m_impl = o.m_impl;
        o.m_impl = nullptr;
    }
    return *this;
}

bool Music::load(const std::string& path) {
    unload();
    if (!g_ready) return false;
    m_impl = new Impl();
    m_impl->mus = LoadMusicStream(path.c_str());
    m_impl->loaded = (m_impl->mus.frameCount > 0);
    return m_impl->loaded;
}

void Music::unload() {
    if (m_impl) {
        g_activeMusic.erase(std::remove(g_activeMusic.begin(), g_activeMusic.end(), &m_impl->mus),
                            g_activeMusic.end());
        if (m_impl->loaded) UnloadMusicStream(m_impl->mus);
        delete m_impl;
        m_impl = nullptr;
    }
}

bool Music::valid() const { return m_impl && m_impl->loaded; }

void Music::play() {
    if (!valid()) return;
    PlayMusicStream(m_impl->mus);
    if (std::find(g_activeMusic.begin(), g_activeMusic.end(), &m_impl->mus) == g_activeMusic.end()) {
        g_activeMusic.push_back(&m_impl->mus);
    }
}

void Music::stop() {
    if (!valid()) return;
    StopMusicStream(m_impl->mus);
    g_activeMusic.erase(std::remove(g_activeMusic.begin(), g_activeMusic.end(), &m_impl->mus),
                        g_activeMusic.end());
}

void Music::pause() {
    if (valid()) PauseMusicStream(m_impl->mus);
}

void Music::resume() {
    if (valid()) ResumeMusicStream(m_impl->mus);
}

bool Music::isPlaying() const {
    return valid() && IsMusicStreamPlaying(m_impl->mus);
}

void Music::setVolume(float v) {
    if (valid()) SetMusicVolume(m_impl->mus, v);
}

void Music::setLooping(bool loop) {
    if (valid()) m_impl->mus.looping = loop;
}

} // namespace mjv
