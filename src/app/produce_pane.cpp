#include "produce_pane.h"

#include <algorithm>
#include <string>

#include "../gfx/art_blit.h"
#include "../gfx/artfile.h"
#include "../gfx/font.h"
#include "../gfx/ui_font.h"
#include "../game/production.h"
#include "../game/production_queue.h"
#include "../audio/sound.h"
#include "../game/structure.h"
#include "../game/tethys_game.h"
#include "../game/unit.h"

namespace op2 {

static uint32_t rgb555(uint16_t c) {
    const uint32_t r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// Tamanho (bbox do frame 0) de uma animação de UI (índice real).
static void artSize(int anim, int& w, int& h) {
    w = h = 0;
    if (!g_art.ok() || anim < 0 || anim >= g_art.numAnimations()) return;
    const ArtAnimation& an = g_art.animation(anim);
    if (an.frames.empty()) return;
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20);
    for (const auto& pic : an.frames[0].pics) {
        const ArtImage& im = g_art.image(pic.image);
        x0 = std::min(x0, static_cast<int>(pic.x)); y0 = std::min(y0, static_cast<int>(pic.y));
        x1 = std::max(x1, static_cast<int>(pic.x) + static_cast<int>(im.width));
        y1 = std::max(y1, static_cast<int>(pic.y) + static_cast<int>(im.height));
    }
    if (x1 > x0) { w = x1 - x0; h = y1 - y0; }
}
static const int kArrowUp = 0x1C6 - 0x65, kArrowDown = 0x1C5 - 0x65;   // "Scroll up/down page"

ListBoxLayout listBoxLayout(const PaneRect& pane, int arg8, int argC, int arg10, int arg14) {   // sub_459940
    ListBoxLayout L{};
    int aw, ah; artSize(kArrowUp, aw, ah);
    L.list = PaneRect{pane.l + arg8 + 0xD, pane.t + argC + 0xD, pane.r - arg10 - 0xE, pane.b - ah - arg14 - 0x10};
    // sub_430260: a altura do item é a da fonte ([ebx+1Ch] = tmHeight), as linhas
    // são (altura − 3) / item (no máximo 0x40) e a BASE da lista recua para
    // topo + linhas × item + 4 — as setas ficam embaixo dessa base recuada.
    L.lineH = markupLineHeight();
    L.rows = std::min(0x40, std::max(1, (L.list.b - L.list.t - 3) / L.lineH));
    L.list.b = L.list.t + L.rows * L.lineH + 4;
    L.up = PaneRect{L.list.l - 3, L.list.b + 2, L.list.l - 3 + aw, L.list.b + 2 + ah};
    int dw, dh; artSize(kArrowDown, dw, dh);
    L.down = PaneRect{L.list.r - dw - 2, L.list.b + 2, L.list.r - 2, L.list.b + 2 + dh};
    return L;
}

void drawListBoxFrame(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L) {    // sub_430400
    const PaneRect& r = L.list;
    for (int y = std::max(0, r.t); y < std::min(fbH, r.b); ++y)
        for (int x = std::max(0, r.l); x < std::min(fbW, r.r); ++x) {
            const bool edge = y == r.t || y == r.b - 1 || x == r.l || x == r.r - 1;
            fb[y * fbW + x] = edge ? rgb555(0x4E0) : rgb555(0x40);
        }
}

void drawListBoxArrows(uint32_t* fb, int fbW, int fbH, const ListBoxLayout& L, const PaneRect& clip,
                       bool canUp, bool canDown, int mx, int my) {
    auto arrow = [&](int anim, const PaneRect& r, bool en) {
        if (!g_art.ok() || anim >= g_art.numAnimations()) return;
        const ArtAnimation& an = g_art.animation(anim);
        const bool over = en && mx >= r.l && mx < r.r && my >= r.t && my < r.b;
        // Frames do botão (sub_40AE50 {0, 1, 2}): normal / aceso (mouse em cima) /
        // desabilitado (sub_459C70: sem página p/ rolar).
        const int fr = !en ? 2 : over ? 1 : 0;
        if (fr >= static_cast<int>(an.frames.size())) return;
        blitArtFrame(fb, fbW, fbH, anim, fr, r.l, r.t, -1, clip.l, clip.t, clip.r, clip.b);   // hotspot no canto
    };
    arrow(kArrowUp, L.up, canUp);
    arrow(kArrowDown, L.down, canDown);
}

void ProducePane::open(int factoryId) {                     // sub_468090
    m_open = true;
    m_factory = factoryId;
    m_sel = 0;                                               // [+0x2CC] = 0, nada selecionado
    m_top = 0;
    m_buildOk = false;                                       // BUILD IT desabilitado (vt[0x14](0))
    m_draft.clear();
    refresh();
}

bool ProducePane::buildEnabled() const {
    return productionQueueOn() ? !m_draft.empty() : m_buildOk;
}

int ProducePane::draftCount(uint32_t item) const {
    return static_cast<int>(std::count(m_draft.begin(), m_draft.end(), item));
}

std::vector<uint32_t> ProducePane::takeDraft() {
    std::vector<uint32_t> d;
    d.swap(m_draft);
    return d;
}

void ProducePane::refresh() {                               // sub_468120
    m_items = factoryProduceList(m_factory);
    m_lastTick = TethysGame::tick();
    if (m_sel && std::find(m_items.begin(), m_items.end(), m_sel) == m_items.end()) {
        m_sel = 0; m_buildOk = false;
    } else if (m_sel) {
        m_buildOk = produceAffordable(m_factory, m_sel);
    }
    if (m_top >= static_cast<int>(m_items.size())) m_top = 0;
}

void ProducePane::select(uint32_t item) {                   // sub_4683F0
    m_sel = item;                                            // [+0x434]
    m_buildOk = produceAffordable(m_factory, item);          // vt14 → botão habilitado?
}

void ProducePane::update(const std::vector<int>& sel) {     // sub_467FF0
    if (!m_open) return;
    if (sel.size() != 1 || sel[0] != m_factory || m_factory <= 0 ||
        m_factory >= static_cast<int>(g_units.size()) || g_units[m_factory].id == -1) {
        m_open = false; return;
    }
    // Produzindo → STATUS. Com a fila do porte a lista fica aberta: é por ela
    // que se soma mais itens enquanto a fábrica trabalha.
    if (g_units[m_factory].action == 9 && !productionQueueOn()) { m_open = false; return; }
    const int t = TethysGame::tick();
    if (t != m_lastTick && (t & 3) == 1) refresh();          // dword_56DEDC & 3 == 1
}

ProducePane::Layout ProducePane::layout(const PaneRect& pane) const {   // sub_459940
    Layout L{};
    int aw, ah; artSize(kArrowUp, aw, ah);
    const int W = pane.r - pane.l;
    L.list = PaneRect{pane.l + 0xD, pane.t + 0x14 + 0xD, pane.l + W - 0xE, pane.b - ah - 0x19 - 0x10};
    // Fila do porte (5.40): a lista encolhe para caber, entre as setas e os
    // botoes, o item em producao com a barra de progresso.
    if (productionQueueOn()) L.list.b -= markupLineHeight() + 10;
    L.up = PaneRect{L.list.l - 3, L.list.b + 2, L.list.l - 3 + aw, L.list.b + 2 + ah};
    int dw, dh; artSize(kArrowDown, dw, dh);
    L.down = PaneRect{L.list.r - dw - 2, L.list.b + 2, L.list.r - 2, L.list.b + 2 + dh};
    L.lineH = markupLineHeight();
    L.rows = std::max(1, (L.list.b - L.list.t - 4) / L.lineH);
    return L;
}

void ProducePane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const Layout L = layout(pane);
    // Título (sub_468320 → sub_459B60): nome da fábrica (vt+0x40) e a instrução.
    std::string title = unitTitle(g_units[m_factory]) +
        (productionQueueOn() ? "\nClick +1, Right-click -1" : "\nSelect Item to Produce");
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t}, title, 0xFF00FF00u, true, kMarkupLine);   // unk_578F08: verde
    // Lista (sub_430400): moldura 0x4E0, fundo, item selecionado com fundo
    // 0x1CE7 e moldura 0x3DEF; texto na fonte do painel (verde).
    auto fill = [&](int x, int y, int w, int h, uint32_t c) {
        for (int yy = std::max(0, y); yy < std::min(fbH, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(fbW, x + w); ++xx) fb[yy * fbW + xx] = c;
    };
    auto frame = [&](int x, int y, int w, int h, uint32_t c) {
        fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c);
    };
    fill(L.list.l, L.list.t, L.list.r - L.list.l, L.list.b - L.list.t, rgb555(0x40));   // [+0x52] = 0x40 (sub_459940)
    frame(L.list.l, L.list.t, L.list.r - L.list.l, L.list.b - L.list.t, rgb555(0x4E0));
    const uint32_t* hoverItem = nullptr;
    for (int i = 0; i < L.rows && m_top + i < static_cast<int>(m_items.size()); ++i) {
        const uint32_t item = m_items[m_top + i];
        const int y = L.list.t + 2 + i * L.lineH;
        if (item == m_sel) {
            fill(L.list.l + 2, y, L.list.r - L.list.l - 4, L.lineH, rgb555(0x1CE7));
            frame(L.list.l + 2, y, L.list.r - L.list.l - 4, L.lineH, rgb555(0x3DEF));
        }
        int nx = L.list.l + 4;
        if (productionQueueOn()) {
            // Fila do porte: a quantidade pedida NA FRENTE do item (amarelo),
            // numa coluna fixa para os nomes ficarem alinhados.
            const int n = draftCount(item);
            if (n > 0) {
                const std::string q = std::to_string(n) + "x";
                UiFont::draw(fb, fbW, fbH, q, nx, y + 1, 0xFFFFD24Au, false);
            }
            nx += UiFont::width("99x ");
        }
        UiFont::draw(fb, fbW, fbH, produceItemName(item), nx, y + 1, 0xFF00FF00u, false);
        if (mx >= L.list.l && mx < L.list.r && my >= y && my < y + L.lineH) hoverItem = &m_items[m_top + i];
    }
    const bool canUp = m_top > 0, canDown = m_top + L.rows < static_cast<int>(m_items.size());
    drawListBoxArrows(fb, fbW, fbH, ListBoxLayout{L.list, L.up, L.down, L.lineH, L.rows}, pane, canUp, canDown, mx, my);
    // Custo do item sob o cursor (provedor vt6 sub_4686E0): a lista (sub_430550)
    // pede o texto ao provedor e o entrega ao objeto de texto unk_575570 — ele
    // sai na linha de estado, não numa caixa junto ao cursor.
    m_help = hoverItem ? produceItemCost(*hoverItem, TethysGame::localPlayer()) : std::string();
    if (productionQueueOn()) drawQueueGauge(fb, fbW, fbH, L.list.l, L.up.b + 3, L.list.r);
}

