# 01 — Visão, escopo e experiência do ShardBound Studio

## 1. Visão

**Definido:** criar um plugin especializado do Blockbench para construir conteúdo utilizável no ShardBound. Ele deve manter o processo de criação familiar para quem conhece **Minecraft + Blockbench + GeckoLib**; não obrigar a aprender um sistema de modelagem/animação diferente.

A meta é que criadores possam **modelar, texturizar, animar, incluir efeitos e exportar** um recurso para o jogo ou para um mod. O conteúdo exportado deve seguir os contratos oficiais do ShardBound. O plugin não substitui o runtime de animação/VFX e não precisa ser uma engine de jogo.

### O que significa "igualzinho ao GeckoLib"

- **Definido, na experiência de autoria:** árvore de pastas/grupos e cubos, pivôs, editor UV/painting do Blockbench, timeline, keyframes e clips; grupos podem ser animados.
- **Definido:** não introduzir no editor um fluxo obrigatório de esqueleto separado, skinning por influência de vértices ou pintura de pesos.
- **Proposta:** reutilizar o editor nativo e o sistema de Animate Effects do Blockbench em vez de recriar visualmente todas as ferramentas.
- **Definido:** a referência ao GeckoLib significa **mesma experiência de edição e animação**, **não** uma exigência de importar projetos GeckoLib existentes ou exportar para o formato GeckoLib.
- **Não prometido:** compatibilidade binária com todos os arquivos/recursos do GeckoLib, igualdade de APIs, redistribuição de código ou de assets de terceiros, ou equivalência absoluta de cada menu.

## 2. Conteúdos que o plugin deverá atender

| Tipo | Requisito confirmado | Ferramentas específicas propostas |
| --- | --- | --- |
| Skin do player | Criar/editar skins | Template do jogador, pintura, UV e pré-visualização no humanoide |
| Item genérico | Criar modelos de itens | Poses de exibição, escala e ponto de encaixe |
| Arma corpo a corpo | Criar e animar armas melee **com o player de referência visível** | Player padrão sem skin, ajuste na mão, poses do player e animação do ataque no mesmo contexto |
| Arma de longa distância | Criar e animar armas ranged | Editor contextual com player de referência (extensão proposta), empunhadura, animações de uso/recarga e origem visual de projéteis |
| Bloco | Criar blocos que compõem grande parte do cenário do ShardBound | Cubos e módulos de cenário, orientação, possíveis variantes e referências de colisão; colocação/remoção ainda indefinidas |
| Armadura | Criar peças **sobrepostas** ao player, organizadas em pastas por parte do corpo | Editor com player de referência, encaixe por braço esquerdo/direito etc.; animações opcionais, sem exigir poses do player |
| Monstro | Criar mobs/criaturas animados com **hierarquia de grupos totalmente livre** | Clips e efeitos por grupo, sem exigir forma humanoide ou estrutura fixa |
| Partículas/VFX | Criar efeitos visuais | Editor de parâmetros, texturas, emissores e preview |

**Definido:** todos os tipos devem contemplar texturas; animações devem estar disponíveis sempre que fizerem sentido (não necessariamente obrigatórias para blocos/skins).

**Definido:** projetos podem conter um único modelo ou vários modelos independentes relacionados, conforme o conteúdo. A estrutura de exportação e as regras para vínculo entre modelos continuam em aberto.

**Definido (direção do mundo):** a maior parte dos cenários do jogo será composta por blocos, tendo Minecraft Dungeons II como referência de construção visual. **Não foi decidido** se o jogador poderá colocar, remover ou modificar blocos em runtime.

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

## 4. Editor contextual de armas e armaduras (definido)

**Armas:** ao modelar uma espada, oferecer um local de edição/pré-visualização com um **modelo padrão do player sem skin**. Nele o autor pode posicionar a arma na mão, ajustar como ela é segurada, alterar posições/poses de partes do jogador (inclusive as que não estão diretamente ligadas à espada) e **animar o ataque nesse mesmo local**. O player serve de referência para autoria, não significa embutir sua geometria visual em cada arma exportada.

**Armaduras:** fornecer um contexto semelhante para verificar como as peças se encaixam sobre o player. As peças acompanham os grupos correspondentes do corpo, organizados por pastas (inclusive braços esquerdo e direito). **Não exigir edição de poses do player no fluxo de armadura**; permitir animações da armadura quando o criador desejar.

**Em aberto:** quais animações do player são reutilizáveis entre armas e como animações customizadas de arma interagem com as animações base do personagem. A extensão do mesmo editor contextual a todas as armas de longa distância é proposta, por consistência, mas ainda merece validação.

## 5. Requisitos de experiência do usuário

- Familiaridade com o Outliner do Blockbench: grupos, subgrupos, cubos e pivôs.
- A timeline visual não deve exigir escrever código para declarar um efeito em certo frame.
- Ao animar uma espada, por exemplo, o autor precisa poder adicionar partículas a **essa mesma animação**, sem abrir um projeto desconectado.
- Presets/templates para reduzir erros repetitivos ao criar mods.
- Mensagens de erro claras para referências/formatos não suportados.
- Pré-visualização da animação e, idealmente, dos efeitos antes da exportação (**preview VFX proposto**, não confirmado tecnicamente).
- Exportação reprodutível, sem alterações inesperadas nos originais.

## 6. Direção artística e integração

- **Definido no ShardBound:** modelos predominantemente cuboides, proporções blocadas e texturas pixel-art.
- **Definido no ShardBound:** GLB/GLTF como formatos-fonte e preferência por textura embutida em GLB.
- **Definido no ShardBound:** identificação pública de assets por `namespace:path`, com declaração explícita no sistema de mods.
- **Alinhamento:** não importar texturas ou outros assets proprietários de Minecraft/Minecraft Dungeons como conteúdo oficial.
- **Proposta:** nomes como "ShardBound Studio", "ShardBound Model" e "Particle Editor" são de trabalho, não marca final confirmada.

## 7. Ordem de implementação (definida)

Primeiro implementar **todas as ferramentas de criação do plugin dentro do Blockbench**; depois integrar o conteúdo produzido ao runtime do ShardBound. O desenvolvimento pode definir schemas de exportação, criar exemplos e testes do editor antes da integração, mas **não deve afirmar** que animações, armaduras e VFX já funcionam no jogo nessa etapa.

## 8. Fora do escopo já confirmado

Não foi solicitado: editor de mapas completo, lógica de IA de monstros dentro do Blockbench, linguagem de programação visual completa ou substituição do sistema `.sl`. Esses recursos não devem ser presumidos.
