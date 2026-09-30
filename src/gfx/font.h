#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

class Font {
public:
    static void drawText(uint32_t* framebuffer, int fbWidth, int fbHeight,
                         const std::string& text, int x, int y,
                         uint32_t color, bool shadow = false, uint32_t shadowColor = 0xFF000000);

    static void drawTextCentered(uint32_t* framebuffer, int fbWidth, int fbHeight,
                                 const std::string& text, int centerX, int y,
                                 uint32_t color, bool shadow = false, uint32_t shadowColor = 0xFF000000);

    static int getTextWidth(const std::string& text);
    static int getTextHeight();
};

} // namespace op2
