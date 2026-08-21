#include "animation.h"

#include <cstdint>
#include <iostream>

namespace
{
bool channels_are_darker(std::uint32_t dark, std::uint32_t light)
{
    for (int shift : {0, 8, 16})
    {
        if (((dark >> shift) & 0xFF) >= ((light >> shift) & 0xFF)) return false;
    }
    return true;
}
} // namespace

int main()
{
    using rwp::AnimationMode;
    if (rwp::parse_animation_mode(" off ") != AnimationMode::Off ||
        rwp::parse_animation_mode("rainbow") != AnimationMode::Rainbow ||
        rwp::parse_animation_mode("purple\r") != AnimationMode::Purple ||
        rwp::parse_animation_mode("pulse") != AnimationMode::Pulse ||
        rwp::parse_animation_mode("unknown").has_value())
    {
        std::cerr << "Animation mode parsing failed.\n";
        return 1;
    }

    const auto rainbow_light = rwp::rainbow_color(0.125, false);
    const auto rainbow_dark = rwp::rainbow_color(0.125, true);
    if (rainbow_light != rwp::rainbow_color(1.125, false) ||
        !channels_are_darker(rainbow_dark, rainbow_light))
    {
        std::cerr << "Rainbow color calculation failed.\n";
        return 2;
    }

    const auto purple_light = rwp::purple_color(0.25, false);
    const auto purple_dark = rwp::purple_color(0.25, true);
    if (purple_light != rwp::purple_color(1.25, false) ||
        !channels_are_darker(purple_dark, purple_light))
    {
        std::cerr << "Purple color calculation failed.\n";
        return 3;
    }

    constexpr std::uint32_t base = 0xA08040;
    if (rwp::pulse_color(base, 0.25) != base ||
        rwp::pulse_color(base, 0.75) == base)
    {
        std::cerr << "Pulse color calculation failed.\n";
        return 4;
    }

    std::cout << "Animation smoke test passed: modes and color calculations are stable.\n";
    return 0;
}
