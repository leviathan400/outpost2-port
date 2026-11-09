#include "cursors.h"

#include <SDL.h>

#include <vector>

#include "../res/ani_cursor.h"
#include "../res/res_manager.h"

namespace op2 {
namespace {

struct Carregado {
    AniCursor ani;
    std::vector<SDL_Cursor*> quadros;        // um SDL_Cursor por imagem
};

Carregado s_cur[CUR_COUNT];
bool s_pronto = false;
int s_lidos = 0;
CursorKind s_atual = CUR_ARROW;
int s_passo = 0;
uint32_t s_proximo = 0;                      // SDL_GetTicks do próximo quadro

// A ordem é a do enum (e a da tabela 0x4E9840 do original).
const char* kArquivo[CUR_COUNT] = {
    nullptr,            // CUR_ARROW: a seta do sistema
    "destruct.ani",
    "move.ani", "no_move.ani",
    "attack.ani", "noattack.ani",
    "bulldoze.ani",
    "build.ani", "nobuild.ani",
    "dock.ani", "nodock.ani",
    "repair.ani", "norepair.ani",
    "reprog.ani", "no_repro.ani",
    "guard.ani",
    "salvage.ani",
    "waypoint.ani",
};

void criaQuadros(Carregado& c) {
    for (auto& f : c.ani.frames) {
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(
            f.data(), c.ani.width, c.ani.height, 32, c.ani.width * 4, SDL_PIXELFORMAT_ARGB8888);
        if (!s) { c.quadros.push_back(nullptr); continue; }
        c.quadros.push_back(SDL_CreateColorCursor(s, c.ani.hotX, c.ani.hotY));
        SDL_FreeSurface(s);                  // o cursor fica com a cópia dele
    }
}

void aplica(CursorKind k, int passo) {
    Carregado& c = s_cur[k];
    if (c.quadros.empty()) { SDL_SetCursor(SDL_GetDefaultCursor()); return; }
    SDL_Cursor* q = c.quadros[static_cast<size_t>(c.ani.frameOfStep(passo))];
    if (q) SDL_SetCursor(q);
}

}  // namespace

void cursorsInit() {
    if (s_pronto) return;
    s_pronto = true;
    for (int k = 1; k < CUR_COUNT; ++k) {
        auto st = g_ResManager.openStream(kArquivo[k]);
        if (!st) continue;
        if (!aniLoad(st->data(), st->size(), s_cur[k].ani)) continue;
        criaQuadros(s_cur[k]);
        ++s_lidos;
    }
}

int cursorsLoaded() { return s_lidos; }

void cursorSet(CursorKind k) {
    if (k < 0 || k >= CUR_COUNT) k = CUR_ARROW;
    if (k == s_atual) return;
    s_atual = k;
    s_passo = 0;
    s_proximo = 0;
    if (k == CUR_ARROW || s_cur[k].quadros.empty()) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        return;
    }
    aplica(k, 0);
    // 1 jiffy = 1/60 s (a cadência do próprio arquivo, chunks `anih`/`rate`).
    s_proximo = SDL_GetTicks() + static_cast<uint32_t>(s_cur[k].ani.rateOfStep(0) * 1000 / 60);
}

void cursorAnimate() {
    if (s_atual == CUR_ARROW) return;
    Carregado& c = s_cur[s_atual];
    if (c.quadros.size() < 2) return;
    const uint32_t agora = SDL_GetTicks();
    if (s_proximo && agora < s_proximo) return;
    ++s_passo;
    aplica(s_atual, s_passo);
    s_proximo = agora + static_cast<uint32_t>(c.ani.rateOfStep(s_passo) * 1000 / 60);
}

} // namespace op2
