#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ui_state.h"
#include "../game/game_map.h"
#include "../game/terrain_renderer.h"
#include "location.h"
#include "produce_pane.h"
#include "objectives_pane.h"
#include "storage_pane.h"
#include "research_pane.h"
#include "game_frame.h"

namespace op2 {

struct WeaponDrawOp;
struct MissionScript;

// UIState do jogo — equivale à UIState estática do engine (dword_574A80, criada
// uma vez em TApp::CreateFrames). Por enquanto desenha o terreno da missão e rola
// com as setas; a simulação (TethysGame/Unit) entra nos próximos slices.
class GameState : public UIState {
public:
    // Carrega o mapa de uma missão (nome do .map, resolvido do export MapName da
    // DLL). false se o mapa/tilesets não forem encontrados.
    bool load(const std::string& mapName);
    // Inicia uma missão a partir da DLL (T##/E##/P##.DLL) — single player de
    // sub_4893A0: DescBlock, mapa, jogadores, InitProc; triggers por turno.
    bool loadMission(const std::string& dllName);
    GameState();
    ~GameState();
    bool hasMap() const { return m_terrain.ok(); }
    const std::string& mapName() const { return m_mapName; }

    void onActivate() override;
    void onIdle(Renderer& renderer, float dt) override;
    void handleEvent(Renderer& renderer, const SDL_Event& e) override;

    void setShowPassability(bool v) { m_showPassability = v; }
    void setSelection(const std::vector<int>& ids) { m_selected = ids; }
    void pressCommand(int cmd) { clickCommand(cmd); }      // testes: botão do painel
    ResearchPane& researchPaneForTest() { return m_research; }
    void setMouse(int nx, int ny) { m_mouseNX = nx; m_mouseNY = ny; }
    void setCamera(int x, int y) { m_camX = x; m_camY = y; }
    int shakeX() const { return m_shakeX; }
    int shakeY() const { return m_shakeY; }

    // Layout da janela do jogo em 640x480 (sub_499E80 / sub_4998D0): moldura
    // com título de 22 px e bordas de 12 → área cliente 616x446 em (12,22):
    //   vista (0,0) 416x416 [(W-200)&~31, H&~31]; filete de 8 px; minimapa
    //   (vista+8, 0) 192x128; faixa de 6; janela do quadro (vista+8, 0x86)
    //   192 x resto, com as abas no topo e o conteúdo 42 px abaixo
    //   (sub_45CB00); barra de status sob a vista.
    static constexpr int kClientX = 12, kClientY = 22, kClientW = 616, kClientH = 446;
    static constexpr int kViewX = kClientX, kViewY = kClientY, kViewW = 416, kViewH = 416;
    static constexpr int kColX = kClientX + kViewW + 8;       // 436
    static constexpr int kColW = 0xC0;                        // 192
    static constexpr int kMiniMapY = kClientY, kMiniMapH = 0x80;
    static constexpr int kPaneWinY = kClientY + 0x86;         // 156: janela do quadro (abas)
    static constexpr int kPaneTop = kPaneWinY + 42;           // 198: conteúdo
    static constexpr int kPaneBottom = kClientY + kClientH;   // 468
    static constexpr int kStatusY = kViewY + kViewH;          // 438
    static constexpr int kStatusH = kClientH - kViewH;        // 30

private:
    void loadSettings();
    bool loadMap(const std::string& mapName);
    std::unique_ptr<MissionScript> m_mission;   // dword_4EFCD8
    std::string m_mapName;
    TerrainRenderer m_terrain;   // desenha sobre o g_gameMap (singleton do engine)
    int m_camX = 0, m_camY = 0;
    int m_maxX = 0, m_maxY = 0;
    float m_tickAccum = 0.0f;     // acumulador p/ avançar o tick de simulação
    float m_tickPeriod = 0.05f;   // s/tick: 1/clamp(GameSpeed*4, 4, 40) (sub_49BF80)
    int m_lastRand = 0;           // última amostra do RNG (1 por tick), p/ o HUD
    bool m_showPassability = false; // overlay de debug (tecla G)
    bool m_showDebugHud = false;    // overlay de depuração (Tick/RNG/economia; tecla F3)
    bool m_stationaryShadows = false; // INI StationaryShadows (dword_575634)
    bool m_mobileShadows = false;     // INI MobileShadows (dword_575638)
    int  m_frameSkip = 1;             // INI FrameSkip (Value)
    int  m_shakeX = 0, m_shakeY = 0;  // tremor acumulado (vista +0x138/+0x13C)

    // --- Seleção / interação (a "MAINE" / command pane) ---
    std::vector<int> m_selected;   // ids de unidades selecionadas
    bool m_dragging = false;       // arrastando caixa de seleção?
    int  m_dragSX = 0, m_dragSY = 0;   // início da caixa (coords nativas)
    int  m_mouseNX = 0, m_mouseNY = 0; // mouse atual (coords nativas)
    int  m_targetCmd = -1;         // comando em modo de mira (UiCommand) ou -1

    // Botoeira do veículo (sub_46D540): botões montados p/ a seleção atual.
    struct CmdButton { int cmd; int l, t, r, b; int iconRaw; std::string tip; char hotkey; };
    std::vector<CmdButton> commandButtons() const;
    // Botões de texto do prédio (sub_459F70 + layout sub_458D10). wide = arte 0x97.
    struct TextButton { int cmd; int l, t, r, b; bool wide; std::string label, tip; char hotkey;
                        bool disabled = false; };
    // Painel de produção (BUILD da fábrica) e seus botões (sub_4682C0).
    ProducePane m_produce;
    ObjectivesPane m_objectives;   // aba de objetivos (sem seleção)
    StoragePane m_storage;         // STORAGE (baias)
    ResearchPane m_research;       // ASSIGN (pesquisa do laboratório)
    static constexpr int kCmdAssign = -5, kCmdCancel = -6, kCmdStart = -7, kCmdTopics = -8;
    void closePanes() { m_produce.close(); m_storage.close(); m_research.close(); }
    static constexpr int kCmdGameOptions = -4;
    static constexpr int kCmdBuildIt = -2, kCmdStatus = -3;
    PaneRect infoPaneRect() const;
    void produceBuildIt();
    std::vector<TextButton> structureButtons() const;
    void clickCommand(int cmd);

    int viewportW() const;         // largura da vista do mapa (416)
    int viewportH() const { return kViewH; }   // altura da vista (416)
    GameFrame m_frame;             // moldura (bitmaps do OUT2RES.DLL)
    // X de tela de um pixel de mundo, tratando o WRAP horizontal do mapa
    // (o engine faz o mapa dar a volta em X; unidades do outro lado da emenda
    // precisam aparecer no lado certo da câmera).
    int screenXOf(int worldPixelX) const;
    void drawBeam(Renderer& renderer, const WeaponDrawOp& op);
    void clampCamera();
    void selectAt(int nx, int ny, bool additive);
    void selectBox(int x0, int y0, int x1, int y1);
    void renderPanel(Renderer& renderer);
};

} // namespace op2
