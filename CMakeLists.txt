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
    src/gfx/renderer.cpp
    src/gfx/bitmap.cpp
    src/gfx/font.cpp
    src/res/pe_resource.cpp
    src/res/lzhuf.cpp
    src/res/vol.cpp
    src/res/res_manager.cpp
    src/gfx/pbmp.cpp
    src/game/game_map.cpp
    src/game/terrain_renderer.cpp
    src/app/tapp.cpp
    src/app/shell_state.cpp
    src/app/game_state.cpp
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
