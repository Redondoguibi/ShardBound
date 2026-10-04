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
