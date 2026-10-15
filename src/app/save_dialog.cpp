#include "save_dialog.h"

#include <cstdio>
#include <ctime>

#include "../gfx/font.h"
#include "../gfx/renderer.h"

namespace op2 {

namespace {
// Dialog 240 (320x222 DLU) em px (x·1,5, y·1,6), centrado em 640x480.
constexpr int kW = 480, kH = 372;
constexpr int kX = (640 - kW) / 2, kY = (480 - kH) / 2;
constexpr int kTop = kY + 22;                       // abaixo da barra de título
constexpr int kSlotX = kX + 15, kSlotW = 186, kSlotH = 22;
constexpr int kSlotY0 = kTop + 44;                  // (10, 41) DLU
constexpr int kSlotStep = 26;
constexpr int kEditX = kX + 222, kEditY = kTop + 44, kEditW = 205, kEditH = 22;   // (148, 40)
constexpr int kPrevX = kX + 222, kPrevY = kTop + 76, kPrevW = 217, kPrevH = 166;  // (148, 60) 145x104
constexpr int kActX = kX + 222, kActY = kTop + 280, kBtnW = 81, kBtnH = 23;       // SAVE (148, 175)
constexpr int kCanX = kX + 358;                                                    // CANCEL (239, 176)
constexpr int kDelX = kX + 288, kDelY = kTop + 314;                                // DELETE (192, 196)

bool in(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
}

void SaveSlotsDialog::open(Mode mode, const std::string& defaultDescription) {
    m_mode = mode;
    m_active = true;
    m_desc = defaultDescription;
    refresh();
    m_sel = 0;
    if (mode == kLoad)                                     // o primeiro slot ocupado
        for (int i = 0; i < 10; ++i) if (m_has[i]) { m_sel = i; break; }
}

void SaveSlotsDialog::refresh() {
    for (int i = 0; i < 10; ++i) {
        m_hdr[i] = SaveHeader{};
        m_has[i] = saveReadHeader(saveSlotPath(i), m_hdr[i]);
    }
}

int SaveSlotsDialog::buttonAt(int x, int y) const {
    for (int i = 0; i < 10; ++i)
        if (in(x, y, kSlotX, kSlotY0 + i * kSlotStep, kSlotW, kSlotH)) return i;
    if (in(x, y, kActX, kActY, kBtnW, kBtnH)) return 20;
    if (in(x, y, kCanX, kActY, kBtnW, kBtnH)) return 21;
    if (in(x, y, kDelX, kDelY, kBtnW, kBtnH)) return 22;
    return -1;
}

void SaveSlotsDialog::render(Renderer& renderer) {
    if (!m_active) return;
    uint32_t* fb = renderer.getFramebuffer();
    const int W = Renderer::NATIVE_WIDTH, H = Renderer::NATIVE_HEIGHT;
    renderer.drawDialogFrame(kX, kY, kW, kH, m_mode == kSave ? "SAVE GAME" : "LOAD GAME");
    const int hov = buttonAt(m_mx, m_my);
    for (int i = 0; i < 10; ++i) {
        // Owner-draw dos slots (sub_4173A0, casos dos 10 slots): a descrição gravada
        // em +0x36C ou "EMPTY SLOT" (off_4E93B0), centrada (DT_CENTER|DT_VCENTER).
        std::string t = m_has[i] ? m_hdr[i].description : std::string("EMPTY SLOT");
        while (!t.empty() && Font::getTextWidth(t) > kSlotW - 12) t.pop_back();
        const int y = kSlotY0 + i * kSlotStep;
        renderer.drawButton(kSlotX, y, kSlotW, kSlotH, t, hov == i || m_sel == i, false,
                            m_mode == kLoad && !m_has[i]);
        if (m_sel == i) renderer.drawRect(kSlotX - 2, y - 2, kSlotW + 4, kSlotH + 4, 0xFFFFD24A);
    }
    if (m_mode == kSave) {                                 // Edit 1070
        renderer.fillRect(kEditX, kEditY, kEditW, kEditH, 0xFF0D121B);
        renderer.drawBevelRect(kEditX, kEditY, kEditW, kEditH, 0xFF05080E, 0xFF35445B);
        std::string t = m_desc + ((SDL_GetTicks() / 400) & 1 ? "_" : "");
        while (!t.empty() && Font::getTextWidth(t) > kEditW - 8) t.erase(0, 1);
        Font::drawText(fb, W, H, t, kEditX + 4, kEditY + 5, 0xFFFFFFFF);
    }
    // Quadro da prévia (caso 15): o original copia a miniatura do slot (0x10810
    // bytes) ou zera o quadro (preto) se o slot está vazio. Sem a miniatura, o
    // port escreve ali a missão, a data e o tick.
    renderer.fillRect(kPrevX, kPrevY, kPrevW, kPrevH, 0xFF000000);
    renderer.drawBevelRect(kPrevX, kPrevY, kPrevW, kPrevH, 0xFF05080E, 0xFF35445B);
    if (m_has[m_sel]) {
        const SaveHeader& h = m_hdr[m_sel];
        char when[64] = "";
        const std::time_t tt = static_cast<std::time_t>(h.time);
        if (const std::tm* lt = std::localtime(&tt)) std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", lt);
        char tick[48];
        std::snprintf(tick, sizeof(tick), "Mark %u", h.tick / 100);
        Font::drawText(fb, W, H, h.description, kPrevX + 8, kPrevY + 10, 0xFFFFFF99);
        Font::drawText(fb, W, H, h.dll, kPrevX + 8, kPrevY + 30, 0xFFCCD6E8);
        Font::drawText(fb, W, H, when, kPrevX + 8, kPrevY + 48, 0xFFCCD6E8);
        Font::drawText(fb, W, H, tick, kPrevX + 8, kPrevY + 66, 0xFFCCD6E8);
    }
    const bool canAct = m_mode == kSave || m_has[m_sel];
    renderer.drawButton(kActX, kActY, kBtnW, kBtnH, m_mode == kSave ? "SAVE" : "LOAD", hov == 20, false, !canAct);
    renderer.drawButton(kCanX, kActY, kBtnW, kBtnH, "CANCEL", hov == 21, false);
    renderer.drawButton(kDelX, kDelY, kBtnW, kBtnH, "DELETE", hov == 22, false, !m_has[m_sel]);
}

SaveSlotsDialog::Result SaveSlotsDialog::handleEvent(Renderer& renderer, const SDL_Event& e) {
    if (!m_active) return kNone;
    auto act = [&]() -> Result {
        if (m_mode == kSave) { m_active = false; return kDoSave; }
        if (!m_has[m_sel]) return kNone;
        m_active = false;
        return kDoLoad;
    };
    switch (e.type) {
    case SDL_MOUSEMOTION:
        renderer.screenToNative(e.motion.x, e.motion.y, m_mx, m_my);
        break;
    case SDL_MOUSEBUTTONUP: {
        if (e.button.button != SDL_BUTTON_LEFT) break;
        int x, y;
        renderer.screenToNative(e.button.x, e.button.y, x, y);
        const int b = buttonAt(x, y);
        if (b >= 0 && b < 10) {
            if (m_mode == kLoad && !m_has[b]) break;
            if (m_sel == b && e.button.clicks >= 2) { m_sel = b; return act(); }
            m_sel = b;
            if (m_mode == kSave && m_has[b]) m_desc = m_hdr[b].description;
        } else if (b == 20) {
            return act();
        } else if (b == 21) {
            m_active = false;
            return kCancel;
        } else if (b == 22 && m_has[m_sel]) {
            std::remove(path().c_str());
            refresh();
        }
        break;
    }
    case SDL_TEXTINPUT:
        if (m_mode == kSave && m_desc.size() < 40) m_desc += e.text.text;
        break;
    case SDL_KEYDOWN:
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE: m_active = false; return kCancel;
        case SDLK_RETURN: case SDLK_KP_ENTER: return act();
        case SDLK_BACKSPACE: if (m_mode == kSave && !m_desc.empty()) m_desc.pop_back(); break;
        case SDLK_UP: if (m_sel > 0) --m_sel; break;
        case SDLK_DOWN: if (m_sel < 9) ++m_sel; break;
        default: break;
        }
        break;
    default: break;
    }
    return kNone;
}

} // namespace op2
