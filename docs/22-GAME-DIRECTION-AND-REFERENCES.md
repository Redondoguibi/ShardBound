# Etapa 13 — direção do jogo e pesquisa de referência

Decisões do autor e pesquisa pública consolidadas em 2026-10-01. Este documento
prepara a etapa 13; não afirma que os sistemas abaixo já estejam implementados.
O estado executável permanece em [18-IMPLEMENTATION-STATUS.md](18-IMPLEMENTATION-STATUS.md).

## Prioridade das decisões

A referência principal é **Minecraft Dungeons II (Minecraft Dungeons 2)**,
especialmente exploração, combate, equipamentos e estética. As decisões
explícitas do autor sobre ShardBound prevalecem sobre comportamentos da referência.
Semelhança desejada não substitui contratos implementáveis nem transforma
informação não verificada em regra definitiva.

ShardBound permanece uma plataforma centrada em mods: a comunidade poderá
expandir conteúdo e possibilidades. Não há lore, campanha, fim ou sequência
obrigatória de progressão definidos. Isso não elimina níveis, raridades,
atributos e sistemas de evolução; suas regras precisam ser extensíveis.
Fontes e interfaces serão próprias. Assets oficiais também serão próprios,
conforme [17-ART-DIRECTION-MODELING.md](17-ART-DIRECTION-MODELING.md).

## Decisões definidas pelo autor

### Arte e hub

- Pixel art nos assets, modelos simples principalmente de cubos/cuboids;
  meshes de geometria mais livre serão exceções.
- Preservar a renderização 3D e as regras de
  [05-CONTENT-AND-ASSETS.md](05-CONTENT-AND-ASSETS.md): a imagem final não
  precisa ser deliberadamente pixelada.
- Hub como abrigo/aldeia segura, inspirado na experiência de MCD II.
- O hub terá uma waystone.
- Primeira região: floresta.

### Mundo e waystones

- Mundo aberto com liberdade de entrar e sair das regiões.
- Waystones encontradas durante exploração são ativadas por proximidade,
  sem exigir interação explícita.
- O mapa permite viajar para waystones já ativadas a partir de qualquer lugar.
- Viagem rápida fica bloqueada durante combate: exige **6 segundos sem
  causar ou receber dano**. Um novo dano causado ou recebido reinicia a janela.
- O sistema de minecarts de MCD II é uma referência de experiência, não uma
  exigência de representar as waystones como veículos ou trilhos.

Raio de ativação, custo, duração da viagem, conservação das ativações após
morte e casos de dano especial ainda não foram explicitamente fechados.
Não impor silenciosamente custos ou perdas de descobertas.

### Combate, itens e morte

- Arma inicial: espada de ferro.
- Diversidade de armas e itens, com estatísticas e atributos diferentes.
- Itens possuem níveis numéricos e raridade.
- Padrões de ataques corpo a corpo variam conforme a arma equipada.
- Há crítico relacionado a pular e atacar; janela, condição e multiplicador
  ainda precisam de contrato concreto.
- Câmera e controles devem seguir a referência MCD II; bindings e parâmetros
  exatos ainda precisam ser verificados antes de congelar a especificação.
- Ao morrer, o jogador retorna ao hub e perde **15% do ouro**.
- Ouro é a moeda definida para ShardBound.
- O fim do jogo não será produzido agora.

Arredondamento da perda de ouro, estado restaurado ao renascer, slots finais,
raridades, fórmulas, loot e balanceamento continuam abertos. Não copiar uma
campanha ou curva obrigatória de progressão como efeito colateral da referência.

## Pesquisa: evidência e limites

Fontes primárias foram priorizadas. Artigos com descrição de gameplay e
screenshots foram consultados; não houve sessão jogada de MCD II, análise
quadro a quadro dos trailers ou medição de tempos de animação/câmera.

