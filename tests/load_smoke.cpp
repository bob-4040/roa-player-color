#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace
{
constexpr const char* kExpectedMilestone = "player-color-x86-v2";
constexpr const char* kExpectedLogLine =
    "Player Color: required color functions unavailable; changes disabled.";

bool log_contains(const std::filesystem::path& path, const std::string& expected)
{
    std::ifstream log(path);
    std::string line;
    while (std::getline(log, line))
    {
        if (line == expected)
        {
            return true;
        }
    }

    return false;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: rwp-load-smoke.exe <dll-path>\n";
        return 2;
    }

    const std::filesystem::path dll_path = std::filesystem::absolute(argv[1]);
    const std::filesystem::path log_path = dll_path.parent_path() / "player-color.log";
    std::error_code error;
    std::filesystem::remove(log_path, error);

    HMODULE module = LoadLibraryW(dll_path.c_str());
    if (module == nullptr)
    {
        std::cerr << "LoadLibraryW failed: " << GetLastError() << '\n';
        return 3;
    }

    using MilestoneFunction = const char* (*)();
    const auto milestone = reinterpret_cast<MilestoneFunction>(GetProcAddress(module, "rwp_milestone"));
    if (milestone == nullptr || std::string(milestone()) != kExpectedMilestone)
    {
        std::cerr << "Milestone export is missing or unexpected.\n";
        FreeLibrary(module);
        return 4;
    }

    bool found = false;
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        if (log_contains(log_path, kExpectedLogLine))
        {
            found = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FreeLibrary(module);

    if (!found)
    {
        std::cerr << "Expected log line was not written.\n";
        return 5;
    }

    std::cout << "Smoke test passed: non-game processes fail closed without reading a script target.\n";
    return 0;
}
