#include <SDL.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "gfx/renderer.h"
#include "shell/shell_menu.h"
#include "core/config.h"
#include "res/res_manager.h"
#include "game/game_map.h"
#include "game/terrain_renderer.h"

// Modo utilitário (sem SDL): extrai um recurso via ResManager e grava em disco.
// Usado para validar o leitor de VOL/LZHUF contra o decoder Python (byte a byte).
//   outpost2_modern.exe --extract <nome_no_vol> <arquivo_saida>
static int runExtract(const std::string& name, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto stream = op2::g_ResManager.openStream(name);
    if (!stream) {
        std::cerr << "[Extract] recurso nao encontrado: " << name << std::endl;
        return 3;
    }
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "[Extract] nao foi possivel criar: " << outPath << std::endl;
        return 4;
    }
    out.write(reinterpret_cast<const char*>(stream->data()),
              static_cast<std::streamsize>(stream->size()));
    std::cout << "[Extract] " << name << " -> " << outPath
              << " (" << stream->size() << " bytes)" << std::endl;
    return 0;
}

// Grava um buffer ARGB (w x h, top-down) como BMP 32-bit.
static bool writeBmp32(const std::string& path, const uint32_t* px, int w, int h) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint8_t hdr[54] = {
        'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0,
        40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 32,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
    };
    uint32_t fileSize = 54 + static_cast<uint32_t>(w) * h * 4;
    memcpy(&hdr[2], &fileSize, 4);
    memcpy(&hdr[18], &w, 4);
    int negH = -h;  // top-down
    memcpy(&hdr[22], &negH, 4);
    fwrite(hdr, 1, 54, f);
    fwrite(px, 4, static_cast<size_t>(w) * h, f);
    fclose(f);
    return true;
}

// Modo utilitário (sem SDL): renderiza um mapa inteiro com terreno para um BMP.
//   outpost2_modern.exe --rendermap <nome.map> <saida.bmp>
static int runRenderMap(const std::string& mapName, const std::string& outPath) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto mapStream = op2::g_ResManager.openStream(mapName);
    if (!mapStream) { std::cerr << "[RenderMap] mapa nao encontrado: " << mapName << std::endl; return 3; }
    if (!op2::g_gameMap.load(mapStream->data(), mapStream->size())) {
        std::cerr << "[RenderMap] .map invalido: " << mapName << std::endl; return 4;
    }
    op2::TerrainRenderer terrain;
    if (!terrain.init(op2::g_gameMap, op2::g_ResManager)) {
        std::cerr << "[RenderMap] falha ao carregar tilesets" << std::endl; return 5;
    }
    int W = terrain.mapPixelWidth();
    int H = terrain.mapPixelHeight();
    std::vector<uint32_t> buf(static_cast<size_t>(W) * H, 0xFF000000u);
    terrain.render(buf.data(), W, H, 0, 0);
    if (!writeBmp32(outPath, buf.data(), W, H)) {
        std::cerr << "[RenderMap] nao foi possivel gravar: " << outPath << std::endl; return 6;
    }
    std::cout << "[RenderMap] " << mapName << " -> " << outPath
              << " (" << W << "x" << H << ", "
              << op2::g_gameMap.width() << "x" << op2::g_gameMap.height() << " tiles, "
              << op2::g_gameMap.tilesets().size() << " tilesets)" << std::endl;
    return 0;
}

// Visualizador de terreno em-engine: abre a janela e rola o mapa com as setas.
//   outpost2_modern.exe --game <nome.map>
static int runGameView(const std::string& mapName) {
    if (!op2::g_ResManager.init(".")) return 2;
    auto mapStream = op2::g_ResManager.openStream(mapName);
    if (!mapStream || !op2::g_gameMap.load(mapStream->data(), mapStream->size())) {
        std::cerr << "[GameView] mapa invalido: " << mapName << std::endl; return 3;
    }
    op2::TerrainRenderer terrain;
    if (!terrain.init(op2::g_gameMap, op2::g_ResManager)) {
        std::cerr << "[GameView] falha ao carregar tilesets" << std::endl; return 4;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << std::endl; return 5;
    }
    op2::Renderer renderer;
    if (!renderer.init("Outpost 2 - Terreno (setas p/ rolar)", 1280, 960, false)) {
        SDL_Quit(); return 6;
    }

    const int VW = op2::Renderer::NATIVE_WIDTH;
    const int VH = op2::Renderer::NATIVE_HEIGHT;
    int maxX = terrain.mapPixelWidth() - VW;
    int maxY = terrain.mapPixelHeight() - VH;
    if (maxY < 0) maxY = 0;
    // começa no clipRect (área jogável), como o jogo
    int camX = op2::g_gameMap.clipRect().x1 * 32;
    int camY = op2::g_gameMap.clipRect().y1 * 32;

    bool running = true;
    uint32_t last = SDL_GetTicks();
    while (running) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        int speed = static_cast<int>(600 * dt);
        if (speed < 1) speed = 1;
        if (ks[SDL_SCANCODE_LEFT])  camX -= speed;
        if (ks[SDL_SCANCODE_RIGHT]) camX += speed;
        if (ks[SDL_SCANCODE_UP])    camY -= speed;
        if (ks[SDL_SCANCODE_DOWN])  camY += speed;
        if (camY < 0) camY = 0;
        if (camY > maxY) camY = maxY;
        // X faz wrap no engine; deixamos rolar livremente.

        renderer.clear(0xFF000000);
        terrain.render(renderer.getFramebuffer(), VW, VH, camX, camY);
        renderer.present();

        uint32_t frame = SDL_GetTicks() - now;
        if (frame < 16) SDL_Delay(16 - frame);
    }
    renderer.shutdown();
    SDL_Quit();
    return 0;
}

