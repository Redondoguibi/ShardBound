# Etapa 13 — direção do jogo e pesquisa de referência

Decisões do autor e pesquisa pública consolidadas em 2026-10-01. Este documento
registra a direção da etapa 13; o primeiro marco de assets/materiais está
implementado, mas os demais sistemas abaixo continuam como planejamento.
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
- Ativações são preservadas após morrer e ao salvar/carregar o jogo.
- Viagem rápida é gratuita e instantânea, quando permitida.
- Viagem rápida fica bloqueada durante combate: exige **6 segundos sem
  causar ou receber dano**. Um novo dano causado ou recebido reinicia a janela.
- O sistema de minecarts de MCD II é uma referência de experiência, não uma
  exigência de representar as waystones como veículos ou trilhos.

Raio de ativação e casos de dano especial ainda precisam de contrato técnico.

### Combate, itens e morte

- Arma inicial: espada de ferro.
- Diversidade de armas e itens, com estatísticas e atributos diferentes.
- Itens possuem níveis numéricos e raridade.
- Padrões de ataques corpo a corpo variam conforme a arma equipada.
- Ataques durante a queda são críticos, com multiplicador de **1,7x**.
- Movimento no PC é **somente WASD**, conforme decisão do autor que refina
  a referência anterior. Não implementar movimento por clique.
- Câmera segue a referência MCD II; parâmetros e outros bindings ainda
  precisam de contrato concreto.
- Ao morrer, o jogador retorna ao hub e perde **15% do ouro**.
- A perda é arredondada para baixo: `floor(ouro * 0.15)`, descontada uma vez
  por morte. O jogador renasce com toda a vida, mantendo itens e equipamentos.
- Ouro é a moeda definida para ShardBound.
- O fim do jogo não será produzido agora.

Slots finais, raridades, demais fórmulas, loot e balanceamento continuam abertos. Não copiar uma
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

### Controles no PC: pesquisa e decisão própria

