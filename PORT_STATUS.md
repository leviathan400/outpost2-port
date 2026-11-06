# Outpost 2: Divided Destiny — Port Moderno (C++17/SDL2)

> Estado do port. Última revisão: **2026-11-05** (199 commits), feita lendo o
> código e rodando os testes, não a documentação anterior — que estava de
> 2026-09-29 e dizia que o port "ainda não reimplementa o jogo em si".

---

## 1. O que isto é hoje

Um porte **jogável** de *Outpost 2: Divided Destiny* (Sierra/Dynamix, 1997)
para C++17 + SDL2. O jogo roda: campanha das duas facções, jogos de colônia,
os 14 tutoriais e partidas de multijogador em rede.

O mecanismo **sai do disassembly** (`Outpost2_src/OUTPOST2.asm`, IDA) e dos
recursos das DLLs originais — nada é inventado. Quando o original tem um
defeito identificado, o porte **corrige em vez de reproduzir**.

Os arquivos de 1997 não são modificados nem redistribuídos: o porte os **lê**
(`OP2SHRES.DLL`, `OUT2RES.DLL`, os `.VOL`, `OP2_ART.BMP`, as DLLs de missão).
As missões originais rodam **de verdade**, carregadas por um carregador PE
próprio com pontes de ABI para a API que elas importam do `OUTPOST2.EXE`.

### Números de hoje

| | |
|---|---|
| Código do porte | **45.200 linhas** de C++17 em `src/` |
| Simulação (`src/game`, 40 .cpp) | 21.400 linhas |
| UI in-game (`src/app`, 20 .cpp) | 8.100 linhas |
| Shell/menus (`src/shell`) | 3.900 linhas |
| Rede (`src/net`) | 1.400 linhas |
| DLLs de missão que carregam e rodam | **94 de 94** |
| Símbolos da API do EXE implementados | **222 de 222** (os que as 94 DLLs importam) |
| Modos de linha de comando (testes e telas) | 96 |

As duas linhas de missões são a medida honesta de cobertura: toda DLL de
missão do disco carrega, cria as unidades e roda, e nenhuma importa um símbolo
que o porte não tenha. Para refazer a conta: varredura com `--missiontest` em
cada DLL, e a cobertura da API comparando as tabelas de importação das DLLs
com o registro de `src/mission/op2_api.cpp`.

---

## 2. Estrutura

```
src/
  main.cpp        entrada + os 96 modos de teste/captura
  app/            TApp, UIState, GameState (o jogo), ShellState, HUD,
                  as 6 abas do quadro, minimapa, globo, diálogos
  game/           a simulação: unidades, estruturas, movimento, pathfinder,
                  combate, economia, pesquisa, desastres, triggers, IA
  gfx/            framebuffer 640x480, PBMP, sprites, fontes (bitmap e GDI)
  res/            leitor de PE (.rsrc + exports), .VOL + LZHUF, ResManager
  core/           OUTPOST2.INI (CConfig)
  audio/          mixer SDL fiel ao DirectSound do original, música (.clm)
  mission/        carregador PE das DLLs de missão + ponte da API
  net/            lobby (descoberta/entrada) e partida em lockstep
tools/            scripts Python de engenharia reversa (formatos, dumps)
Outpost2_src/     disassembly de referência (NÃO versionado)
```

---

## 3. O que está portado

### Simulação
- **Relógio e RNG** determinísticos, bit-exatos com o original.
- **Unidades**: pool, criação, desenho fiel (sprites `op2_art` com paleta e
  cor de jogador), sombras, nº de série.
- **Movimento e pathfinder** portados da arquitetura real (rotas, filas,
  bloqueios) — nunca um A* inventado.
- **Estruturas**: construção, ligar/desligar, dano, destruição, entulho,
  desmonte, tomada, reparo, animações de ambiente.
- **Economia da colônia** completa: rede de tubos, energia, trabalhadores,
  cientistas, **comida, população e moral** (incluindo a moral forçada pelas
  missões).
- **Mineração**: Robo-Miner/GeoCon, `MINES.TXT`, caminhões, docas, smelters.
- **Obras de terreno**: tubos, muros, Robo-Dozer, varredura de região.
- **Combate**: todas as armas de tanque e de torre, EMP/espuma/ESG/ácido/
  Thor/Scorpion, autodestruição, dano em área com armadura, paredes.
- **Pesquisa**: as quatro árvores, laboratórios, cientistas, propagação.
- **Desastres**: meteoro, raio, tornado, terremoto, erupção/lava e a
  **praga (micróbio)**, com as três fases de aviso do original.
- **Triggers**: os 16 `Create*Trigger`, vitória/derrota, objetivos.
- **IA**: grupos (Sc/Fight/Build/Mine), o padrão do computador e o Pinwheel.
- **Salvar/carregar** (`SGAME%d.OP2`), com teste de determinismo.

### Interface
- **Shell**: menu principal, campanha (briefing/objetivos/capítulos do
  `STORY.VOL`), jogos de colônia com placar, tutoriais, preferências, sobre.
- **In-game**: painel direito, quadro da unidade, botoeira 3x3, botões de
  prédio, linha de ajuda, as 6 abas de relatório, minimapa com zoom, visão
  tática e globo, ciclo dia/noite.
- **Controles** no padrão Windows: foco por teclado, combos com lista
  suspensa, barras de rolagem.

### Áudio
Mixer SDL fiel ao do original (volume e pan por tile, fila de vozes), música
`.clm` e **volume mestre** próprio do porte.

### Manual (ajuda) e demo de atração
O botão HELP e o F1 abrem o **OUTPOST2.HLP** — "Outpost 2 Online Manual", 145
tópicos — numa tela do porte, porque o WinHelp que o original chamava não
existe mais no Windows. O arquivo é lido onde está (`src/res/winhelp.*`), não
convertido nem copiado; sem ele, o HELP procura um manual em PDF ao lado do
jogo e o entrega ao leitor do sistema.

Um minuto parado no menu principal dispara o **demo de atração**, como no
original: `ademo{1,2,3}.dll` em rodízio pelo contador `[Game] Demo` do INI
(que é um contador, não um liga-desliga), com desastres e dia/noite ligados.
Qualquer tecla ou clique interrompe. Dois acréscimos deste porte deixam o demo
assistível: a **câmera passeia** pelos pontos de ação (no original ela fica
parada e boa parte da cena acontece fora da tela) e o **inimigo aparece no
escuro** — só no demo; numa partida a cegueira noturna continua valendo.

### Multijogador
Partida em **rede, em lockstep bit-exato**, com lobby sincronizado
(descoberta por broadcast, entrada, chat, EJECT, opções), as telas do
`OUT2RES.DLL` e as 51 missões M*.

### Acréscimos do porte (não existem no original)
Três chaves no `[Game]` do `OUTPOST2.INI`, com controle na tela de
preferências; o original ignora chaves que não conhece.

| Chave | Faixa | Padrão | O que faz |
|---|---|---|---|
| `AILevel` | 0..3 | 0 (AUTO) | Força o nível da IA nas campanhas e colônias. AUTO = o comportamento do original. **Não se aplica em rede**, onde a dificuldade é por jogador e vem do lobby. |
| `NightDarkness` | 0..100 | 75 | Quanto da curva de escurecimento noturno se aplica (100 = o original, que leva a noite plena a ~15% do brilho e faz as unidades, que nunca escurecem, recortarem contra o preto). |
| `AudioVolume` | 0..100 | 50 | Volume mestre sobre a mistura final (sons e música). O original só tem `MusicVol`/`SoundVol`, por canal e em 6 degraus. |
| `DemoCamera` | 0/1 | 1 | No demo de atração a câmera passeia pelos pontos de ação (desastre, combate, unidades). 0 devolve a câmera parada do original. |
| `Fullscreen` | 0/1 | 1 | Abre em tela cheia (sem troca de modo de vídeo; a vista de 640×480 escalada com a proporção mantida). Alt+Enter alterna a qualquer momento. |

