find_package(Threads REQUIRED)
add_library(manifest_console STATIC ManifestConsole.cpp ManifestConsole.h)
target_include_directories(manifest_console PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(manifest_console
    PUBLIC Manifest::Game Jettison::Devtools
    PRIVATE manifest_warnings)
set_target_properties(manifest_console PROPERTIES FOLDER "Tools")

add_executable(manifest_tui main.cpp ManifestTui.cpp ManifestTui.h)
set_target_properties(manifest_tui PROPERTIES FOLDER "Tools")
target_link_libraries(manifest_tui PRIVATE manifest_console
    ftxui::screen ftxui::component ftxui::dom Threads::Threads manifest_warnings)
target_compile_definitions(manifest_tui PRIVATE
    MANIFEST_DATA_DIRECTORY="${MANIFEST_DATA_OUTPUT_DIR}"
    MANIFEST_UNIVERSE_FILE="${MANIFEST_DATA_SOURCE_DIR}/universe/solar-system-names.yaml")
if(TARGET manifest_data)
    add_dependencies(manifest_tui manifest_data)
endif()

if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
    add_executable(manifest_console_tests tests/ManifestConsoleTests.cpp)
    target_link_libraries(manifest_console_tests PRIVATE
        manifest_console manifest_test_helpers doctest::doctest_with_main manifest_warnings)
    target_compile_definitions(manifest_console_tests PRIVATE
        MANIFEST_DATA_DIRECTORY="${MANIFEST_DATA_OUTPUT_DIR}"
        MANIFEST_UNIVERSE_FILE="${MANIFEST_DATA_SOURCE_DIR}/universe/solar-system-names.yaml")
    add_dependencies(manifest_console_tests manifest_data)
    set_target_properties(manifest_console_tests PROPERTIES FOLDER "Test")
    add_test(NAME console COMMAND manifest_console_tests)
endif()
