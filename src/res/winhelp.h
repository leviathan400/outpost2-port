#pragma once

#include <string>
#include <vector>

namespace op2 {

// Leitor do OUTPOST2.HLP — o "Outpost 2 Online Manual", que no original o
// botão HELP e a tecla F1 abriam pelo WinHelp do Windows
// (TApp::GetHelp -> ResManager::GetFilePath("OUTPOST2.HLP") -> WinHelpA, com
// HELP_INDEX no shell e HELP_CONTEXT dentro da partida).
//
// O WinHelp não existe mais no Windows atual, então o porte lê o arquivo e
// mostra o manual na sua própria janela. O arquivo original não é convertido
// nem copiado: é lido onde está, como os .VOL e as DLLs de missão.
//
// FORMATO (WinHelp 3.x/4.0, magic 0x00035F3F). Tudo abaixo foi conferido no
// próprio OUTPOST2.HLP:
//
//   Cabeçalho: magic(4), início do diretório(4), cadeia livre(4), tamanho(4).
//   Em qualquer posição de "arquivo interno" há um FILEHEADER de 9 bytes
//   (reservado(4), usado(4), flags(1)) antes do conteúdo.
//
//   O diretório é uma árvore B+: BTREEHEADER (magic 0x293B, páginas de
//   `pagesize`, `levels` níveis, raiz `root`), páginas de índice
//   { não usado(2), nº entradas(2), página anterior(2) } e folhas
//   { não usado(2), nº entradas(2), anterior(2), próxima(2) } com entradas
//   "nome\0" + posição(4). São 216 arquivos internos: |SYSTEM, |TOPIC,
//   |TTLBTREE, |CONTEXT, |KWBTREE e ~200 bitmaps |bmNN.
//
//   |SYSTEM: magic 0x036C, minor/major, data, FLAGS. **Neste arquivo FLAGS é
//   0 — sem compressão — e não há |Phrases/|PhrIndex**, o que dispensa o LZ77
//   e a compressão por frases do formato. Os registros que seguem dão o título
//   ("Outpost 2 Online Manual"), o copyright, a janela etc.
//
//   |TTLBTREE: outra árvore B+, de TopicOffset(4) -> "título\0". 145 entradas.
//
//   |TOPIC: blocos de 4096 bytes, cada um com 12 bytes de cabeçalho
//   (último link, primeiro link, último cabeçalho de tópico). Dentro, uma
//   corrente de TOPICLINK de 21 bytes:
//       tamanho(4), tamanho dos dados 2(4), anterior(4), próximo(4),
//       tamanho dos dados 1(4), tipo(1)
//   Os endereços são TopicPos: bloco nos bits 14+, deslocamento nos 14 baixos
//   — e o deslocamento CONTA os 12 bytes de cabeçalho do bloco, de modo que um
//   link que passa do fim do bloco continua no byte 12 do bloco seguinte.
//   O "tamanho dos dados 1" conta a partir do começo do link (inclui os 21
//   bytes), então o texto fica em [dados1, dados1+dados2).
//   Tipos: 2 = cabeçalho de tópico, 0x20 = parágrafo, 0x23 = linha de tabela.
//   O texto é uma sequência de pedaços terminados em '\0'.
//
//   Os 145 tópicos aparecem na corrente na mesma ordem dos 145 títulos da
//   |TTLBTREE — conferido: em TODOS os 145 o título é o primeiro texto do
//   tópico.

struct HelpPara {
    bool table = false;              // link 0x23: os pedaços são células
    std::vector<std::string> cells;  // num parágrafo comum, um pedaço só
};

struct HelpTopic {
    std::string title;
    std::vector<HelpPara> paras;
};

// Queda para quando o OUTPOST2.HLP nao estiver no disco: procura um manual em
// PDF ao lado do jogo (preferindo o nome que mencione "outpost" ou "manual") e
// o entrega ao leitor de PDF do sistema. Devolve o arquivo aberto, ou "" se
// nao achou nenhum.
std::string helpOpenExternalManual();

class WinHelpFile {
public:
    // Lê o arquivo inteiro e monta os tópicos. false = não existe ou não é um
    // .HLP que este leitor entenda (e `error()` diz por quê).
    bool load(const std::string& path);

    bool ok() const { return !m_topics.empty(); }
    const std::string& error() const { return m_error; }
    const std::string& manualTitle() const { return m_title; }
    const std::vector<HelpTopic>& topics() const { return m_topics; }

private:
    std::vector<uint8_t> m_data;
    std::vector<HelpTopic> m_topics;
    std::string m_title;
    std::string m_error;
};

} // namespace op2
