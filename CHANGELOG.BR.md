[English](CHANGELOG.EN.md) · **Português (BR)**

# Registro de mudanças

Mudanças deste fork desde que ele saiu de
[marcelomgoncalves/outpost2](https://github.com/marcelomgoncalves/outpost2)
no commit `1697e7f` (2026-11-06). As mais recentes primeiro.

## 2026-11-07

### Adicionado
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
