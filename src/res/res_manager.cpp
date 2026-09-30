#include "res_manager.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace op2 {

ResManager g_ResManager;

// ----- StreamIO -----

size_t StreamIO::readBytes(void* dst, size_t n) {
    size_t avail = remaining();
    size_t got = std::min(n, avail);
    std::memcpy(dst, m_data.data() + m_pos, got);
    m_pos += got;
    return got;
}

uint32_t StreamIO::readU32() {
    uint32_t v = 0;
    readBytes(&v, 4);
    return v;  // x86 little-endian nativo
}

uint16_t StreamIO::readU16() {
    uint16_t v = 0;
    readBytes(&v, 2);
    return v;
}

uint8_t StreamIO::readU8() {
    uint8_t v = 0;
    readBytes(&v, 1);
    return v;
}

// ----- ResManager -----

bool ResManager::init(const std::string& baseDir) {
    m_baseDir = baseDir;
    m_vols.clear();
    m_volNames.clear();

    std::error_code ec;
    if (!fs::exists(baseDir, ec)) {
        std::cerr << "[ResManager] baseDir inexistente: " << baseDir << std::endl;
        return false;
    }

    // Varre os *.VOL da pasta do jogo (ordem alfabética para determinismo).
    std::vector<std::string> volPaths;
    for (const auto& de : fs::directory_iterator(baseDir, ec)) {
        if (!de.is_regular_file()) continue;
        std::string ext = de.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (ext == ".VOL") volPaths.push_back(de.path().string());
    }
    std::sort(volPaths.begin(), volPaths.end());

    for (const auto& p : volPaths) {
        auto v = std::make_unique<Vol>();
        if (v->load(p)) {
            m_volNames.push_back(fs::path(p).filename().string());
            m_vols.push_back(std::move(v));
        } else {
            std::cerr << "[ResManager] falha ao ler VOL: " << p << std::endl;
        }
    }

    std::cout << "[ResManager] " << m_vols.size() << " volumes carregados";
    if (!m_volNames.empty()) {
        std::cout << " (";
        for (size_t i = 0; i < m_volNames.size(); ++i)
            std::cout << (i ? ", " : "") << m_volNames[i];
        std::cout << ")";
    }
    std::cout << std::endl;
    return true;
}

void ResManager::shutdown() {
    m_vols.clear();
    m_volNames.clear();
}

std::string ResManager::looseFilePath(const std::string& name) const {
    // Arquivo solto na pasta do jogo tem prioridade sobre os VOLs (como o
    // GetFilePath original, que checa o diretório instalado antes).
    fs::path direct = fs::path(m_baseDir) / name;
    std::error_code ec;
    if (fs::exists(direct, ec) && fs::is_regular_file(direct, ec))
        return direct.string();

    // Busca case-insensitive na pasta base (sistemas de arquivos sensíveis).
    std::string target = name;
    std::transform(target.begin(), target.end(), target.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& de : fs::directory_iterator(m_baseDir, ec)) {
        if (!de.is_regular_file()) continue;
        std::string fn = de.path().filename().string();
        std::string low = fn;
        std::transform(low.begin(), low.end(), low.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (low == target) return de.path().string();
    }
    return {};
}

std::unique_ptr<StreamIO> ResManager::openStream(const std::string& name) const {
    // 1) Arquivo solto tem prioridade.
    std::string loose = looseFilePath(name);
    if (!loose.empty()) {
        std::ifstream f(loose, std::ios::binary);
        if (f) {
            f.seekg(0, std::ios::end);
            std::streamoff len = f.tellg();
            f.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(static_cast<size_t>(len));
            if (len > 0) f.read(reinterpret_cast<char*>(buf.data()), len);
            return std::make_unique<StreamIO>(std::move(buf));
        }
    }

    // 2) Entrada em algum VOL (descomprimida).
    for (const auto& v : m_vols) {
        const Vol::Entry* e = v->find(name);
        if (e) {
            std::vector<uint8_t> data = v->read(*e);
            return std::make_unique<StreamIO>(std::move(data));
        }
    }
    return nullptr;
}

bool ResManager::exists(const std::string& name) const {
    if (!looseFilePath(name).empty()) return true;
    for (const auto& v : m_vols)
        if (v->find(name)) return true;
    return false;
}

} // namespace op2
