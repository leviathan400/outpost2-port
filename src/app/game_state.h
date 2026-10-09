#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ui_state.h"
#include "../game/game_map.h"
#include "../game/terrain_renderer.h"
#include "location.h"

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
    void setMouse(int nx, int ny) { m_mouseNX = nx; m_mouseNY = ny; }
    void setCamera(int x, int y) { m_camX = x; m_camY = y; }
    int shakeX() const { return m_shakeX; }
    int shakeY() const { return m_shakeY; }

    // Painel de comando à direita (216 px, medido na tela original 640x480):
    // minimapa (y 0..134), linha de 6 abas (135..180) e o quadro de informação
    // (181..479). Barra de status de 22 px sob a visão do mapa.
    static constexpr int kPanelW = 216;
    static constexpr int kMiniMapH = 135;
    static constexpr int kTabsH = 46;
    static constexpr int kStatusBarH = 22;

private:
    void loadSettings();
    bool loadMap(const std::string& mapName);
    void drawObjectives(uint32_t* fb, int fbW, int fbH, int l, int t, int r, int b);
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
    void clickCommand(int cmd);

    int viewportW() const;         // largura da área de jogo (tela - painel)
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
