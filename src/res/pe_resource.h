#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace op2 {

struct PeDialogControl {
    uint32_t style = 0;
    uint32_t extStyle = 0;
    int16_t x = 0;
    int16_t y = 0;
    int16_t cx = 0;
    int16_t cy = 0;
    uint16_t id = 0;
    std::string className;
    std::string text;
};

struct PeDialogTemplate {
    uint32_t style = 0;
    uint32_t extStyle = 0;
    int16_t x = 0;
    int16_t y = 0;
    int16_t cx = 0;
    int16_t cy = 0;
    std::string title;
    std::string fontName;
    uint16_t fontSize = 0;
    std::vector<PeDialogControl> controls;
};

class PeResourceReader {
public:
    PeResourceReader();
    ~PeResourceReader();

    bool load(const std::string& filePath);

    // Retrieve raw resource data
    std::vector<uint8_t> getResource(uint32_t typeId, uint32_t resId) const;

    // Load Bitmap by ID (Type 2 = RT_BITMAP)
    std::vector<uint8_t> getBitmapDIB(uint32_t bitmapId) const;

    // Load Dialog by ID (Type 5 = RT_DIALOG)
    bool getDialog(uint32_t dialogId, PeDialogTemplate& outDialog) const;

    // Read a named data export as a C-string (ex.: "LevelDesc" nas missões T##.DLL).
    // As missões do Outpost 2 expõem LevelDesc/MapName/TechtreeName como exports
    // de dados apontando para strings; é assim que o shell monta as listas.
    std::string getExportString(const std::string& exportName) const;

private:
    std::vector<uint8_t> m_fileData;
    uint32_t m_imageBase = 0;

    struct SectionHeader {
        char name[8];
        uint32_t virtualSize;
        uint32_t virtualAddress;
        uint32_t rawSize;
        uint32_t rawOffset;
    };
    std::vector<SectionHeader> m_sections;

    uint32_t rvaToOffset(uint32_t rva) const;
};

} // namespace op2
