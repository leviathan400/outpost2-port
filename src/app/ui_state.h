#pragma once

#include <SDL.h>

namespace op2 {

class Renderer;

// Estado de UI polimórfico — porta a classe UIState do engine (ver
// [[op2-engine-architecture]]). No original o TApp guarda a UIState ativa em
// TApp+0x44 e o loop (TApp::Run) chama métodos virtuais dela:
//   vtbl+0x30 OnActivate · vtbl+0x34 OnDeactivate · vtbl+0x38 OnIdle (por frame)
//   vtbl+0x24 PreTranslate(MSG*)  — aqui adaptado p/ eventos SDL.
// Como usamos SDL2 (sem HWND/janelas Win32 nem fila de MSG), OnIdle desenha+tica
// e handleEvent substitui o Pretranslate/Dispatch da mensagem do Windows.
class UIState {
public:
    virtual ~UIState() = default;

    virtual void onActivate() {}                          // vtbl+0x30
    virtual void onDeactivate() {}                        // vtbl+0x34
    virtual void onIdle(Renderer& renderer, float dt) = 0; // vtbl+0x38 (tick+render)
    virtual void handleEvent(Renderer& renderer, const SDL_Event& e) {} // Pretranslate/Dispatch

    int stateId = 0;   // UIState+0x04 (id de comando/estado no original)
};

} // namespace op2
