cmake_minimum_required(VERSION 3.16)

project(nba_all_star_challenge_c_port LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

set(ALLSTAR_SOURCES
    src/main.c
    src/allstar_cli.c
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
    src/audio/allstar_audio.c
    src/scenes/scene_intro.c
    src/scenes/scene_menu.c
    src/scenes/scene_roster_select.c
    src/scenes/scene_one_on_one.c
    src/scenes/scene_three_point.c
    src/scenes/scene_free_throw.c
    src/scenes/scene_horse.c
    src/scenes/scene_tournament.c
)

add_executable(allstar_port ${ALLSTAR_SOURCES})

if(WIN32)
    target_link_libraries(allstar_port PRIVATE user32 gdi32)
endif()
