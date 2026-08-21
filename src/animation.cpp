#include "animation.h"

#include <cmath>

namespace rwp
{
namespace
{
constexpr double kTau = 6.283185307179586;

std::string_view trim(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() &&
        (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
    {
        value.remove_suffix(1);
    }
    return value;
}
} // namespace

std::optional<AnimationMode> parse_animation_mode(std::string_view value)
{
    value = trim(value);
    if (value == "off") return AnimationMode::Off;
    if (value == "rainbow") return AnimationMode::Rainbow;
    if (value == "purple") return AnimationMode::Purple;
    if (value == "pulse") return AnimationMode::Pulse;
    return std::nullopt;
}

std::uint32_t rainbow_color(double hue, bool dark)
{
    hue -= std::floor(hue);
    const double scaled = hue * 6.0;
    const int sector = static_cast<int>(scaled) % 6;
    const double fraction = scaled - std::floor(scaled);
    constexpr double saturation = 0.92;
    const double value = dark ? 0.38 : 1.0;
    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * fraction);
    const double t = value * (1.0 - saturation * (1.0 - fraction));
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    switch (sector)
    {
    case 0: red = value; green = t; blue = p; break;
    case 1: red = q; green = value; blue = p; break;
    case 2: red = p; green = value; blue = t; break;
    case 3: red = p; green = q; blue = value; break;
    case 4: red = t; green = p; blue = value; break;
    default: red = value; green = p; blue = q; break;
    }
    const auto r = static_cast<std::uint32_t>(red * 255.0 + 0.5);
    const auto g = static_cast<std::uint32_t>(green * 255.0 + 0.5);
    const auto b = static_cast<std::uint32_t>(blue * 255.0 + 0.5);
    return (b << 16) | (g << 8) | r;
}

std::uint32_t purple_color(double cycle, bool dark)
{
    const double blend = (std::sin(cycle * kTau) + 1.0) * 0.5;
    double red = 106.0 + (255.0 - 106.0) * blend;
    double green = 32.0 + (79.0 - 32.0) * blend;
    double blue = 255.0 + (216.0 - 255.0) * blend;
    if (dark)
    {
        red *= 0.38;
        green *= 0.38;
        blue *= 0.38;
    }
    const auto r = static_cast<std::uint32_t>(red + 0.5);
    const auto g = static_cast<std::uint32_t>(green + 0.5);
    const auto b = static_cast<std::uint32_t>(blue + 0.5);
    return (b << 16) | (g << 8) | r;
}

std::uint32_t pulse_color(std::uint32_t base, double cycle)
{
    const double factor = 0.58 + 0.42 * ((std::sin(cycle * kTau) + 1.0) * 0.5);
    const auto red = static_cast<std::uint32_t>((base & 0xFF) * factor + 0.5);
    const auto green = static_cast<std::uint32_t>(((base >> 8) & 0xFF) * factor + 0.5);
    const auto blue = static_cast<std::uint32_t>(((base >> 16) & 0xFF) * factor + 0.5);
    return (blue << 16) | (green << 8) | red;
}
} // namespace rwp
