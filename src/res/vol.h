#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Leitor do container .VOL do Outpost 2 (portado 1:1 de tools/vol.py, validado
// nos 5 volumes / 437 arquivos). Diretório de entradas (chunks vols/voli) +
// blocos VBLK concatenados. Compressão por arquivo: 0=cru, 3=LZHUF (SHEETS).
class Vol {
public:
    struct Entry {
        std::string name;
        uint32_t nameOff = 0;
        uint32_t dataOff = 0;    // offset do "VBLK" no arquivo
        uint32_t size = 0;       // tamanho armazenado (comprimido) do bloco
        uint8_t  comp = 0;       // 0=cru, 1=RLE, 2=LZ, 3=LZHUF (byte +0x0C)
        uint8_t  flag = 0;       // byte +0x0D (0x01 = em uso)
    };

    // Carrega e faz o parse de um arquivo .VOL. ok() indica sucesso.
    bool load(const std::string& path);
    bool ok() const { return m_ok; }

    const std::vector<Entry>& entries() const { return m_entries; }
    const std::string& path() const { return m_path; }

    // Acha uma entrada por nome (case-insensitive). nullptr se não existir.
    const Entry* find(const std::string& name) const;

    // Bytes descomprimidos de uma entrada. Vazio em erro/compressão não suportada.
    std::vector<uint8_t> read(const Entry& e) const;

private:
    std::string m_path;
    std::vector<uint8_t> m_data;
    std::vector<Entry> m_entries;
    size_t m_strBase = 0;
    bool m_ok = false;

    uint32_t u32(size_t off) const;
    std::string nameAt(uint32_t off) const;
};

} // namespace op2