| Fonte | Evidência utilizável | Limite para ShardBound |
| --- | --- | --- |
| [Novos sistemas de gameplay — Minecraft.net, 2026-08-31](https://www.minecraft.net/en-us/article/minecraft-dungeons-ii-gameplay-systems) | Regiões conectadas; salto e ataques aéreos; armadura dividida em quatro peças; talismãs; efeitos aleatórios; livros de encantamento e biblioteca de efeitos. | Não fornece fórmula de crítico, timings ou bindings completos. Esses sistemas são referências, não contratos já aprovados. |
| [Novidades da gamescom — Xbox Wire, 2026-08-26](https://news.xbox.com/en-us/2026/08/26/minecraft-dungeons-ii-new-features-gamescom-2026/) | Hub Brave Haven, minecarts, serviços de melhoria de equipamentos, feedback de ataques e consulta dos efeitos da build. | Serviços, menus e regras precisam de desenho próprio. |
| [Mundo interconectado — Xbox Wire, 2026-09-28](https://news.xbox.com/en-us/2026/09/28/how-minecraft-dungeons-iis-interconnected-world-makes-every-journey-an-adventure/) | Viagem rápida por estações/estátuas, retorno ao hub na morte e exploração fora de uma sequência de missões isoladas. | Não comprova a regra de 6 segundos; a perda de ouro de ShardBound vem do autor. |
| [Revelação de gameplay — Minecraft.net, 2026-06-07](https://www.minecraft.net/en-us/article/minecraft-dungeons-ii-date-announce) | Exemplos publicados de combate, inventário e variedade de equipamentos. | Imagens não bastam para deduzir controles ou valores de combate. |
| [Trailer de gameplay — Minecraft.net, 2026-08-20](https://www.minecraft.net/en-us/article/minecraft-dungeons-ii-gameplay-trailer) | Material oficial para futura comparação visual e de animações. | Trailer identificado; não tratado como medição de jogabilidade. |
| [Página oficial do jogo](https://www.minecraft.net/en-us/about-dungeons-ii) | Visão geral e links para materiais oficiais. | Copiar toda a lista de funcionalidades ampliaria o escopo além do roadmap atual. |

### Controles no PC: investigação ainda necessária

[Relatos diretos de jogadores no Steam](https://steamcommunity.com/app/1912410/discussions/0/581683736959055311/)
descrevem movimento/ataque por clique e ausência de WASD no lançamento. São
evidência de usuários, não uma tabela oficial de bindings ou teste realizado
nesta pesquisa. Guias de terceiros encontrados se contradizem sobre WASD e
botões; não são base suficiente para congelar o contrato.

O sandbox atual usa WASD, mas seu controlador provisório não define a escolha
final da etapa 13. Verificar movimento por clique, navegação ao destino,
seleção de alvos, separação entre mover/atacar/interagir, controles de gamepad
e regras de remapeamento antes de publicar a especificação definitiva.

### Ataque aéreo e crítico

A documentação oficial confirma ataque durante salto. Ela não estabelece
que todo ataque aéreo seja um crítico garantido, nem o multiplicador ou a
fase do salto em que o acerto ocorre. O requisito de crítico de ShardBound
permanece válido, mas precisa ser refinado com evidência ou decisão do autor.
Não presumir regras do Minecraft convencional ou do primeiro Dungeons.

## Proposta de execução modular

Os marcos abaixo são proposta técnica, não decisões criativas adicionais.

1. **Contratos e movimento:** especificar mundo/áreas, câmera, controlador,
   salto, navegação/seleção e rig. Completar as capacidades necessárias de
   física e animação nas APIs próprias existentes.
2. **Exploração:** um hub e um trecho de floresta conectados, com circulação
   livre. Waystones, mapa e bloqueio de viagem por dano; sem conclusão de
   missão obrigatória para entrar ou sair da floresta.
3. **Combate:** espada de ferro, um inimigo inicial e ataques legíveis com
   colisão, animação, som e VFX; definir ataque aéreo/crítico. Separar padrão
   de ataque, equipamento e regras de dano para permitir alteração por mods.
4. **Itens:** definir instâncias de itens, níveis, raridades, atributos e
   equipamento sem fixar campanha, curva de XP ou catálogo grande de conteúdo.
5. **Persistência:** morte e perda de ouro, localização e descobertas; integrar
   com saves versionados da etapa 12, migrações e preservação de conteúdo de
   mods ausentes. Publicar qualquer extensão do formato antes de utilizá-la.
6. **Validação:** jogar o ciclo hub/floresta/combate/viagem/morte/save;
   demonstrar extensão por mod e avaliar desempenho com conteúdo representativo.

Manter UI, mundo, combate, itens e persistência separados. Conteúdo oficial
usa registries/IDs e contratos de mods, sem regras especiais espalhadas pela
Application. Multiplayer permanece fora do escopo inicial documentado;
semelhança com a referência não o adiciona automaticamente.

## Critérios para a especificação executável

Antes de implementar cada sistema, documentar entradas, estados, eventos,
IDs, persistência, erros e pontos de extensão. Antes da produção definitiva
de assets, definir escala, pivôs, rig e requisitos de exportação.

Verificações planejadas para waystones incluem descoberta por proximidade,
destino indisponível antes da descoberta, viagem a partir de posição arbitrária,
bloqueio por dano causado e recebido, reinício da janela e liberação ao completar
6 segundos. A morte deve aplicar a perda uma única vez. Save/load e remoção de
mods devem preservar referências e estados conforme os contratos publicados.

O primeiro marco jogável será pequeno, mas deverá demonstrar liberdade de
exploração e extensibilidade. A Etapa 13 completa não será declarada concluída
apenas por existir esse marco ou por passarem os testes do sandbox.
