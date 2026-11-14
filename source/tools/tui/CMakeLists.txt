find_package(Threads REQUIRED)
add_executable(manifest_tui main.cpp ManifestTui.cpp ManifestTui.h)
set_target_properties(manifest_tui PROPERTIES FOLDER "Tools")
target_link_libraries(manifest_tui PRIVATE Manifest::Game
    ftxui::screen ftxui::component ftxui::dom Threads::Threads manifest_warnings)
target_compile_definitions(manifest_tui PRIVATE
    MANIFEST_DATA_DIRECTORY="${MANIFEST_DATA_OUTPUT_DIR}"
    MANIFEST_UNIVERSE_FILE="${MANIFEST_DATA_SOURCE_DIR}/universe/solar-system-names.yaml")
if(TARGET manifest_data)
    add_dependencies(manifest_tui manifest_data)
endif()
