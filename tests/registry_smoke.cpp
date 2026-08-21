#include "script_registry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

namespace
{
constexpr std::string_view kExpectedName = "gml_Script_get_player_rgb";
constexpr std::size_t kEntryRva = 32;
constexpr std::size_t kNameRva = 96;
constexpr std::size_t kFunctionRva = 192;

void write_pointer(std::byte* destination, const void* value)
{
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(value);
    std::memcpy(destination, &address, sizeof(address));
}
}

int main()
{
    std::array<std::byte, 256> image{};
    std::memcpy(
        image.data() + kNameRva,
        kExpectedName.data(),
        kExpectedName.size());

    write_pointer(image.data() + kEntryRva, image.data() + kNameRva);
    write_pointer(
        image.data() + kEntryRva + sizeof(std::uintptr_t),
        image.data() + kFunctionRva);

    void* resolved = rwp::resolve_script_registry_entry(
        image.data(),
        image.size(),
        kEntryRva,
        kExpectedName);
    if (resolved != image.data() + kFunctionRva)
    {
        std::cerr << "Valid registry entry was not resolved.\n";
        return 1;
    }

    if (rwp::resolve_script_registry_entry(
            image.data(),
            image.size(),
            kEntryRva,
            "gml_Script_wrong_name") != nullptr)
    {
        std::cerr << "Mismatched script name was accepted.\n";
        return 2;
    }

    write_pointer(image.data() + kEntryRva, reinterpret_cast<void*>(1));
    if (rwp::resolve_script_registry_entry(
            image.data(),
            image.size(),
            kEntryRva,
            kExpectedName) != nullptr)
    {
        std::cerr << "Out-of-range name pointer was accepted.\n";
        return 3;
    }

    if (rwp::resolve_script_registry_entry(
            image.data(),
            image.size(),
            image.size(),
            kExpectedName) != nullptr)
    {
        std::cerr << "Out-of-range registry entry was accepted.\n";
        return 4;
    }

    std::cout << "Registry smoke test passed: valid entries resolve and unsafe entries fail closed.\n";
    return 0;
}
