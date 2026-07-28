cmake_minimum_required(VERSION 3.25)

project(AutoCattery VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AutoCattery phase 01 supports Windows x64 only.")
endif()

option(AUTOCATTERY_BUILD_TESTS "Build phase 01 unit tests" ON)

include(cmake/Warnings.cmake)

add_library(mew_ui_api STATIC
    third_party/mew_ui_api/src/native/mew_ui_api.c
)
target_include_directories(mew_ui_api
    PUBLIC
        third_party/mew_ui_api/src/native
)
target_compile_definitions(mew_ui_api
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
)
if(MSVC)
    target_compile_options(mew_ui_api PRIVATE /W3 /utf-8)
endif()
target_link_libraries(mew_ui_api PUBLIC user32 kernel32)

add_library(auto_cattery_core STATIC
    src/config.cpp
    src/logger.cpp
    src/module_registry.cpp
    src/ui/scene_context.cpp
    src/ui/mew_ui_bridge.cpp
    src/ui/mew_ui_scene_probe.c
)
target_link_libraries(auto_cattery_core PUBLIC mew_ui_api)
target_compile_features(auto_cattery_core PUBLIC cxx_std_20)
target_include_directories(auto_cattery_core
    PUBLIC
        include
        third_party/mewjector
    PRIVATE
        src/ui
        third_party/json/include
)
target_compile_definitions(auto_cattery_core
    PUBLIC
        AUTOCATTERY_VERSION="${PROJECT_VERSION}"
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
autocattery_enable_warnings(auto_cattery_core)

add_library(AutoCattery SHARED src/mod_main.cpp)
target_link_libraries(AutoCattery PRIVATE auto_cattery_core)
target_include_directories(AutoCattery PRIVATE third_party/mewjector)
target_compile_features(AutoCattery PRIVATE cxx_std_20)
autocattery_enable_warnings(AutoCattery)
set_target_properties(AutoCattery PROPERTIES
    OUTPUT_NAME "AutoCattery"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(AutoCattery PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/out/${config}"
    )
endforeach()

if(AUTOCATTERY_BUILD_TESTS)
    enable_testing()
    add_executable(auto_cattery_tests
        tests/config_tests.cpp
        tests/module_registry_tests.cpp
        tests/scene_context_tests.cpp
        tests/test_main.cpp
    )
    target_link_libraries(auto_cattery_tests PRIVATE auto_cattery_core)
    target_compile_features(auto_cattery_tests PRIVATE cxx_std_20)
    autocattery_enable_warnings(auto_cattery_tests)
    add_test(NAME phase02_unit_tests COMMAND auto_cattery_tests)

    add_executable(dll_smoke_tests tests/dll_smoke_tests.cpp)
    target_compile_features(dll_smoke_tests PRIVATE cxx_std_20)
    autocattery_enable_warnings(dll_smoke_tests)
    add_test(
        NAME phase02_dll_load_smoke
        COMMAND dll_smoke_tests $<TARGET_FILE:AutoCattery>
    )
endif()
