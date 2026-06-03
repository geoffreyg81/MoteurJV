#pragma once

#include <cstdint>

namespace mjv {

// Couleur RGBA simple (0-255). Indépendante de la stack graphique.
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

namespace Colors {
inline constexpr Color White{255, 255, 255, 255};
inline constexpr Color Black{0, 0, 0, 255};
inline constexpr Color Red{230, 41, 55, 255};
inline constexpr Color Green{0, 180, 90, 255};
inline constexpr Color Blue{0, 120, 215, 255};
inline constexpr Color Yellow{253, 203, 0, 255};
inline constexpr Color DarkGray{40, 40, 48, 255};
inline constexpr Color SkyBlue{102, 191, 255, 255};
} // namespace Colors

} // namespace mjv