---

## 4. O que FALTA portar

**Nada.** Em 2026-11-06 a última pendência (o item 21) foi fechada. O que
sobrou abaixo é o histórico: cada item com o que era, como foi resolvido e,
nos oito casos em que a "pendência" não existia, por que o comentário mentia.

Duas coisas seguem **registradas como lacuna consciente**, e nenhuma muda o
jogo: o sinalizador `dword_4E0BD0` dos cheats (um único leitor, `sub_431EB0`,
num caminho que não identifiquei — ver 5.6) e a **API de cheat das missões**
sobre o pacote 0x2B, com os oito índices mapeados mas nenhuma das 94 missões
originais a importando.

Revisado item a item em 2026-10-28 e em 2026-11-03/06, conferindo no código.

Isso vale para o que **falta portar**, e não quer dizer que não haja defeito no
que já está: em 2026-11-06, depois do inventário, a aba Options apareceu com
dois (os itens da tela de fim de missão não faziam nada) e foi consertada —
ver 5.7.

### Grande

1. ~~Rede com mais de 2 jogadores~~ — **feito** em 2026-10-30. A topologia é a
   mesma da camada de mensagens do lobby: **estrela com o host no meio**. O
   host fala com cada lugar ocupado e **repassa** os turnos de cada cliente aos
   demais; o cliente fala só com o host, então ninguém precisa alcançar todo
   mundo (NAT, firewall). A máquina de turnos já era de N jogadores — só o
   `sendRaw` estava preso a um par. Medido: 3 e 4 jogadores em lockstep, com
   hash idêntico em todas as máquinas (`--mplan host|join <DLL> <ticks> <N>`).

### Multijogador, peças soltas

Todas resolvidas em **2026-11-03**, e duas delas eram leitura errada minha:

2. ~~Ações da aba Options~~ e ~~TRANSFER~~ — **feitos**. A aba virou as três
   telas do original (menu, `PREFERENCES`, `END MISSION`) com os seis botões de
   `sub_457880`. E o TRANSFER da botoeira **faz** algo: o `nullsub_84` é o
   vt[3], o clique no MAPA, que ele não usa. O vt[1] (`sub_455A80`) abre o
   escolhedor de jogador *"Transfer Vehicle(s) to:"* (`sub_46D230`), e a
   escolha monta o pacote **0x18** com as unidades selecionadas e o destino
   (`sub_455950`); o case 24 troca o dono de cada uma e avisa os dois lados.
   O botão aparece com a trapaça `dword_56DE58` ou com um **Trade Center
   operando** numa partida multijogador.
3. ~~Trade Center~~ — **feito**: não havia "UI de troca de recursos"; o que o
   prédio faz é habilitar o TRANSFER. Ele voltou à lista de produção, porque
   `sub_468120` só deixa o 0x20 passar com `dword_56DC78 != 0` — e esse
   ponteiro é a **camada de rede**, ou seja o prédio só existe em partida em
   rede.
4. ~~Modo Chat da aba Communications~~ — **feito**, e também não era o que o
   nome sugeria: o modo 1 é a tela *"Set Up Allies and Chat Partners"*, com as
   duas listas de jogadores e as caixas de chat (máscara local) e de aliança
   (pacote **0x32**). O botão `&NET STATS` abre a terceira tela
   (`sub_467760`), com os seis campos do original. A conversa em si é a
   **linha do jogo** (`sub_4115F0`): Enter abre, Enter manda o pacote **0x30**,
   Esc desiste.
5. ~~Estatísticas de fim de partida~~ — **feitas**, ver a correção do item 9.

### Miudezas de simulação (todas marcadas no código)

6. ~~Diálogo de confirmação do IDLE quando o prédio guarda metal~~ — **feito**,
   e o original avisa de menos. `sub_455CB0` (o vt[1] do botão IDLE) compara a
   capacidade **deste** prédio com a capacidade **total** do jogador (+0x18
   comum, +0x1C raro) e só pergunta quando são **iguais** — isto é, quando ele
   é o único armazém daquele metal, que é o único caso em que o texto "All
   metal in storage will be lost" é verdade. Com uma fundição (10000) e um
   armazém (5000), desligar a fundição com 15000 guardados perde 10000 **em
   silêncio** (`sub_471F30` trunca). Aqui a conta é o transbordo, então a
   pergunta cobre qualquer perda e o número entra no lugar do "All". Duas
   outras correções: o índice do bloco de stats é o nibble **alto** de `[+0x1D]`
   (`sar al, 4`), não o dono; e o ramo do metal raro do original exige
   `rareOre != 0` enquanto o do comum não, de modo que ele pergunta com o
   estoque vazio — o transbordo cuida dos dois. Teste: `--idletest <DLL>`
   (quatro casos por prédio; E10/E12/P10 cobrem a perda parcial).
7. ~~O painel STORAGE~~ — **não é pendência**: conferido em 2026-10-28, a
   regra do original (`loc_456560`) habilita o comando para 0x19, 0x1A, 0x27,
   0x29, 0x2F e 0x36–0x38, que são exatamente os oito tipos que o painel
   cobre. A mensagem de "ainda não portado" é só uma guarda inalcançável.
8. ~~Ordem "atirar no chão"~~ — **feito, e não era ordem**: o bit 0x2000 de um
   veículo só é ligado em `loc_44B8F0` de `sub_44B650` (o movimento), quando
   uma unidade **do computador** armada de Laser (0x3D) ou Microwave (0x3E)
   esbarra num tubo/parede sem ter alvo — é assim que a IA abre caminho. Não
   existe comando do jogador para isso.
9. ~~Estatísticas de baixas por jogador~~ — **feitas em 2026-11-03**, e a
   conclusão anterior ("nunca lidos") estava **errada**: o `MissionResults`
   **copia sim**. O fim de `sub_489A30` (`loc_489CF3`) leva SETE dwords de
   `[jogador+0x1BC]` para `MissionResults+0xC2 + jogador*28`, e o `OP2Shell`
   lê de lá — os motivos 7, 8 e 9 da mensagem 0x73a mandam todos `0x727`, que
   abre o **Dialog 155 "MULTIPLAYER GAME STATISTICS"**. São seis contadores
   (`+0x1BC` prédios construídos, `+0x1C0` veículos construídos, `+0x1C4`
   prédios perdidos, `+0x1C8` veículos perdidos, `+0x1CC` prédios destruídos,
   `+0x1D0` veículos destruídos); o sétimo dword é a soma dos minérios, que o
   original escreve por cima.
10. ~~Revelação do mapa ao construir~~ — **não existe**: conferido em
    2026-10-29. O trecho de `dword_574C88+0x110` converte o retângulo do prédio
    para a tela e o passa a `sub_46F550`, que é chamada de 36 pontos (entre
    eles o realce de hover) — é a lista de **retângulos sujos** da vista. O
    Outpost 2 não tem névoa de guerra. Este porte repinta a tela inteira a cada
    quadro, então não há o que portar.
11. ~~Pacote 0x1D (tomar posse) para os grupos de IA~~ — **feito** em
    2026-10-29: `orderTakeOver` em `ai_group.cpp` (porte de `sub_42A100`),
    chamado do caso 4 do `classHandler`. A Spider do computador toma veículos
    inimigos **paralisados por EMP** a até 256 px (`sub_427CC0`). Teste:
    `--takeovertest <map>`.
