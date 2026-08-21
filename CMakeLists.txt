cmake_minimum_required(VERSION 3.21)

project(rivals-workshop-practice VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This mod targets the Windows build of Rivals of Aether 1.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This mod must be configured for Win32/x86. Use the Visual Studio generator with -A Win32.")
endif()

add_library(rivals-workshop-practice SHARED
    src/animation.cpp
    src/color_mapping.cpp
    src/dllmain.cpp
    src/script_registry.cpp
)

add_executable(rwp-load-smoke
    tests/load_smoke.cpp
)

add_executable(rwp-registry-smoke
    tests/registry_smoke.cpp
    src/script_registry.cpp
)

add_executable(rwp-color-mapping-smoke
    tests/color_mapping_smoke.cpp
    src/color_mapping.cpp
)

add_executable(rwp-animation-smoke
    tests/animation_smoke.cpp
    src/animation.cpp
)

target_compile_features(rivals-workshop-practice PRIVATE cxx_std_20)
target_compile_features(rwp-load-smoke PRIVATE cxx_std_20)
target_compile_features(rwp-registry-smoke PRIVATE cxx_std_20)
target_compile_features(rwp-color-mapping-smoke PRIVATE cxx_std_20)
target_compile_features(rwp-animation-smoke PRIVATE cxx_std_20)
target_include_directories(rivals-workshop-practice PRIVATE src)
target_include_directories(rwp-registry-smoke PRIVATE src)
target_include_directories(rwp-color-mapping-smoke PRIVATE src)
target_include_directories(rwp-animation-smoke PRIVATE src)
target_compile_definitions(rivals-workshop-practice PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_definitions(rwp-load-smoke PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_definitions(rwp-registry-smoke PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_definitions(rwp-color-mapping-smoke PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_compile_definitions(rwp-animation-smoke PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(rivals-workshop-practice PRIVATE
        /W4
        /permissive-
        /EHsc
    )
    target_compile_options(rwp-load-smoke PRIVATE
        /W4
        /permissive-
        /EHsc
    )
    target_compile_options(rwp-registry-smoke PRIVATE
        /W4
        /permissive-
        /EHsc
    )
    target_compile_options(rwp-color-mapping-smoke PRIVATE
        /W4
        /permissive-
        /EHsc
    )
    target_compile_options(rwp-animation-smoke PRIVATE
        /W4
        /permissive-
        /EHsc
    )
    set_property(TARGET rivals-workshop-practice PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_property(TARGET rwp-load-smoke PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_property(TARGET rwp-registry-smoke PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_property(TARGET rwp-color-mapping-smoke PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    set_property(TARGET rwp-animation-smoke PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

set_target_properties(rivals-workshop-practice PROPERTIES
    OUTPUT_NAME "player-color"
    PREFIX ""
)
