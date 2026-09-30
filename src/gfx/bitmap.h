#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace op2 {

struct RgbColor {
    uint8_t b = 0;
    uint8_t g = 0;
    uint8_t r = 0;
    uint8_t a = 255;
};

class Bitmap {
public:
    Bitmap();
    ~Bitmap();

    bool loadFromDIB(const uint8_t* dibData, size_t dibSize);
    bool loadFromFile(const std::string& filePath);

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getBpp() const { return m_bpp; }

    const std::vector<uint32_t>& getPixels32() const { return m_pixels32; }
    const std::vector<uint8_t>& getIndexedPixels() const { return m_pixels8; }
    const std::vector<RgbColor>& getPalette() const { return m_palette; }

    uint32_t getPixel32(int x, int y) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_bpp = 0;
    std::vector<RgbColor> m_palette;
    std::vector<uint8_t> m_pixels8;
    std::vector<uint32_t> m_pixels32; // ARGB format

    bool decompressRLE8(const uint8_t* stream, size_t streamSize);
};

} // namespace op2