12. ~~Modo 3 do pathfinder~~ — **não é pendência**: conferido em 2026-11-03,
    `sub_438850` está portado em `pfMoveCost` — parede/tubo bloqueia sem
    exceção e o objetivo é ficar ao lado do alvo (quatro vizinhos). O
    comentário em `pathfinder.h` é que dizia "pendente".
13. ~~Fila de waypoints com Shift~~ — **não é pendência**: conferido em
    2026-10-29, a fila `unk_5471B0`/`dword_5471AC` é código morto no
    executável. O contador é o campo +0x3C do objeto do modo de mira, que
    `sub_41B550` zera ao entrar no modo e que ninguém incrementa — as três
    funções que o leem só leem. O jogo publicado manda sempre UM ponto por
    comando, que é o que o porte faz.
14. ~~Dano em paredes nas autodestruições~~ — **não é pendência**: conferido
    em 2026-11-03, `explodeDestruct` chama `damageWall` com
    (raio − d) * (Concussion + Penetration) / raio, que é exatamente a conta
    de `sub_4A4090`. Dois comentários (em `weapon.cpp` e `disaster.h`) é que
    anunciavam a pendência.

### Encontradas no inventário de 2026-11-03

Varredura dos marcadores do próprio código contra o disassembly. Cinco
"pendências" que os comentários anunciavam **não existiam** (a garagem, o
micróbio, o áudio das mensagens, a fase B da economia e a serialização de rede
do `CommandPacket` estão todos portados — os comentários é que tinham ficado
para trás).

**Segunda passada, no mesmo dia**, agora conferindo cada item no código em vez
de ler o comentário que o anunciava: **outras três eram falsas** (as 12, 14 e
17 — modo 3 do pathfinder, dano em paredes nas autodestruições e animações de
ambiente — estavam portadas, com o comentário atrasado). Das seis reais,
16, 18, 19 e 20 foram fechadas no mesmo dia; **sobra a 21**.

16. ~~Veículo guardado em garagem não conta em lugar nenhum~~ — **feito**. Era
    o único item que podia quebrar um objetivo de missão: a garagem foi portada
    em 2026-10-14 (`garageSlot[6]`, `garageCount`), mas seis pontos de contagem
    ignoravam quem estava dentro dela, e no original **todos** tratam o 0x29.
    Os seis, com a regra de cada um lida no disassembly:

    | Ponto | Baias que varre | Regra |
    |---|---|---|
    | `sub_493AF0` (contagem por jogador) | **6, fixo** (`loc_493D0E`) | tipo + filtro de carga de `byte_493FD4` |
    | `sub_494280` (gatilho KIT) | **Storage Bays** | ConVec (tipo 2) carregando o kit |
    | `sub_494E70` (contagem de veículos) | `vt[63]` | soma a contagem à lista |
    | `sub_494600` (gatilho EVAC) | **6, fixo** | transportes 0x0D guardados |
    | `?HasOccupiedBay` | `vt[63]` | devolve a **contagem**, não 0/1 |
    | `?hasVehicle` | **6, fixo** | tipo + carga (sempre a palavra inteira) |

    A inconsistência das baias (uns laços fixam 6, outros vão até a Storage
    Bays) é **do original**, e foi mantida. A regra de carga também difere entre
    as duas metades de `sub_493AF0`: o ramo de **um** jogador aceita os tipos 1,
    2, 5, 6, 7 e 0x0D (tabela `byte_493FD4`) e varre as garagens; o de **todos**
    (`loc_493DE2`) só 1, 2 e 0x0D, e **não** olha garagem nenhuma — também do
    original, conferido, e marcado no código para ninguém "unificar".
    Teste: `--garagecounttest <DLL>`, dez casos; neutralizando a correção, oito
    reprovam e os dois controles negativos continuam passando.
17. ~~Animações de ambiente dos prédios~~ — **não é pendência**: conferido em
    2026-11-03, `structureAmbientTick` é `sub_4835C0` inteiro, com a tabela
    `byte_4E4138` (11 registros) e a variante por tipo (`desc+0x2A6/+0x2A7`),
    o gate de 100 ticks defasado por y e id, e a opção `Ambient` do INI. O
    comentário em `structure.h` é que dizia "ainda não portado".
18. ~~Auto-engajamento: "atacado nos últimos 12 ticks"~~ — **feito**. Era a
    única pendência deste inventário que mudava o que se vê jogando, e o que
    `sub_44DE10` faz não é escolher alvo: é **alertar a vizinhança**. A unidade
    atingida nos últimos 12 ticks passa o próprio agressor para todo veículo
    armado do próprio lado a menos de 96 px (3 tiles) em x e y que esteja
    parado (`[+0x21]` = 0) e ainda sem agressor (`[+0x26]` = 0), datando o
    alerta de 25 ticks atrás — sobram 125 dos 150 ticks da janela de revide.
    Resultado: o esquadrão reage junto em vez de só quem levou o tiro. Cada
    alerta soma uma referência (`inc byte [+0x32]`) no agressor.
    **Só vale para dono humano**: o gate do topo de `sub_44DE10` é
    `dword_56E304[dono]`, que é `isHuman` (`[jogador+0x28]`); a IA tem o
    caminho dela, `ScGroup::HasBeenAttacked`. Teste: `--alerttest <DLL>`, onze
    casos, inclusive a borda exata dos 96 px (o asm compara com `<` estrito) e
    o gate do dono do PC; neutralizando a correção, cinco reprovam e os sete
    controles negativos continuam passando.

    A caixa de busca é por células em vez da lista de unidades ordenada por
    `[+0x18]` que o original percorre nos dois sentidos — o conjunto é o mesmo
    (veículo ocupa uma célula) e o teste de pixel ficou idêntico.
19. ~~`vt[35]` na morte do veículo~~ — **feito**, e não era "descarga por
    classe": extraí as 40 vtables de unidade do asm e em todas, menos
    `off_4D1788`, a entrada 35 é a **mesma** função, `sub_483850` — que põe
    `dword_56E37C[dono] = 1`, o `colonyMarkDirty` do porte. `loc_43D8B3` só a
    chama com o bit 1 de `[+0x44]`, ou seja só em veículo. Uma linha em
    `vehicleDeath`: veículo morto obriga a economia do dono a recalcular.
20. ~~Grupo de IA com prazo vencido~~ — **feito** (`sub_42B360`). A ordem dos
    dois comandos importa e é esta: vencido o prazo, **nos primeiros 400 ticks**
    cada membro leva a autodestruição (0x14) e, **de 400 ticks em diante**, quem
    ainda estiver na lista leva a de sair do mapa (0x2A). Na prática o segundo
    ramo só alcança quem a autodestruição recusa — `unitDoSelfDestruct` exige
    comandável, então um veículo sob EMP sobra para o 0x2A. Quem não é veículo
    (sem o bit 1 de `[+0x44]`) sai do grupo antes, pelo `vt[11]`, e leva a
    ordem do mesmo jeito. O grupo não é apagado aí: cai no passe em que a
    lista chega a zero. Teste: `--deletetest <DLL>`, oito casos cobrindo os
    três ramos; neutralizando a correção, três reprovam.
