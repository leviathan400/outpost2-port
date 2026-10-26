#include "game_options_pane.h"

#include <cstdio>

#include "../core/config.h"
#include "../gfx/font.h"
#include "objectives_pane.h"
#include "produce_pane.h"

namespace op2 {

namespace {
// Espelho do [Game] enquanto o painel está aberto (o original mexe direto nas
// globais e regrava o INI).
GameSettings g_opt;

const char* kDetail[3] = {"Low", "Medium", "High"};

// Limites do original: volumes e velocidades em 0..10, detalhe 0..2.
void clampAll() {
    auto cl = [](int& v, int lo, int hi) { if (v < lo) v = lo; if (v > hi) v = hi; };
    cl(g_opt.musicVol, 0, 10);
    cl(g_opt.soundVol, 0, 10);
    cl(g_opt.gameSpeed, 1, 10);
    cl(g_opt.scrollRate, 0, 10);
    cl(g_opt.spiffyDraw, 0, 2);
}
}

void GameOptionsPane::open() {
    Config cfg("OUTPOST2.INI");
    if (cfg.load()) g_opt.loadFrom(cfg);
    clampAll();
    m_list.reset();
    m_list.sel = 0;
    m_open = true;
}

void GameOptionsPane::saveIni() const {
    Config cfg("OUTPOST2.INI");
    cfg.load();
    g_opt.saveTo(cfg);
    cfg.save();
}

std::vector<std::string> GameOptionsPane::items() const {
    auto onOff = [](int v) { return v ? "On" : "Off"; };
    char b[96];
    std::vector<std::string> out;
    auto add = [&](const char* fmt, auto v) { std::snprintf(b, sizeof(b), fmt, v); out.push_back(b); };
    // Os rótulos e o formato "<R>" (valor alinhado à direita) são os do original.
    add("Music\n\tVolume:<R>%i", g_opt.musicVol);
    add("Sound Effects\n\tVolume:<R>%i", g_opt.soundVol);
    add("Game Speed\n\tGame Speed:<R>%i", g_opt.gameSpeed);
    add("Scrolling Speed\n\tScrolling Speed:<R>%i", g_opt.scrollRate);
    add("Graphic Detail\n\tGraphic Detail Level:<R>%s", kDetail[g_opt.spiffyDraw < 0 || g_opt.spiffyDraw > 2 ? 2 : g_opt.spiffyDraw]);
    add("Play Music<R>%s", onOff(g_opt.music));
    add("Play Sound Effects<R>%s", onOff(g_opt.sound));
    add("Display Ambient Animations<R>%s", onOff(g_opt.ambient));
    add("Display Computer Overlay<R>%s", onOff(g_opt.overlay));
    add("Display Structure Shadows<R>%s", onOff(g_opt.stationaryShadows));
    add("Display Vehicle Shadows<R>%s", onOff(g_opt.mobileShadows));
    add("Display Completed Objectives<R>%s", onOff(objectivesShowCompleted() ? 1 : 0));
    add("Skip Frames<R>%s", onOff(g_opt.frameSkip > 1));
    return out;
}

ListBoxLayout GameOptionsPane::layout(const PaneRect& pane) const {
    return listBoxLayout(pane, 0, 0x0A, 0, 0x12);
}

void GameOptionsPane::bump(int i, int dir) {
    switch (i) {
    case OPT_MUSIC_VOL:  g_opt.musicVol  += dir; break;
    case OPT_SOUND_VOL:  g_opt.soundVol  += dir; break;
    case OPT_GAME_SPEED: g_opt.gameSpeed += dir; break;
    case OPT_SCROLL_RATE:g_opt.scrollRate+= dir; break;
    case OPT_DETAIL:     g_opt.spiffyDraw+= dir; break;
    case OPT_MUSIC:      g_opt.music = !g_opt.music; break;
    case OPT_SOUND:      g_opt.sound = !g_opt.sound; break;
    case OPT_AMBIENT:    g_opt.ambient = !g_opt.ambient; break;
    case OPT_OVERLAY:    g_opt.overlay = !g_opt.overlay; break;
    case OPT_STRUCT_SHADOW: g_opt.stationaryShadows = !g_opt.stationaryShadows; break;
    case OPT_VEH_SHADOW:    g_opt.mobileShadows = !g_opt.mobileShadows; break;
    case OPT_DONE_OBJECTIVES: objectivesSetShowCompleted(!objectivesShowCompleted()); return;   // dword_57563C
    case OPT_FRAME_SKIP: g_opt.frameSkip = g_opt.frameSkip > 1 ? 1 : 2; break;
    default: return;
    }
    clampAll();
    saveIni();
}

void GameOptionsPane::draw(uint32_t* fb, int fbW, int fbH, const PaneRect& pane, int mx, int my) const {
    if (!m_open) return;
    const ListBoxLayout L = layout(pane);
    drawMarkupText(fb, fbW, fbH, PaneRect{pane.l + 0xD, pane.t + 2, pane.r - 0xE, L.list.t},
                   "Game Options", 0xFF00FF00u, true);
    m_list.draw(fb, fbW, fbH, L, pane, items(), mx, my);
}

bool GameOptionsPane::click(const PaneRect& pane, int x, int y, bool& changed, bool& back) {
    changed = false;
    back = false;
    if (!m_open) return false;
    const ListBoxLayout L = layout(pane);
    int item = -1;
    if (m_list.click(L, items(), x, y, item)) {
        if (item >= 0 && item < OPT_COUNT) {
            m_list.sel = item;
            if (!valueItem(item)) { bump(item, 0); changed = true; }   // alternância no próprio clique
        }
        return true;
    }
    return false;
}

} // namespace op2
