#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace op2 {

// Os cursores de comando do jogo: os 17 .ANI da pasta do Outpost 2 são
// cursores ANIMADOS do Windows, no formato RIFF/ACON padrão (chunks `anih`,
// `rate`, `seq ` e uma LIST `fram` de chunks `icon`, cada um um .CUR inteiro).
// O original os carrega com LoadCursorFromFileA (sub_488900), achando o arquivo
// pelo ResManager::GetFilePath e caindo na seta do sistema quando falta.
//
// Formato público — nada aqui vem de engenharia reversa; o que veio do
// disassembly foi a TABELA de qual cursor é de qual comando (ver cursors.h).
struct AniCursor {
    int width = 0, height = 0;          // tamanho do quadro, em pixels
    int hotX = 0, hotY = 0;             // ponto quente (do .CUR do 1º quadro)
    int jiffies = 10;                   // `anih`: 1 jiffy = 1/60 s
    // Um quadro por imagem, ARGB (0xAARRGGBB), de cima para baixo.
    std::vector<std::vector<uint32_t>> frames;
    // `seq `: ordem em que os quadros aparecem (vazio = 0,1,2,... na ordem).
    std::vector<int> seq;
    // Duração de cada passo da sequência, em jiffies (`rate`; vazio = jiffies).
    std::vector<int> rates;

    int steps() const {
        return seq.empty() ? static_cast<int>(frames.size()) : static_cast<int>(seq.size());
    }
    int frameOfStep(int i) const {
        if (frames.empty()) return 0;
        const int n = steps();
        if (n <= 0) return 0;
        const int k = ((i % n) + n) % n;
        const int f = seq.empty() ? k : seq[static_cast<size_t>(k)];
        return (f >= 0 && f < static_cast<int>(frames.size())) ? f : 0;
    }
    int rateOfStep(int i) const {
        const int n = steps();
        if (n <= 0 || rates.empty()) return jiffies;
        const int k = ((i % n) + n) % n;
        return k < static_cast<int>(rates.size()) ? rates[static_cast<size_t>(k)] : jiffies;
    }
};

// Lê um .ANI (RIFF/ACON) inteiro da memória. false se não for ACON ou se não
// sobrar nenhum quadro legível.
bool aniLoad(const uint8_t* data, size_t size, AniCursor& out);

// Lê um .CUR/.ICO solto (também usado para cada quadro do .ANI).
bool curLoad(const uint8_t* data, size_t size, int& w, int& h, int& hotX, int& hotY,
             std::vector<uint32_t>& argb);

} // namespace op2