21. ~~Três miudezas de apresentação~~ — **feito** em 2026-11-06. As três, e o
    que cada uma era de verdade:

    **(a) A plataforma do Spaceport não anunciava.** `sub_47F600` não toca um
    som posicional: chama `sub_47F060`, que é a **fila de vozes do Savant**. A
    SULV (0x69) e o míssil (0x6B) anunciam com a voz **0x80**; a RLV (0x6A), com
    a **0x7F**. E só o **dono** ouve (`cmp eax, dword_56DEE8`).

    **(b) O ícone do painel STORAGE ignorava a arma.** `sub_458EF0` já estava
    portado — é o `drawVehicleIcon` que o quadro da unidade usa, com o corpo na
    cor do dono e a torre da arma por cima. O painel STORAGE é que não o
    chamava: desenhava o quadro 0 do corpo e pronto. Agora chama, e um Laser
    Lynx e um Microwave Lynx aparecem diferentes na caixa da doca.

    **(c) O anel anti-duplicata `[_Player+0xBD8]`** — 16 posições, uma por
    turno. **Dez** comandos passam por ele (os `case` de `sub_40E210` que tocam
    `[ebp+eax*4+0BD8h]`): 0x09 produzir, 0x0A..0x0D as cargas, 0x12 e 0x13
    ociosar e religar, 0x16 pesquisar, 0x17 treinar e 0x19 lançar. Antes de
    aplicar, o original vê se a **mesma unidade** já foi assunto de um desses
    comandos na janela recente; se foi, o comando é **descartado**
    (`loc_40E93C` devolve 1 e o despachante sai sem aplicar). É a proteção
    contra comando repetido — o clique duplo, ou o pacote que chegou duas vezes.
    Vale **só para jogador humano** (`[_Player+0x28]`).

    A janela não é a óbvia, e foi conferida instrução por instrução: com
    `turno = tique >> log2(período)`, ele confere os slots de `turno−4` a
    `turno−2` — três, porque `dword_56DED8` é o mesmo **kTurnLead** do porte —
    e **grava** no slot do `turno`. O turno corrente e o anterior ficam de fora
    da conferência, o que casa com o lockstep: o comando aplicado no turno T foi
    emitido em T−kTurnLead, logo a janela cobre o turno da emissão e os dois
    anteriores. O anel mora no próprio `PlayerData`, como no original, e por
    isso zera em `resetPlayers` e entra no jogo salvo por `playersSerialize`.

    Uma diferença **proposital**: o índice do original sai de um `%` **com
    sinal**, então nos primeiros turnos ele lê alguns dwords *antes* do anel —
    campos vizinhos do jogador. Aqui o índice é normalizado para 0..15; na
    prática dá no mesmo (os slots começam zerados e nada casa), sem ler fora do
    vetor.

    O único dos dez que o porte não guarda é o **0x0D**, que o despachante do
    porte não trata (o enum vai de 0x0C a 0x0E); a entrada está em `usaAnel`
    para quando ele existir.

    Teste: `--item21test <DLL>`, quinze casos. Neutralizando as três peças,
    cinco reprovam — ao menos um por peça — e os dez restantes, controles
    incluídos, continuam passando.

### Não aplicável nesta instalação

15. **Filmes (AVI).** O shell original toca `I.AVI`/`IE`/`IP` na abertura,
    `E4.AVI`, `MEF`/`MPF` e as oito variantes de fim de campanha
    (`MEBL`/`MEBW`/`MEUL`/`MEUW`/`MPBL`/`MPBW`/`MPUL`/`MPUW`) e as quatro de fim
    de colônia (`CES`/`CEP`/`CPS`/`CPP`). **Nenhum `.AVI` existe no disco** —
    ficavam no CD. Portar o player não teria o que tocar.

### Decisões deliberadas (não são pendências)

- **A tela 259 "PICK CONNECTION METHOD" não existe aqui.** Dos seis meios da
  tabela `unk_4DF370` só o TCP/IP sobreviveu — o SIGS era o serviço da Sierra,
  o IPX saiu do Windows, modem e serial eram DirectPlay sobre linha discada.
  O MULTIPLAYER abre direto o Dialog 260. Por isso `MpNetOptions` é a primeira
  tela de multijogador, e os testes `m_state >= MpNetOptions` dependem da ordem
  do enum.

---

## 5. Ferramentas de análise (`tools/`)

Scripts Python que usam **`pefile`** + **`capstone`** (disassembler) para
engenharia reversa dos recursos e do código:
- **`extract_menu.py` / `dump_dialogs.py` / `parse_dlg.py`** — extraem e
  interpretam `DLGTEMPLATE` das DLLs (geram JSON como `menu_dialog.json`).
- **`extract_bmps.py` / `extract_btns.py` / `inspect_bmps.py`** — extraem
  bitmaps (fundos, botões) para PNG/BMP de conferência.
- **`analyze_shell.py`** — busca no assembly onde IDs de recurso (ex.: bitmap
  225/190, string "MAIN MENU") são referenciados, para descobrir a lógica.
- **`inspect_vol.py` / `inspect_table.py`** — inspecionam os containers `.VOL`.

> **Dependências Python** (não versionadas): `pip install pefile capstone pillow`.

---

## 5.1. Formato `.VOL` — DECODIFICADO ✅ (2026-09-29)

Container de assets do jogo, conceitualmente igual ao `.WAD` do Doom/Quake:
diretório de entradas + dados concatenados. Usa **chunks com tag** (estilo
RIFF/IFF). Parser funcional em [tools/vol.py](tools/vol.py).

**Layout:**
```
"VOL " <u32 len>            container do diretório (payload = volh+vols+voli)
    "volh" <u32 len=0>      volume header (vazio)
    "vols" <u32 len>        STRING TABLE:  <u32 strLen> + nomes '\0'-separados
    "voli" <u32 len>        INDEX:         (len/14) entradas de 14 bytes
"VBLK" <u32 len> data...    um bloco por arquivo (dados; alinhado a 2 bytes)
"VBLK" <u32 len> data...
...
```
- Todo `u32 len` de chunk tem o bit `0x80000000` como flag "em uso"; tamanho
  real = `len & 0x7FFFFFFF`.
- **IndexEntry (14 bytes):** `u32 nameOffset` (0xFFFFFFFF = slot livre) ·
  `u32 dataBlockOffset` (aponta pro "VBLK") · `u32 fileSize` (tamanho
  **armazenado/comprimido**) · `u8 compressionType` (+0x0C) · `u8 flag` (0x01).

**Compressão** (confirmada no disassembly — `sub_4A0E50` faz switch sobre
`[entry+0x0C]`):

| Tipo | Rotina no EXE | Algoritmo |
|:----:|---------------|-----------|
| 0 | (bloco cru) | sem compressão |
| 1 | `sub_4AFA60` | provável RLE |
| 2 | `sub_434460` | provável LZ |
| 3 | `sub_433F80` | **LZHUF** (LZSS + Huffman adaptativo, Okumura 1989) |

O construtor do tipo 3 (`sub_433F80`) preenche `4096-60 = 0xFC4` posições de um
ring buffer com `' '` e inicia `r = 0xFC4` → parâmetros **N=4096, F=60,
THRESHOLD=2** (a assinatura clássica do LZHUF).

**LZHUF implementado** ✅ em [tools/lzhuf.py](tools/lzhuf.py). As tabelas de
decodificação de posição `d_code[256]`/`d_len[256]` foram **extraídas do binário**
(offsets de arquivo `0xE9700` e `0xE9800`) para garantir decode byte-exato — não
reconstruídas de memória. Os 10 `.txt` do `SHEETS.VOL` descomprimem para texto
tab-separado limpo (tabelas de balanceamento: `building.txt`, `vehicles.txt`,
`weapons.txt`, `mines.txt`, `morale.txt`, `space.txt`, `edentek.txt`,
`ply_tek.txt`, `multitek.txt`, `tutortek.txt`).

