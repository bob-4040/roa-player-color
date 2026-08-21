#include "color_mapping.h"

#include <array>

namespace rwp
{
namespace
{
bool is_online_main_caller(std::optional<std::uint32_t> caller)
{
    return caller == kOnlineSelfMainCallerRva || caller == kOnlinePlayerBgMainCallerRva ||
        caller == kOnlineHudMainCallerRva || caller == kOnlineHudMainAltCallerRva ||
        caller == kOnlineHudMainExtraCallerRva || caller == kOnlineHudMainExtra2CallerRva ||
        caller == kOnlineHudMainExtra3CallerRva;
}

bool is_online_dark_caller(std::optional<std::uint32_t> caller)
{
    return caller == kOnlineSelfDarkCallerRva || caller == kOnlineHudDarkCallerRva;
}
}

std::optional<ColorSlot> identify_color_slot(
    std::uint32_t packed_bgr,
    bool dark,
    std::optional<std::uint32_t> caller_rva)
{
    // Online cards share one draw function and use different source colors.
    if (!dark && is_online_main_caller(caller_rva))
    {
        if (packed_bgr == 0x764870)
        {
            return ColorSlot::OnlineSelfMain;
        }
        if (packed_bgr == 0xE96A84)
        {
            return ColorSlot::OnlineOpponentMain;
        }
    }

    // Vanilla local P1/P2 share one source color and use different callers.
    if (!dark && packed_bgr == 0x241CED)
    {
        if (caller_rva == static_cast<std::uint32_t>(0x009DACEE)) return ColorSlot::Local1Main;
        if (caller_rva == static_cast<std::uint32_t>(0x00993564)) return ColorSlot::Local2Main;
    }
    if (dark && is_online_dark_caller(caller_rva))
    {
        if (packed_bgr == 0x5C3652)
        {
            return ColorSlot::OnlineSelfDark;
        }
        if (packed_bgr == 0xA44B5D)
        {
            return ColorSlot::OnlineOpponentDark;
        }
    }

    // Online-opponent main palette entry #846AE9 in GameMaker BGR order.
    if (!dark && packed_bgr == 0xE96A84)
    {
        return ColorSlot::OnlineOpponentMain;
    }

    constexpr std::array<std::uint32_t, 4> main_colors = {
        0x96637B, 0x6753C2, 0xB1A3FF, 0x1DE6A8};
    constexpr std::array<std::uint32_t, 4> dark_colors = {
        0x4B323E, 0x342A61, 0x595280, 0x0F7354};
    const auto& colors = dark ? dark_colors : main_colors;
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        if (colors[index] == packed_bgr)
        {
            return static_cast<ColorSlot>(index + (dark ? 4 : 0));
        }
    }
    if (dark && packed_bgr == 0x120E77) return ColorSlot::Local1Dark;
    if (dark && packed_bgr == 0x785C00) return ColorSlot::Local2Dark;
    return std::nullopt;
}

} // namespace rwp
