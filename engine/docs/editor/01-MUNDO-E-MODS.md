# Mundo e mods

**Atualizado em 17 de novembro de 2026.** Veja também o [guia de partidas com mapas](../27-PROCEDURAL-WORLDS.md).

## Implementação disponível

A instalação começa sem mods, blocos, mapas ou personagens. Uma partida precisa de um mapa e de um mod que crie o jogador. O formato admite mapas de até 512 × 512 colunas e 48 camadas. Cada partida guarda uma cópia do mapa usado.

O catálogo carrega blocos declarados em `world.json`, inclusive definições de mods que só registram blocos. Uma estrutura só vira terreno onde for colocada. Mods podem adicionar colocações pelo campo `map_placements` em `world.json`, com entradas `[id, x, y, z, rotação]` e rotação em passos de 90 graus.

O campo `map_overrides` aceita o caminho do arquivo de mapa ou um objeto com `source` e `baked_placements`. O editor usa a segunda forma para publicar o terreno já editado e declara os mods de colocação como dependências. Colocações de mods adicionados depois continuam sendo aplicadas pelo jogo.

Use **Interagir** (E por padrão) junto a uma porta vinculada. O save guarda a posição do jogador e as edições de terreno também dentro da sala.

Em `shardbound-map` versão 1, cada entrada de `interiors` declara `entrance` e `return` no mundo exterior e contém o `map` da sala. O mapa da sala usa `spawn` como ponto de chegada e `exit` como saída. Todos esses pontos são pares `[x,z]`. As portas exigem piso sólido e espaço livre para chegada e saída. O limite atual é de 32 interiores diretos.

## Um mundo finito

O ShardBound terá um único mundo jogável, finito e composto por biomas conectados. O jogador atravessa os biomas a pé, sem tela de carregamento entre eles. O mundo interconectado de Minecraft Dungeons II é a referência para a experiência de exploração. [Xbox Wire](https://news.xbox.com/en-us/2026/09/28/how-minecraft-dungeons-iis-interconnected-world-makes-every-journey-an-adventure/)

Registrar uma estrutura adiciona uma definição reutilizável à biblioteca do editor. Cada colocação declara a estrutura, a posição e a orientação. Um castelo cadastrado aparece no jogo somente onde o autor configurar sua colocação, inclusive quando vier de um mod.

Uma porta colocada no mundo pode levar a um interior separado, como uma sala, uma caverna ou uma fortaleza. O autor liga a porta à sala e define os pontos de chegada e retorno. Entrar pela porta abre o interior definido pelo autor. A entrada por si só não troca o layout nem reinicia a sala. A saída devolve o jogador à porta correspondente.

## Catálogo de blocos

- O jogo e o construtor de mapas leem o catálogo completo de blocos registrados pelos mods ativos no projeto, inclusive dependências. Um bloco válido aparece na paleta mesmo antes de ser usado em qualquer chunk ou estrutura.
- O autor pode buscar blocos pelo nome ou ID `namespace:path`, selecioná-los e usá-los no terreno e nas estruturas. O mapa salva os IDs dos blocos. O jogo resolve esses IDs no mesmo catálogo, com as propriedades declaradas pelo conteúdo, como textura, material e colisão.
- Criar ou editar um bloco no editor atualiza o catálogo do projeto. Exportar o mod leva as definições próprias e declara as dependências de blocos externos. Uma referência ausente ou inválida é exibida com o ID, o mod de origem quando conhecido e o local de uso. O teste no jogo e a exportação publicável exigem que essas referências sejam resolvidas.

## Direção técnica e aceite

Reutilizar o carregamento de definições dos mods, a malha, a colisão e o streaming por chunks do runtime. A fonte de terreno finito fornece limites e voxels armazenados no mapa. Usar formato versionado para mapa, estruturas, colocações, interiores e pontos de entrada. Editor, jogo e `modcheck` compartilham a resolução do catálogo e a validação das referências.

Validar com dois biomas contíguos, uma porta com interior e um bloco registrado por um mod dependente que começa sem uso. O bloco deve aparecer na paleta, ser colocado no mapa e funcionar no jogo após exportar. O castelo de teste aparece apenas depois de colocado. Salvar dentro do interior, reabrir a partida e sair pela porta correspondente.
