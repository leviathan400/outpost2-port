#include "bitmap.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace op2 {

#pragma pack(push, 1)
struct BitmapInfoHeader {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

Bitmap::Bitmap() = default;
Bitmap::~Bitmap() = default;

bool Bitmap::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (size < 14 + sizeof(BitmapInfoHeader)) return false;

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (data[0] != 'B' || data[1] != 'M') return false;

    // Skip BITMAPFILEHEADER (14 bytes)
    return loadFromDIB(data.data() + 14, size - 14);
}

bool Bitmap::loadFromDIB(const uint8_t* dibData, size_t dibSize) {
    if (dibSize < sizeof(BitmapInfoHeader)) return false;

    const auto* hdr = reinterpret_cast<const BitmapInfoHeader*>(dibData);
    if (hdr->biSize < 40) return false;

    m_width = hdr->biWidth;
    bool isBottomUp = (hdr->biHeight > 0);
    m_height = std::abs(hdr->biHeight);
    m_bpp = hdr->biBitCount;

    if (m_width <= 0 || m_height <= 0) return false;

    uint32_t colors = hdr->biClrUsed;
    if (colors == 0 && m_bpp <= 8) {
        colors = (1 << m_bpp);
    }

    const uint8_t* palettePtr = dibData + hdr->biSize;
    m_palette.resize(colors);
    for (uint32_t i = 0; i < colors; ++i) {
        m_palette[i].b = palettePtr[i * 4 + 0];
        m_palette[i].g = palettePtr[i * 4 + 1];
        m_palette[i].r = palettePtr[i * 4 + 2];
        m_palette[i].a = 255;
    }

    const uint8_t* pixelData = dibData + hdr->biSize + (colors * 4);
    size_t pixelDataSize = dibSize - (hdr->biSize + (colors * 4));

    m_pixels8.assign(m_width * m_height, 0);
    m_pixels32.assign(m_width * m_height, 0);

    if (hdr->biCompression == 1) { // BI_RLE8
        if (!decompressRLE8(pixelData, pixelDataSize)) {
            std::cerr << "[Bitmap] RLE8 decompression failed!" << std::endl;
            return false;
        }
    } else if (hdr->biCompression == 0) { // BI_RGB
        if (m_bpp == 8) {
            int rowStride = (m_width + 3) & ~3;
            for (int y = 0; y < m_height; ++y) {
                int srcY = isBottomUp ? (m_height - 1 - y) : y;
                const uint8_t* row = pixelData + (srcY * rowStride);
                for (int x = 0; x < m_width; ++x) {
                    m_pixels8[y * m_width + x] = row[x];
                }
            }
        } else if (m_bpp == 24) {
            int rowStride = (m_width * 3 + 3) & ~3;
            for (int y = 0; y < m_height; ++y) {
                int srcY = isBottomUp ? (m_height - 1 - y) : y;
                const uint8_t* row = pixelData + (srcY * rowStride);
                for (int x = 0; x < m_width; ++x) {
                    uint8_t b = row[x * 3 + 0];
                    uint8_t g = row[x * 3 + 1];
                    uint8_t r = row[x * 3 + 2];
                    m_pixels32[y * m_width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
            return true;
        }
    } else {
        std::cerr << "[Bitmap] Unsupported compression: " << hdr->biCompression << std::endl;
        return false;
    }

    // Convert 8bpp indexed to 32bpp ARGB using palette
    if (m_bpp == 8 && !m_palette.empty()) {
        for (int i = 0; i < m_width * m_height; ++i) {
            uint8_t idx = m_pixels8[i];
            if (idx < m_palette.size()) {
                const auto& col = m_palette[idx];
                m_pixels32[i] = (0xFF << 24) | (col.r << 16) | (col.g << 8) | col.b;
            }
        }
    }

    return true;
}

bool Bitmap::decompressRLE8(const uint8_t* stream, size_t streamSize) {
    size_t pos = 0;
    int x = 0;
    int y = m_height - 1; // Standard Windows DIB RLE8 is bottom-up!

    while (pos + 1 < streamSize) {
        uint8_t b0 = stream[pos++];
        uint8_t b1 = stream[pos++];

        if (b0 > 0) {
            // Encoded mode: repeat b1, b0 times
            for (int k = 0; k < b0; ++k) {
                if (x < m_width && y >= 0 && y < m_height) {
                    m_pixels8[y * m_width + x] = b1;
                }
                x++;
            }
        } else {
            // Escape code
            if (b1 == 0) {
                // End of line
                x = 0;
                y--;
            } else if (b1 == 1) {
                // End of bitmap
                break;
            } else if (b1 == 2) {
                // Delta (dx, dy)
                if (pos + 1 >= streamSize) break;
                uint8_t dx = stream[pos++];
                uint8_t dy = stream[pos++];
                x += dx;
                y -= dy; // bottom-up, dy decreases y
            } else {
                // Literal run of b1 bytes
                int count = b1;
                for (int k = 0; k < count; ++k) {
                    if (pos >= streamSize) break;
                    uint8_t val = stream[pos++];
                    if (x < m_width && y >= 0 && y < m_height) {
                        m_pixels8[y * m_width + x] = val;
                    }
                    x++;
                }
                // Pad to WORD boundary if count is odd
                if (count % 2 != 0) {
                    pos++;
                }
            }
        }
    }

    return true;
}

uint32_t Bitmap::getPixel32(int x, int y) const {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_pixels32[y * m_width + x];
    }
    return 0;
}

} // namespace op2
