#pragma once

namespace op2 {

// Gerenciador de som — porta o objeto unk_56C610 do OUTPOST2.EXE (DirectSound):
//   sub_47E020  carrega os 252 WAVs da tabela off_4E0160 (SOUND.VOL /
//               VOICES.VOL; id = índice) e lê "Sound"/"SoundVol" do INI
//   sub_47EE00  toca: 63 canais (lista livre; um som global pode roubar um
//               canal posicional — sub_47E760). Volume DirectSound em 1/100 dB:
//               global = −SoundVol*250; posicional (x,y em pixels) também −125
//               por tile de distância do centro da vista (maior eixo + metade
//               do menor, x com wrap); a partir de 32 tiles ou −4000 → mudo;
//               pan = dx(tiles)*125
//   sub_47F020  som global (sem posição)
//   sub_47F060 / sub_47F0A0  voz do Savant: fila de 8 (prioridade byte_56DB90,
//               nunca preenchida → FIFO); toca uma de cada vez.
bool soundInit();                          // após SDL_INIT_AUDIO
void soundShutdown();
void soundSetListener(int centerPixelX, int centerPixelY);   // [+0x90C]/[+0x910]
void soundPlay(int id);                    // sub_47F020
void soundPlayAt(int pixelX, int pixelY, int id);   // sub_47EE00
void soundVoice(int id, int pixelX, int pixelY);    // sub_47F060
void soundUpdate();                        // fila de vozes (sub_47F250)

// Volume mestre, de 0 a 100 por cento ("[Game] AudioVolume"; 50 por padrao).
// Nao existe no original: la cada canal tem a sua atenuacao fixa (MusicVol
// 0..5 pela tabela dword_4D4AD0, SoundVol a -250/100 dB por degrau) e nada
// regula o conjunto. Multiplica a mistura final, depois dos sons e da musica,
// portanto vale para os dois e nao mexe em nenhuma conta do original. Linear
// na amplitude: 50% = metade, ou cerca de -6 dB.
void soundSetMasterVolume(int percent);
int  soundMasterVolume();

// Trava/destrava o callback do mixer (a música altera o estado dele).
void soundLockAudio(bool lock);

// Som de seleção do tipo (campo +0x1F0 do descritor off_4E1340[map_id]).
int unitSelectSound(int mapId);

} // namespace op2
