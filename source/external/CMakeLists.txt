# Jettison feature-base, pinned rather than following main or a moving branch.
# Updating this commit and the development shell is one SDK change. Fetch the engine
# first: EnTT, yaml-cpp and doctest must be its targets, never second copies here.
set(MANIFEST_JETTISON_REVISION f85092027eda49c29001f7589608df1f0b8827fb)
set(JETTISON_SOURCE_DIR "" CACHE PATH "Use a local Jettison checkout instead of the pinned revision")
if(NOT TARGET Jettison::Math)
    if(JETTISON_SOURCE_DIR)
        FetchContent_Declare(jettison SOURCE_DIR "${JETTISON_SOURCE_DIR}" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(jettison
            GIT_REPOSITORY https://github.com/ivanhawkes/jettison.git
            GIT_TAG ${MANIFEST_JETTISON_REVISION}
            EXCLUDE_FROM_ALL)
    endif()
    block()
        set(JETTISON_BUILD_TESTS OFF)
        set(JETTISON_PLANNED_DEPENDENCIES OFF)
        set(JETTISON_INSTALL OFF)
        FetchContent_MakeAvailable(jettison)
    endblock()
endif()

# FTXUI is only a development frontend; it adds no ABI to the shipped mod or core.
# The engine has no terminal UI dependency. Keep Manifest's existing pin.
if(MANIFEST_BUILD_TUI)
    block()
        set(BUILD_SHARED_LIBS OFF)
        set(FTXUI_BUILD_EXAMPLES OFF)
        set(FTXUI_BUILD_DOCS OFF)
        set(FTXUI_BUILD_TESTS OFF)
        set(FTXUI_ENABLE_INSTALL OFF)
        FetchContent_Declare(ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
            GIT_TAG v7.0.3
            GIT_SHALLOW TRUE
            SYSTEM
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(ftxui)
    endblock()
endif()
