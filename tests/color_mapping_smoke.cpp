#include "color_mapping.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>

namespace
{
bool expect_slot(
    std::uint32_t color,
    bool dark,
    std::optional<std::uint32_t> caller,
    rwp::ColorSlot expected)
{
    return rwp::identify_color_slot(color, dark, caller) == expected;
}
}

int main()
{
    constexpr std::array<std::uint32_t, 4> main_colors = {
        0x96637B, 0x6753C2, 0xB1A3FF, 0x1DE6A8};
    constexpr std::array<std::uint32_t, 4> dark_colors = {
        0x4B323E, 0x342A61, 0x595280, 0x0F7354};

    for (std::size_t index = 0; index < main_colors.size(); ++index)
    {
        if (!expect_slot(
                main_colors[index],
                false,
                std::nullopt,
                static_cast<rwp::ColorSlot>(index)))
        {
            std::cerr << "Local main color mapping failed.\n";
            return 1;
        }
        if (!expect_slot(
                dark_colors[index],
                true,
                std::nullopt,
                static_cast<rwp::ColorSlot>(index + 4)))
        {
            std::cerr << "Local dark color mapping failed.\n";
            return 2;
        }
    }

    if (!expect_slot(0x764870, false, rwp::kOnlineSelfMainCallerRva, rwp::ColorSlot::OnlineSelfMain))
    {
        std::cerr << "Online self main caller mapping failed.\n";
        return 3;
    }
    if (!expect_slot(0x764870, false, rwp::kOnlinePlayerBgMainCallerRva, rwp::ColorSlot::OnlineSelfMain))
    {
        std::cerr << "Online self player background mapping failed.\n";
        return 9;
    }
    if (!expect_slot(0x764870, false, rwp::kOnlineHudMainCallerRva, rwp::ColorSlot::OnlineSelfMain))
    {
        std::cerr << "Online HUD self main mapping failed.\n";
        return 10;
    }
    if (!expect_slot(0x6753C2, false, rwp::kOnlineHudMainCallerRva, rwp::ColorSlot::Local2Main))
    {
        std::cerr << "Shared P2 main color was incorrectly forced to online opponent.\n";
        return 11;
    }
    if (!expect_slot(0x5C3652, true, rwp::kOnlineHudDarkCallerRva, rwp::ColorSlot::OnlineSelfDark))
    {
        std::cerr << "Online HUD self dark mapping failed.\n";
        return 12;
    }
    if (!expect_slot(0x342A61, true, rwp::kOnlineHudDarkCallerRva, rwp::ColorSlot::Local2Dark))
    {
        std::cerr << "Shared P2 dark color was incorrectly forced to online opponent.\n";
        return 13;
    }
    if (!expect_slot(0x6753C2, false, rwp::kOnlineSelfMainCallerRva, rwp::ColorSlot::Local2Main))
    {
        std::cerr << "P2 main color was incorrectly classified by source color alone.\n";
        return 4;
    }
    if (!expect_slot(0x5C3652, true, rwp::kOnlineSelfDarkCallerRva, rwp::ColorSlot::OnlineSelfDark))
    {
        std::cerr << "Online self dark caller mapping failed.\n";
        return 5;
    }
    if (!expect_slot(0x342A61, true, rwp::kOnlineSelfDarkCallerRva, rwp::ColorSlot::Local2Dark))
    {
        std::cerr << "P2 dark color was incorrectly classified by source color alone.\n";
        return 6;
    }
    if (!expect_slot(0xE96A84, false, std::nullopt, rwp::ColorSlot::OnlineOpponentMain))
    {
        std::cerr << "Online opponent main mapping failed.\n";
        return 7;
    }
    if (rwp::identify_color_slot(0x123456, false, std::nullopt).has_value())
    {
        std::cerr << "Unknown color was assigned to a slot.\n";
        return 8;
    }

    std::cout << "Color mapping smoke test passed: confirmed slots stay separate.\n";
    return 0;
}
