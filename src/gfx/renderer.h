#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>
#include <vector>
#include "bitmap.h"

namespace op2 {

class Renderer {
public:
    static constexpr int NATIVE_WIDTH = 640;
    static constexpr int NATIVE_HEIGHT = 480;

    Renderer();
    ~Renderer();

    bool init(const std::string& title, int windowWidth = 640, int windowHeight = 480, bool fullscreen = false);
    void shutdown();

    void clear(uint32_t color = 0xFF000000);
    void present();

    // Drawing primitives directly on the 640x480 native buffer
    void blitBitmap(const Bitmap& bmp, int destX, int destY, bool useColorKey = false, uint32_t colorKey = 0xFF000000);
    // Blit com opacidade (alpha 0..255; 255 = opaco). Mistura com o fundo.
    void blitBitmapAlpha(const Bitmap& bmp, int destX, int destY, uint8_t alpha,
                         bool useColorKey = false, uint32_t colorKey = 0xFF000000);
    void blitBitmapSub(const Bitmap& bmp, int srcX, int srcY, int srcW, int srcH, int destX, int destY);

    void fillRect(int x, int y, int w, int h, uint32_t color);
    // Escurece o que já está desenhado (alpha 0..255; 255 = preto). O fillRect
    // grava a cor crua, então não serve para velar o fundo.
    void dimRect(int x, int y, int w, int h, uint8_t amount);
    void drawRect(int x, int y, int w, int h, uint32_t color);
    void drawBevelRect(int x, int y, int w, int h, uint32_t topLeftColor, uint32_t bottomRightColor);

    // Dialog / Window frame (classic Outpost 2 dark metallic beveled box)
    void drawDialogFrame(int x, int y, int w, int h, const std::string& title = "");
    void drawButton(int x, int y, int w, int h, const std::string& text, bool hovered, bool pressed, bool disabled = false);

    uint32_t* getFramebuffer() { return m_framebuffer.data(); }
    int getWidth() const { return NATIVE_WIDTH; }
    int getHeight() const { return NATIVE_HEIGHT; }

    SDL_Window* getWindow() const { return m_window; }

    // Map screen coordinate to native 640x480 coordinate
    void screenToNative(int screenX, int screenY, int& nativeX, int& nativeY) const;

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_sdlRenderer = nullptr;
    SDL_Texture* m_nativeTexture = nullptr;

    std::vector<uint32_t> m_framebuffer;
    SDL_Rect m_viewportRect = {0, 0, NATIVE_WIDTH, NATIVE_HEIGHT};

    void updateViewport();
};

} // namespace op2
