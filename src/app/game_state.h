#pragma once

#include <string>
#include <vector>

#include "ui_state.h"
#include "../game/game_map.h"
#include "../game/terrain_renderer.h"
#include "location.h"

namespace op2 {

// UIState do jogo — equivale à UIState estática do engine (dword_574A80, criada
// uma vez em TApp::CreateFrames). Por enquanto desenha o terreno da missão e rola
// com as setas; a simulação (TethysGame/Unit) entra nos próximos slices.
class GameState : public UIState {
public:
    // Carrega o mapa de uma missão (nome do .map, resolvido do export MapName da
    // DLL). false se o mapa/tilesets não forem encontrados.
    bool load(const std::string& mapName);
    bool hasMap() const { return m_terrain.ok(); }
    const std::string& mapName() const { return m_mapName; }

    void onActivate() override;
    void onIdle(Renderer& renderer, float dt) override;
    void handleEvent(Renderer& renderer, const SDL_Event& e) override;

    void setShowPassability(bool v) { m_showPassability = v; }

    // Largura do painel de comando (à direita); viewport = resto da tela.
    static constexpr int kPanelW = 144;

private:
    std::string m_mapName;
    TerrainRenderer m_terrain;   // desenha sobre o g_gameMap (singleton do engine)
    int m_camX = 0, m_camY = 0;
    int m_maxX = 0, m_maxY = 0;
    float m_tickAccum = 0.0f;     // acumulador p/ avançar o tick de simulação
    int m_lastRand = 0;           // última amostra do RNG (1 por tick), p/ o HUD
    bool m_showPassability = false; // overlay de debug (tecla G)

    // --- Seleção / interação (a "MAINE" / command pane) ---
    std::vector<int> m_selected;   // ids de unidades selecionadas
    bool m_dragging = false;       // arrastando caixa de seleção?
    int  m_dragSX = 0, m_dragSY = 0;   // início da caixa (coords nativas)
    int  m_mouseNX = 0, m_mouseNY = 0; // mouse atual (coords nativas)

    int viewportW() const;         // largura da área de jogo (tela - painel)
    // X de tela de um pixel de mundo, tratando o WRAP horizontal do mapa
    // (o engine faz o mapa dar a volta em X; unidades do outro lado da emenda
    // precisam aparecer no lado certo da câmera).
    int screenXOf(int worldPixelX) const;
    void clampCamera();
    void selectAt(int nx, int ny, bool additive);
    void selectBox(int x0, int y0, int x1, int y1);
    void renderPanel(Renderer& renderer);
};

} // namespace op2
