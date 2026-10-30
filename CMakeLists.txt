cmake_minimum_required(VERSION 3.15)
project(Outpost2Port LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2 from third_party (pre-built development package)
set(SDL2_ROOT "${CMAKE_SOURCE_DIR}/third_party/SDL2-2.30.9")

# Detect architecture for correct lib subdir
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL2_ARCH "x64")
else()
    set(SDL2_ARCH "x86")
endif()

set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")
set(SDL2_LIBRARY "${SDL2_ROOT}/lib/${SDL2_ARCH}/SDL2.lib")
set(SDL2_MAIN_LIBRARY "${SDL2_ROOT}/lib/${SDL2_ARCH}/SDL2main.lib")
set(SDL2_DLL "${SDL2_ROOT}/lib/${SDL2_ARCH}/SDL2.dll")

# Add source files (all 6 cpp files)
add_executable(outpost2_modern
    src/main.cpp
    src/shell/shell_menu.cpp
    src/shell/rtf_text.cpp
    src/shell/scores.cpp
    src/gfx/renderer.cpp
    src/gfx/bitmap.cpp
    src/gfx/font.cpp
    src/res/pe_resource.cpp
    src/res/lzhuf.cpp
    src/res/vol.cpp
    src/res/winhelp.cpp
    src/res/res_manager.cpp
    src/gfx/pbmp.cpp
    src/gfx/artfile.cpp
    src/gfx/art_blit.cpp
    src/game/game_map.cpp
    src/game/terrain_cost.cpp
    src/game/command.cpp
    src/game/command_dispatch.cpp
    src/game/player_cmd.cpp
    src/game/pathfinder.cpp
    src/game/movement.cpp
    src/game/vehicle_anim.cpp
    src/game/structure_anim.cpp
    src/game/structure.cpp
    src/game/weapon.cpp
    src/game/missile.cpp
    src/game/salvage.cpp
    src/game/garage.cpp
    src/audio/music.cpp
    src/game/repair.cpp
    src/game/trigger.cpp
    src/game/walls.cpp
    src/game/scenario.cpp
    src/game/research.cpp
    src/game/start_info.cpp
    src/game/save_game.cpp
    src/game/disaster.cpp
    src/game/commands.cpp
    src/game/production.cpp
    src/game/docking.cpp
    src/game/construction.cpp
    src/game/research_lab.cpp
    src/app/produce_pane.cpp
    src/app/vehicle_list_pane.cpp
    src/app/comms_pane.cpp
    src/app/report_list.cpp
    src/app/factories_pane.cpp
    src/app/labs_pane.cpp
    src/app/resources_pane.cpp
    src/app/space_pane.cpp
    src/app/game_frame.cpp
    src/app/objectives_pane.cpp
    src/app/game_options_pane.cpp
    src/app/minimap.cpp
    src/app/globe.cpp
    src/net/net_game.cpp
    src/net/net_lobby.cpp
    src/shell/shell_menu_mp.cpp
    src/app/storage_pane.cpp
    src/app/research_pane.cpp
    src/gfx/ui_font.cpp
    src/audio/sound.cpp
    src/game/mining.cpp
    src/game/earthwork.cpp
    src/game/ai_group.cpp
    src/game/pinwheel.cpp
    src/game/unit_names.cpp
    src/mission/pe_loader.cpp
    src/mission/mission.cpp
    src/mission/op2_api.cpp
    src/game/building_sheet.cpp
    src/game/colony.cpp
    src/game/messages.cpp
    src/game/terrain_renderer.cpp
    src/game/game_rng.cpp
    src/game/tethys_game.cpp
    src/game/player.cpp
    src/game/unit.cpp
    src/app/tapp.cpp
    src/app/info_pane.cpp
    src/app/shell_state.cpp
    src/app/game_state.cpp
    src/app/save_dialog.cpp
    src/core/config.cpp
)

target_include_directories(outpost2_modern PRIVATE
    ${SDL2_INCLUDE_DIR}
    src
    src/gfx
    src/shell
    src/res
    src/game
    src/app
    src/core
)

target_link_libraries(outpost2_modern PRIVATE
    ${SDL2_LIBRARY}
    ${SDL2_MAIN_LIBRARY}
)

# O exe é gerado normalmente em build/<Config>/ e, após o build, é copiado para a
# pasta raiz do jogo (onde ficam todos os assets: VOLs, DLLs, OUTPOST2.INI...),
# junto com a SDL2.dll. Assim ele roda no diretório do jogo, achando os arquivos
# diretamente (sem busca de pasta).
add_custom_command(TARGET outpost2_modern POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:outpost2_modern>"
        "${CMAKE_SOURCE_DIR}/outpost2_modern.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DLL}"
        "${CMAKE_SOURCE_DIR}/SDL2.dll"
    COMMENT "Copiando outpost2_modern.exe e SDL2.dll para a pasta raiz do jogo"
)
