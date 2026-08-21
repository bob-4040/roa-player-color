#include <windows.h>

#include "script_registry.h"
#include "color_mapping.h"
#include "animation.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <intrin.h>

namespace
{
struct CInstance;
struct RValue
{
    union
    {
        std::int32_t value_s32;
        std::int64_t value_s64;
        double value_real;
        void* value_ptr;
    };
    std::int32_t flags;
    std::int32_t type;
};

constexpr std::string_view kPlayerRgbName = "gml_Script_get_player_rgb";
constexpr std::size_t kPlayerRgbRegistryRva = 0x05BB8BA8;
constexpr std::string_view kPlayerColorUpdateName = "gml_Script_update_char_color";
constexpr std::size_t kPlayerColorUpdateRegistryRva = 0x05BB8B98;
constexpr std::string_view kPlayerRgbNoCpuName = "gml_Script_get_player_rgb_no_cpu_ext";
constexpr std::size_t kPlayerRgbNoCpuRegistryRva = 0x05BB8BB0;
constexpr std::string_view kPlayerRgbOldName = "gml_Script_old_get_player_rgb";
constexpr std::size_t kPlayerRgbOldRegistryRva = 0x05BB8BB8;
constexpr std::string_view kPlayerDarkerName = "gml_Script_get_player_darker_color";
constexpr std::size_t kPlayerDarkerRegistryRva = 0x05BB8BC8;
constexpr std::size_t kMakeColorRgbRva = 0x01586F20;
constexpr std::size_t kGlobalInstancePointerRva = 0x05C4A8D8;
constexpr std::size_t kLocalP1MainColorRva = 0x001697E;
constexpr std::size_t kLocalP2MainColorRva = 0x00169D7;
constexpr std::size_t kLocalP1DarkColorRva = 0x0016EBE;
constexpr std::size_t kLocalP2DarkColorRva = 0x0016F14;
constexpr std::size_t kOnlineSelfMainVariableIndex = 4199;
constexpr std::size_t kOnlineSelfDarkVariableIndex = 4200;
constexpr std::size_t kOnlineOpponentMainVariableIndex = 4201;
constexpr std::size_t kOnlineOpponentDarkVariableIndex = 4202;
constexpr std::uint32_t kOnlineOpponentMainColorReturnRva = 0x00166C63;
constexpr std::uint32_t kOnlineOpponentDarkColorReturnRva = 0x00018361;
constexpr char kHookCreateExport[] = "?loader_hook_create@@YAHPAX0PAPAX@Z";
constexpr char kHookEnableExport[] = "?loader_hook_enable@@YAHPAX@Z";
constexpr char kHookRemoveExport[] = "?loader_hook_remove@@YAHPAX@Z";

constexpr std::string_view kFoundLine =
    "Player Color: compatible color functions found.";
constexpr std::string_view kUnavailableLine =
    "Player Color: required color functions unavailable; changes disabled.";
constexpr std::string_view kColorConfigName = "player-color.txt";
using rwp::AnimationMode;
using rwp::ColorSlot;
using rwp::pulse_color;
using rwp::purple_color;
using rwp::rainbow_color;

using PlayerRgbScript = RValue&(__cdecl*)(CInstance*, CInstance*, RValue&, int, RValue*);
using MakeColorRgbFunction = int(__cdecl*)(int, int, int);
using InstanceVariableGetter = RValue*(__thiscall*)(CInstance*, int);
using LoaderHookCreate = int(__cdecl*)(void*, void*, void**);
using LoaderHookEnable = int(__cdecl*)(void*);
using LoaderHookRemove = int(__cdecl*)(void*);

HMODULE g_module = nullptr;
PlayerRgbScript g_player_rgb_original = nullptr;
PlayerRgbScript g_player_color_update_original = nullptr;
PlayerRgbScript g_player_rgb_no_cpu_original = nullptr;
PlayerRgbScript g_player_rgb_old_original = nullptr;
PlayerRgbScript g_player_darker_original = nullptr;
MakeColorRgbFunction g_make_color_rgb_original = nullptr;
std::atomic_uint32_t g_local_cache_scan_attempts{0};
std::optional<std::uint32_t> g_override_color;
std::array<std::optional<std::uint32_t>, static_cast<std::size_t>(ColorSlot::Count)> g_slot_overrides{};
AnimationMode g_animation_mode = AnimationMode::Off;
double g_animation_speed = 0.08;
constexpr std::size_t kAnimationGroupCount = 6;
std::array<std::optional<AnimationMode>, kAnimationGroupCount> g_animation_mode_overrides{};
std::array<std::optional<double>, kAnimationGroupCount> g_animation_speed_overrides{};
std::array<std::optional<std::uint32_t>, static_cast<std::size_t>(ColorSlot::Count)> g_animated_colors{};
bool g_any_animation_enabled = false;
ULONGLONG g_last_animation_tick = 0;
ULONGLONG g_last_local_cache_scan_tick = 0;

struct CachedColorRegion
{
    void* base = nullptr;
    std::size_t size = 0;
};

struct CachedColorAddress
{
    RValue* address = nullptr;
    ColorSlot slot = ColorSlot::Local1Main;
    double source = 0.0;
    double last_written = 0.0;
    std::size_t region_index = 0;
    bool active = false;
};
std::array<CachedColorRegion, 128> g_cached_color_regions{};
std::size_t g_cached_color_region_count = 0;
std::array<CachedColorAddress, 512> g_cached_color_addresses{};
std::size_t g_cached_color_address_count = 0;

std::filesystem::path module_directory(HMODULE module);
void write_log_line(HMODULE module, std::string_view line);
void apply_online_palette_globals();

std::optional<std::uint32_t> parse_color(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t'))
    {
        value.pop_back();
    }
    std::size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t'))
    {
        ++first;
    }
    value.erase(0, first);
    if (!value.empty() && value.front() == '#')
    {
        value.erase(0, 1);
    }
    if (value.size() != 6)
    {
        return std::nullopt;
    }

    std::uint32_t color = 0;
    for (const char digit : value)
    {
        color <<= 4;
        if (digit >= '0' && digit <= '9') color |= static_cast<std::uint32_t>(digit - '0');
        else if (digit >= 'a' && digit <= 'f') color |= static_cast<std::uint32_t>(digit - 'a' + 10);
        else if (digit >= 'A' && digit <= 'F') color |= static_cast<std::uint32_t>(digit - 'A' + 10);
        else return std::nullopt;
    }
    const std::uint32_t red = (color >> 16) & 0xFF;
    const std::uint32_t green = (color >> 8) & 0xFF;
    const std::uint32_t blue = color & 0xFF;
    return (blue << 16) | (green << 8) | red;
}

void load_color_override(HMODULE module)
{
    const auto path = module_directory(module) / std::string(kColorConfigName);
    std::ifstream config(path);
    std::string line;
    bool found = false;
    while (config && std::getline(config, line))
    {
        std::size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            g_override_color = parse_color(line);
            found = g_override_color.has_value();
            continue;
        }

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        if (key == "animation")
        {
            const auto mode = rwp::parse_animation_mode(value);
            g_animation_mode = mode.value_or(AnimationMode::Off);
            continue;
        }
        if (key == "animation_speed")
        {
            char* end = nullptr;
            const double parsed_speed = std::strtod(value.c_str(), &end);
            if (end != value.c_str() && parsed_speed >= 0.01 && parsed_speed <= 2.0)
            {
                g_animation_speed = parsed_speed;
            }
            continue;
        }
        const std::array<std::string_view, kAnimationGroupCount> group_names = {{
            "local_1p", "local_2p", "local_3p", "local_4p", "online_self", "online_opponent"}};
        bool animation_setting = false;
        for (std::size_t group = 0; group < group_names.size(); ++group)
        {
            if (key == std::string(group_names[group]) + "_animation")
            {
                const auto mode = rwp::parse_animation_mode(value);
                if (mode.has_value()) g_animation_mode_overrides[group] = *mode;
                animation_setting = true;
                break;
            }
            if (key == std::string(group_names[group]) + "_animation_speed")
            {
                char* end = nullptr;
                const double parsed_speed = std::strtod(value.c_str(), &end);
                if (end != value.c_str() && parsed_speed >= 0.01 && parsed_speed <= 2.0)
                {
                    g_animation_speed_overrides[group] = parsed_speed;
                }
                animation_setting = true;
                break;
            }
        }
        if (animation_setting) continue;
        const auto parsed = parse_color(value);
        if (!parsed.has_value())
        {
            continue;
        }

        const std::array<std::pair<std::string_view, ColorSlot>, 12> keys = {{
            {"local_1p_main", ColorSlot::Local1Main},
            {"local_2p_main", ColorSlot::Local2Main},
            {"local_3p_main", ColorSlot::Local3Main},
            {"local_4p_main", ColorSlot::Local4Main},
            {"local_1p_dark", ColorSlot::Local1Dark},
            {"local_2p_dark", ColorSlot::Local2Dark},
            {"local_3p_dark", ColorSlot::Local3Dark},
            {"local_4p_dark", ColorSlot::Local4Dark},
            {"online_self_main", ColorSlot::OnlineSelfMain},
            {"online_self_dark", ColorSlot::OnlineSelfDark},
            {"online_opponent_main", ColorSlot::OnlineOpponentMain},
            {"online_opponent_dark", ColorSlot::OnlineOpponentDark},
        }};
        for (const auto& [name, slot] : keys)
        {
            if (key == name)
            {
                g_slot_overrides[static_cast<std::size_t>(slot)] = parsed;
                found = true;
            }
        }
    }
    write_log_line(module, found ? "Player RGB color overrides loaded from config." :
        "Player RGB color config not found; override disabled.");
    g_any_animation_enabled = g_animation_mode != AnimationMode::Off;
    for (const auto& mode : g_animation_mode_overrides)
    {
        if (mode.has_value() && *mode != AnimationMode::Off) g_any_animation_enabled = true;
    }
    if (g_any_animation_enabled)
    {
        write_log_line(module, "Per-player color animation settings loaded.");
    }
}


