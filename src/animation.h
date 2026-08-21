#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rwp
{
enum class AnimationMode
{
    Off,
    Rainbow,
    Purple,
    Pulse,
};

std::optional<AnimationMode> parse_animation_mode(std::string_view value);
std::uint32_t rainbow_color(double hue, bool dark);
std::uint32_t purple_color(double cycle, bool dark);
std::uint32_t pulse_color(std::uint32_t base, double cycle);
} // namespace rwp