> Nota: o `.VOL` não armazena o tamanho descomprimido. O engine trata o bloco
> como um *stream* e decodifica sob demanda até os bits acabarem; o parser de
> planilhas para na primeira linha inválida. Nosso decoder reproduz isso fielmente
> — a saída pode ter alguns bytes de "lixo" no fim (bits de padding do último
> byte), que serão ignorados ao parsear as tabelas, tal como no jogo original.

**Conteúdo dos 5 volumes (437 arquivos):**

| Volume | Arquivos | Compressão | Conteúdo |
|--------|:--------:|------------|----------|
| `MAPS.VOL` | 71 | cru | `.map` (mapas) + `.bmp` |
| `SOUND.VOL` | 97 | cru | `.wav` (efeitos) |
| `STORY.VOL` | 124 | cru | `.rtf` (briefings/narrativa) |
| `VOICES.VOL` | 135 | cru | `.wav` (vozes, ex.: `savant*.wav`) |
| `SHEETS.VOL` | 10 | **LZHUF** | `.txt` (tabelas de balanceamento) |

**Status:** ✅ **Formato `.VOL` 100% decodificado** — todos os 437 arquivos dos 5
volumes extraem e validam (crus e LZHUF). Uso: `python tools/vol.py` (scan) ou
`python tools/vol.py extract <VOL> <saida/>`. Parser: [tools/vol.py](tools/vol.py);
decoder LZHUF: [tools/lzhuf.py](tools/lzhuf.py).

---

## 5.2. Formato `.map` — DECODIFICADO ✅ (2026-09-29)

Mapas de terreno do jogo (tag `0x00001011`). Parser em [tools/op2map.py](tools/op2map.py),
validado visualmente (render do grid mostra terreno coerente: mesas, lava, minério).

**Estrutura** (deduzida dos bytes + disassembly, validada em vários mapas):
```
Header (5 x u32 = 20 bytes):
  u32 tag        = 0x00001011
  u32 versionTag = 0
  u32 lgWidth    -> width = 1 << lgWidth   (ex.: 6 -> 64, 7 -> 128)
  u32 height     (em tiles)
  u32 numTileMappings  (ex.: 512 — usado na seção "TILE SET", ainda TODO)

TileData [width*height] (u32 cada) — LAYOUT EM FAIXAS (NÃO row-major):
  idx_u32 = ((x>>5) * height + y) * 32 + (x & 31)    (faixas verticais de 32 col)
  bits 0-4  : cellType  (passabilidade/terreno; 0-31)
  bits 5-15 : tileIndex (índice no pool global; máscara 0xFFE0 >> 5, 11 bits)
  bit  27   : flag por célula (lava/dirty), usado pelas rotinas de paint
  x dá wrap horizontal (widthMask = width-1); height é sempre potência de 2.

clipRect (4 x i32): x1, y1, x2, y2  (área jogável; eden01 = 32,0,95,62)

Tabela de tilesets: repete { u32 nameLen; char name[nameLen]; u32 numTiles }
  enquanto nameLen for plausível. Ex.: well0000..well0012 (13 fontes .bmp;
  soma de numTiles = pool global de tiles, ex.: 2012).

(Depois, em "TILE SET": tile mappings + grupos de terreno/animações — TODO.)
```

**Conteúdo do `MAPS.VOL`:** 54 mapas `.map` (dimensões 64×64, 128×64, etc.) +
alguns `.bmp` (paletas). Dimensões vêm do header; largura sempre potência de 2.

**Status:** ✅ header, grid de tiles (cellType + tileIndex), clipRect, tabela de
tilesets e render com gráficos — completos. **Mecanismo confirmado no disassembly**:
o acesso em faixa aparece idêntico em `GameMap::GetTile` (@191591) e nas rotinas de
paint `sub_4322C0`/`sub_432560` (`((x>>5)<<lgHeight + y)*128 + (x&31)*4` sobre
`dword_54F01C`). O render do port é **pixel-idêntico ao render original da wiki**
(eden01/eden02). Uso: `python tools/op2map.py eden01.map` ou
`outpost2_modern.exe --rendermap eden01.map out.bmp`.

**Seção "TILE SET"** (após a tabela de tilesets) — *entendida, não necessária para
renderizar*: uma **tabela de tile mappings** (`numTileMappings`, ex. 512, registros
`{u16 tileNumber, u16 tilesetIndex}` + índice) seguida de **grupos de terreno
nomeados** (`BLUE`, `mudbas1`, `mudcrk1`… = tipos de terreno / sequências de
animação de tile, ex. lava). O render de terreno funciona sem ela (o `tileIndex`
já aponta pro pool global); ela serve para animação de tile e tipagem de terreno
(polish de runtime do motor).

---

## 5.3. Tilesets `PBMP` + render de terreno — DECODIFICADO ✅ (2026-09-29)

Os gráficos de terreno (`well0000.bmp`..`well0012.bmp`) ficam **dentro do
`MAPS.VOL`** e usam o formato próprio **`PBMP`** (chunked, IFF-like). Carregados
em runtime via `ResManager::OpenStream("well####.bmp")` (busca em VOLs/soltos).

**Formato PBMP** — parser em [tools/pbmp.py](tools/pbmp.py):
```
"PBMP" u32 size
  "head" u32 len(20)  { u32 ver=2; u32 width; u32 height; u32 bpp=8; u32 ? }
  "PPAL" u32 len      (paleta)
      "head" u32 len(4)   { u32 numPalettes=1 }
      "data" u32 len(1024){ RGBQUAD[256]: b,g,r,a }
  "data" u32 len      { pixels 8-bit, width*height, top-down }
```
Cada tileset é uma coluna de tiles **32×32** empilhados (`height = numTiles*32`).
Também no `MAPS.VOL`: `op2_art.prt` (paleta da arte, chunks `CPAL`/`PPAL`) e
`virmask.raw` (máscara). O `OP2_ART.BMP` (raiz) é um BMP 8-bit 2732×2731.

**Render de terreno** — [tools/render_map.py](tools/render_map.py): junta a
pipeline completa `.VOL → .map → PBMP` e desenha o mapa. O `tileIndex` de cada
célula indexa o **pool global** (concatenação dos tiles de todos os tilesets do
mapa, na ordem listada). ✅ **Validado**: `eden01.map` renderiza como terreno
coerente (lava azul, rocha, vegetação, cânions). Uso:
`python tools/render_map.py eden01.map saida.png`.

**Status:** ✅ pipeline de terreno completa e funcional (decodificar + renderizar
qualquer um dos 54 mapas com gráficos originais). Isto cobre o essencial para o
motor desenhar o mundo.

---

## 5.4. Planilhas de dados `.txt` — DECODIFICADAS ✅ (2026-09-29)

Os 10 `.txt` do `SHEETS.VOL` (LZHUF) são as tabelas de dados/balanceamento do
jogo. Parser em [tools/sheets.py](tools/sheets.py). Dois formatos:

**Tabelas TSV** (1–2 linhas de cabeçalho + linhas separadas por TAB):

| Arquivo | Registros | Conteúdo |
|---------|:---------:|----------|
| `building.txt` | 42 | estruturas (custo, energia, HP, tamanho…) |
| `vehicles.txt` | 17 | veículos (HP, armadura, movimento, arma…) |
| `weapons.txt` | 29 | armas **e desastres** (EARTHQUAKE, METEOR… dão dano) |
| `mines.txt` | 9 | rendimento de minas (High/Med/Low × variante) |
| `morale.txt` | 72 | moral (estados + modificadores por evento; multi-seção) |
| `space.txt` | 23 | objetos da corrida espacial |

