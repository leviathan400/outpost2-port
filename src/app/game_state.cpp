#include "game_state.h"

#include <algorithm>
#include <iostream>

#include <cstdio>
#include <cstdlib>

#include "tapp.h"
#include "../gfx/renderer.h"
#include "../gfx/font.h"
#include "../res/res_manager.h"
#include "../game/tethys_game.h"
#include "../game/player.h"
#include "../game/unit.h"
#include "../game/command.h"
#include "../game/pathfinder.h"
#include "../game/movement.h"
#include "../game/vehicle_anim.h"
#include "../gfx/artfile.h"

namespace op2 {

bool GameState::load(const std::string& mapName) {
    m_mapName = mapName;
    auto stream = g_ResManager.openStream(mapName);
    if (!stream || !g_gameMap.load(stream->data(), stream->size())) {
        std::cerr << "[GameState] mapa invalido: " << mapName << std::endl;
        return false;
    }
    if (!m_terrain.init(g_gameMap, g_ResManager)) {
        std::cerr << "[GameState] falha ao carregar tilesets de " << mapName << std::endl;
        return false;
    }
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    m_maxX = m_terrain.mapPixelWidth() - VW;
    m_maxY = m_terrain.mapPixelHeight() - VH;
    if (m_maxY < 0) m_maxY = 0;
    // começa na área jogável (clipRect), como o jogo
    m_camX = g_gameMap.clipRect().x1 * 32;
    m_camY = g_gameMap.clipRect().y1 * 32;
    clampCamera();

    // Inicia o núcleo de simulação (relógio zerado + RNG semeado). O seed virá da
    // missão/netgame nos próximos slices; por ora um valor fixo p/ determinismo.
    TethysGame::reset(/*seed*/ 0x12345678u, /*numPlayers*/ 1, /*localPlayer*/ 0);
    m_tickAccum = 0.0f;

    // Semeia o jogador local como uma missão faria no InitProc (via Set*). A
    // colônia/recursos reais virão da DLL da missão no próximo slice.
    resetPlayers(TethysGame::noPlayers());
    _Player& p = Player[TethysGame::localPlayer()];
    bool plymouth = (mapName.size() >= 3 &&
                     (mapName[0] == 'p' || mapName[0] == 'P') &&
                     (mapName[1] == 'l' || mapName[1] == 'L'));
    if (plymouth) p.GoPlymouth(); else p.GoEden();
    p.GoHuman();
    p.SetOre(5000);
    p.SetRareOre(0);
    p.SetFoodStored(3000);
    p.SetWorkers(16);
    p.SetScientists(8);
    p.SetKids(4);

    // Cria algumas unidades iniciais (como o InitProc/CreateUnit de uma missão),
    // centradas na área jogável — prova o pool de unidades + CreateUnit + render.
    resetUnits();
    int cx = (g_gameMap.clipRect().x1 + g_gameMap.clipRect().x2) / 2;
    int cy = (g_gameMap.clipRect().y1 + g_gameMap.clipRect().y2) / 2;
    int owner = TethysGame::localPlayer();
    // Posiciona cada unidade na célula passável mais próxima do ponto desejado
    // (evita montanhas/cânions). As posições reais virão da missão depois.
    auto place = [&](map_id type, int dx, int dy) {
        Unit u;
        // Célula livre (passável E desocupada) p/ não sobrepor footprint/unidades.
        LOCATION loc = g_gameMap.findFreeNear(LOCATION{cx + dx, cy + dy});
        TethysGame::CreateUnit(u, type, loc, owner);
    };
    place(kFirstBuilding, 0, 0);   // prédio (marcador por ora)
    place(7, 2, 0);    // CON_TRUCK
    place(3, -2, 1);   // BIG_TANK
    place(13, 1, -2);  // SCOUT
    place(2, -3, -2);  // MINER
    place(6, 3, 2);    // LIGHT_TANK

    // Centraliza a câmera na base inicial (como CenterViewOn do jogo).
    m_camX = cx * 32 - Renderer::NATIVE_WIDTH / 2;
    m_camY = cy * 32 - Renderer::NATIVE_HEIGHT / 2;
    clampCamera();

    std::cout << "[GameState] mapa " << mapName << " carregado ("
              << g_gameMap.width() << "x" << g_gameMap.height() << " tiles)" << std::endl;
    return true;
}

void GameState::clampCamera() {
    if (m_camY < 0) m_camY = 0;
    if (m_camY > m_maxY) m_camY = m_maxY;
    // X faz wrap no engine; deixamos rolar livremente (terrain faz o wrap).
}

void GameState::onActivate() {
    std::cout << "[GameState] ativo: " << m_mapName << std::endl;
}

void GameState::onIdle(Renderer& renderer, float dt) {
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    int speed = static_cast<int>(600 * dt);
    if (speed < 1) speed = 1;
    if (ks[SDL_SCANCODE_LEFT])  m_camX -= speed;
    if (ks[SDL_SCANCODE_RIGHT]) m_camX += speed;
    if (ks[SDL_SCANCODE_UP])    m_camY -= speed;
    if (ks[SDL_SCANCODE_DOWN])  m_camY += speed;
    clampCamera();

    // Avança o relógio de simulação a uma cadência fixa (~10 ticks/s por ora).
    const float kTickPeriod = 0.1f;
    m_tickAccum += dt;
    while (m_tickAccum >= kTickPeriod) {
        TethysGame::advanceTick();
        m_lastRand = TethysGame::getRand(100);  // 1 consumo determinístico por tick
        tickMovement();                          // move as unidades pela rota do pathfinder
        m_tickAccum -= kTickPeriod;
    }

    renderer.clear(0xFF000000);
    m_terrain.render(renderer.getFramebuffer(), Renderer::NATIVE_WIDTH,
                     Renderer::NATIVE_HEIGHT, m_camX, m_camY);

    // Overlay de passabilidade (debug, tecla G): tinge de vermelho as células
    // impassáveis visíveis. Base visual para waypoints/pathfinding.
    if (m_showPassability) {
        uint32_t* fb = renderer.getFramebuffer();
        int t0x = m_camX / 32, t0y = m_camY / 32;
        int t1x = (m_camX + Renderer::NATIVE_WIDTH) / 32;
        int t1y = (m_camY + Renderer::NATIVE_HEIGHT) / 32;
        for (int ty = t0y; ty <= t1y; ++ty) {
            if (ty < 0 || ty >= g_gameMap.height()) continue;
            for (int tx = t0x; tx <= t1x; ++tx) {
                if (g_gameMap.isPassable(LOCATION{tx, ty})) continue;
                int sx = tx * 32 - m_camX, sy = ty * 32 - m_camY;
                for (int yy = 0; yy < 32; ++yy) {
                    int py = sy + yy;
                    if (py < 0 || py >= Renderer::NATIVE_HEIGHT) continue;
                    for (int xx = 0; xx < 32; ++xx) {
                        int px = sx + xx;
                        if (px < 0 || px >= Renderer::NATIVE_WIDTH) continue;
                        uint32_t c = fb[py * Renderer::NATIVE_WIDTH + px];
                        uint32_t r = ((c >> 16) & 0xFF), g = (c >> 8) & 0xFF, b = c & 0xFF;
                        r = r / 2 + 128; g = g / 2; b = b / 2;   // tinge vermelho
                        fb[py * Renderer::NATIVE_WIDTH + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }

    // Unidades: por ora marcadores coloridos por dono (os sprites op2_art entram
    // num slice futuro). Prédio = quadrado maior; veículo = menor.
    static const uint32_t kOwnerColor[7] = {
        0xFF40A0FF, 0xFFFF5040, 0xFF40D070, 0xFFFFD040,
        0xFFC060FF, 0xFF40E0E0, 0xFFFF9030,
    };
    // Overlay de ordem de movimento: linha da unidade até o destino + marcador no
    // tile-alvo. Lê o estado REAL da ordem (order.waypoints, gravado por DoMove ->
    // dispatcher -> bloco de ordem). Prova visível do pipeline de comando (o
    // movimento em si entra quando o pathfinder/integrador estiverem portados).
    {
        uint32_t* fb = renderer.getFramebuffer();
        const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
        auto putPx = [&](int x, int y, uint32_t c) {
            if (x >= 0 && x < VW && y >= 0 && y < VH) fb[y * VW + x] = c;
        };
        auto drawLine = [&](int x0, int y0, int x1, int y1, uint32_t c) {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            for (;;) {
                putPx(x0, y0, c);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        };
        for (size_t i = 1; i < g_units.size(); ++i) {
            const UnitRecord& r = g_units[i];
            if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
            if (!r.hasOrder || r.action != CMD_MOVE || r.order.waypoints.empty()) continue;
            uint32_t dest = r.order.waypoints.back();
            int dxt = unpackLocX(dest), dyt = unpackLocY(dest);
            int ux = (r.pixelX >> 5) * 32 + 16 - m_camX;
            int uy = (r.pixelY >> 5) * 32 + 16 - m_camY;
            int dsx = dxt * 32 + 16 - m_camX;
            int dsy = dyt * 32 + 16 - m_camY;
            uint32_t col = kOwnerColor[(r.owner & 0x0F) % 7];
            drawLine(ux, uy, dsx, dsy, col);
            // Marcador em X no destino.
            for (int k = -5; k <= 5; ++k) {
                putPx(dsx + k, dsy + k, 0xFFFFFFFF);
                putPx(dsx + k, dsy - k, 0xFFFFFFFF);
            }
        }
    }

    // Blit de um frame de animação do op2_art (g_art) na posição de referência da
    // unidade na tela (pictures posicionadas por x,y; índice 0 transparente).
    auto drawSprite = [&](int animIdx, int frameIdx, int refX, int refY) {
        if (!g_art.ok() || animIdx < 0 || animIdx >= g_art.numAnimations()) return false;
        const ArtAnimation& an = g_art.animation(animIdx);
        if (an.frames.empty()) return false;
        if (frameIdx < 0 || frameIdx >= (int)an.frames.size()) frameIdx %= (int)an.frames.size();
        uint32_t* fb = renderer.getFramebuffer();
        const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
        std::vector<ArtPicture> pics = an.frames[frameIdx].pics;
        std::sort(pics.begin(), pics.end(),
                  [](const ArtPicture& a, const ArtPicture& b){ return a.slot < b.slot; });
        std::vector<uint32_t> px;
        for (const auto& p : pics) {
            const ArtImage& m = g_art.image(p.image);
            g_art.decode(p.image, px, /*transparent*/ true);
            for (uint32_t y = 0; y < m.height; ++y) {
                int py = refY + p.y + (int)y;
                if (py < 0 || py >= VH) continue;
                for (uint32_t x = 0; x < m.width; ++x) {
                    uint32_t c = px[y * m.width + x];
                    if (!(c >> 24)) continue;
                    int pxs = refX + p.x + (int)x;
                    if (pxs < 0 || pxs >= VW) continue;
                    fb[py * VW + pxs] = c | 0xFF000000u;
                }
            }
        }
        return true;
    };

    for (size_t i = 1; i < g_units.size(); ++i) {
        const UnitRecord& r = g_units[i];
        if (r.id == -1 || (r.flags & UNIT_DEAD)) continue;
        int refX = (r.pixelX) + 16 - m_camX;    // centro do tile (pixelX = tile*32)
        int refY = (r.pixelY) + 16 - m_camY;

        bool drew = false;
        if ((r.flags & UNIT_VEHICLE) && hasVehicleAnim(r.type)) {
            int ow = r.owner & 0x0F;
            bool eden = (ow >= 0 && ow < kMaxPlayers) ? (g_playerData[ow].isEden != 0) : true;
            int anim = vehicleAnimIndex(r.type, r.rotation, eden);   // resolver fiel (facção)
            if (anim >= 0) {
                int nf = g_art.ok() && anim < g_art.numAnimations()
                         ? (int)g_art.animation(anim).frames.size() : 1;
                int frame = nf > 0 ? (TethysGame::tick() + (int)i) % nf : 0;
                drew = drawSprite(anim, frame, refX, refY);
            }
        }
        if (!drew) {
            // Fallback: marcador colorido (prédios/tipos sem sprite ainda).
            int sx = (r.pixelX >> 5) * 32 - m_camX, sy = (r.pixelY >> 5) * 32 - m_camY;
            uint32_t col = kOwnerColor[(r.owner & 0x0F) % 7];
            int sz = (r.flags & UNIT_BUILDING) ? 26 : 16, off = (32 - sz) / 2;
            renderer.fillRect(sx + off, sy + off, sz, sz, col);
            renderer.drawRect(sx + off, sy + off, sz, sz, 0xFF101010);
        }
    }

    // Overlay: relógio (Tick/Time) + uma amostra do RNG — prova visível de que o
    // núcleo de simulação está rodando determinístico.
    uint32_t* fb = renderer.getFramebuffer();
    const int VW = Renderer::NATIVE_WIDTH, VH = Renderer::NATIVE_HEIGHT;
    const _Player& p = Player[TethysGame::localPlayer()];

    char l1[96], l2[128];
    std::snprintf(l1, sizeof(l1), "Tick %d   Time %d   Rand %d",
                  TethysGame::tick(), TethysGame::time(), m_lastRand);
    std::snprintf(l2, sizeof(l2),
                  "%s   Ore %d  Rare %d  Food %d   W%d S%d K%d",
                  p.IsEden() ? "EDEN" : "PLYMOUTH",
                  p.Ore(), p.RareOre(), p.FoodStored(),
                  p.Workers(), p.Scientists(), p.Kids());
    int liveUnits = 0;
    for (size_t i = 1; i < g_units.size(); ++i) { Unit u((int)i); if (u.IsLive()) ++liveUnits; }
    char l3[128];
    std::snprintf(l3, sizeof(l3), "%s   Units %d   (setas rolar | btn-dir mover | G passab | ESC menu)",
                  m_mapName.c_str(), liveUnits);
    Font::drawText(fb, VW, VH, l1, 8, 8, 0xFFFFE060, true);
    Font::drawText(fb, VW, VH, l2, 8, 20, 0xFF80E0A0, true);
    Font::drawText(fb, VW, VH, l3, 8, 32, 0xFFB0C4DE, true);

    renderer.present();
}

void GameState::handleEvent(Renderer& renderer, const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN) {
        // ESC volta ao shell (menu), como o jogo (ActivateShell).
        if (e.key.keysym.sym == SDLK_ESCAPE) gTApp.activateShell();
        // G alterna o overlay de passabilidade (debug).
        if (e.key.keysym.sym == SDLK_g) m_showPassability = !m_showPassability;
    }
    // Clique-direito: ordena mover todos os veículos do jogador local para o tile
    // clicado — exercita o pipeline real Unit::DoMove -> dispatcher -> bloco de
    // ordem -> stepper por tick.
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int nx, ny;
        renderer.screenToNative(e.button.x, e.button.y, nx, ny);
        int tileX = (m_camX + nx) / 32;
        int tileY = (m_camY + ny) / 32;
        if (g_gameMap.ok()) tileX &= (g_gameMap.width() - 1);
        int me = TethysGame::localPlayer();
        // UM CommandPacket com todos os veículos do jogador — assim recebem índices
        // 0,1,2... e a espiral (sub_4230F0) os espalha ao redor do destino.
        CommandPacket pkt;
        pkt.type = CMD_MOVE;
        pkt.waypoints.push_back(packLocation(tileX, tileY));
        for (int i = 1; i < static_cast<int>(g_units.size()); ++i) {
            Unit u(i);
            if (!u.IsLive() || u.OwnerID() != me || !u.IsVehicle()) continue;
            pkt.unitIds.push_back(i);
        }
        if (!pkt.unitIds.empty()) processCommandPacket(me, pkt);
    }
}

} // namespace op2
