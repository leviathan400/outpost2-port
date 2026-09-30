#include "vol.h"
#include "lzhuf.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

namespace op2 {

namespace {
constexpr uint32_t LEN_MASK = 0x7FFFFFFF;
constexpr uint32_t FREE = 0xFFFFFFFF;
constexpr int ENTRY_SIZE = 14;

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace

uint32_t Vol::u32(size_t off) const {
    uint32_t v;
    std::memcpy(&v, m_data.data() + off, 4);  // x86: little-endian nativo
    return v;
}

std::string Vol::nameAt(uint32_t off) const {
    size_t start = m_strBase + off;
    size_t end = start;
    while (end < m_data.size() && m_data[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(m_data.data() + start), end - start);
}

bool Vol::load(const std::string& path) {
    m_path = path;
    m_ok = false;
    m_entries.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len < 16) return false;
    f.seekg(0, std::ios::beg);
    m_data.resize(static_cast<size_t>(len));
    f.read(reinterpret_cast<char*>(m_data.data()), len);
    if (!f) return false;

    if (std::memcmp(m_data.data(), "VOL ", 4) != 0) return false;

    // Localiza os sub-chunks vols e voli dentro do diretório (primeiros 4096 bytes).
    auto findTag = [&](const char* tag) -> long {
        size_t limit = std::min<size_t>(m_data.size() - 4, 4096);
        for (size_t i = 0; i <= limit; ++i) {
            if (std::memcmp(m_data.data() + i, tag, 4) == 0) return static_cast<long>(i);
        }
        return -1;
    };
    long vols = findTag("vols");
    long voli = findTag("voli");
    if (vols < 0 || voli < 0) return false;

    // --- string table ---
    // vols: "vols" <u32 len> <u32 strTableLen> nomes...
    m_strBase = static_cast<size_t>(vols) + 12;

    // --- index ---
    uint32_t idxLen = u32(static_cast<size_t>(voli) + 4) & LEN_MASK;
    size_t idxBase = static_cast<size_t>(voli) + 8;
    uint32_t count = idxLen / ENTRY_SIZE;

    for (uint32_t i = 0; i < count; ++i) {
        size_t e = idxBase + static_cast<size_t>(i) * ENTRY_SIZE;
        if (e + ENTRY_SIZE > m_data.size()) break;
        uint32_t nameOff = u32(e);
        if (nameOff == FREE) continue;  // slot livre
        Entry ent;
        ent.nameOff = nameOff;
        ent.dataOff = u32(e + 4);
        ent.size = u32(e + 8);
        ent.comp = m_data[e + 12];
        ent.flag = m_data[e + 13];
        ent.name = nameAt(nameOff);
        m_entries.push_back(std::move(ent));
    }

    m_ok = true;
    return true;
}

const Vol::Entry* Vol::find(const std::string& name) const {
    std::string target = toLower(name);
    for (const auto& e : m_entries) {
        if (toLower(e.name) == target) return &e;
    }
    return nullptr;
}

std::vector<uint8_t> Vol::read(const Entry& e) const {
    // Payload do VBLK: pula tag "VBLK" (4) + u32 len (4).
    size_t start = e.dataOff + 8;
    if (start + e.size > m_data.size()) return {};
    const uint8_t* raw = m_data.data() + start;

    if (e.comp == 0) {
        return std::vector<uint8_t>(raw, raw + e.size);
    }
    if (e.comp == 3) {  // LZHUF
        return lzhufDecompress(raw, e.size);
    }
    // Tipos 1 (RLE) e 2 (LZ) ainda não implementados (não usados pelos VOLs atuais).
    return {};
}

} // namespace op2