**Árvores de tecnologia** (blocos `BEGIN_TECH "Nome" <id> … END_TECH`):
`edentek` (135), `ply_tek` (137), `multitek` (137), `tutortek` (95) = **504 techs**.
Propriedades: CATEGORY, REQUIRES, COST, DESCRIPTION, TEASER, MAX_SCIENTISTS,
LAB, UNIT_PROP, EDEN_COST/PLYMOUTH_COST.

**Mecanismo confirmado no disassembly** (diretiva: sempre ir ao código):
o jogo lê linha a linha (512B), para em `0x1A` (Ctrl-Z) ou fim do stream, e
acessa registros **por nome** (não por contagem) — por isso o resíduo de padding
do LZHUF é inofensivo. Campos são separados por TAB; techs usam keywords com
comparação case-insensitive. Uso: `python tools/sheets.py`.

---

## 5.5. Sprites `op2_art.prt` + `OP2_ART.BMP` — DECODIFICADOS ✅ (2026-09-30)

Sistema de sprites do jogo (estruturas, unidades, UI). Parser em
[tools/artfile.py](tools/artfile.py), validado visualmente (estruturas da colônia
renderizam com cores corretas). Carregado no código por `sub_404360("op2_art")`.

- **`OP2_ART.BMP`** (raiz): atlas de pixels 8-bit, 2732×2731, sem compressão.
- **`op2_art.prt`** (dentro do `MAPS.VOL`): metadados. Estrutura:
```
"CPAL" u32 numPalettes(=9)
numPalettes x "PPAL" { "head"{u32=1}; "data"{ RGB[256]: r,g,b,flag } }
u32 numImages(=5390)
ImageMeta[numImages] (20 bytes cada):
   u32 scanLineByteWidth  (largura em bytes, múltiplo de 4)
   u32 pixelDataOffset    (offset nos pixels do OP2_ART.BMP)
   u32 height
   u32 width
   u16 type
   u16 paletteIndex
(depois: definições de animação/grupos de sprites — TODO)
```
As imagens são pixels 8-bit indexados, top-down, **empacotados sequencialmente**
no atlas: `offset[i+1] = offset[i] + scanLineByteWidth[i]*height[i]` (verificado).
A paleta é R,G,B (confirmado no loader: `byte0→vermelho` no empacotamento RGB565).
As **9 paletas são independentes** (não variações de cor de jogador) — cada imagem
declara a sua em `paletteIndex`; **renderizar sempre com a paleta própria da
imagem** (`m.pal`). Cor de jogador no OP2 é remapeamento em runtime, não uma das 9.

**Status:** ✅ **5390 imagens** extraíveis do atlas com paleta correta (estruturas,
unidades e UI renderizam certo). Uso: `python tools/artfile.py`.

**Animações** — parser em [tools/artanim.py](tools/artanim.py), validado por
**consumo exato** (contagens batem com o header) e por render (frames coerentes).
Estrutura (após o array de ImageMeta), confirmada no disassembly (`sub_404360`):
```
u32 numAnimations(2079); u32 numFrames(24185); u32 numPictures(160922); u32 numFrameExtra(6012)
Animation[numAnimations]:
   u32 header[9]  (header[8]=numFrames desta anim; header[3..6] ~ bounding box)
   Frame[numFrames]:
      u8 b1 -> numLayers = b1 & 0x7F ; flag1 = b1 & 0x80
      u8 b2 -> flag2 = b2 & 0x80
      if flag1: u16   ; if flag2: u16   (offset sombra/velocidade)
      Picture[numLayers] (uma "layer" do frame):
         u16 imageIndex ; u8 flag ; u8 slot(ordem) ; i16 x ; i16 y
   u32 extraCount ; ExtraRecord[extraCount] (4 x u32)
```
✅ **2079 animações, 24185 frames, 160922 layers** parseados; cada frame é uma
pilha de imagens do atlas posicionadas por (x,y), desenhadas na ordem `slot` e com
a paleta própria de cada imagem. Uso: `python tools/artanim.py`.

> A tabela final (~12KB, offset 0x178E56) **não é lida pelo loader de arte**: o
> disassembly mostra `sub_404360` liberando o stream e retornando logo após o loop
> de animações (`loc_404C49`). É dado vestigial ou de outro subsistema — registros
> ~8 bytes referenciando as últimas imagens do atlas em posições de grade.

---

---

## 5.6. Cheats digitados no chat — DECODIFICADOS ✅ (2026-11-06)

Os cheats de *Outpost 2* não são teclas: são **frases escritas na linha de
conversa**. O jogador digita, aperta Enter, e o pacote de chat (0x30) passa pelo
reconhecedor `sub_58642A` — que fica no segmento **DSEG** do `OUTPOST2.EXE`,
escrito à mão em assembly, fora do código gerado pelo compilador.

**As frases não existem no binário.** Só ficam guardados quatro **hashes** de
32 bits (VA 0x5863B8), e os quatro handlers são instalados em tempo de execução
por `sub_58650C` (um `rep movsd` de 4 dwords para 0x5863C8). É por isso que
procurar "Dan" ou "cheat" no executável não acha nada: o que achou o caminho foi
o literal `"Woof"`, que o código monta com imediatos (`mov eax, 666F6F57h`).

### O reconhecedor

`sub_58642A(texto, remetente)` anda no texto em grupos de 4 bytes. Por grupo o
hash é rodado à esquerda por `cl` — o nibble alto do byte baixo do grupo
**anterior** — e XORado com esse grupo anterior, uma vez por byte, parando no
primeiro zero; fechado o grupo, o byte baixo do hash leva um XOR com o 4º byte.
Ao achar o zero devolve na hora, **então os bytes depois do fim da frase nunca
entram na conta**: o hash depende só do texto.

Casando, o efeito é aplicado e o texto do chat é **substituído por "Woof Woof!"**
— que é o que todos leem, e a confirmação de que o cheat pegou.

Dois detalhes que importam:

- O remetente vem de `[pacote+0x0E]`, que `sub_4115F0` preenche com
  `dword_56DEE8` (o jogador local). **O cheat vale para quem digitou.**
- O reconhecedor roda **antes** do teste da máscara de destinatários
  (`loc_40FC96`), logo ele roda em **todas as máquinas**. O efeito é idêntico em
  todas e o lockstep da rede continua de pé — não é um buraco de sincronia.
- Não há porteiro: `TethysGame::CanAllowCheats` existe, mas **nunca é chamada
  dentro do executável** — é só exportada para as DLLs de missão. Os cheats
  funcionam em qualquer partida.
- A quinta entrada da tabela é zero e o `jcxz` depois do `repne scasd` a torna
  inalcançável **de propósito**: é o "nenhum casou" do laço, não um bug.

### As quatro frases e o que cada uma faz

| # | Hash | Frase | Efeito, lido do código |
|---|---|---|---|
| 0 | `FF27BC04` | `Dan's dog could make more ore.` | `sub_49E030`: os dois estoques vão para a capacidade (`ore = oreCapacity`, `rareOre = rareCapacity`) |
| 1 | `48D60158` | `Dan's car could hold more people.` | `sub_49E070`: trabalhadores **e** cientistas ganham **25%** (`v + v/4`, divisão truncada) |
| 2 | `AC0FF0E5` | `Dan's bike could backfire more often` | `sub_49E0D0`: a recarga (bloco +0x30) das **16** armas 0x3B..0x4A vira `min(recarga − 2, 20)` |
| 3 | `ABE0CC11` | **desconhecida** | `sub_49E120`: a penetração (bloco +0x2C) das **14** armas 0x3D..0x4A vira `max(pen × 33/32, 500)` |

**A pontuação faz parte do hash.** As duas primeiras exigem o ponto final; a
terceira **não pode** ter ponto. Errar a pontuação não faz nada — a frase vai
para o chat como conversa comum.

