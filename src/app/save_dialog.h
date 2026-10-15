#pragma once

#include <SDL.h>
#include <string>

#include "../game/save_game.h"

namespace op2 {

class Renderer;

// Diálogo de slots do jogo salvo (TFileDialog, off_4D6288): Dialog 240 (salvar:
// 10 slots, descrição, prévia, SAVE/CANCEL/DELETE) e 241 (carregar: 10 slots,
// prévia, LOAD/CANCEL/DELETE) do OUT2RES.DLL. Os slots 1..10 são os arquivos
// SGAME0..9.OP2. No quadro da prévia o port mostra a missão e a data (o
// original desenha uma miniatura).
class SaveSlotsDialog {
public:
    enum Mode { kSave, kLoad };
    enum Result { kNone, kDoSave, kDoLoad, kCancel };

    void open(Mode mode, const std::string& defaultDescription);
    void close() { m_active = false; }
    bool active() const { return m_active; }
    Mode mode() const { return m_mode; }
    int slot() const { return m_sel; }
    std::string path() const { return saveSlotPath(m_sel); }
    const std::string& description() const { return m_desc; }

    void render(Renderer& renderer);
    Result handleEvent(Renderer& renderer, const SDL_Event& e);

private:
    void refresh();
    int buttonAt(int x, int y) const;   // 0..9 slot, 20 ação, 21 cancelar, 22 apagar, -1
    bool m_active = false;
    Mode m_mode = kSave;
    int m_sel = 0;
    std::string m_desc;
    SaveHeader m_hdr[10];
    bool m_has[10] = {};
    int m_mx = 0, m_my = 0;
};

} // namespace op2
