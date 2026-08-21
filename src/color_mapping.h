#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rwp
{
enum class ColorSlot : std::size_t
{
    Local1Main,
    Local2Main,
    Local3Main,
    Local4Main,
    Local1Dark,
    Local2Dark,
    Local3Dark,
    Local4Dark,
    OnlineSelfMain,
    OnlineSelfDark,
    OnlineOpponentMain,
    OnlineOpponentDark,
    Count,
};

constexpr std::uint32_t kOnlineSelfMainCallerRva = 0x03F80C26;
constexpr std::uint32_t kOnlinePlayerBgMainCallerRva = 0x03DBBA96;
constexpr std::uint32_t kOnlineHudMainCallerRva = 0x03DEA92B;
constexpr std::uint32_t kOnlineHudMainAltCallerRva = 0x03DFA462;
constexpr std::uint32_t kOnlineHudMainExtraCallerRva = 0x04068CDD;
constexpr std::uint32_t kOnlineHudMainExtra2CallerRva = 0x03E94258;
constexpr std::uint32_t kOnlineHudMainExtra3CallerRva = 0x03E958F4;
constexpr std::uint32_t kOnlineSelfDarkCallerRva = 0x03F80E20;
constexpr std::uint32_t kOnlineHudDarkCallerRva = 0x03D04FD3;

std::optional<ColorSlot> identify_color_slot(
    std::uint32_t packed_bgr,
    bool dark,
    std::optional<std::uint32_t> caller_rva);
} // namespace rwp