As duas últimas escrevem no mesmo bloco de 0x44 bytes que as melhorias de
pesquisa usam, então entram no jogo salvo e `reloadSheets()` as desfaz no começo
da partida seguinte. Repetir o cheat empilha o efeito.

As duas também põem `dword_4E0BD0 = 1`. Esse sinalizador tem **um** leitor no
original — `sub_431EB0`, um caminho de criação de unidade tipo 4 onde ele limpa
a flag 0x400 (danificável) da unidade criada. Não identifiquei essa classe, e
por isso o porte **não** tem o sinalizador: ele não teria leitor nenhum, e
inventar um consumidor seria pior que registrar a lacuna aqui.

### A quarta frase, e o extra do porte

A frase do índice 3 nunca foi publicada e **não dá para recuperá-la do hash**:
tentei 66 mil candidatas no molde `Dan's <algo> could <verbo> <algo>` e nenhuma
casou. O hash `ABE0CC11` fica na tabela do porte: se alguém descobrir a frase,
ela funciona, sem mudar uma linha.

Para que o quarto efeito tenha uma frase utilizável, o porte acrescenta **uma
entrada extra que o original não tem**, apontando para o mesmo handler:

| Hash | Frase | |
|---|---|---|
| `9AC1C27E` | `Dan's gun could die more people.` | **extra do porte** |

### O que NÃO existe (alegações conferidas e descartadas)

Listas de cheats que circulam na internet trazem coisas que o código desmente.
Conferido em 2026-11-06:

- **`Dan's dog could make more ore` sem o ponto** não "adiciona sete
  trabalhadores e três cientistas": o hash dela é `86352271`, que **não está na
  tabela**. Ela não faz nada.
- **`Ctrl+F4` não aumenta a população.** Os únicos escritores de trabalhadores
  (`dword_56E370`) e cientistas (`dword_56E374`) no executável inteiro são o
  handler de `GameOpt` (`sub_4229F0`), a API exportada
  `_Player::SetWorkers`/`SetScientists`, e o cheat 1. Não há atalho de teclado.
- A descrição "aumenta a capacidade do carro do Dan" para o cheat 1 é leitura
  literal da piada: o que ele faz é somar 25% à população.

### A linha de conversa abre em jogo solo (consertado em 2026-11-06)

Achado logo depois, conferindo o caminho até aqui: o porte só abria a linha de
conversa com o Enter **em partida multijogador**, e com isso os cheats ficavam
inalcançáveis justamente onde servem. O filtro do original (`sub_4115F0`) não
testa o tipo de jogo: trata o `WM_CHAR` 0x0D com a linha fechada e abre, ponto.
O porteiro saiu.

O `--chattest` **devolvia 0 sempre**, mesmo imprimindo `FALHOU` — foi por isso
que a regressão parecia limpa com a linha morta em jogo solo. Agora devolve o
código certo e tem um caso novo: digitar `Dan's dog could make more ore.` na
linha, em jogo solo, e ver o estoque encher — o caminho inteiro, da tecla ao
`sub_58642A`, e não só o hash.

### A outra via: a API de cheat das missões (ainda não portada)

Separado das frases, o `OUTPOST2.EXE` exporta uma API de cheat para as DLLs de
missão, por cima do pacote **0x2B** (`GameOpt`) via
`TethysGame::sIssueOptPacket(indice, valor)`. Os oito primeiros índices caem no
caso padrão de `sub_4229F0` e viram dwords no objeto `dword_56DE58`:

| Índice | Variável | Função exportada | O que faz |
|---|---|---|---|
| 0 | `56DE58` | `SetCheatUnlimitedResources` | não desconta recursos |
| 1 | `56DE5C` | `SetCheatProduceAll` | produção ignora a pesquisa (`sub_472C50`) |
| 2 | `56DE60` | — | overlay de depuração ("Srpls") |
| 3 | `56DE64` | — | dano × 4 no cálculo de `sub_433...` |
| 4 | `56DE68` | `SetCheatFastUnits` | pula o custo de terreno do movimento |
| 5 | `56DE6C` | `SetCheatFastProduction` | timer de produção = 1 tique |
| 6 | `56DE70` | — | não identificado |
| 7 | `56DE74` | — | desliga `sub_43A330` em `sub_44CD10` |

**Nenhuma das 94 DLLs de missão originais importa essas funções** — é por isso
que elas nunca fizeram falta para a conta de "222 de 222 símbolos". Os outros
índices do mesmo pacote (morál forçada, luz do dia) **estão** portados, em
`colonyForceMorale` e `TethysGame::SetDaylight*`. Portar as cinco de cheat é
trabalho em aberto, útil só para quem escrever missões novas.

Teste: `--cheattest <DLL>`, dezenove casos — os quatro hashes contra os valores
gravados no executável, o efeito de cada cheat (inclusive o teto de 20, o piso
de 500 e a EMP ficando de fora da faixa) e cinco controles negativos de
pontuação.

---

## 5.7. A aba Options (`GAME OPTIONS`) — CONSERTADA ✅ (2026-11-06)

Relatado pelo usuário: *"todos os menus do game options: Restart Mission, End
Mission, Exit Game, etc estão sem função"*. Estava certo, e eram **dois
defeitos independentes** — mais três divergências de fidelidade que apareceram
ao conferir `sub_465830` e `sub_457880` no disassembly.

### Defeito 1: `L&ESS` e `&MORE` com o id de outro comando

Os três ids `kCmdOptLess`, `kCmdOptMore` e `kCmdOptBack` valiam −20, −21 e −22
— exatamente os de `kCmdLabsCurrent`, `kCmdResStructures` e `kCmdResSummary`.
Em `clickCommand` os tratadores dos relatórios vêm **antes** e saem com
`return` incondicional (`if (cmd == kCmdResStructures) { if (m_tab == 2) ...;
return; }`), então na aba Options o clique morria ali. Os dois botões do
`PREFERENCES` não faziam nada. Agora são −70, −71 e −72.

As alternâncias ("Play Music", "Display Vehicle Shadows", ...) sempre
funcionaram, porque mudam no clique da **lista**, não nesses botões — foi por
isso que o defeito passou batido.

### Defeito 2: a confirmação se fechava sozinha

Cada item da tela de fim de missão abre a confirmação `dword_564EA8` (os casos
3 a 7 de `sub_457880`), e é o `&YES` que executa o caso 8..0xC
correspondente. No porte ela abria e **morria no quadro seguinte**, por três
motivos somados:

- `ConfirmPane::update` fechava a confirmação sempre que a seleção não fosse
  exatamente a unidade que a abriu. Em `sub_45C1A0` esse auto-fecho tem uma
  exceção que faltava: `cmp eax, offset unk_565AC8` — **se quem a abriu foi o
  painel de Game Options, ela não depende de seleção nenhuma**. Sem nada
  selecionado (o caso normal nessa tela), ela fechava no quadro seguinte.
- os botões `&YES`/`&NO` (`sub_45C200`) nunca eram montados, porque o ramo da
  aba 5 devolvia os botões da própria tela e saía antes;
- e o desenho mostrava o painel de opções em vez da mensagem (`sub_45C0B0`).

No original a confirmação **toma** o painel (`sub_45D020` troca o painel do
contentor) e o `&NO` devolve o painel que a abriu. É assim agora: a tela de fim
reaparece inteira ao desistir.

### Divergência A: a tela de fim de missão é UMA DE DUAS

`sub_465830` tem cinco modos, e o botão `E&ND MISSION` escolhe entre o 3 e o 4
pelo mesmo teste de `sub_4027D0`. O porte juntava os quatro itens numa tela só:

