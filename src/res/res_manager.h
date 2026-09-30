#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vol.h"

namespace op2 {

// Stream de bytes em memória (equivale ao StreamIO do engine original: o
// ResManager entrega um stream, seja de arquivo solto ou de entrada de VOL já
// descomprimida). Cursor de leitura MSB-agnóstico; dados são little-endian (x86).
class StreamIO {
public:
    explicit StreamIO(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    const uint8_t* data() const { return m_data.data(); }
    size_t size() const { return m_data.size(); }
    size_t tell() const { return m_pos; }
    bool eof() const { return m_pos >= m_data.size(); }

    void seek(size_t pos) { m_pos = pos; }
    size_t remaining() const { return m_pos < m_data.size() ? m_data.size() - m_pos : 0; }

    // Lê raw bytes; retorna quantos foram efetivamente lidos.
    size_t readBytes(void* dst, size_t n);
    uint32_t readU32();
    uint16_t readU16();
    uint8_t  readU8();

    const std::vector<uint8_t>& bytes() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
    size_t m_pos = 0;
};

// Gateway de assets — porta ResManager (asm @181729). Init() varre os *.VOL da
// pasta do jogo; OpenStream(name) acha um arquivo solto (prioridade) ou uma
// entrada em algum VOL, descomprime e devolve um StreamIO. Ver
// [[op2-engine-architecture]] e [[op2-vol-format]].
class ResManager {
public:
    // baseDir = pasta do jogo (onde estão os *.VOL e arquivos soltos).
    bool init(const std::string& baseDir = ".");
    void shutdown();

    // Abre um recurso por nome (case-insensitive). nullptr se não existir.
    std::unique_ptr<StreamIO> openStream(const std::string& name) const;

    // true se o recurso existe (solto ou em VOL).
    bool exists(const std::string& name) const;

    const std::vector<std::string>& loadedVols() const { return m_volNames; }

private:
    std::string m_baseDir;
    std::vector<std::unique_ptr<Vol>> m_vols;
    std::vector<std::string> m_volNames;

    std::string looseFilePath(const std::string& name) const;
};

// Instância global, como o g_ResManager original.
extern ResManager g_ResManager;

} // namespace op2
