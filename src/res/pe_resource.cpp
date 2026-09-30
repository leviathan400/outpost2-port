#include "pe_resource.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace op2 {

#pragma pack(push, 1)
struct ImageDosHeader {
    uint16_t e_magic;    // 0x5A4D "MZ"
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t  e_lfanew;   // Offset to PE signature
};

struct ImageFileHeader {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct ImageDataDirectory {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct ImageOptionalHeader32 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    ImageDataDirectory DataDirectory[16];
};

struct ImageSectionHeader {
    char     Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

struct ImageResourceDirectory {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint16_t NumberOfNamedEntries;
    uint16_t NumberOfIdEntries;
};

struct ImageResourceDirectoryEntry {
    uint32_t Name;
    uint32_t OffsetToData;
};

struct ImageResourceDataEntry {
    uint32_t OffsetToData;
    uint32_t Size;
    uint32_t CodePage;
    uint32_t Reserved;
};
#pragma pack(pop)

PeResourceReader::PeResourceReader() = default;
PeResourceReader::~PeResourceReader() = default;

uint32_t PeResourceReader::rvaToOffset(uint32_t rva) const {
    for (const auto& sec : m_sections) {
        if (rva >= sec.virtualAddress && rva < sec.virtualAddress + sec.virtualSize) {
            return sec.rawOffset + (rva - sec.virtualAddress);
        }
    }
    return 0;
}

bool PeResourceReader::load(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "[PE] Failed to open: " << filePath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    m_fileData.resize(size);
    file.read(reinterpret_cast<char*>(m_fileData.data()), size);

    if (size < sizeof(ImageDosHeader)) {
        return false;
    }

    const auto* dos = reinterpret_cast<const ImageDosHeader*>(m_fileData.data());
    if (dos->e_magic != 0x5A4D) { // 'MZ'
        return false;
    }

    if (static_cast<size_t>(dos->e_lfanew + 4 + sizeof(ImageFileHeader)) > size) {
        return false;
    }

    const uint8_t* ntHeaders = m_fileData.data() + dos->e_lfanew;
    if (std::memcmp(ntHeaders, "PE\0\0", 4) != 0) {
        return false;
    }

    const auto* fileHdr = reinterpret_cast<const ImageFileHeader*>(ntHeaders + 4);
    const auto* optHdr = reinterpret_cast<const ImageOptionalHeader32*>(ntHeaders + 4 + sizeof(ImageFileHeader));
    m_imageBase = optHdr->ImageBase;

    const auto* secHeaders = reinterpret_cast<const ImageSectionHeader*>(
        ntHeaders + 4 + sizeof(ImageFileHeader) + fileHdr->SizeOfOptionalHeader
    );

    m_sections.clear();
    for (int i = 0; i < fileHdr->NumberOfSections; ++i) {
        SectionHeader sec;
        std::memcpy(sec.name, secHeaders[i].Name, 8);
        sec.virtualSize = secHeaders[i].VirtualSize;
        sec.virtualAddress = secHeaders[i].VirtualAddress;
        sec.rawSize = secHeaders[i].SizeOfRawData;
        sec.rawOffset = secHeaders[i].PointerToRawData;
        m_sections.push_back(sec);
    }

    return true;
}

std::vector<uint8_t> PeResourceReader::getResource(uint32_t typeId, uint32_t resId) const {
    if (m_fileData.empty()) return {};

    const auto* dos = reinterpret_cast<const ImageDosHeader*>(m_fileData.data());
    const uint8_t* ntHeaders = m_fileData.data() + dos->e_lfanew;
    const auto* optHdr = reinterpret_cast<const ImageOptionalHeader32*>(ntHeaders + 4 + sizeof(ImageFileHeader));

    if (optHdr->NumberOfRvaAndSizes <= 2) return {};
    uint32_t resRva = optHdr->DataDirectory[2].VirtualAddress; // IMAGE_DIRECTORY_ENTRY_RESOURCE
    if (resRva == 0) return {};

    uint32_t resOffset = rvaToOffset(resRva);
    if (resOffset == 0 || resOffset >= m_fileData.size()) return {};

    const uint8_t* resRoot = m_fileData.data() + resOffset;
    const auto* rootDir = reinterpret_cast<const ImageResourceDirectory*>(resRoot);

    int totalEntries = rootDir->NumberOfNamedEntries + rootDir->NumberOfIdEntries;
    const auto* entries = reinterpret_cast<const ImageResourceDirectoryEntry*>(resRoot + sizeof(ImageResourceDirectory));

    for (int i = 0; i < totalEntries; ++i) {
        if (!(entries[i].Name & 0x80000000) && entries[i].Name == typeId) {
            if (entries[i].OffsetToData & 0x80000000) {
                uint32_t idDirOffset = entries[i].OffsetToData & 0x7FFFFFFF;
                const auto* idDir = reinterpret_cast<const ImageResourceDirectory*>(resRoot + idDirOffset);
                int idEntriesCount = idDir->NumberOfNamedEntries + idDir->NumberOfIdEntries;
                const auto* idEntries = reinterpret_cast<const ImageResourceDirectoryEntry*>(resRoot + idDirOffset + sizeof(ImageResourceDirectory));

                for (int j = 0; j < idEntriesCount; ++j) {
                    if (!(idEntries[j].Name & 0x80000000) && idEntries[j].Name == resId) {
                        if (idEntries[j].OffsetToData & 0x80000000) {
                            uint32_t langDirOffset = idEntries[j].OffsetToData & 0x7FFFFFFF;
                            const auto* langDir = reinterpret_cast<const ImageResourceDirectory*>(resRoot + langDirOffset);
                            const auto* langEntries = reinterpret_cast<const ImageResourceDirectoryEntry*>(resRoot + langDirOffset + sizeof(ImageResourceDirectory));
                            if (langDir->NumberOfNamedEntries + langDir->NumberOfIdEntries > 0) {
                                uint32_t dataEntryOffset = langEntries[0].OffsetToData;
                                const auto* dataEntry = reinterpret_cast<const ImageResourceDataEntry*>(resRoot + dataEntryOffset);
                                uint32_t fileOffset = rvaToOffset(dataEntry->OffsetToData);
                                if (fileOffset != 0 && fileOffset + dataEntry->Size <= m_fileData.size()) {
                                    return std::vector<uint8_t>(
                                        m_fileData.data() + fileOffset,
                                        m_fileData.data() + fileOffset + dataEntry->Size
                                    );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return {};
}

std::vector<uint8_t> PeResourceReader::getBitmapDIB(uint32_t bitmapId) const {
    return getResource(2, bitmapId); // Type 2 is RT_BITMAP
}

std::string PeResourceReader::getExportString(const std::string& exportName) const {
    if (m_fileData.empty()) return {};

    const auto* dos = reinterpret_cast<const ImageDosHeader*>(m_fileData.data());
    const uint8_t* ntHeaders = m_fileData.data() + dos->e_lfanew;
    const auto* optHdr = reinterpret_cast<const ImageOptionalHeader32*>(ntHeaders + 4 + sizeof(ImageFileHeader));

    if (optHdr->NumberOfRvaAndSizes < 1) return {};
    uint32_t expRva = optHdr->DataDirectory[0].VirtualAddress; // IMAGE_DIRECTORY_ENTRY_EXPORT
    if (expRva == 0) return {};

    uint32_t expOff = rvaToOffset(expRva);
    if (expOff == 0 || expOff + 40 > m_fileData.size()) return {};

    const uint8_t* ed = m_fileData.data() + expOff;
    uint32_t numNames        = *reinterpret_cast<const uint32_t*>(ed + 0x18);
    uint32_t addrFunctions   = rvaToOffset(*reinterpret_cast<const uint32_t*>(ed + 0x1C));
    uint32_t addrNames       = rvaToOffset(*reinterpret_cast<const uint32_t*>(ed + 0x20));
    uint32_t addrOrdinals    = rvaToOffset(*reinterpret_cast<const uint32_t*>(ed + 0x24));
    if (addrNames == 0 || addrOrdinals == 0 || addrFunctions == 0) return {};

    const auto* nameRvas = reinterpret_cast<const uint32_t*>(m_fileData.data() + addrNames);
    const auto* ordinals = reinterpret_cast<const uint16_t*>(m_fileData.data() + addrOrdinals);
    const auto* funcRvas = reinterpret_cast<const uint32_t*>(m_fileData.data() + addrFunctions);

    for (uint32_t i = 0; i < numNames; ++i) {
        uint32_t nameOff = rvaToOffset(nameRvas[i]);
        if (nameOff == 0 || nameOff >= m_fileData.size()) continue;
        const char* nm = reinterpret_cast<const char*>(m_fileData.data() + nameOff);
        if (exportName == nm) {
            uint16_t ord = ordinals[i];
            uint32_t dataRva = funcRvas[ord];             // export de dados: RVA da string
            uint32_t dataOff = rvaToOffset(dataRva);
            if (dataOff == 0 || dataOff >= m_fileData.size()) return {};
            const char* str = reinterpret_cast<const char*>(m_fileData.data() + dataOff);
            return std::string(str);
        }
    }
    return {};
}

bool PeResourceReader::getDialog(uint32_t dialogId, PeDialogTemplate& outDialog) const {
    std::vector<uint8_t> data = getResource(5, dialogId); // Type 5 is RT_DIALOG
    if (data.size() < 18) return false;

    outDialog.controls.clear();

    const uint8_t* ptr = data.data();
    uint16_t ver = *reinterpret_cast<const uint16_t*>(ptr);
    uint16_t sig = *reinterpret_cast<const uint16_t*>(ptr + 2);

    if (ver == 1 && sig == 0xFFFF) {
        // DLGTEMPLATEEX (not used for standard dialogs in OP2SHRES, but handled)
        return false;
    }

    // DLGTEMPLATE
    uint32_t style = *reinterpret_cast<const uint32_t*>(ptr);
    uint32_t extStyle = *reinterpret_cast<const uint32_t*>(ptr + 4);
    uint16_t cdit = *reinterpret_cast<const uint16_t*>(ptr + 8);
    int16_t x = *reinterpret_cast<const int16_t*>(ptr + 10);
    int16_t y = *reinterpret_cast<const int16_t*>(ptr + 12);
    int16_t cx = *reinterpret_cast<const int16_t*>(ptr + 14);
    int16_t cy = *reinterpret_cast<const int16_t*>(ptr + 16);

    outDialog.style = style;
    outDialog.extStyle = extStyle;
    outDialog.x = x;
    outDialog.y = y;
    outDialog.cx = cx;
    outDialog.cy = cy;

    size_t pos = 18;

    // Menu
    uint16_t menuVal = *reinterpret_cast<const uint16_t*>(ptr + pos);
    pos += 2;
    if (menuVal == 0xFFFF) {
        pos += 2;
    } else if (menuVal != 0) {
        while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
        pos += 2;
    }

    // Window Class
    uint16_t classVal = *reinterpret_cast<const uint16_t*>(ptr + pos);
    pos += 2;
    if (classVal == 0xFFFF) {
        pos += 2;
    } else if (classVal != 0) {
        while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
        pos += 2;
    }

    // Title
    size_t tStart = pos;
    while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
    for (size_t k = tStart; k < pos; k += 2) {
        char16_t ch = *reinterpret_cast<const char16_t*>(ptr + k);
        outDialog.title.push_back(static_cast<char>(ch & 0x7F));
    }
    pos += 2;

    // Font if DS_SETFONT (0x40)
    if (style & 0x40) {
        outDialog.fontSize = *reinterpret_cast<const uint16_t*>(ptr + pos);
        pos += 2;
        size_t fStart = pos;
        while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
        for (size_t k = fStart; k < pos; k += 2) {
            char16_t ch = *reinterpret_cast<const char16_t*>(ptr + k);
            outDialog.fontName.push_back(static_cast<char>(ch & 0x7F));
        }
        pos += 2;
    }

    // Controls
    for (int i = 0; i < cdit; ++i) {
        if (pos % 4 != 0) pos += 4 - (pos % 4);
        if (pos + 18 > data.size()) break;

        PeDialogControl ctrl;
        ctrl.style = *reinterpret_cast<const uint32_t*>(ptr + pos);
        ctrl.extStyle = *reinterpret_cast<const uint32_t*>(ptr + pos + 4);
        ctrl.x = *reinterpret_cast<const int16_t*>(ptr + pos + 8);
        ctrl.y = *reinterpret_cast<const int16_t*>(ptr + pos + 10);
        ctrl.cx = *reinterpret_cast<const int16_t*>(ptr + pos + 12);
        ctrl.cy = *reinterpret_cast<const int16_t*>(ptr + pos + 14);
        ctrl.id = *reinterpret_cast<const uint16_t*>(ptr + pos + 16);
        pos += 18;

        // Class
        uint16_t cVal = *reinterpret_cast<const uint16_t*>(ptr + pos);
        pos += 2;
        if (cVal == 0xFFFF) {
            uint16_t cAtom = *reinterpret_cast<const uint16_t*>(ptr + pos);
            pos += 2;
            switch (cAtom) {
                case 0x0080: ctrl.className = "Button"; break;
                case 0x0081: ctrl.className = "Edit"; break;
                case 0x0082: ctrl.className = "Static"; break;
                case 0x0083: ctrl.className = "ListBox"; break;
                case 0x0084: ctrl.className = "ScrollBar"; break;
                case 0x0085: ctrl.className = "ComboBox"; break;
                default: ctrl.className = "Custom"; break;
            }
        } else if (cVal != 0) {
            size_t cs = pos - 2;
            while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
            for (size_t k = cs; k < pos; k += 2) {
                char16_t ch = *reinterpret_cast<const char16_t*>(ptr + k);
                ctrl.className.push_back(static_cast<char>(ch & 0x7F));
            }
            pos += 2;
        }

        // Text
        uint16_t tVal = *reinterpret_cast<const uint16_t*>(ptr + pos);
        pos += 2;
        if (tVal == 0xFFFF) {
            pos += 2;
        } else if (tVal != 0) {
            size_t ts = pos - 2;
            while (pos + 1 < data.size() && (ptr[pos] != 0 || ptr[pos+1] != 0)) pos += 2;
            for (size_t k = ts; k < pos; k += 2) {
                char16_t ch = *reinterpret_cast<const char16_t*>(ptr + k);
                ctrl.text.push_back(static_cast<char>(ch & 0x7F));
            }
            pos += 2;
        }

        // Extra data
        if (pos + 2 <= data.size()) {
            uint16_t extra = *reinterpret_cast<const uint16_t*>(ptr + pos);
            pos += 2 + extra;
        }

        outDialog.controls.push_back(ctrl);
    }

    return true;
}

} // namespace op2
