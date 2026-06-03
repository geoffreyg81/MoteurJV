#pragma once

#include <string>
#include <unordered_map>

// Animation de sprites par spritesheet.
//
//   Une spritesheet = une image découpée en frames de taille fixe.
//   Une AnimationClip = une ligne de la sheet (idle, walk, ...) : un nombre de
//   frames, un délai par frame, et si elle boucle.
//   L'Animator = le composant qui garde la frame courante + le timer, et permet
//   de changer d'animation avec play("walk") / play("idle").
//
// Le calcul du rectangle source se fait au rendu :
//   src = { frame * frameWidth, row * frameHeight, frameWidth, frameHeight }
namespace mjv {

struct AnimationClip {
    int row = 0;              // ligne dans la spritesheet
    int frameCount = 1;       // nombre de frames sur cette ligne
    float frameDelay = 0.1f;  // secondes par frame
    bool loop = true;
};

struct Animator {
    int frameWidth = 0;
    int frameHeight = 0;

    std::unordered_map<std::string, AnimationClip> clips;
    std::string current;
    int frame = 0;
    float timer = 0.0f;
    bool finished = false;

    void add(const std::string& name, AnimationClip clip) { clips[name] = clip; }

    // Démarre une animation (sans effet si elle est déjà en cours).
    void play(const std::string& name) {
        if (current == name) return;
        current = name;
        frame = 0;
        timer = 0.0f;
        finished = false;
    }

    const AnimationClip* clip() const {
        auto it = clips.find(current);
        return it == clips.end() ? nullptr : &it->second;
    }
};

} // namespace mjv