// Fila do porte (5.40): acima dos botoes, o item em producao e a sua
// porcentagem, com a mesma barra do quadro de status (sub_45B850: 0x3E0 sobre
// 0x1E0); parada, o proximo da fila esperando, ou "Idle". A direita, quantos
// ainda estao na fila.
void ProducePane::drawQueueGauge(uint32_t* fb, int fbW, int fbH, int l, int t, int r) const {
    const UnitRecord& f = g_units[m_factory];
    const std::vector<uint32_t>& fila = productionQueueOf(m_factory);
    std::string txt;
    int cur = 0, max = 0;
    if (f.action == 9 && f.produceTotal > 0) {
        const uint32_t item = static_cast<uint32_t>(f.produceType) | (static_cast<uint32_t>(f.kitWeapon) << 16);
        cur = f.produceTotal - f.field3C; max = f.produceTotal;
        txt = produceItemName(item) + " " + std::to_string(cur * 100 / max) + "%";
    } else if (!fila.empty()) {
        txt = "Waiting: " + produceItemName(fila.front());
    } else {
        txt = "Idle";
    }
    std::string resto;
    if (!fila.empty()) resto = "+" + std::to_string(fila.size() - (f.action == 9 ? 0 : 1));
    if (resto == "+0") resto.clear();
    const int rw = resto.empty() ? 0 : UiFont::width(resto) + 4;
    // O nome longo e cortado para nao invadir o contador.
    while (txt.size() > 1 && UiFont::width(txt) > r - l - rw) txt.erase(txt.size() - 2, 1);
    UiFont::draw(fb, fbW, fbH, txt, l, t, 0xFF00FF00u, false);
    if (!resto.empty()) UiFont::draw(fb, fbW, fbH, resto, r - UiFont::width(resto), t, 0xFFFFD24Au, false);
    const int by = t + markupLineHeight();
    drawProgressBar(fb, fbW, fbH, l, by, r, by + 6, cur, max > 0 ? max : 1, 0x3E0, 0x1E0);
}

