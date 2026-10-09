#pragma once

#include <vector>

namespace op2 {

// Pesquisa nos laboratórios (Basic 0x21, Standard 0x31, Advanced 0x32):
//   sub_4731F0  tópicos disponíveis p/ o tipo de laboratório (1/2/3)
//   sub_457610  botão START → pacote 0x16 {lab, tópico, cientistas}
//   sub_40E210 case 22: lab parado/pesquisando e operando, tópico não
//               pesquisado → ação 0x16, [+0x60] tópico, [+0x64] cientistas
//   sub_43ACA0 case 22 ([+0x20]): sub-estado 0x0C, [+0x3C] = custo << 8,
//               [+0x24] = 0xC0
//   sub_43DE50 case 12: a cada 0xC0 ticks (lab operando) desconta
//               eficiência[máx + cientistas*15] * (bônus de moral + 256) *
//               cientistas (x3 em multiplayer)
//   sub_43ACA0 jpt_43B312 case 22: "%s research completed", sub_473830,
//               moral NEW_TECH_BORN (sub_44F840), libera os cientistas.

int labResearchKind(int labType);                       // 0x21→1, 0x31→2, 0x32→3, senão 0
std::vector<int> labTopics(int labId);                  // sub_4731F0 (jogador local)
void sendStartResearch(int labId, int topic, int scientists);   // sub_457610
bool processStartResearch(int owner, int labId, int topic, int scientists);   // case 22
void labResearchStart(int labId);                       // case 22 com [+0x20]
void labResearchProgress(int labId);                    // sub-estado 0x0C
void labResearchFinish(int labId);                      // jpt_43B312 case 22

// Treino de cientistas na University (0x26):
//   pacote 0x17 {univ, alunos} → sub_40E210 case 23: University parada e
//   operando → ação 0x17, [+0x60] alunos, [+0x64] duração = coluna "Storage
//   Capacity" do bloco (/2 em multiplayer)
//   sub_43ACA0 case 23 ([+0x20]): sub-estado 7 (conta só operando) por [+0x64]
//   jpt_43B312 case 23: trabalhadores −n, cientistas +n, "Scientist training
//   completed", moral PHD_TRAINED (sub_44F330).
void sendTrain(int univId, int students);
bool processTrainPacket(int owner, int univId, int students);
void universityTrainStart(int univId);
void universityTrainFinish(int univId);

// sub_43ACA0 case 5 num prédio: ação/sub-estado 0 e vt[0xC0] — labs
// (sub_432060) zeram tópico e cientistas; University (sub_49D610) os alunos.
void structureStopTick(int id);

} // namespace op2
