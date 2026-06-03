#pragma once

#include <string>

// Sous-système audio de MoteurJV (raylib caché derrière l'API mjv::).
//
//   Audio::init/shutdown sont appelés automatiquement par Application.
//   Audio::update() (streaming musique) aussi, à chaque frame.
//   Sound  = un effet court chargé en mémoire (pas, choc, pièce...).
//   Music  = un flux audio long (musique de fond), mis à jour en streaming.
namespace mjv {

namespace Audio {
void init();      // ouvre le périphérique audio (idempotent)
void shutdown();  // ferme le périphérique audio
void update();    // met à jour le streaming des musiques en cours
bool ready();
void setMasterVolume(float v); // 0..1
} // namespace Audio

// Effet sonore court (entièrement chargé en mémoire).
class Sound {
public:
    Sound() = default;
    ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    Sound(Sound&& o) noexcept;
    Sound& operator=(Sound&& o) noexcept;

    bool load(const std::string& path);
    void unload();
    bool valid() const;

    void play() const;
    void setVolume(float v); // 0..1

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

// Musique de fond, lue en flux (streaming).
class Music {
public:
    Music() = default;
    ~Music();
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;
    Music(Music&& o) noexcept;
    Music& operator=(Music&& o) noexcept;

    bool load(const std::string& path);
    void unload();
    bool valid() const;

    void play();
    void stop();
    void pause();
    void resume();
    bool isPlaying() const;

    void setVolume(float v);    // 0..1
    void setLooping(bool loop);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace mjv
