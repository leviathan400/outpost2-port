#include "winhelp.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace op2 {

namespace {

constexpr uint32_t kHlpMagic = 0x00035F3F;
constexpr uint16_t kBtreeMagic = 0x293B;
constexpr int kBlock = 4096;        // tamanho do bloco do |TOPIC
constexpr int kBlockHdr = 12;       // cabeçalho de cada bloco
constexpr int kLinkHdr = 21;        // TOPICLINK

} // namespace

std::string helpOpenExternalManual() {
    namespace fs = std::filesystem;
    std::string melhor;
    int melhorNota = -1;
    std::error_code ec;
    for (const fs::directory_entry& e : fs::directory_iterator(".", ec)) {
        if (ec || !e.is_regular_file(ec)) continue;
        std::string nome = e.path().filename().string();
        std::string baixo = nome;
        std::transform(baixo.begin(), baixo.end(), baixo.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (baixo.size() < 5 || baixo.substr(baixo.size() - 4) != ".pdf") continue;
        int nota = 0;
        if (baixo.find("outpost") != std::string::npos) nota += 2;
        if (baixo.find("manual") != std::string::npos) nota += 2;
        if (nota > melhorNota) { melhorNota = nota; melhor = nome; }
    }
    if (melhor.empty()) return "";
#ifdef _WIN32
    // Entrega ao leitor de PDF do sistema; o jogo segue rodando atras.
    const HINSTANCE r = ShellExecuteA(nullptr, "open", melhor.c_str(), nullptr, ".", SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) return "";
#endif
    return melhor;
}

bool WinHelpFile::load(const std::string& path) {
    m_topics.clear();
    m_title.clear();
    m_error.clear();

    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { m_error = "nao encontrei " + path; return false; }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 16) { std::fclose(f); m_error = "arquivo curto demais"; return false; }
    m_data.resize(static_cast<size_t>(size));
    const size_t lido = std::fread(m_data.data(), 1, m_data.size(), f);
    std::fclose(f);
    if (lido != m_data.size()) { m_error = "leitura incompleta"; return false; }

    const uint8_t* D = m_data.data();
    const size_t N = m_data.size();
    auto u16 = [&](size_t o) -> uint16_t {
        return (o + 2 <= N) ? uint16_t(D[o] | (D[o + 1] << 8)) : 0;
    };
    auto i16 = [&](size_t o) -> int16_t { return static_cast<int16_t>(u16(o)); };
    auto u32 = [&](size_t o) -> uint32_t {
        return (o + 4 <= N) ? (uint32_t(D[o]) | (uint32_t(D[o + 1]) << 8) |
                               (uint32_t(D[o + 2]) << 16) | (uint32_t(D[o + 3]) << 24)) : 0;
    };
    auto i32 = [&](size_t o) -> int32_t { return static_cast<int32_t>(u32(o)); };

    if (u32(0) != kHlpMagic) { m_error = "nao e um arquivo de ajuda do Windows"; return false; }
    const size_t dirStart = u32(4);
    if (dirStart + 9 >= N) { m_error = "diretorio fora do arquivo"; return false; }

    // Percorre as folhas de uma árvore B+ chamando `entrada` para cada uma; ela
    // devolve a posição logo depois do registro que consumiu.
    auto percorre = [&](size_t fileOff, const std::function<size_t(size_t)>& entrada) -> bool {
        const size_t p = fileOff + 9;                 // pula o FILEHEADER
        if (u16(p) != kBtreeMagic) return false;
        const int pageSize = u16(p + 4);
        const int root = i16(p + 26);
        const int levels = i16(p + 32);
        if (pageSize <= 0 || levels <= 0) return false;
        const size_t pages = p + 38;
        int page = root;
        for (int l = 0; l < levels - 1; ++l)           // desce pela borda esquerda
            page = i16(pages + size_t(page) * pageSize + 4);
        int next = page;
        int guarda = 0;
        while (next != -1 && ++guarda < 10000) {
            const size_t o = pages + size_t(next) * pageSize;
            if (o + 8 > N) return false;
            const int n = i16(o + 2);
            next = i16(o + 6);
            size_t q = o + 8;
            for (int k = 0; k < n; ++k) q = entrada(q);
        }
        return true;
    };

    auto cstr = [&](size_t o) -> std::string {
        size_t e = o;
        while (e < N && D[e]) ++e;
        return std::string(reinterpret_cast<const char*>(D + o), e - o);
    };

    // ---- diretório: nome -> posição do arquivo interno ----
    size_t offSystem = 0, offTopic = 0, offTitles = 0;
    if (!percorre(dirStart, [&](size_t q) -> size_t {
            const std::string nome = cstr(q);
            const size_t fo = u32(q + nome.size() + 1);
            if (nome == "|SYSTEM") offSystem = fo;
            else if (nome == "|TOPIC") offTopic = fo;
            else if (nome == "|TTLBTREE") offTitles = fo;
            return q + nome.size() + 5;
        })) {
        m_error = "diretorio ilegivel";
        return false;
    }
    if (!offTopic || !offTitles) { m_error = "faltam |TOPIC ou |TTLBTREE"; return false; }

    // ---- |SYSTEM: o título do manual e, sobretudo, a compressão ----
    if (offSystem) {
        const size_t p = offSystem + 9;
        const size_t usado = u32(offSystem + 4);
        const uint16_t flags = u16(p + 10);
        if (flags != 0) {
            // Só o OUTPOST2.HLP interessa aqui, e ele não comprime; implementar
            // o LZ77 e as frases do formato sem um arquivo para validar seria
            // escrever às cegas.
            m_error = "o arquivo usa compressao (flags=" + std::to_string(flags) + ")";
            return false;
        }
        size_t q = p + 12;
        const size_t fim = p + usado;
        while (q + 4 <= fim) {
            const uint16_t tipo = u16(q), tam = u16(q + 2);
            if (tipo == 1 && m_title.empty()) m_title = cstr(q + 4);   // TITLE
            q += 4u + tam;
        }
    }

    // ---- |TTLBTREE: os títulos, em ordem de TopicOffset ----
    std::vector<std::string> titulos;
    percorre(offTitles, [&](size_t q) -> size_t {
        const std::string t = cstr(q + 4);
        titulos.push_back(t);
        return q + 4 + t.size() + 1;
    });

    // ---- |TOPIC: a corrente de TOPICLINK ----
    const size_t tp = offTopic + 9;
    const size_t usado = u32(offTopic + 4);
    const size_t fimTopic = tp + usado;

    // TopicPos -> bytes, pulando o cabeçalho de 12 bytes de cada bloco.
    auto leia = [&](uint32_t pos, size_t n, std::vector<uint8_t>& out) -> bool {
        out.clear();
        size_t blk = pos >> 14, off = pos & 0x3FFF;
        while (n > 0) {
            const size_t src = tp + blk * kBlock + off;
            if (src >= fimTopic) return false;
            size_t take = size_t(kBlock) - off;
            if (take > n) take = n;
            if (src + take > N) return false;
            out.insert(out.end(), D + src, D + src + take);
            n -= take;
            ++blk;
            off = kBlockHdr;
        }
        return true;
    };

    std::vector<uint8_t> hdr, corpo;
    uint32_t pos = kBlockHdr;              // o primeiro link do bloco 0
    int guarda = 0;
    while (++guarda < 200000) {
        if (!leia(pos, kLinkHdr, hdr)) break;
        const int32_t tamanho = int32_t(hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (uint32_t(hdr[3]) << 24));
        const int32_t len2 = int32_t(hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | (uint32_t(hdr[7]) << 24));
        const int32_t prox = int32_t(hdr[12] | (hdr[13] << 8) | (hdr[14] << 16) | (uint32_t(hdr[15]) << 24));
        const int32_t len1 = int32_t(hdr[16] | (hdr[17] << 8) | (hdr[18] << 16) | (uint32_t(hdr[19]) << 24));
        const uint8_t tipo = hdr[20];
        if (tamanho <= 0 || tamanho > 0x20000) break;

        if (tipo == 2) {                                   // começa um tópico
            m_topics.push_back(HelpTopic());
            if (m_topics.size() <= titulos.size())
                m_topics.back().title = titulos[m_topics.size() - 1];
        } else if (!m_topics.empty() && len2 > 0 && len1 >= kLinkHdr) {
            if (leia(pos, size_t(tamanho), corpo) &&
                size_t(len1) + size_t(len2) <= corpo.size()) {
                HelpPara par;
                par.table = (tipo == 0x23);
                const uint8_t* t = corpo.data() + len1;
                const uint8_t* fim = t + len2;
                const uint8_t* ini = t;
                for (const uint8_t* c = t; c <= fim; ++c) {
                    if (c == fim || *c == 0) {
                        if (c > ini) par.cells.emplace_back(reinterpret_cast<const char*>(ini), c - ini);
                        else par.cells.emplace_back();
                        ini = c + 1;
                    }
                }
                // Um parágrafo só de pedaços vazios é formatação, não texto.
                bool algo = false;
                for (const std::string& s : par.cells)
                    for (char ch : s) if (ch > ' ') { algo = true; break; }
                if (algo) m_topics.back().paras.push_back(std::move(par));
            }
        }
        if (prox == -1) break;
        pos = uint32_t(prox);
    }

    if (m_topics.empty()) { m_error = "nenhum topico"; return false; }
    return true;
}

} // namespace op2
