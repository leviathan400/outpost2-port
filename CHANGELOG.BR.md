[English](CHANGELOG.EN.md) · **Português (BR)**

# Registro de mudanças

Mudanças deste fork desde que ele saiu de
[marcelomgoncalves/outpost2](https://github.com/marcelomgoncalves/outpost2)
no commit `1697e7f` (2026-11-06). As mais recentes primeiro.

## 2026-11-08

### Corrigido
- **A vista do mapa parou de dar a volta em todo mapa.** O porte deixava a
  vista rolar sem fim para a esquerda e a direita em qualquer mapa,
  repetindo o mapa. O original só faz isso nos mapas "around the world" (512
  tiles de largura); em todos os outros a vista para nas bordas esquerda e
  direita do mapa, como em qualquer RTS. Agora esse é o padrão. Unidades,
  terreno e a câmera do modo demonstração seguem a mesma regra.
- **Plataforma da doca das minas e dos tanques.** As minas de minério comum
  e raro, o Magma Well e os dois tanques de armazenamento têm doca de
  caminhão, mas a plataforma não está na arte do prédio, então ela nunca
  aparecia. O original a desenha num passe separado de "camada de chão" por
  baixo de todas as unidades (slot 30 da vtable do prédio), que o porte não
  tinha. Esse passe agora está portado:
  - minas e Magma Well: a plataforma própria, azul-acinzentada, animação
    `0x747` (`sub_44B280`);
  - tanques: animação `0x881`, ou `0x882`/`0x883` quando danificados
    (`sub_409590` / `sub_409840`);
  - Agridome: animação `0x87E`, ou `0x87F`/`0x880` quando danificado
    (`sub_401970`). Substitui a plataforma posta à mão em 2026-11-07, que
    era igual mas não mostrava dano.

  Some enquanto o prédio está sendo construído ou destruído, como no
  original.

- **Os respiradouros de magma (Magma Vent) não apareciam.** O respiradouro
  onde a GeoCon constrói o Magma Well (unidade tipo `0x52`) nunca era
  desenhado. Ele faz parte do mesmo passe de camada de chão: animação `0x2A2`
  (`sub_4057F0`), animada, escurecida pelo ciclo dia/noite na mesma escala do terreno
  (a chave `NightDarkness` -- o escurecimento dos sprites usava a rampa
  inteira do original e a deixava mais escura que o chão em volta), e
  escondida quando há um Magma Well em cima.

### Alterado
- **Clicar com o botão direito numa doca sempre manda o caminhão até lá.** No
  original, o clique direito na doca de um prédio com um veículo que não pode
  usá-la -- por exemplo um caminhão carregado numa mina, que só aceita
  vazios -- era ignorado (`sub_451280` volta sem ordem), então o caminhão não
  saía do lugar. Agora ele vai até o tile da doca com um mover simples. Um
  veículo que pode usar a doca continua recebendo a ordem de atracar e
  carrega/descarrega como antes.

### Adicionado
- **Chave `MapWrap`** (seção `[Game]` do `OUTPOST2.INI`):
  - `MapWrap=0` (padrão): como no original (ver acima).
  - `MapWrap=1`: todo mapa dá a volta, o comportamento antigo do porte.
- `--dockclicktest <DLL> [tipo]`: tipo de prédio opcional (hex, padrão
  `27` = Agridome), por exemplo `15` / `16` / `2A` para as minas, `19` /
  `1A` para os tanques. Passa em todos no `t13` e no `t14`.

## 2026-11-07

### Corrigido
- **Mover com o botão direito errava o tile e nunca atracava.** O clique
  direito tirava o alvo das coordenadas da janela em vez das da vista do
  mapa, então as unidades iam para um ponto 12 px à direita e 22 px abaixo
  do clique — quase sempre o tile de baixo. E pulava a lógica de mover do
  original (`sub_451280`), então clicar na doca de um prédio nunca mandava a
  ordem de atracar. Agora usa as coordenadas da vista e o mesmo caminho do
  botão MOVE: clicar com o botão direito na doca, com um caminhão que pode
  usá-la, atraca. (Defeito do upstream, já presente em `1697e7f`.)
- **Doca do Agridome.** O Agridome tem doca de caminhão (`BUILDING.TXT`:
  Docking Truck = Y, tile inferior esquerdo), mas a arte dele não tem a
  plataforma, então os caminhões paravam em chão vazio. Agora ele ganha a
  mesma plataforma "X" que os outros prédios com doca trazem na arte
  (imagens 385–388, posicionadas exatamente como nos 10 grupos de Structure
  Factory / Smelter / Spaceport / GORF), como o OPU mostra. As duas facções;
  desenhada depois que o prédio fica pronto.
- **Capturas de teste depois da mudança de resolução.** Os modos `--*shot` /
  `--screenshot` / `--missiontest` salvavam uma imagem fixa de 640×480, que
  saía embaralhada nas capturas do jogo agora que a tela do jogo tem o
  tamanho da janela. Agora salvam no tamanho real do quadro.

### Adicionado
- `--dockclicktest <DLL>`: clica com o botão direito na doca de um Agridome
  com um caminhão, pelo caminho real de entrada, e confere que ele atraca;
  depois clica num tile livre e confere que ele chega exatamente lá. Falha
  no código antigo (o caminhão para um tile abaixo), passa no `t13` e `t14`.
- `--missiontest <DLL> <saida> [ticks] [camTileX camTileY]`: posição
  opcional da câmera para a captura.
- **Chave `MenuSkin`** (seção `[Game]` do `OUTPOST2.INI`) para escolher o
  estilo dos menus (`98c305b`):
  - `MenuSkin=0` (padrão): o visual do próprio porte, exatamente como o
    upstream desenha: painéis azul-escuros, menu principal translúcido à
    direita.
  - `MenuSkin=1`: a pele de diálogo do Outpost 2 original (ver abaixo).

  Conferido em 19 telas de menu: `0` é idêntico pixel a pixel ao build do
  upstream, e `1` ao build com a pele do OP2.
- **Pele de diálogo do OP2 original** em todas as telas de menu (`1038b5a`).
  Toda a arte é lida do próprio `OP2SHRES.DLL` do jogo:
  - **Janelas:** o fundo azul-marinho "blueprint" (bitmap 225, alinhado à
    tela como no original), borda vermelha e barra de título azul-marinho
    com o título em Arial negrito verde.
  - **Botões:** a arte vermelha de botão do jogo (bitmaps 235/236). O botão
    com foco ganha o contorno vermelho vivo. O texto é Arial negrito
    amarelo-esverdeado, com a letra de atalho sublinhada.
  - **Texto:** Arial verde; itens desabilitados em cinza.
  - **Listas:** pretas com borda clara, seleção azul com texto branco, barra
    de rolagem padrão e linhas de 14 px.
  - **Caixas de texto e listas suspensas:** pretas, com botão de seta.
  - **Botões de opção e caixas de seleção:** glifos clássicos.
  - **Menu principal:** o tamanho (192×348), a posição e os botões medidos
    do original (93% dos pixels iguais a uma captura do original).
  - Se os bitmaps não carregarem, todo controle volta ao estilo do porte.

### Alterado
- **A tela do jogo acompanha o tamanho da janela** (`9480d5d`). Antes era
  uma imagem fixa de 640×480 esticada até a janela, então uma janela maior
  só deixava tudo maior. Agora, como no original e no OPU, o jogo é
  desenhado no tamanho real da janela:
  - a coluna da direita (minimapa, abas, painel) mantém os 192 px de
    largura, presa à borda direita;
  - a vista do mapa ocupa o resto;
  - redimensionar a janela ou alternar a tela cheia refaz o layout;
  - os menus continuam em 640×480 escalados, como no original.
  - O diálogo de salvar/carregar e a caixa "Success" ficam centralizados.
  - A moldura da janela vai até a borda real, então as bordas inferior e
    direita não somem mais.
  - Diferença do original: a vista preenche o espaço exato em vez de
    arredondar para múltiplo de 32 px, para não sobrar faixa vazia.

## 2026-11-06

### Adicionado
- **Documentação em inglês** (`7ead70a`): `README.EN.md` e
  `PORT_STATUS.EN.md`. Os originais em português foram renomeados para
  `README.BR.md` e `PORT_STATUS.BR.md`, e o `README.md` virou uma página de
  entrada com links para os dois idiomas.
