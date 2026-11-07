#include "renderer.h"
#include "font.h"
#include "ui_font.h"
#include "dialog_skin.h"
#include <iostream>
#include <algorithm>

namespace op2 {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(const std::string& title, int windowWidth, int windowHeight, bool fullscreen) {
    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        flags
    );

    if (!m_window) {
        std::cerr << "[Renderer] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_sdlRenderer = SDL_CreateRenderer(
        m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_sdlRenderer) {
        // Fallback to software renderer
        m_sdlRenderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!m_sdlRenderer) {
        std::cerr << "[Renderer] SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_nativeTexture = SDL_CreateTexture(
        m_sdlRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_w, m_h
    );

    if (!m_nativeTexture) {
        std::cerr << "[Renderer] SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        return false;
    }

    m_framebuffer.assign(m_w * m_h, 0xFF000000);
    updateViewport();

    return true;
}

void Renderer::shutdown() {
    if (m_nativeTexture) {
        SDL_DestroyTexture(m_nativeTexture);
        m_nativeTexture = nullptr;
    }
    if (m_sdlRenderer) {
        SDL_DestroyRenderer(m_sdlRenderer);
        m_sdlRenderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Renderer::updateViewport() {
    if (!m_window) return;
    int winW, winH;
    SDL_GetWindowSize(m_window, &winW, &winH);

    // Compute 4:3 aspect ratio letterboxed/pillarboxed rect
    float scaleX = static_cast<float>(winW) / m_w;
    float scaleY = static_cast<float>(winH) / m_h;
    float scale = std::min(scaleX, scaleY);

    int viewW = static_cast<int>(m_w * scale);
    int viewH = static_cast<int>(m_h * scale);
    int viewX = (winW - viewW) / 2;
    int viewY = (winH - viewH) / 2;

    m_viewportRect = {viewX, viewY, viewW, viewH};
}

void Renderer::screenToNative(int screenX, int screenY, int& nativeX, int& nativeY) const {
    if (m_viewportRect.w <= 0 || m_viewportRect.h <= 0) {
        nativeX = 0; nativeY = 0;
        return;
    }
    int relX = screenX - m_viewportRect.x;
    int relY = screenY - m_viewportRect.y;

    nativeX = (relX * m_w) / m_viewportRect.w - m_inputOffX;
    nativeY = (relY * m_h) / m_viewportRect.h - m_inputOffY;
}

// Tamanho logico do quadro. O shell usa 640x480 escalado (como o original); o
// jogo usa o tamanho da janela, 1:1, para a vista crescer com ela.
bool Renderer::setLogicalSize(int w, int h) {
    if (w < 1 || h < 1 || (w == m_w && h == m_h)) return false;
    if (m_sdlRenderer) {
        SDL_Texture* t = SDL_CreateTexture(m_sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!t) {
            std::cerr << "[Renderer] SDL_CreateTexture " << w << "x" << h
                      << " falhou: " << SDL_GetError() << std::endl;
            return false;
        }
        if (m_nativeTexture) SDL_DestroyTexture(m_nativeTexture);
        m_nativeTexture = t;
    }
    m_w = w;
    m_h = h;
    m_framebuffer.assign(static_cast<size_t>(w) * h, 0xFF000000);
    updateViewport();
    return true;
}

bool Renderer::fitToWindow(int minW, int minH) {
    if (!m_window) return false;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(m_window, &winW, &winH);
    return setLogicalSize(std::max(winW, minW), std::max(winH, minH));
}

void Renderer::clear(uint32_t color) {
    std::fill(m_framebuffer.begin(), m_framebuffer.end(), color);
}

void Renderer::present() {
    updateViewport();

    SDL_UpdateTexture(
        m_nativeTexture,
        nullptr,
        m_framebuffer.data(),
        m_w * sizeof(uint32_t)
    );

    SDL_SetRenderDrawColor(m_sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(m_sdlRenderer);

    SDL_RenderCopy(m_sdlRenderer, m_nativeTexture, nullptr, &m_viewportRect);
    SDL_RenderPresent(m_sdlRenderer);
}

void Renderer::blitBitmap(const Bitmap& bmp, int destX, int destY, bool useColorKey, uint32_t colorKey) {
    int w = bmp.getWidth();
    int h = bmp.getHeight();

    for (int y = 0; y < h; ++y) {
        int dy = destY + y;
        if (dy < 0 || dy >= m_h) continue;

        for (int x = 0; x < w; ++x) {
            int dx = destX + x;
            if (dx < 0 || dx >= m_w) continue;

            uint32_t pixel = bmp.getPixel32(x, y);
            if (useColorKey && (pixel & 0x00FFFFFF) == (colorKey & 0x00FFFFFF)) {
                continue;
            }
            m_framebuffer[dy * m_w + dx] = pixel;
        }
    }
}

void Renderer::blitBitmapAlpha(const Bitmap& bmp, int destX, int destY, uint8_t alpha,
                               bool useColorKey, uint32_t colorKey) {
    int w = bmp.getWidth();
    int h = bmp.getHeight();
    uint32_t a = alpha;
    uint32_t ia = 255u - a;

    for (int y = 0; y < h; ++y) {
        int dy = destY + y;
        if (dy < 0 || dy >= m_h) continue;

        for (int x = 0; x < w; ++x) {
            int dx = destX + x;
            if (dx < 0 || dx >= m_w) continue;

            uint32_t src = bmp.getPixel32(x, y);
            if (useColorKey && (src & 0x00FFFFFF) == (colorKey & 0x00FFFFFF)) {
                continue;
            }
            uint32_t dst = m_framebuffer[dy * m_w + dx];
            uint32_t r = (((src >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * ia) / 255;
            uint32_t g = (((src >> 8)  & 0xFF) * a + ((dst >> 8)  & 0xFF) * ia) / 255;
            uint32_t b = (((src)       & 0xFF) * a + ((dst)       & 0xFF) * ia) / 255;
            m_framebuffer[dy * m_w + dx] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
}

void Renderer::blitBitmapSub(const Bitmap& bmp, int srcX, int srcY, int srcW, int srcH, int destX, int destY) {
    for (int y = 0; y < srcH; ++y) {
        int sy = srcY + y;
        int dy = destY + y;
        if (dy < 0 || dy >= m_h) continue;

        for (int x = 0; x < srcW; ++x) {
            int sx = srcX + x;
            int dx = destX + x;
            if (dx < 0 || dx >= m_w) continue;

            m_framebuffer[dy * m_w + dx] = bmp.getPixel32(sx, sy);
        }
    }
}

void Renderer::fillRect(int x, int y, int w, int h, uint32_t color) {
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(m_w, x + w);
    int y2 = std::min(m_h, y + h);

    for (int cy = y1; cy < y2; ++cy) {
        for (int cx = x1; cx < x2; ++cx) {
            m_framebuffer[cy * m_w + cx] = color;
        }
    }
}

void Renderer::dimRect(int x, int y, int w, int h, uint8_t amount) {
    const int x1 = std::max(0, x), y1 = std::max(0, y);
    const int x2 = std::min(m_w, x + w), y2 = std::min(m_h, y + h);
    const uint32_t keep = 255u - amount;
    for (int cy = y1; cy < y2; ++cy) {
        for (int cx = x1; cx < x2; ++cx) {
            uint32_t& p = m_framebuffer[cy * m_w + cx];
            const uint32_t r = ((p >> 16) & 0xFF) * keep / 255;
            const uint32_t g = ((p >> 8) & 0xFF) * keep / 255;
            const uint32_t b = (p & 0xFF) * keep / 255;
            p = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

void Renderer::drawRect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int cx = x; cx < x + w; ++cx) {
        if (cx >= 0 && cx < m_w) {
            if (y >= 0 && y < m_h) m_framebuffer[y * m_w + cx] = color;
            if (y + h - 1 >= 0 && y + h - 1 < m_h) m_framebuffer[(y + h - 1) * m_w + cx] = color;
        }
    }
    for (int cy = y; cy < y + h; ++cy) {
        if (cy >= 0 && cy < m_h) {
            if (x >= 0 && x < m_w) m_framebuffer[cy * m_w + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < m_w) m_framebuffer[cy * m_w + (x + w - 1)] = color;
        }
    }
}

void Renderer::drawBevelRect(int x, int y, int w, int h, uint32_t topLeftColor, uint32_t bottomRightColor) {
    if (w <= 1 || h <= 1) return;
    // Top and Left lines
    for (int cx = x; cx < x + w - 1; ++cx) {
        if (cx >= 0 && cx < m_w && y >= 0 && y < m_h) {
            m_framebuffer[y * m_w + cx] = topLeftColor;
        }
    }
    for (int cy = y; cy < y + h - 1; ++cy) {
        if (x >= 0 && x < m_w && cy >= 0 && cy < m_h) {
            m_framebuffer[cy * m_w + x] = topLeftColor;
        }
    }

    // Bottom and Right lines
    for (int cx = x; cx < x + w; ++cx) {
        int by = y + h - 1;
        if (cx >= 0 && cx < m_w && by >= 0 && by < m_h) {
            m_framebuffer[by * m_w + cx] = bottomRightColor;
        }
    }
    for (int cy = y; cy < y + h; ++cy) {
        int rx = x + w - 1;
        if (rx >= 0 && rx < m_w && cy >= 0 && cy < m_h) {
            m_framebuffer[cy * m_w + rx] = bottomRightColor;
        }
    }
}

bool Renderer::fullscreen() const {
    return m_window && (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

void Renderer::setFullscreen(bool on) {
    if (!m_window) return;
    SDL_SetWindowFullscreen(m_window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    // Sem o cursor preso ao conteudo, sair da tela cheia pode deixar o rato
    // num canto; recentrar a janela evita a janela nascer fora da area util.
    if (!on) SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Renderer::fillRectAlpha(int x, int y, int w, int h, uint32_t color, uint8_t opacity) {
    if (opacity == 0) return;
    if (opacity == 255) { fillRect(x, y, w, h, color); return; }
    const int x1 = std::max(0, x), y1 = std::max(0, y);
    const int x2 = std::min(m_w, x + w), y2 = std::min(m_h, y + h);
    const uint32_t a = opacity, inv = 255u - opacity;
    const uint32_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    for (int cy = y1; cy < y2; ++cy)
        for (int cx = x1; cx < x2; ++cx) {
            uint32_t& p = m_framebuffer[cy * m_w + cx];
            const uint32_t r = (sr * a + ((p >> 16) & 0xFF) * inv) / 255;
            const uint32_t g = (sg * a + ((p >> 8) & 0xFF) * inv) / 255;
            const uint32_t b = (sb * a + (p & 0xFF) * inv) / 255;
            p = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
}

namespace {
// "&File" -> "File", accelerator index 0 (or -1). "&&" is a literal '&'.
std::string stripAccel(const std::string& s, int& accel) {
    std::string out;
    accel = -1;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&' && i + 1 < s.size()) {
            ++i;
            if (s[i] != '&' && accel < 0) accel = static_cast<int>(out.size());
        }
        out += s[i];
    }
    return out;
}

// One coordinate of a 9-slice: keep `m` px at each end, stretch the middle.
int slice9(int d, int dLen, int sLen, int m) {
    if (d < m) return d;
    if (d >= dLen - m) return sLen - (dLen - d);
    if (dLen - 2 * m <= 0) return m;
    return m + (d - m) * (sLen - 2 * m) / (dLen - 2 * m);
}

// Colours of the original shell dialog skin (sampled from the real main menu).
constexpr uint32_t kSkinTitleBg  = 0xFF000033;   // title bar navy
constexpr uint32_t kSkinTitleTx  = 0xFF00FF00;   // title text
constexpr uint32_t kSkinRed      = 0xFFFC0000;   // bright border / separator
constexpr uint32_t kSkinRedDark  = 0xFF720000;   // outer rows of the top/bottom border
constexpr uint32_t kSkinRedSide  = 0xFFA01F10;   // left/right border
constexpr uint32_t kSkinBtnText  = 0xFFDDFF22;   // button text (yellow-green)
constexpr uint32_t kSkinBtnOff   = 0xFF808080;   // disabled button text
} // namespace

void Renderer::drawDialogFrame(int x, int y, int w, int h, const std::string& title,
                               uint8_t opacity) {
    if (DialogSkin::ok()) {
        // Fork: the original's skinned dialog (measured on the 192x348 main
        // menu). The body is the blueprint (OP2SHRES 225), aligned to the
        // screen. Border: 3 rows top and bottom (dark/bright/dark red), 1 column
        // each side; title bar navy from row 3 to 25 with a bright red line at
        // row 26 and the title in green Arial bold. The original dialogs are
        // opaque, so `opacity` is not used here.
        (void)opacity;
        const Bitmap& bp = DialogSkin::blueprint();
        const int bw = bp.getWidth(), bh = bp.getHeight();
        const int x1 = std::max(0, x), y1 = std::max(0, y);
        const int x2 = std::min(m_w, x + w), y2 = std::min(m_h, y + h);
        for (int cy = y1; cy < y2; ++cy)
            for (int cx = x1; cx < x2; ++cx)
                m_framebuffer[cy * m_w + cx] = bp.getPixel32(cx % bw, cy % bh);
        if (!title.empty()) {
            fillRect(x + 1, y + 3, w - 2, 23, kSkinTitleBg);
            fillRect(x + 1, y + 26, w - 2, 1, kSkinRed);
            // "MAIN MENU" has its cap height on rows 10..19: baseline after row 19.
            const int face = UiFont::kDialogTitle;
            UiFont::drawCentered(m_framebuffer.data(), m_w, m_h, title, x + w / 2,
                                 y + 20 - UiFont::ascent(face), kSkinTitleTx, face);
        }
        fillRect(x, y, w, 1, kSkinRedDark);
        fillRect(x, y + 1, w, 1, kSkinRed);
        fillRect(x, y + 2, w, 1, kSkinRedDark);
        fillRect(x, y + h - 3, w, 1, kSkinRedDark);
        fillRect(x, y + h - 2, w, 1, kSkinRed);
        fillRect(x, y + h - 1, w, 1, 0xFF780000);
        fillRect(x, y + 3, 1, h - 6, kSkinRedSide);
        fillRect(x + w - 1, y + 3, 1, h - 6, kSkinRedSide);
        return;
    }

    // Outpost 2 UI style frame
    // Dark metallic background
    fillRectAlpha(x, y, w, h, 0xFF141923, opacity);

    // Double bevel border
    drawRect(x, y, w, h, 0xFF000000);
    drawBevelRect(x + 1, y + 1, w - 2, h - 2, 0xFF6C7B95, 0xFF0A0D14);
    drawBevelRect(x + 2, y + 2, w - 4, h - 4, 0xFF4A5568, 0xFF10141D);

    if (!title.empty()) {
        // Title banner bar
        int bannerH = 18;
        fillRectAlpha(x + 3, y + 3, w - 6, bannerH, 0xFF2B3A4F, opacity);
        drawBevelRect(x + 3, y + 3, w - 6, bannerH, 0xFF6C7B95, 0xFF10141D);

        // Title text centered
        Font::drawTextCentered(m_framebuffer.data(), m_w, m_h,
                               title, x + (w / 2), y + 6, 0xFFD8E4F8, true, 0xFF000000);
    }
}

void Renderer::drawButton(int x, int y, int w, int h, const std::string& rawText, bool hovered,
                          bool pressed, bool disabled, uint8_t opacity) {
    int accel = -1;
    const std::string text = stripAccel(rawText, accel);
    if (DialogSkin::ok()) {
        // Fork: the original's skinned push button. The art is a state cell of
        // OP2SHRES 236 (146x26, the main menu buttons) or 235 (75x26), drawn
        // 9-sliced into the control's rect. Text: Arial bold −11, yellow-green,
        // with the '&' letter underlined 2 px under the baseline.
        const Bitmap& strip = (w >= 110) ? DialogSkin::wideButtons() : DialogSkin::buttons();
        const int cw = strip.getWidth() / 5, ch = strip.getHeight();
        const int cell = disabled ? 1 : pressed ? 3 : hovered ? 2 : 0;
        for (int dy = 0; dy < h; ++dy) {
            const int ty = y + dy;
            if (ty < 0 || ty >= m_h) continue;
            const int sy = slice9(dy, h, ch, 7);
            for (int dx = 0; dx < w; ++dx) {
                const int tx = x + dx;
                if (tx < 0 || tx >= m_w) continue;
                m_framebuffer[ty * m_w + tx] = strip.getPixel32(cell * cw + slice9(dx, w, cw, 7), sy);
            }
        }
        const int face = UiFont::kTitle;
        const uint32_t col = disabled ? kSkinBtnOff : kSkinBtnText;
        const int tw = UiFont::width(text, face);
        int tx = x + (w - tw) / 2, ty = UiFont::centeredY(y, y + h - 1, face);
        if (pressed) { ++tx; ++ty; }
        UiFont::draw(m_framebuffer.data(), m_w, m_h, text, tx, ty, col, face);
        if (accel >= 0) {
            const int ux = tx + UiFont::width(text.substr(0, accel), face);
            const int uw = UiFont::width(text.substr(accel, 1), face);
            fillRect(ux, ty + UiFont::ascent(face) + 2, uw, 1, col);
        }
        return;
    }

    uint32_t bgColor;
    uint32_t borderTL, borderBR;
    uint32_t textColor;

    if (disabled) {
        bgColor = 0xFF1A1F29;
        borderTL = 0xFF2D3545;
        borderBR = 0xFF0D1016;
        textColor = 0xFF505A6E;
    } else if (pressed) {
        bgColor = 0xFF182230;
        borderTL = 0xFF0B0E14;
        borderBR = 0xFF657590;
        textColor = 0xFFE0ECFF;
    } else if (hovered) {
        bgColor = 0xFF35445B;
        borderTL = 0xFF8AA0C2;
        borderBR = 0xFF141923;
        textColor = 0xFFFFFF99; // Classic highlight yellow
    } else {
        bgColor = 0xFF253041;
        borderTL = 0xFF5A6B85;
        borderBR = 0xFF10151E;
        textColor = 0xFFCCD6E8;
    }

    // Fill button background
    fillRectAlpha(x, y, w, h, bgColor, opacity);

    // Bevels
    drawRect(x, y, w, h, 0xFF000000);
    drawBevelRect(x + 1, y + 1, w - 2, h - 2, borderTL, borderBR);

    // Button text centered
    int textY = y + (h - Font::getTextHeight()) / 2;
    int textX = x + (w / 2);
    if (pressed) {
        textX += 1;
        textY += 1;
    }

    Font::drawTextCentered(m_framebuffer.data(), m_w, m_h,
                           text, textX, textY, textColor, !disabled, 0xFF000000);
}

// ---------------------------------------------------------------------------
// Fork: skinned shell controls (the original menus on Windows 10).

void Renderer::drawSkinBox(int x, int y, int w, int h) {
    if (!DialogSkin::ok()) {
        fillRect(x, y, w, h, 0xFF0D121B);
        drawBevelRect(x, y, w, h, 0xFF05080E, 0xFF35445B);
        return;
    }
    fillRect(x, y, w, h, DialogSkin::kBoxBg);
    drawRect(x, y, w, h, DialogSkin::kBoxEdgeDk);
    drawRect(x + 1, y + 1, w - 2, h - 2, DialogSkin::kBoxEdge);
}

void Renderer::drawSkinArrowButton(int x, int y, int w, int h, bool up) {
    if (!DialogSkin::ok()) {
        drawButton(x, y, w, h, up ? "^" : "v", false, false);
        return;
    }
    fillRect(x, y, w, h, 0xFFF0F0F0);
    // chevron: 7 px wide, 4 rows, centred
    const int cx = x + w / 2, cy = y + h / 2;
    for (int r = 0; r < 4; ++r) {
        const int half = up ? r : 3 - r;
        fillRect(cx - half, cy - 2 + r, 2 * half + 1, 1, 0xFF606060);
    }
}

void Renderer::drawSkinScrollBar(int x, int y, int w, int h, int first, int visible, int total) {
    const int ab = std::min(w, h / 2);                 // arrow buttons are square
    if (!DialogSkin::ok()) {
        fillRect(x, y, w, h, 0xFF11161F);
        drawBevelRect(x, y, w, h, 0xFF05080E, 0xFF35445B);
    } else {
        fillRect(x, y, w, h, 0xFFF0F0F0);
    }
    drawSkinArrowButton(x, y, w, ab, true);
    drawSkinArrowButton(x, y + h - ab, w, ab, false);
    const int trackY = y + ab, trackH = h - 2 * ab;
    if (trackH <= 0 || total <= visible || visible <= 0) return;   // nothing to scroll
    const int maxFirst = total - visible;
    int thumbH = std::max(12, trackH * visible / total);
    thumbH = std::min(thumbH, trackH);
    const int f = std::max(0, std::min(first, maxFirst));
    const int thumbY = trackY + (trackH - thumbH) * f / maxFirst;
    if (DialogSkin::ok()) {
        fillRect(x + 1, thumbY, w - 2, thumbH, 0xFFCDCDCD);
    } else {
        fillRect(x + 2, thumbY, w - 4, thumbH, 0xFF3A557E);
        drawBevelRect(x + 2, thumbY, w - 4, thumbH, 0xFF5A7AA8, 0xFF223144);
    }
}

void Renderer::drawSkinRadio(int x, int y, bool on, bool disabled) {
    // 13x13 circle drawn from rows (classic radio glyph), black inside.
    static const char* kRing[13] = {
        "....#####....", "..##.....##..", ".#.........#.", ".#.........#.", "#...........#",
        "#...........#", "#...........#", "#...........#", "#...........#", ".#.........#.",
        ".#.........#.", "..##.....##..", "....#####...."};
    const uint32_t edge = DialogSkin::ok() ? DialogSkin::kBoxEdge : 0xFF8090A8;
    const uint32_t dot = disabled ? DialogSkin::kTextOff : DialogSkin::kText;
    for (int r = 0; r < 13; ++r) {
        int a = -1, b = -1;
        for (int c = 0; c < 13; ++c)
            if (kRing[r][c] == '#') { if (a < 0) a = c; b = c; }
        if (a >= 0 && b > a) fillRect(x + a + 1, y + r, b - a - 1, 1, 0xFF000000);
        for (int c = 0; c < 13; ++c)
            if (kRing[r][c] == '#') fillRect(x + c, y + r, 1, 1, edge);
    }
    if (on) {
        fillRect(x + 5, y + 4, 3, 5, dot);
        fillRect(x + 4, y + 5, 5, 3, dot);
    }
}

void Renderer::drawSkinCheck(int x, int y, bool on, bool disabled) {
    const uint32_t edge = DialogSkin::ok() ? DialogSkin::kBoxEdge : 0xFF8090A8;
    fillRect(x, y, 13, 13, 0xFF000000);
    drawRect(x, y, 13, 13, edge);
    if (on) {
        const uint32_t mark = disabled ? DialogSkin::kTextOff : DialogSkin::kText;
        // the classic 7-step check mark, 2 px thick
        static const int kY[7] = {4, 5, 6, 5, 4, 3, 2};
        for (int i = 0; i < 7; ++i) fillRect(x + 3 + i, y + 2 + kY[i], 1, 3, mark);
    }
}


} // namespace op2