int main(int argc, char* argv[]) {
    // Modos utilitários que não precisam de janela/SDL.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--extract") == 0 && i + 2 < argc) {
            return runExtract(argv[i + 1], argv[i + 2]);
        }
        if (std::strcmp(argv[i], "--rendermap") == 0 && i + 2 < argc) {
            return runRenderMap(argv[i + 1], argv[i + 2]);
        }
        if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            return runGameView(argv[i + 1]);
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  Outpost 2: Divided Destiny Modern Port " << std::endl;
    std::cout << "  Engine Version: 1.0.0 (MSVC + SDL2)   " << std::endl;
    std::cout << "========================================" << std::endl;

    // Carrega as configurações do jogo (OUTPOST2.INI), como o executável original.
    op2::Config config("OUTPOST2.INI");
    op2::GameSettings settings;
    if (config.load()) {
        settings.loadFrom(config);
        std::cout << "[Config] OUTPOST2.INI carregado (jogador='" << settings.name
                  << "', GameSpeed=" << settings.gameSpeed
                  << ", ScrollRate=" << settings.scrollRate << ")" << std::endl;
    } else {
        std::cout << "[Config] OUTPOST2.INI não encontrado; usando defaults." << std::endl;
    }

    // Gateway de assets (VOLs + arquivos soltos), como o g_ResManager original.
    op2::g_ResManager.init(".");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
        return 1;
    }

    op2::Renderer renderer;
    // 640x480 native, scaled up to 960x720 (1.5x) or 1280x960 (2x) window for crisp modern displays
    if (!renderer.init("Outpost 2: Divided Destiny", 1280, 960, false)) {
        std::cerr << "Failed to initialize Renderer!" << std::endl;
        SDL_Quit();
        return 1;
    }

    op2::ShellMenu shellMenu;
    if (!shellMenu.init("OP2SHRES.DLL")) {
        std::cerr << "Warning: OP2SHRES.DLL resources could not be loaded directly." << std::endl;
    }

    // Check for --screenshot mode
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--tutorials") {
            shellMenu.showTutorials();
        }
        if (std::string(argv[i]) == "--menu") {
            shellMenu.showMainMenu();
        }
        if (std::string(argv[i]) == "--campaign") shellMenu.showNewCampaign();
        if (std::string(argv[i]) == "--prefs")    shellMenu.showPreferences();
        if (std::string(argv[i]) == "--colony")   shellMenu.showColonyGames();
        if (std::string(argv[i]) == "--about")    shellMenu.showAbout();
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            std::string outPath = argv[i + 1];
            renderer.clear(0xFF000000);
            shellMenu.render(renderer);
            
            // Save framebuffer as 32-bit BMP
            FILE* f = fopen(outPath.c_str(), "wb");
            if (f) {
                int w = op2::Renderer::NATIVE_WIDTH;
                int h = op2::Renderer::NATIVE_HEIGHT;
                uint8_t bmpHdr[54] = {
                    'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0,
                    40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 32,0,
                    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
                };
                uint32_t fileSize = 54 + w * h * 4;
                memcpy(&bmpHdr[2], &fileSize, 4);
                memcpy(&bmpHdr[18], &w, 4);
                int negH = -h; // top-down
                memcpy(&bmpHdr[22], &negH, 4);
                fwrite(bmpHdr, 1, 54, f);
                fwrite(renderer.getFramebuffer(), 4, w * h, f);
                fclose(f);
                std::cout << "[Screenshot] Saved screenshot to: " << outPath << std::endl;
            }
            renderer.shutdown();
            SDL_Quit();
            return 0;
        }
    }

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();

    while (running && !shellMenu.shouldQuit()) {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEMOTION: {
                    int nx, ny;
                    renderer.screenToNative(event.motion.x, event.motion.y, nx, ny);
                    shellMenu.handleMouseMove(nx, ny);
                    break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                    int nx, ny;
                    renderer.screenToNative(event.button.x, event.button.y, nx, ny);
                    shellMenu.handleMouseDown(nx, ny, event.button.button);
                    break;
                }

                case SDL_MOUSEBUTTONUP: {
                    int nx, ny;
                    renderer.screenToNative(event.button.x, event.button.y, nx, ny);
                    shellMenu.handleMouseUp(nx, ny, event.button.button);
                    break;
                }

                case SDL_MOUSEWHEEL:
                    shellMenu.handleMouseWheel(event.wheel.y);
                    break;

                case SDL_KEYDOWN:
                    shellMenu.handleKeyDown(event.key.keysym.sym);
                    break;

                default:
                    break;
            }
        }

        shellMenu.update(dt);

        renderer.clear(0xFF000000);
        shellMenu.render(renderer);
        renderer.present();

        // Target ~60 FPS cap
        uint32_t frameElapsed = SDL_GetTicks() - currentTicks;
        if (frameElapsed < 16) {
            SDL_Delay(16 - frameElapsed);
        }
    }

    std::cout << "[Main] Exiting cleanly." << std::endl;
    renderer.shutdown();
    SDL_Quit();
    return 0;
}
