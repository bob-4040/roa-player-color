#pragma once

#include <cstddef>
#include <string_view>

namespace rwp
{
void* resolve_script_registry_entry(
    const std::byte* image_base,
    std::size_t image_size,
    std::size_t entry_rva,
    std::string_view expected_name) noexcept;
}
