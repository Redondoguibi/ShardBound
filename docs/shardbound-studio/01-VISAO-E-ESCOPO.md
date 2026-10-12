# 01 — Visão, escopo e experiência do ShardBound Studio

## 1. Visão

**Definido:** criar um plugin especializado do Blockbench para construir conteúdo utilizável no ShardBound. Ele deve manter o processo de criação familiar para quem conhece **Minecraft + Blockbench + GeckoLib**; não obrigar a aprender um sistema de modelagem/animação diferente.

A meta é que criadores possam **modelar, texturizar, animar, incluir efeitos e exportar** um recurso para o jogo ou para um mod. O conteúdo exportado deve seguir os contratos oficiais do ShardBound. O plugin não substitui o runtime de animação/VFX e não precisa ser uma engine de jogo.

### O que significa "igualzinho ao GeckoLib"

- **Definido, na experiência de autoria:** árvore de pastas/grupos e cubos, pivôs, editor UV/painting do Blockbench, timeline, keyframes e clips; grupos podem ser animados.
- **Definido:** não introduzir no editor um fluxo obrigatório de esqueleto separado, skinning por influência de vértices ou pintura de pesos.
- **Proposta:** reutilizar o editor nativo e o sistema de Animate Effects do Blockbench em vez de recriar visualmente todas as ferramentas.
- **Não prometido:** compatibilidade binária com todos os arquivos/recursos do GeckoLib, igualdade de APIs, redistribuição de código ou de assets de terceiros, ou equivalência absoluta de cada menu.

## 2. Conteúdos que o plugin deverá atender

| Tipo | Requisito confirmado | Ferramentas específicas propostas |
| --- | --- | --- |
| Skin do player | Criar/editar skins | Template do jogador, pintura, UV e pré-visualização no humanoide |
| Item genérico | Criar modelos de itens | Poses de exibição, escala e ponto de encaixe |
| Arma corpo a corpo | Criar e animar armas melee | Empunhadura, animações e marcos visuais do golpe |
| Arma de longa distância | Criar e animar armas ranged | Pontos de origem de projéteis, animações de uso, recarga e efeitos |
| Bloco | Criar blocos personalizados | Orientação, possíveis variantes e referência de colisão |
| Armadura | Criar peças que acompanhem o jogador | Templates por parte do corpo, encaixes e pré-visualização |
| Monstro | Criar mobs/criaturas animados | Grupos animáveis, clips e efeitos vinculados |
| Partículas/VFX | Criar efeitos visuais | Editor de parâmetros, texturas, emissores e preview |

**Definido:** todos os tipos devem contemplar texturas; animações devem estar disponíveis sempre que fizerem sentido (não necessariamente obrigatórias para blocos/skins).

**Distinção técnica:** uma arma visual e o comportamento de combate da arma não são a mesma coisa. Dano, cooldown, projétil, colisão/hitbox e efeitos de gameplay continuam dependentes das definições e dos sistemas do ShardBound. O plugin pode facilitar sua declaração, mas não define sozinho a lógica do jogo.

## 3. Fluxo de autoria desejado

1. Escolher um template de conteúdo do ShardBound no Blockbench.
2. Modelar com cubos/cuboids e organizar em pastas/grupos.
3. Pintar texturas e configurar UVs, preferencialmente em baixa resolução/pixel art.
4. Criar clips com keyframes para grupos na timeline.
5. No mesmo projeto e sessão, criar/selecionar partículas e associá-las a frames, com sons e eventos quando desejado.
6. Validar escala, IDs, referências e compatibilidade.
7. Exportar o conjunto para o ShardBound, sem separar manualmente texturas do modelo.

**Proposta:** o projeto editável permanece em `.bbmodel`; a exportação gera um `.glb` de modelo/texturas/animações e metadados associados. Arquivos, nomes e botão de exportação ainda exigem contrato.

## 4. Requisitos de experiência do usuário

- Familiaridade com o Outliner do Blockbench: grupos, subgrupos, cubos e pivôs.
- A timeline visual não deve exigir escrever código para declarar um efeito em certo frame.
- Ao animar uma espada, por exemplo, o autor precisa poder adicionar partículas a **essa mesma animação**, sem abrir um projeto desconectado.
- Presets/templates para reduzir erros repetitivos ao criar mods.
- Mensagens de erro claras para referências/formatos não suportados.
- Pré-visualização da animação e, idealmente, dos efeitos antes da exportação (**preview VFX proposto**, não confirmado tecnicamente).
- Exportação reprodutível, sem alterações inesperadas nos originais.

## 5. Direção artística e integração

- **Definido no ShardBound:** modelos predominantemente cuboides, proporções blocadas e texturas pixel-art.
- **Definido no ShardBound:** GLB/GLTF como formatos-fonte e preferência por textura embutida em GLB.
- **Definido no ShardBound:** identificação pública de assets por `namespace:path`, com declaração explícita no sistema de mods.
- **Alinhamento:** não importar texturas ou outros assets proprietários de Minecraft/Minecraft Dungeons como conteúdo oficial.
- **Proposta:** nomes como "ShardBound Studio", "ShardBound Model" e "Particle Editor" são de trabalho, não marca final confirmada.

## 6. Fora do escopo já confirmado

Não foi solicitado: editor de mapas completo, lógica de IA de monstros dentro do Blockbench, linguagem de programação visual completa ou substituição do sistema `.sl`. Esses recursos não devem ser presumidos.
