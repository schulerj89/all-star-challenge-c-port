cmake_minimum_required(VERSION 3.16)

project(nba_all_star_challenge_c_port LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ALLSTAR_BUILD_SDL_GAME "Build the cross-platform SDL game" ON)
option(ALLSTAR_BUILD_WIN32_REFERENCE
       "Build the original Win32 game host for transition testing" OFF)
set(ALLSTAR_ASSETPACK_PATH "" CACHE FILEPATH
    "Path to a user-generated allstar.assetpack to bundle with the game")

set(ALLSTAR_CORE_SOURCES
    src/allstar_rom.c
    src/allstar_asset_pack.c
    src/allstar_font.c
    src/allstar_renderer.c
    src/allstar_controls.c
    src/allstar_roster.c
    src/allstar_game.c
    src/gameplay/allstar_physics.c
    src/gameplay/allstar_rng.c
    src/gameplay/allstar_ai.c
    src/gameplay/allstar_one_on_one.c
    src/gameplay/allstar_free_throw.c
    src/gameplay/allstar_horse.c
    src/gameplay/allstar_accuracy.c
    src/gameplay/allstar_tournament.c
    src/gameplay/allstar_postgame.c
    src/gameplay/allstar_select.c
    src/gameplay/allstar_shot_result.c
    src/gameplay/allstar_court_state.c
    src/gameplay/allstar_game_clock.c
    src/gameplay/allstar_status_panel.c
    src/gameplay/allstar_menu.c
    src/gameplay/allstar_settings_screen.c
    src/gameplay/allstar_system.c
    src/gameplay/allstar_link.c
    src/gameplay/allstar_cpu_target.c
    src/audio/allstar_voice_state.c
    src/scenes/scene_intro.c
    src/scenes/scene_menu.c
    src/scenes/scene_settings.c
    src/scenes/scene_roster_select.c
    src/scenes/scene_one_on_one.c
    src/scenes/scene_three_point.c
    src/scenes/scene_free_throw.c
    src/scenes/scene_horse.c
    src/scenes/scene_tournament.c
)

add_library(allstar_core STATIC ${ALLSTAR_CORE_SOURCES})
target_include_directories(allstar_core PUBLIC include)

if(NOT MSVC)
    target_compile_options(allstar_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT IOS)
    add_executable(allstar_port
        src/main.c
        src/allstar_cli.c
        src/audio/allstar_audio.c
    )
    target_link_libraries(allstar_port PRIVATE allstar_core)

    if(WIN32)
        target_link_libraries(allstar_port PRIVATE winmm)
    endif()
endif()

if(ALLSTAR_BUILD_SDL_GAME)
    include(FetchContent)

    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    if(IOS)
        option(ALLSTAR_IOS_USE_ASSET_CATALOG
            "Compile the iOS asset catalog instead of using legacy PNG icons"
            ON)
        if(NOT ALLSTAR_ASSETPACK_PATH)
            set(ALLSTAR_ASSETPACK_PATH
                "${CMAKE_CURRENT_SOURCE_DIR}/build/macos/allstar.assetpack")
        endif()
        if(NOT EXISTS "${ALLSTAR_ASSETPACK_PATH}")
            message(FATAL_ERROR
                "Build a version-17 allstar.assetpack and pass its path with "
                "-DALLSTAR_ASSETPACK_PATH=/absolute/path/allstar.assetpack")
        endif()

        set(ALLSTAR_IOS_ASSET_CATALOG
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ios/Assets.xcassets")
        set(ALLSTAR_IOS_LEGACY_ICONS
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ios/AppIcon-60@2x.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ios/AppIcon-60@3x.png"
        )
        set(ALLSTAR_IOS_RESOURCES
            "${ALLSTAR_ASSETPACK_PATH}"
            ${ALLSTAR_IOS_LEGACY_ICONS}
        )
        if(ALLSTAR_IOS_USE_ASSET_CATALOG)
            list(APPEND ALLSTAR_IOS_RESOURCES
                "${ALLSTAR_IOS_ASSET_CATALOG}")
        endif()
        set_source_files_properties(${ALLSTAR_IOS_RESOURCES} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        add_executable(allstar_port_game MACOSX_BUNDLE
            src/sdl_game_main.c
            src/audio/allstar_audio.c
            ${ALLSTAR_IOS_RESOURCES}
        )
        set_target_properties(allstar_port_game PROPERTIES
            RESOURCE "${ALLSTAR_IOS_RESOURCES}"
            MACOSX_BUNDLE_INFO_PLIST
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ios/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "NBA All-Star Challenge"
            MACOSX_BUNDLE_GUI_IDENTIFIER
                "com.schulerj89.allstarchallenge.ios"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
            MACOSX_BUNDLE_BUNDLE_VERSION "1"
            XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES "NO"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
                "com.schulerj89.allstarchallenge.ios"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
        )
        if(ALLSTAR_IOS_USE_ASSET_CATALOG)
            set_property(TARGET allstar_port_game PROPERTY
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
        endif()
    elseif(APPLE)
        set(ALLSTAR_MACOS_ICON
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/nba_allstar_challenge.icns")
        set_source_files_properties(${ALLSTAR_MACOS_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        add_executable(allstar_port_game MACOSX_BUNDLE
            src/sdl_game_main.c
            src/audio/allstar_audio.c
            ${ALLSTAR_MACOS_ICON}
        )
        set_target_properties(allstar_port_game PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "NBA All-Star Challenge"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.schulerj89.allstarchallenge"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
            MACOSX_BUNDLE_BUNDLE_VERSION "1"
            MACOSX_BUNDLE_ICON_FILE "nba_allstar_challenge.icns"
        )
    elseif(WIN32)
        add_executable(allstar_port_game WIN32
            src/sdl_game_main.c
            src/audio/allstar_audio.c
        )
    else()
        add_executable(allstar_port_game
            src/sdl_game_main.c
            src/audio/allstar_audio.c
        )
    endif()

    target_compile_definitions(allstar_port_game PRIVATE ALLSTAR_USE_SDL_AUDIO=1)
    target_link_libraries(allstar_port_game PRIVATE allstar_core SDL3::SDL3-static)

    if(NOT MSVC)
        target_compile_options(allstar_port_game PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(WIN32 AND ALLSTAR_BUILD_WIN32_REFERENCE)
    add_executable(allstar_port_game_win32 WIN32
        src/win32_game_main.c
        src/audio/allstar_audio.c
    )
    target_link_libraries(allstar_port_game_win32
        PRIVATE allstar_core user32 gdi32 winmm)
endif()
