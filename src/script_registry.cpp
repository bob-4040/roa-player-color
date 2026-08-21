#include "script_registry.h"

#include <cstdint>
#include <cstring>
#include <limits>

namespace rwp
{
namespace
{
bool range_is_inside(
    std::uintptr_t image_begin,
    std::size_t image_size,
    std::uintptr_t address,
    std::size_t length) noexcept
{
    if (image_size > std::numeric_limits<std::uintptr_t>::max() - image_begin)
    {
        return false;
    }

    const std::uintptr_t image_end = image_begin + image_size;
    return address >= image_begin &&
        address <= image_end &&
        length <= image_end - address;
}
}

void* resolve_script_registry_entry(
    const std::byte* image_base,
    std::size_t image_size,
    std::size_t entry_rva,
    std::string_view expected_name) noexcept
{
    constexpr std::size_t entry_size = sizeof(std::uintptr_t) * 2;

    if (image_base == nullptr ||
        expected_name.empty() ||
        entry_rva > image_size ||
        entry_size > image_size - entry_rva)
    {
        return nullptr;
    }

    std::uintptr_t name_address = 0;
    std::uintptr_t function_address = 0;
    std::memcpy(&name_address, image_base + entry_rva, sizeof(name_address));
    std::memcpy(
        &function_address,
        image_base + entry_rva + sizeof(name_address),
        sizeof(function_address));

    const std::uintptr_t image_begin = reinterpret_cast<std::uintptr_t>(image_base);
    const std::size_t name_size = expected_name.size() + 1;
    if (!range_is_inside(image_begin, image_size, name_address, name_size) ||
        !range_is_inside(image_begin, image_size, function_address, 1))
    {
        return nullptr;
    }

    const auto* actual_name = reinterpret_cast<const char*>(name_address);
    if (std::memcmp(actual_name, expected_name.data(), expected_name.size()) != 0 ||
        actual_name[expected_name.size()] != '\0')
    {
        return nullptr;
    }

    return reinterpret_cast<void*>(function_address);
}
}