bool ProducePane::click(const PaneRect& pane, int x, int y, bool dbl, bool& buildNow) {
    buildNow = false;
    if (!m_open) return false;
    const Layout L = layout(pane);
    auto in = [&](const PaneRect& r) { return x >= r.l && x < r.r && y >= r.t && y < r.b; };
    if (in(L.up)) { m_top = std::max(0, m_top - L.rows); return true; }   // sub_430910: página acima
    if (in(L.down)) {                                                      // sub_430960: página abaixo
        if (m_top + L.rows < static_cast<int>(m_items.size())) m_top += L.rows;
        return true;
    }
    if (!in(L.list)) return false;
    const int k = itemAt(pane, x, y);
    if (k < 0) return true;
    select(m_items[k]);
    if (productionQueueOn()) {
        // Cada clique (o duplo também conta dois) soma 1 ao pedido, se o
        // minério e a comida -- descontado o que as filas já comprometem --
        // ainda cobrem; senão não soma e toca o 0x58 do "sem recursos".
        if (productionQueueCanAdd(m_factory, m_draft, m_items[k])) m_draft.push_back(m_items[k]);
        else soundPlay(0x58);
        return true;
    }
    buildNow = dbl && m_buildOk;                             // sub_4683B0: duplo → BUILD IT
    return true;
}

int ProducePane::itemAt(const PaneRect& pane, int x, int y) const {
    const Layout L = layout(pane);
    if (x < L.list.l || x >= L.list.r || y < L.list.t || y >= L.list.b) return -1;
    const int i = (y - L.list.t - 2) / L.lineH;
    if (i < 0 || i >= L.rows || m_top + i >= static_cast<int>(m_items.size())) return -1;
    return m_top + i;
}

bool ProducePane::rightClick(const PaneRect& pane, int x, int y) {
    if (!m_open || !productionQueueOn()) return false;
    const int k = itemAt(pane, x, y);
    if (k < 0) return false;
    // Tira a ÚLTIMA unidade pedida desse item (a ordem dos outros fica).
    for (auto it = m_draft.rbegin(); it != m_draft.rend(); ++it)
        if (*it == m_items[k]) { m_draft.erase(std::next(it).base()); break; }
    return true;
}

} // namespace op2