bool readable_memory(const void* address, std::size_t size)
{
    if (address == nullptr || size == 0)
    {
        return false;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0)
    {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region_start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto region_end = region_start + info.RegionSize;
    return start >= region_start && start <= region_end && size <= region_end - start;
}

bool copy_readable(const void* address, void* destination, std::size_t size)
{
    if (!readable_memory(address, size))
    {
        return false;
    }
    std::memcpy(destination, address, size);
    return true;
}

bool executable_memory(const void* address)
{
    if (address == nullptr)
    {
        return false;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0)
    {
        return false;
    }
    const DWORD protection = info.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool is_writable_protection(DWORD protection)
{
    protection &= 0xff;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool writable_memory(const void* address, std::size_t size)
{
    if (!readable_memory(address, size)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    return is_writable_protection(info.Protect);
}


std::size_t animation_group(ColorSlot slot)
{
    switch (slot)
    {
    case ColorSlot::Local1Main: case ColorSlot::Local1Dark: return 0;
    case ColorSlot::Local2Main: case ColorSlot::Local2Dark: return 1;
    case ColorSlot::Local3Main: case ColorSlot::Local3Dark: return 2;
    case ColorSlot::Local4Main: case ColorSlot::Local4Dark: return 3;
    case ColorSlot::OnlineSelfMain: case ColorSlot::OnlineSelfDark: return 4;
    case ColorSlot::OnlineOpponentMain: case ColorSlot::OnlineOpponentDark: return 5;
    default: return 0;
    }
}

AnimationMode animation_mode_for_slot(ColorSlot slot)
{
    const auto& override = g_animation_mode_overrides[animation_group(slot)];
    return override.value_or(g_animation_mode);
}

double animation_speed_for_slot(ColorSlot slot)
{
    const auto& override = g_animation_speed_overrides[animation_group(slot)];
    return override.value_or(g_animation_speed);
}

std::optional<std::uint32_t> effective_color(ColorSlot slot)
{
    const auto index = static_cast<std::size_t>(slot);
    if (animation_mode_for_slot(slot) != AnimationMode::Off && g_animated_colors[index].has_value())
    {
        return g_animated_colors[index];
    }
    return g_slot_overrides[index];
}

bool is_dark_slot(ColorSlot slot)
{
    return slot == ColorSlot::Local1Dark || slot == ColorSlot::Local2Dark ||
        slot == ColorSlot::Local3Dark || slot == ColorSlot::Local4Dark ||
        slot == ColorSlot::OnlineSelfDark || slot == ColorSlot::OnlineOpponentDark;
}

int animation_phase(ColorSlot slot)
{
    return static_cast<int>(animation_group(slot));
}

bool cached_region_is_writable(const CachedColorRegion& cached)
{
    if (cached.base == nullptr || cached.size == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(cached.base, &info, sizeof(info)) != sizeof(info)) return false;
    return info.BaseAddress == cached.base && info.RegionSize >= cached.size &&
        info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
        is_writable_protection(info.Protect);
}

void refresh_animated_colors()
{
    if (!g_any_animation_enabled) return;
    const ULONGLONG now = GetTickCount64();
    if (now - g_last_animation_tick < 33) return;
    g_last_animation_tick = now;
    for (std::size_t index = 0; index < g_animated_colors.size(); ++index)
    {
        if (!g_slot_overrides[index].has_value()) continue;
        const auto slot = static_cast<ColorSlot>(index);
        const auto mode = animation_mode_for_slot(slot);
        if (mode == AnimationMode::Off)
        {
            g_animated_colors[index].reset();
            continue;
        }
        const double phase = static_cast<double>(animation_phase(slot)) / 6.0;
        const double base_hue = (static_cast<double>(now) / 1000.0) * animation_speed_for_slot(slot);
        const double cycle = base_hue + phase;
        if (mode == AnimationMode::Rainbow)
        {
            g_animated_colors[index] = rainbow_color(cycle, is_dark_slot(slot));
        }
        else if (mode == AnimationMode::Purple)
        {
            g_animated_colors[index] = purple_color(cycle, is_dark_slot(slot));
        }
        else
        {
            g_animated_colors[index] = pulse_color(*g_slot_overrides[index], cycle);
        }
    }
    std::array<bool, g_cached_color_regions.size()> valid_regions{};
    for (std::size_t index = 0; index < g_cached_color_region_count; ++index)
    {
        valid_regions[index] = cached_region_is_writable(g_cached_color_regions[index]);
    }
    for (std::size_t index = 0; index < g_cached_color_address_count; ++index)
    {
        auto& cached = g_cached_color_addresses[index];
        if (!cached.active || cached.region_index >= g_cached_color_region_count ||
            !valid_regions[cached.region_index])
        {
            cached.active = false;
            continue;
        }
        RValue current{};
        std::memcpy(&current, cached.address, sizeof(current));
        if (current.type != 0 || current.flags != 0)
        {
            cached.active = false;
            continue;
        }
        const auto configured = g_slot_overrides[static_cast<std::size_t>(cached.slot)];
        if (current.value_real != cached.last_written && current.value_real != cached.source &&
            (!configured.has_value() || current.value_real != static_cast<double>(*configured)))
        {
            cached.active = false;
            continue;
        }
        const auto color = effective_color(cached.slot);
        if (!color.has_value()) continue;
        cached.last_written = static_cast<double>(*color);
        std::memcpy(&cached.address->value_real, &cached.last_written, sizeof(cached.last_written));
    }
    apply_online_palette_globals();
}

void apply_online_palette_globals()
{
    const auto game = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    CInstance* global_instance = nullptr;
    if (game == nullptr ||
        !copy_readable(game + kGlobalInstancePointerRva, &global_instance, sizeof(global_instance)) ||
        global_instance == nullptr)
    {
        return;
    }
    void* vtable = nullptr;
    void* getter_address = nullptr;
    if (!copy_readable(global_instance, &vtable, sizeof(vtable)) ||
        !readable_memory(vtable, sizeof(void*) * 2) ||
        !copy_readable(reinterpret_cast<const std::byte*>(vtable) + sizeof(void*), &getter_address, sizeof(getter_address)) ||
        !executable_memory(getter_address))
    {
        return;
    }
    const auto getter = reinterpret_cast<InstanceVariableGetter>(getter_address);
    const std::array<std::pair<std::size_t, ColorSlot>, 4> slots = {{
        {kOnlineSelfMainVariableIndex, ColorSlot::OnlineSelfMain},
        {kOnlineSelfDarkVariableIndex, ColorSlot::OnlineSelfDark},
        {kOnlineOpponentMainVariableIndex, ColorSlot::OnlineOpponentMain},
        {kOnlineOpponentDarkVariableIndex, ColorSlot::OnlineOpponentDark},
    }};
    for (const auto& [index, slot] : slots)
    {
        const auto configured = effective_color(slot);
        if (!configured.has_value()) continue;
        RValue* value = getter(global_instance, static_cast<int>(index));
        if (!writable_memory(value, sizeof(RValue))) continue;
        RValue current{};
        if (!copy_readable(value, &current, sizeof(current)) || current.type != 0) continue;
        current.value_real = static_cast<double>(*configured);
        std::memcpy(value, &current, sizeof(current));
    }
}

std::optional<std::size_t> find_or_add_cached_region(const MEMORY_BASIC_INFORMATION& info)
{
    for (std::size_t index = 0; index < g_cached_color_region_count; ++index)
    {
        if (g_cached_color_regions[index].base == info.BaseAddress) return index;
    }
    if (g_cached_color_region_count >= g_cached_color_regions.size()) return std::nullopt;
    const std::size_t index = g_cached_color_region_count++;
    g_cached_color_regions[index] = {info.BaseAddress, info.RegionSize};
    return index;
}

void patch_cached_local_palette_values()
{
    constexpr std::uint32_t kMaximumAttempts = 3;
    constexpr ULONGLONG kRetryDelayMilliseconds = 250;
    const ULONGLONG now = GetTickCount64();
    std::uint32_t attempt = g_local_cache_scan_attempts.load(std::memory_order_relaxed);
    if (attempt >= kMaximumAttempts ||
        (attempt > 0 && now - g_last_local_cache_scan_tick < kRetryDelayMilliseconds))
    {
        return;
    }
    if (!g_local_cache_scan_attempts.compare_exchange_strong(
            attempt, attempt + 1, std::memory_order_relaxed))
    {
        return;
    }
    g_last_local_cache_scan_tick = now;

    struct Replacement { std::uint32_t source; ColorSlot slot; };
    constexpr Replacement replacements[] = {
        {0x241CED, ColorSlot::Local1Main}, {0xEFB700, ColorSlot::Local2Main},
        {0x120E77, ColorSlot::Local1Dark}, {0x785C00, ColorSlot::Local2Dark},
    };
    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    auto* address = static_cast<std::byte*>(system_info.lpMinimumApplicationAddress);
    const auto* maximum = static_cast<const std::byte*>(system_info.lpMaximumApplicationAddress);
    std::size_t changed = 0;
    for (; address < maximum; )
    {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(address, &info, sizeof(info)) == 0) break;
        const auto next = static_cast<std::byte*>(info.BaseAddress) + info.RegionSize;
        const bool eligible = info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
            is_writable_protection(info.Protect);
        if (eligible)
        {
            for (auto* cursor = static_cast<std::byte*>(info.BaseAddress);
                 cursor + sizeof(RValue) <= next; cursor += sizeof(double))
            {
                RValue candidate{};
                std::memcpy(&candidate, cursor, sizeof(candidate));
                if (candidate.type != 0 || candidate.flags != 0) continue;
                for (const auto& replacement : replacements)
                {
                    if (candidate.value_real != static_cast<double>(replacement.source)) continue;
                    const auto configured = effective_color(replacement.slot);
                    if (!configured.has_value()) continue;
                    const double updated = static_cast<double>(*configured);
                    std::memcpy(cursor, &updated, sizeof(updated));
                    if (animation_mode_for_slot(replacement.slot) != AnimationMode::Off &&
                        g_cached_color_address_count < g_cached_color_addresses.size())
                    {
                        const auto region_index = find_or_add_cached_region(info);
                        if (region_index.has_value())
                        {
                            auto& cached = g_cached_color_addresses[g_cached_color_address_count++];
                            cached.address = reinterpret_cast<RValue*>(cursor);
                            cached.slot = replacement.slot;
                            cached.source = static_cast<double>(replacement.source);
                            cached.last_written = updated;
                            cached.region_index = *region_index;
                            cached.active = true;
                        }
                    }
                    ++changed;
                    break;
                }
            }
        }
        address = (next > address) ? next : address + system_info.dwPageSize;
    }
    if (changed > 0)
    {
        // Retry only when the cache did not exist during the first scan.
        g_local_cache_scan_attempts.store(kMaximumAttempts, std::memory_order_relaxed);
    }
    write_log_line(g_module,
        "Local cached palette scan " + std::to_string(attempt + 1) +
        "; changed=" + std::to_string(changed) +
        "; tracked=" + std::to_string(g_cached_color_address_count) + ".");
}

std::optional<std::uint32_t> caller_rva(void* return_address)
{
    const auto game = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto caller = reinterpret_cast<std::uintptr_t>(return_address);
    if (game == 0 || caller < game || caller - game > UINT32_MAX)
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(caller - game);
}

void apply_color_override(
    RValue& result,
    bool dark,
    void* return_address)
{
    if (result.type != 0 || result.value_real < 0.0 || result.value_real > 16777215.0)
    {
        return;
    }

    const auto color = static_cast<std::uint32_t>(result.value_real);
    const auto slot = rwp::identify_color_slot(color, dark, caller_rva(return_address));
    if (slot.has_value())
    {
        const auto override = effective_color(*slot);
        if (override.has_value())
        {
            result.value_real = static_cast<double>(*override);
            return;
        }
    }
    if (g_override_color.has_value())
    {
        result.value_real = static_cast<double>(*g_override_color);
    }
}

std::filesystem::path module_directory(HMODULE module)
{
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
    {
        return {};
    }

    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

void write_log_line(HMODULE module, std::string_view line)
{
    const std::filesystem::path directory = module_directory(module);

    if (!directory.empty())
    {
        std::ofstream log(directory / "player-color.log", std::ios::app);
        if (log)
        {
            log << line << '\n';
            log.flush();
        }
    }

    const std::string debug_line = std::string(line) + "\n";
    OutputDebugStringA(debug_line.c_str());
}

bool write_executable_byte(const std::byte* address, std::uint8_t expected, std::uint8_t value)
{
    if (address == nullptr || !readable_memory(address, 1)) return false;
    std::uint8_t current = 0;
    std::memcpy(&current, address, sizeof(current));
    if (current != expected) return false;
    DWORD old_protection = 0;
    if (!VirtualProtect(const_cast<std::byte*>(address), 1, PAGE_EXECUTE_READWRITE, &old_protection)) return false;
    *const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(address)) = value;
    DWORD unused = 0;
    VirtualProtect(const_cast<std::byte*>(address), 1, old_protection, &unused);
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    return true;
}

bool patch_executable_triplet(
    const std::byte* base,
    std::size_t b_offset,
    std::uint8_t b_expected,
    std::size_t g_offset,
    std::uint8_t g_expected,
    std::size_t r_offset,
    std::uint8_t r_expected,
    std::uint32_t color)
{
    if (base == nullptr || !readable_memory(base + b_offset, 1) ||
        !readable_memory(base + g_offset, 1) || !readable_memory(base + r_offset, 1))
    {
        return false;
    }
    std::uint8_t b = 0, g = 0, r = 0;
    std::memcpy(&b, base + b_offset, 1);
    std::memcpy(&g, base + g_offset, 1);
    std::memcpy(&r, base + r_offset, 1);
    if (b != b_expected || g != g_expected || r != r_expected)
    {
        return false;
    }
    return write_executable_byte(base + b_offset, b_expected, static_cast<std::uint8_t>((color >> 16) & 0xFF)) &&
        write_executable_byte(base + g_offset, g_expected, static_cast<std::uint8_t>((color >> 8) & 0xFF)) &&
        write_executable_byte(base + r_offset, r_expected, static_cast<std::uint8_t>(color & 0xFF));
}

void patch_local_palette_constants(HMODULE module)
{
    const auto image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) return;
    const auto patch_slot = [&](std::size_t rva, ColorSlot slot, std::uint8_t b, std::uint8_t g, std::uint8_t r)
    {
        const auto& configured = g_slot_overrides[static_cast<std::size_t>(slot)];
        if (!configured.has_value()) return;
        const auto color = *configured;
        const bool p1_main_layout = slot == ColorSlot::Local1Main;
        bool ok = p1_main_layout ?
            patch_executable_triplet(image + rva, 1, b, 6, g, 8, r, color) :
            patch_executable_triplet(image + rva, 1, b, 3, g, 5, r, color);
        if (!ok && slot == ColorSlot::Local1Main)
        {
            ok = patch_executable_triplet(image + rva, 1, 0x24, 3, 0x1C, 5, 0xED, color);
        }
        else if (!ok && slot == ColorSlot::Local2Main)
        {
            ok = patch_executable_triplet(image + rva, 1, 0xEF, 6, 0xB7, 11, 0x00, color);
        }
        else if (!ok && slot == ColorSlot::Local1Dark)
        {
            ok = patch_executable_triplet(image + rva, 1, 0x12, 3, 0x0E, 5, 0x77, color);
        }
        else if (!ok && slot == ColorSlot::Local2Dark)
        {
            ok = patch_executable_triplet(image + rva, 1, 0x78, 3, 0x5C, 5, 0x00, color);
        }
        write_log_line(module, std::string("Local palette runtime patch; slot=") +
            std::to_string(static_cast<std::size_t>(slot)) + (ok ? "; applied." : "; skipped (signature mismatch)."));
    };
    patch_slot(kLocalP1MainColorRva, ColorSlot::Local1Main, 0x96, 0x63, 0x7B);
    patch_slot(kLocalP2MainColorRva, ColorSlot::Local2Main, 0x67, 0x53, 0xC2);
    patch_slot(kLocalP1DarkColorRva, ColorSlot::Local1Dark, 0x4B, 0x32, 0x3E);
    patch_slot(kLocalP2DarkColorRva, ColorSlot::Local2Dark, 0x34, 0x2A, 0x61);
}

RValue& player_rgb_detour(
    CInstance* self,
    CInstance* other,
    RValue& out,
    int arg_count,
    RValue* args)
{
    void* return_address = _ReturnAddress();
    if (g_player_rgb_original == nullptr)
    {
        return out;
    }

    RValue& result = g_player_rgb_original(self, other, out, arg_count, args);
    refresh_animated_colors();
    apply_color_override(result, false, return_address);
    return result;
}

RValue& player_color_update_detour(
    CInstance* self,
    CInstance* other,
    RValue& out,
    int arg_count,
    RValue* args)
{
    if (g_player_color_update_original == nullptr)
    {
        return out;
    }

    RValue& result = g_player_color_update_original(self, other, out, arg_count, args);
    refresh_animated_colors();
    apply_online_palette_globals();
    patch_cached_local_palette_values();
    return result;
}

RValue& player_darker_detour(
    CInstance* self,
    CInstance* other,
    RValue& out,
    int arg_count,
    RValue* args)
{
    void* return_address = _ReturnAddress();
    if (g_player_darker_original == nullptr)
    {
        return out;
    }

    RValue& result = g_player_darker_original(self, other, out, arg_count, args);
    refresh_animated_colors();
    apply_color_override(result, true, return_address);
    return result;
}

RValue& player_rgb_no_cpu_detour(
    CInstance* self,
    CInstance* other,
    RValue& out,
    int arg_count,
    RValue* args)
{
    void* return_address = _ReturnAddress();
    if (g_player_rgb_no_cpu_original == nullptr)
    {
        return out;
    }

    RValue& result = g_player_rgb_no_cpu_original(self, other, out, arg_count, args);
    refresh_animated_colors();
    // Used by local 1P/2P cards on the character-select screen.
    apply_color_override(result, false, return_address);
    return result;
}

RValue& player_rgb_old_detour(
    CInstance* self,
    CInstance* other,
    RValue& out,
    int arg_count,
    RValue* args)
{
    void* return_address = _ReturnAddress();
    if (g_player_rgb_old_original == nullptr)
    {
        return out;
    }
    RValue& result = g_player_rgb_old_original(self, other, out, arg_count, args);
    refresh_animated_colors();
    apply_color_override(result, false, return_address);
    return result;
}

int make_color_rgb_detour(int blue, int green, int red)
{
    if (g_make_color_rgb_original == nullptr)
    {
        return (blue << 16) | (green << 8) | red;
    }

    const int original = g_make_color_rgb_original(blue, green, red);
    refresh_animated_colors();
    void* return_address = _ReturnAddress();
    const auto caller = caller_rva(return_address);
    std::optional<ColorSlot> slot;
    if (caller == kOnlineOpponentMainColorReturnRva)
    {
        slot = ColorSlot::OnlineOpponentMain;
    }
    else if (caller == kOnlineOpponentDarkColorReturnRva)
    {
        slot = ColorSlot::OnlineOpponentDark;
    }

    if (!slot.has_value())
    {
        return original;
    }

    const auto override = effective_color(*slot);
    if (!override.has_value())
    {
        return original;
    }

    return static_cast<int>(*override);
}

bool install_color_hook(
    void* target,
    void* detour,
    PlayerRgbScript* original,
    std::string_view label)
{
    const HMODULE loader = GetModuleHandleW(L"loader.dll");
    if (loader == nullptr || target == nullptr || original == nullptr)
    {
        write_log_line(g_module, std::string(label) + ": loader or target unavailable; no hook installed.");
        return false;
    }

    const auto hook_create = reinterpret_cast<LoaderHookCreate>(
        GetProcAddress(loader, kHookCreateExport));
    const auto hook_enable = reinterpret_cast<LoaderHookEnable>(
        GetProcAddress(loader, kHookEnableExport));
    const auto hook_remove = reinterpret_cast<LoaderHookRemove>(
        GetProcAddress(loader, kHookRemoveExport));
    if (hook_create == nullptr || hook_enable == nullptr || hook_remove == nullptr)
    {
        write_log_line(g_module, std::string(label) + ": loader hook API unavailable; no hook installed.");
        return false;
    }

    const int create_status = hook_create(
        target,
        detour,
        reinterpret_cast<void**>(original));
    if (create_status != 0 || *original == nullptr)
    {
        hook_remove(target);
        write_log_line(g_module, std::string(label) + ": hook creation failed; no hook installed.");
        return false;
    }

    const int enable_status = hook_enable(target);
    if (enable_status != 0)
    {
        hook_remove(target);
        *original = nullptr;
        write_log_line(g_module, std::string(label) + ": hook enable failed; no hook installed.");
        return false;
    }

    write_log_line(g_module, std::string(label) + ": hook installed.");
    return true;
}

bool install_make_color_hook(void* target)
{
    const HMODULE loader = GetModuleHandleW(L"loader.dll");
    if (loader == nullptr || target == nullptr)
    {
        write_log_line(g_module, "Online palette initializer: loader or target unavailable; no hook installed.");
        return false;
    }

    const auto hook_create = reinterpret_cast<LoaderHookCreate>(GetProcAddress(loader, kHookCreateExport));
    const auto hook_enable = reinterpret_cast<LoaderHookEnable>(GetProcAddress(loader, kHookEnableExport));
    const auto hook_remove = reinterpret_cast<LoaderHookRemove>(GetProcAddress(loader, kHookRemoveExport));
    if (hook_create == nullptr || hook_enable == nullptr || hook_remove == nullptr)
    {
        write_log_line(g_module, "Online palette initializer: loader hook API unavailable; no hook installed.");
        return false;
    }

    const int create_status = hook_create(
        target,
        reinterpret_cast<void*>(&make_color_rgb_detour),
        reinterpret_cast<void**>(&g_make_color_rgb_original));
    if (create_status != 0 || g_make_color_rgb_original == nullptr)
    {
        hook_remove(target);
        write_log_line(g_module, "Online palette initializer: hook creation failed; no hook installed.");
        return false;
    }

    const int enable_status = hook_enable(target);
    if (enable_status != 0)
    {
        hook_remove(target);
        g_make_color_rgb_original = nullptr;
        write_log_line(g_module, "Online palette initializer: hook enable failed; no hook installed.");
        return false;
    }

    write_log_line(g_module, "Online palette initializer: hook installed.");
    return true;
}

DWORD WINAPI initialize_player_color(LPVOID parameter)
{
    const auto module = static_cast<HMODULE>(parameter);
    g_module = module;
    load_color_override(module);
    const HMODULE game = GetModuleHandleW(nullptr);
    if (game == nullptr)
    {
        write_log_line(module, kUnavailableLine);
        return 0;
    }

    const auto* image_base = reinterpret_cast<const std::byte*>(game);
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(image_base);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE ||
        dos_header->e_lfanew <= 0 ||
        dos_header->e_lfanew > 0x1000)
    {
        write_log_line(module, kUnavailableLine);
        return 0;
    }

    const auto* nt_headers =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(image_base + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE ||
        nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        write_log_line(module, kUnavailableLine);
        return 0;
    }

    patch_local_palette_constants(module);

    void* player_rgb = rwp::resolve_script_registry_entry(
        image_base,
        nt_headers->OptionalHeader.SizeOfImage,
        kPlayerRgbRegistryRva,
        kPlayerRgbName);
    void* player_color_update = rwp::resolve_script_registry_entry(
        image_base,
        nt_headers->OptionalHeader.SizeOfImage,
        kPlayerColorUpdateRegistryRva,
        kPlayerColorUpdateName);
    void* player_rgb_no_cpu = rwp::resolve_script_registry_entry(
        image_base,
        nt_headers->OptionalHeader.SizeOfImage,
        kPlayerRgbNoCpuRegistryRva,
        kPlayerRgbNoCpuName);
    void* player_rgb_old = rwp::resolve_script_registry_entry(
        image_base,
        nt_headers->OptionalHeader.SizeOfImage,
        kPlayerRgbOldRegistryRva,
        kPlayerRgbOldName);

    void* player_darker = rwp::resolve_script_registry_entry(
        image_base,
        nt_headers->OptionalHeader.SizeOfImage,
        kPlayerDarkerRegistryRva,
        kPlayerDarkerName);
    if (player_rgb == nullptr || player_darker == nullptr)
    {
        write_log_line(module, kUnavailableLine);
        return 0;
    }

    write_log_line(module, kFoundLine);
    void* make_color_rgb = nullptr;
    if (kMakeColorRgbRva < nt_headers->OptionalHeader.SizeOfImage)
    {
        make_color_rgb = const_cast<std::byte*>(image_base) + kMakeColorRgbRva;
    }
    install_make_color_hook(make_color_rgb);
    install_color_hook(
        player_rgb,
        reinterpret_cast<void*>(&player_rgb_detour),
        &g_player_rgb_original,
        "Player RGB main");
    if (player_color_update != nullptr)
    {
        install_color_hook(
            player_color_update,
            reinterpret_cast<void*>(&player_color_update_detour),
            &g_player_color_update_original,
            "Player color update");
    }
    if (player_rgb_no_cpu != nullptr)
    {
        install_color_hook(
            player_rgb_no_cpu,
            reinterpret_cast<void*>(&player_rgb_no_cpu_detour),
            &g_player_rgb_no_cpu_original,
            "Player RGB no-cpu");
    }
    else
    {
        write_log_line(module, "Player RGB no-cpu: entry unavailable; no hook installed.");
    }
    if (player_rgb_old != nullptr)
    {
        install_color_hook(
            player_rgb_old,
            reinterpret_cast<void*>(&player_rgb_old_detour),
            &g_player_rgb_old_original,
            "Player RGB old");
    }
    else
    {
        write_log_line(module, "Player RGB old: entry unavailable; no hook installed.");
    }
    install_color_hook(
        player_darker,
        reinterpret_cast<void*>(&player_darker_detour),
        &g_player_darker_original,
        "Player RGB dark");
    return 0;
}
} // namespace

extern "C" __declspec(dllexport) const char* rwp_milestone()
{
    return "player-color-x86-v2";
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);

        const HANDLE thread = CreateThread(nullptr, 0, initialize_player_color, module, 0, nullptr);
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
