# Itens, armaduras e animações

**Estado de 13 de novembro de 2026.** Requisitos de autoria visual para a primeira versão. O fluxo completo vai da definição do equipamento ao teste no jogo.

## Implementação disponível

Em **Equipamentos**, clique em **Criar arma** ou escolha **Cabeça**, **Torso**, **Pernas** ou **Pés**. A aba abre o Blockbench com um modelo inicial texturizado sobre o jogador de referência. Modele, pinte e use **Salvar na engine** ou **Ctrl+S**. O equipamento aparece na lista da revisão atual. Selecione-o e clique em **Editar selecionado** para reabrir o projeto e salvar outra versão com o mesmo ID. Salve também o mapa para conservar essa revisão ao reabrir.

Em armaduras, **Atributos** permite definir proteção, velocidade e vida máxima. As pastas da peça já vêm encaixadas nos grupos do corpo. A armadura acompanha as poses do jogador em jogo e o editor abre a peça sem clipes próprios.

Em armas, **Atributos → Golpes e animação** permite ordenar os golpes de chão e aéreos e configurar dano, alcance, largura, forma, tempo até acerto e recuperação de cada um. **Animar clipe** abre o idle ou o golpe escolhido na linha do tempo do Blockbench, com a arma na mão do jogador de referência. **Ver área de acerto** mostra a forma e o alcance vistos de cima. O salvamento publica os atributos junto com o modelo e os clipes. Com **Disponível no inventário ao começar**, use **Testar no jogo** no mapa para experimentar o equipamento em uma nova partida.

## Escopo planejado para a autoria visual

A prévia atual da área de acerto é independente da animação. O tempo até acerto é configurado em segundos no editor de equipamento. A sobreposição do volume na animação e a edição de uma janela de acerto na linha do tempo ficam no escopo abaixo.

### Criar um item ou uma armadura

- Criar e configurar conteúdo pela interface visual. Escolher tipo, modelo, textura, encaixe e atributos. Para armas, configurar dano, velocidade de ataque e demais atributos de combate. Para armaduras, configurar slot, proteção e bônus, com prévia de como a peça encaixa no corpo do jogador. O autor conclui esse fluxo pela interface e pelo Blockbench integrado, sem precisar escrever código.
- Editar visualmente o **volume que acerta o alvo**. Exibir formato, tamanho, orientação e alcance junto do modelo do jogador e do equipamento. Diferenciar a geometria visível do item da forma usada pelo combate. O autor consegue ver onde o golpe alcança enquanto ajusta e reproduz a animação.
- O editor registra e exporta as definições de equipamento e os vínculos com os assets. A autoria fica integrada ao catálogo de mods e aos atributos do ShardBound. Campos aplicáveis aparecem conforme o tipo de equipamento e as ações que o autor habilitar.

### Configurar e animar os golpes da arma

1. Selecionar a arma e ajustar seu encaixe no jogador de referência.
2. Definir a sequência de ataques no chão e a sequência disponível após pular. Em cada sequência, escolher a quantidade e a ordem dos golpes. Cada golpe herda os atributos base e pode mudar valores como dano, alcance, formato e velocidade em relação à base.
3. O editor mostra **uma entrada de animação para cada golpe configurado** e outra para o idle do jogador equipado. Selecionar uma entrada e abrir o contexto correspondente no Blockbench com o jogador e o equipamento visíveis. Animar o jogador com o equipamento e pré-visualizar o movimento do começo ao fim.
4. Marcar na linha do tempo do golpe o momento ou intervalo em que ele pode acertar. A prévia mostra o volume e os valores daquele golpe. Ao exportar, vincular cada animação ao golpe correspondente por ID estável, inclusive se o autor reordenar a sequência.
5. Testar no jogo com o equipamento colocado. Cada golpe usa sua animação e seus valores. O ataque iniciado no ar usa a sequência aérea configurada. Continuar aplicando as regras de combate do jogo, como o crítico durante a queda, conforme o runtime.

### Modelar e encaixar armaduras

O autor modela e pinta a armadura, configura os atributos e vincula cada peça ao grupo correspondente do corpo. A armadura não tem clipes de animação próprios. A prévia reproduz as animações do jogador para conferir o encaixe das peças em movimento. No jogo, cada peça acompanha o movimento do grupo ao qual foi vinculada. [Planejamento do ShardBound Studio](../shardbound-studio/02-MODELAGEM-TEXTURAS-E-ANIMACOES.md)

## Aceite

Criar uma espada com três golpes no chão e um após pular. Dar ao segundo golpe dano diferente e ao aéreo outro alcance. Animar o idle e os quatro golpes com a espada equipada, exportar e verificar no jogo os clipes e valores correspondentes. Reordenar dois golpes e confirmar que cada animação e seus atributos continuam vinculados ao golpe correto após salvar e reabrir. Criar uma armadura, conferir seus atributos e verificar que as peças acompanham o corpo no idle, no pulo e durante os ataques da espada.
