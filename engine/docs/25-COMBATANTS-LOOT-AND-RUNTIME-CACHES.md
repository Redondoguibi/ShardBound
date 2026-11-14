# Etapa 13 — variedade de combate, loot e custos de atualização

Marco incremental; não encerra o jogo. Decisões de 22 e contratos 23/24 prevalecem.

Inimigos continuam com componente Sentinel v1. Sua definição real determina
vida, corpo, visual, velocidade, preparação, alcance, recuperação, perseguição,
recompensa e drops. O inimigo original conserva parâmetros anteriores.
Scout é rápido/frágil; Brute é lento, resistente e tem preparação longa.
Scout e Brute possuem modelos originais, criados no Blockbench a partir do rig
rígido do personagem: 21 e 24 cuboids, respectivamente, com texturas próprias
16×16. Os seis grupos/pivôs continuam compatíveis com as poses procedurais.
A espada completa foi exportada do `.bbmodel` do autor para `iron_sword.glb`,
com seis meshes (incluindo cabo/guarda); o export antigo permanece na galeria.
Não introduzir lore, XP, campanha ou níveis mínimos para explorar.

Regiões podem adicionar `encounters`: lista de `{key,definition,position}`.
Keys são únicos no catálogo e persistem em state.encounters. Saves anteriores
recebem encontros novos sem duplicar inimigos já gerados. `enemies` (vec3) mantém
compatibilidade. Definições de arma novas demonstram padrões rápido/pesado.

`drops` é uma lista determinística de IDs de itens. Ao derrotar um inimigo,
ouro é aplicado uma vez e os itens ficam no mundo como entidades com componente
GroundLoot v1. Aproximação a 2,5 unidades coleta itens disponíveis; inventário
cheio conserva o loot no chão. Drops e equipamentos permanecem ao morrer e
salvar/carregar. Itens de mods ausentes ficam preservados e sem coleta até
o conteúdo retornar. Sorteio de atributos e tabelas aleatórias continuam abertos.

APIs aditivas: `set_model(entity, asset_id)` troca o visual, conserva tint e
limpa poses; carrega o recurso antes de alterar a entidade.
`entity_definition(entity)` retorna ID; `get_entities_with(type)`
retorna snapshot com componente customizado registrado, incluindo inativos.
`registry_revision()` muda somente após add/patch/replace bem-sucedidos.
O conteúdo cacheia catálogos de região/destino por revisão, com invalidação
automática após modify. O cache de UI reutiliza textura quando os pixels não
mudam; marcadores são relevantes apenas quando o mapa está visível.

Validar dano/alcance/vida por tipo, preparação e segurança do hub, recompensa
única, coleta/capacidade, save de loot e encontros, mods, invalidação de caches
e pixels D3D11. Tempos/cores/nomes deste lote são provisórios de balanceamento.

## Contratos do pacote adventure

`Enemy` é a base pública. As variantes são definições `Entities` derivadas;
o componente `Sentinel` reúne a IA, sem uma lista de nomes fixa no núcleo.
`enemy_at(x,z,definition="adventure:Enemy")` configura o corpo e o filho visual.

| Campo de inimigo | Significado |
| --- | --- |
| `body` / `visual` / `visual_scale` / `tint` | Meias dimensões do corpo físico, ID de entidade visual, escala e cor |
| `max_health`, `speed`, `damage` | Vida inicial/máxima, velocidade e dano |
| `aggro`, `stop`, `trigger`, `reach` | Distâncias de perseguição, parada, início e resolução do ataque |
| `windup`, `recovery`, `respawn` | Tempos em segundos |
| `reward`, `drops` | Ouro e lista determinística de IDs de itens |

Armas podem fornecer `model` (ID de asset), `visual_scale` e `grip` (Y do cabo
em unidades do modelo). Campos ausentes recebem os valores da espada de ferro,
preservando armas escritas contra o contrato anterior. Uma falha ao carregar
o modelo não confirma a nova identidade equipada. A arma leve e a pesada
usam o modelo completo em escalas diferentes; ainda não têm modelos próprios.

`encounters` é opcional em regiões. Cada registro contém chave estável, ID de
inimigo e posição; X/Z definem o local, a altura de repouso vem do corpo físico.
Limite de 256 encontros e chaves de 1–128 bytes. Herança copia a lista: regiões
novas devem definir chaves próprias ou `.encounters = []`, como no exemplo
`trailcraft`. Alterar uma chave faz nascer um encontro novo ao reconciliar;
não desloca um encontro existente nem remove os inimigos do save.

`GroundLoot.item` identifica o item. O visual fica fixo no mundo, sem colisão,
com o modelo/tint disponíveis. A coleta cria a instância validada pelo hook
`adventure:create_item` antes de conceder a contagem e consumir o drop. Hook
rejeitado mantém o item no chão. `adventure:collected` recebe `[item_id]` depois
da coleta. A capacidade atual continua sendo 256 instâncias de armas.
O estado de derrota é confirmado antes de criar drops para impedir recompensas
repetidas se uma extensão falhar. Não há magnetismo, interação manual, tempo
de desaparecimento ou sorteio. Entidades comuns suportam save/load desse estado.

## Verificação e limites

`combat_content_tests` exercita os dois modelos GLB no importador real, dano e
preparação distintos, derivação de inimigo por mod, golpe em tipo derivado,
recompensa única, loot distante, capacidade, hook rejeitado, morte, save/load,
retorno de definição ausente, migração de encontros, contrato anterior de armas,
cache de catálogos e reutilização/invalidação da textura do mapa.
`--preview` gera readback D3D11 em `user/combat-content-preview.ppm`.

O smoke de 120 frames usa `--no-vsync` na janela oculta: a sincronização com o
monitor elevava esse teste a cerca de 36s neste dispositivo. O mesmo teste
Release, limitado a 60 frames/s, passou em aproximadamente 2,5s sem ela.
Isso não é uma medição de FPS do jogo completo. Não houve benchmark com muitos
mods, streaming ou sessão manual prolongada. UI, áudio e cenário final, catálogo
amplo, armaduras e distribuição continuam pendentes; este marco não os encerra.
