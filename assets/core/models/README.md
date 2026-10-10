# Modelos do pacote oficial

`player.bbmodel` / `player.glb` são o personagem original do protótipo.
Scout e Brute derivam desse rig: seis grupos (Head, Body, LeftArm, RightArm,
LeftLeg, RightLeg) e pivôs preservados. Os GLBs desses personagens usam escala
1/16 para converter unidades de autoria em unidades do mundo.

| Modelo | Fonte editável | Geometria e autoria |
| --- | --- | --- |
| Scout | `scout.bbmodel` | 21 cuboids, capuz/máscara, bolsa e mochila; texturas próprias 16×16 |
| Brute | `brute.bbmodel` | 24 cuboids, capacete, peitoral, ombreiras, braçadeiras e joelheiras; texturas próprias 16×16 |
| Iron sword | `iron_sword.bbmodel` | Cópia da espada completa fornecida pelo autor; seis meshes, exportada em unidades de autoria |

Modelos e texturas foram aplicados, conferidos visualmente e exportados pelo
plugin ShardBound Blockbench. As texturas dos inimigos ficam em
`../textures/characters/` e também estão embutidas nos GLBs. Não se usam modelos
ou texturas de Minecraft Dungeons. Os nomes de peças mantêm o prefixo do grupo
para que `pose_part` anime cada parte rígida.

`iron_sword.glb` inclui o cabo/guarda que faltavam no `sword.glb` anterior.
O visual do jogo usa escala 0,05 e ponto de empunhadura Y=7. O original `sword.*`
e os assets de árvore/waystone fornecidos pelo autor foram preservados.
As variantes leve/pesada usam escalas 0,03 e 0,065 deste mesmo modelo.

Rig rígido e poses procedurais; nenhum skinning ou clip glTF está implícito.
As fontes `.bbmodel` podem ser abertas e modificadas no formato Generic Model.

## Revisão de apresentação (outubro de 2026)

- `woodland_pine.bbmodel` / `.glb`: pinheiro autoral criado no Blockbench pelo
  bridge, 11 cuboides, três texturas de 16×16; altura 6,3125 unidades após export.
- `grass_block`, `earth_block`, `plaster_block`, `roof_block`, `stone_block`:
  cubos unitários com texturas originais de 32×32, reproduzíveis por
  `python tools/create-scenery-materials.py` (Pillow). PNGs em `textures/woodland`.
  O gerador reutiliza somente a geometria original de `solid.glb`.
- Os modelos originais de árvore e waystone são usados na composição oficial.
  Desde a correção de identidade de 2026-10-09, o pinheiro continua disponível
  como asset, mas não substitui a árvore do autor no cenário.

`explorer_concept.bbmodel` é um estudo editável interrompido a pedido do autor,
que continuará o personagem. Mantém o rig existente e adiciona roupa/acessórios;
as seis texturas `ranger_*` ficam em `textures/woodland`. Não há GLB desse estudo
nem integração no jogo. `player.bbmodel` e `player.glb` permanecem como estavam.
