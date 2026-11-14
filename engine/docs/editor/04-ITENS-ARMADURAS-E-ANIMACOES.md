# Itens, armaduras e animações

**Estado de 13 de novembro de 2026.** Requisitos de autoria visual para a primeira versão. O fluxo completo vai da definição do equipamento ao teste no jogo.

## Implementação disponível

Em **Criar equipamento…**, escolha arma ou slot de armadura, configure o modelo e os atributos e publique o mod. Para armas, adicione golpes de chão e aéreos, ajuste a ordem e configure dano, alcance, largura, forma, tempo até acerto, recuperação e nome do clipe. Novos golpes recebem uma cópia dos atributos do primeiro golpe de chão e um clipe próprio por padrão. A prévia do volume mostra o alcance e a forma vistos de cima. Com o Studio ativo no Blockbench, use **Abrir golpe** ou **Abrir idle** para selecionar o clipe na linha do tempo do Blockbench. Exporte o pacote para vincular a animação ao equipamento.

A armadura recebe proteção, bônus de velocidade e bônus de vida máxima. As peças usam os encaixes por grupos do Studio e acompanham a animação do jogador. A opção de conceder o equipamento ao começar permite testá-lo em uma nova partida pelo inventário.

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