| | jogo solo (modo 3) | multijogador (modo 4) |
|---|---|---|
| itens | `RESTART MISSIO&N` (caso 8), `&BACK TO MAIN MENU` (9), `E&XIT OUTPOST 2` (0xA) | `SELF-&DESTRUCT` (0xB), `&TRANSFER` (0xC) |
| título | `Game End: Single Player` | `Game End: Multiplayer` |

Ou seja: **não se reinicia nem se volta ao menu numa partida em rede**, e **não
existe autodestruição em jogo solo**. O `&TRANSFER` ("&Transfer
vehicles/destroy structures") faltava por inteiro; ele nasce desabilitado a
menos que `sub_4909F0` ache outro jogador **humano** com o meu bit na máscara
de aliança — é para ele que os veículos vão.

### Divergência B: o título não é "End Mission"

São cinco, um por modo (`off_4E8B28`..`off_4E8B38`): "Game Options", "Mission
Objectives (Alt+O)", "Game Preferences", "Game End: Single Player" e "Game End:
Multiplayer". O porte escrevia "End Mission", que não existe no executável.

### Divergência C: o `BACK TO MAIN MENU` manda pacote

O original não encerra na mão: os casos 9, 0xB e 0xC chamam `sub_48A5D0`, que
monta o **pacote 0x31** com o método (`GoAI`, `SelfDestruct`, `Transfer`) e um
atraso — **4 na autodestruição e 2 nos outros dois**, detalhe que o porte
tratava como constante. E `applyQuit` tinha um `method != QUIT_GO_AI` a mais:
em `loc_40FD54` quem saiu encerra em **qualquer** método, com o código 6 em
jogo solo e 9 em rede.

### O teste

`--optmenutest <DLL>`: 27 casos em jogo solo e 29 em multijogador (roda com
`E04.DLL` e `MF2_01.DLL`). Cobre os dois botões do `PREFERENCES` separadamente,
a composição e o título das duas telas de fim, a confirmação sobrevivendo ao
quadro seguinte, a troca dos botões por `&YES`/`&NO`, o `&NO` devolvendo a
tela, o `&YES` do `E&XIT` pedindo o fim do programa, o `sub_4909F0` nos três
estados e o caminho inteiro do clique até o pacote 0x31, medindo o atraso.

Neutralizando cada peça, uma por vez: ids de volta **2** falhas (uma por
botão), auto-fecho da confirmação **7/6**, sem `&YES`/`&NO` **6/5**, tela de
fim sem o par solo/rede **0/10** (o esperado: em jogo solo a lista antiga já
era a certa), título **1/1**, `GoAI` sem encerrar **4/3** e atraso fixo
**0/1**.

---

## 6. Build e execução

**Toolchain:** CMake >= 3.15 + MSVC, C++17, SDL2 2.30.9 (em `third_party/`).
O build é **Win32 (x86)** — o carregador PE das missões executa código de 32
bits das DLLs originais, então x64 não serve.

```powershell
cmake -B build -S . -A Win32
cmake --build build --config Release
```

O passo de pós-build copia `outpost2_modern.exe` e `SDL2.dll` para a raiz do
jogo, que é de onde o executável tem de rodar (ele lê os `.VOL`, as DLLs de
missão e os recursos originais do diretório de trabalho).

> Se a cópia falhar com "Permission denied", o jogo está aberto — feche-o e
> refaça o build. Filtrar a saída do build por `"error C"` **esconde** essa
> falha; filtre por `error|denied`.

### Testes por linha de comando

São 96 modos. Os que cobrem regressão de verdade:

| Modo | O que prova |
|---|---|
| `--savetest <DLL> <ticks> <mais>` | determinismo: salvar, recarregar e continuar tem de dar o mesmo hash |
| `--mplan host\|join <DLL> <ticks>` | lockstep em rede: as duas máquinas fecham com o mesmo hash |
| `--mptest <DLL>` | missão multijogador local |
| `--mpui host\|join` | o fluxo das telas de rede, pelo despacho de verdade dos botões |
| `--uitest` | combos e barras de rolagem, pelo caminho do mouse e do teclado |
| `--chattest <DLL> [tela]` | a linha de conversa: mandar, desistir, a máscara de destinatários e um cheat digitado |
| `--transfertest <DLL>` | o TRANSFER: escolhedor, desistência e a troca de dono |
| `--idletest <DLL>` | o aviso de perder metal ao desligar armazém/fundição, nos dois lados da fronteira |
| `--garagecounttest <DLL>` | os seis pontos em que um veículo guardado em garagem tem de contar |
| `--alerttest <DLL>` | a unidade atacada alerta os vizinhos armados do próprio lado (`sub_44DE10`) |
| `--deletetest <DLL>` | grupo de IA com prazo vencido: autodestruição e, 400 ticks depois, sair do mapa |
| `--cheattest <DLL>` | os cheats do chat: os quatro hashes do executável e o efeito de cada um |
| `--item21test <DLL>` | as vozes da plataforma, a arma no ícone do STORAGE e o anel anti-duplicata |
| `--optmenutest <DLL>` | a aba Options: os botões do PREFERENCES e a tela de fim de missão inteira |
| `--missiontest <DLL> <saida> <ticks>` | carrega e roda qualquer missão |
| `--aitest <DLL campanha> <DLL multi>` | o `AILevel` chega ao jogador 0 e o multijogador fica intacto |
| `--helptest [tópico]` | o leitor do `.HLP` e a navegação da tela do manual |
| `--demotest` | o rodízio dos `ademo`, o `GameStartInfo` do demo e a interrupção por tecla |
| `--salvagetest`, `--oretest`, `--earthtest`, `--combattest`, `--researchtest`, `--garagetest`, `--gptest`, `--pinwheeltest` | cada subsistema |
| `--screenshot` + `--menu`/`--prefs`/`--mpsettings`/`--panetest`/... | captura de telas para conferência visual |

---

## 7. Resumo por área

| Área | Estado |
|---|---|
| Infra (SDL2, framebuffer, entrada, escala) | Completo |
| Leitura dos originais (PE, `.VOL`/LZHUF, PBMP, sprites, planilhas) | Completo |
| Terreno e mapa | Completo (render pixel-idêntico ao original) |
| Unidades, movimento, pathfinder | Completo |
| Estruturas e construção | Completo |
| Economia, comida, população, moral | Completo |
| Combate | Completo |
| Pesquisa | Completo |
| Desastres, lava e praga | Completo |
| Triggers, objetivos, vitória/derrota | Completo |
| IA (grupos, computador padrão, Pinwheel) | Completo |
| Campanha, colônia, tutoriais | Completo |
| Shell e telas | Completo |
| Manual (OUTPOST2.HLP) e demo de atração | Completo |
| HUD, abas, minimapa, globo | Completo |
| Áudio e música | Completo |
| Salvar/carregar | Completo |
| Multijogador em rede | Completo: até 6 jogadores, conversa, aliança, troca e estatísticas |
| Filmes | Não aplicável (arquivos não existem) |

---

## 8. Notas técnicas úteis

- **Imagebase do EXE original:** `0x400000`. Compilado com Visual C++, timestamp
  Aug 09 1997. PDB original: `C:\Tethys\release\Outpost2.pdb` ("Tethys" era o
  codinome/mundo do jogo).
- **Seções do EXE:** `.text` 842 KB de código, mais `.rdata/.data/.rsrc/.reloc`.
- **`.i64`** é a database do IDA Pro — abra nele para navegar o disassembly com os
  nomes/comentários já atribuídos, em vez de reprocessar o `.asm` cru.
- O port **não modifica** nenhum arquivo original de 1997; apenas os lê.
```