[Relatos diretos de jogadores no Steam](https://steamcommunity.com/app/1912410/discussions/0/581683736959055311/)
descrevem movimento/ataque por clique e ausência de WASD no lançamento. São
evidência de usuários, não uma tabela oficial de bindings ou teste realizado
nesta pesquisa. Guias de terceiros encontrados se contradizem sobre WASD e
botões; não são base suficiente para congelar o contrato.

Após a pesquisa, o autor escolheu somente WASD para movimento no PC.
Essa decisão prevalece sobre a referência. O controlador do sandbox continua
provisório: seleção de alvos, separação entre atacar/interagir, controles de
gamepad e remapeamento ainda precisam de especificação.

### Ataque aéreo e crítico

A documentação oficial confirma ataque durante salto. Ela não estabelece
que todo ataque aéreo seja um crítico garantido, nem o multiplicador ou a
fase do salto em que o acerto ocorre. O autor definiu para ShardBound crítico
durante a queda com multiplicador de 1,7x; trata-se de decisão própria,
não de uma fórmula confirmada para MCD II.
Não presumir regras do Minecraft convencional ou do primeiro Dungeons.

## Proposta de execução modular

Os marcos abaixo são proposta técnica, não decisões criativas adicionais.

1. **Contratos e movimento:** especificar mundo/áreas, câmera, controlador,
   salto, movimento WASD/seleção e rig. Completar as capacidades necessárias de
   física e animação nas APIs próprias existentes.
2. **Exploração:** um hub e um trecho de floresta conectados, com circulação
   livre. Waystones, mapa e bloqueio de viagem por dano; sem conclusão de
   missão obrigatória para entrar ou sair da floresta.
3. **Combate:** espada de ferro, um inimigo inicial e ataques legíveis com
   colisão, animação, som e VFX; implementar crítico de queda de 1,7x. Separar padrão
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

## Primeiro lote sugerido de assets do autor

Brief de protótipo, não congelamento da escala/rig oficiais. Prioridade:

1. **Espada de ferro:** lâmina, guarda e cabo legíveis, geometria simples,
   textura pixel art própria e origem no ponto de empunhadura.
2. **Waystone:** marco de pedra reconhecível na floresta e no hub; origem no
   centro da base. Preparar duas aparências de textura (inativa/ativa), sem
   depender de transparência, emissive ou partículas para distingui-las.
3. **Uma árvore:** tronco e copa blocados, origem no centro da base; usar
   volumes opacos na primeira versão em vez de folhas com recorte alpha.

Entregar o arquivo editável da ferramenta usada e uma exportação GLB com
texturas embutidas, UVs e normais, geometria triangulada e materiais OPAQUE ou
MASK (recorte implementado no primeiro marco). Manter os modelos estáticos,
sem skeleton/skin/clips: o importador ainda rejeita animações e BLEND.
As duas aparências da
waystone podem ser fornecidas como duas exportações GLB para o protótipo;
a troca de aparência ainda precisará de integração no gameplay.

Texturas de 32×32 ou 64×64 são sugestões iniciais, não limites obrigatórios.
Guardar PNGs/fontes editáveis para revisão. Não embutir iluminação final.
Escala relativa e pivôs devem ser consistentes e ajustáveis nos arquivos-fonte;
dimensões definitivas serão alinhadas ao template oficial antes da produção
em quantidade. Personagens, armaduras e animações aguardam o contrato de rig.

### Recebimento do primeiro lote

Inspeção dos arquivos fornecidos em `assets/core/models`, antes da extensão
de materiais do primeiro marco:

| Arquivo | Conteúdo encontrado | Integração |
| --- | --- | --- |
| `sword.glb` | 1 mesh, 20 triângulos, textura PNG embutida 32×32, UVs e normais, sem skin/animação. | Material `MASK` e `doubleSided`; ainda não compatível com o importador opaco v0. |
| `tree.glb` | 47 meshes, 564 triângulos, duas texturas PNG embutidas 64×64, UVs e normais, sem skin/animação. | Materiais `MASK` e `doubleSided`; ainda não compatíveis com o importador opaco v0. |
| `waystone.gltf` | 184 bytes; uma cena com um nó/grupo, sem meshes, buffers, materiais ou imagens. | Exportação sem geometria; solicitar o projeto editável do Blockbench para recuperar o modelo. |

As texturas dos dois GLBs contêm pixels totalmente transparentes. A inspeção
não determina se todas essas regiões são usadas pelas UVs; não alterar os
materiais para opacos sem avaliar o resultado. O suporte a alpha-test já faz
parte da direção de materiais, mas ainda precisa ser implementado e validado
antes da integração visual destes assets. Também avaliar faces duplas.

Os GLBs originais foram preservados. Esta conferência de estrutura e texturas
não equivale a validação no renderer, registro público dos assets ou aprovação
da escala/pivôs.

O autor forneceu depois `waystone.bbmodel` e `waystone.png`. A conversão
reproduzível com `python tools/export-waystone.py` gerou `waystone.glb`:
8 meshes, 288 triângulos e PNG 256×256 embutido. O GLB passou pelo
Khronos glTF Validator 2.0.0-dev.3.10 sem erros ou avisos. Uma importação
independente confirmou as oito partes; o autor confirmou o visual da waystone.
O script mantém coordenadas/pivôs em unidades de autoria, sem congelar a escala
final do jogo. Ele atende este projeto estático de meshes; rejeita formatos,
hierarquias ou rotações que ainda não suporta, antes de gravar a saída.
O antigo `waystone.gltf` vazio permanece apenas como arquivo local do autor;
o recurso utilizável é `waystone.glb`.

## Primeiro marco executável — assets e materiais

O importador, o cache v2 e o renderer agora suportam MASK e faces duplas.
Espada, árvore e waystone ficam disponíveis pelos IDs públicos `core:sword`,
`core:tree` e `core:waystone`. A cena em
[examples/asset-gallery](../examples/asset-gallery/README.md) cria os três
objetos por script e os exibe com escalas provisórias de apresentação.

Esse marco não implementa ainda movimento definitivo, salto, hub/floresta,
waystones funcionais ou combate. O autor não precisa produzir mais assets
para o próximo avanço técnico; os arquivos editáveis da espada e árvore
continuam úteis para ajustes futuros.
