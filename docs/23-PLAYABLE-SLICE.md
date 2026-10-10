# Etapa 13 — contrato do primeiro ciclo jogável

Contrato técnico de protótipo, 2026-10-03. Não congela arte, balanceamento,
catálogo, rig de skinning ou progressão. As decisões do autor em [22](22-GAME-DIRECTION-AND-REFERENCES.md) prevalecem.

## Plataforma e conteúdo

O pacote `adventure` contém o mundo e as regras em módulos `.sl`, usando os
registries, hooks e saves existentes. A Application apenas conecta serviços.
O hub e a floresta são uma área contínua pequena, sem carregamento/streaming.
O limite do protótipo é físico; nenhuma missão bloqueia a passagem entre regiões.

Este contrato inicial foi ampliado em [24](24-EQUIPMENT-AND-WORLD-CONTENT.md).

Entradas provisórias: WASD, Espaço para salto, botão esquerdo para ataque na
direção do último movimento, M para mapa, 1/2/3 para destinos descobertos,
I para inventário, Q para selecionar, E para confirmar, F5/F9 para salvar/carregar
e Escape para sair. Fora dos menus Q alterna entre instâncias disponíveis.
O mapa não pausa inimigos nem o relógio de combate. Não há movimento por clique.

O controlador preserva a física Jolt a 60 Hz e usa sondas descendentes que
ignoram o próprio corpo para detectar apoio. Salto só ocorre vivo, ativo e
apoiado, nunca durante subida. A API retorna false quando não pode saltar.
Velocidade vertical negativa, sem apoio, define queda. Diagonais são normalizadas.
Câmera oblíqua acompanha o centro físico com offset provisório (7,10,-10).
WASD usa os vetores da câmera projetados no chão. A câmera ainda acompanha
verticalmente o salto; smoothing, oclusão e parâmetros finais permanecem abertos.

## Arte e movimento rígido

Template original no Blockbench: cabeça 8×8×8, tronco 8×12×4, membros 4×12×4,
pés em Y=0, frente -Z. Autoria em 16 unidades por unidade de mundo; pivôs
Head=(0,24,0), Body=(0,24,0), braços=(±4,22,0), pernas=(±2,12,0).
Nomes de peças começam com esses seis nomes. A pose rígida é transitória e
aplicada em coordenadas de modelo antes do transform da entidade; não é skinning
nem suporte a clips glTF. A fonte `.bbmodel` permanece editável.

## Combate e itens

Espada de ferro inicial; definições de arma contêm nível, raridade, dano,
alcance, arco, preparação, recuperação e padrão. Valores são provisórios e
podem ser alterados por `modify`/hooks. Acerto consulta alcance, direção e
linha de visão no instante de impacto; cada alvo recebe no máximo um acerto
por ataque. Crítico de queda aplica 1,7× no instante de impacto.
Inimigos têm perseguição limitada à floresta, preparação visível e recuperação.
O hub é seguro. Loot e recompensas não impõem campanha ou curva de XP.

## Waystones, morte e persistência

Raio provisório de descoberta: 2,5 unidades. Descobertas são independentes
por destino. Viagem gratuita e instantânea exige seis segundos completos sem
dano positivo causado/recebido pelo jogador. Dano zero não reinicia o relógio.
Teleportes zeram velocidade. Dano ambiental ao jogador também bloqueia viagem.
Na morte: uma aplicação de `floor(ouro * 0.15)`, retorno ao hub com vida cheia,
equipamento, itens e descobertas preservados. Sem final/campanha.

O envelope de save continua v1. Estado do pacote usa schema 1 e IDs persistentes;
referências de runtime são reconstruídas em `on_loaded`. Cooldowns e descobertas
fazem parte do estado, para carregar não liberar viagem durante combate.
Tint RGB é campo opcional do modelo no save, padrão branco para saves antigos;
poses rígidas são reconstruídas. Saves futuros incompatíveis geram diagnóstico.
`player.camera` é opcional, com default histórico (0,8,-10) em saves antigos.
`on_validate_load(state_por_mod)` roda antes da troca do mundo e deve apenas
validar dados, sem mutações. `script_error(message)` aborta a validação com
diagnóstico; `on_save_diagnostic(loading,message)` permite feedback de UI.
Ausência de mods mantém a preservação de órfãos da Etapa 12.

## APIs acrescentadas

| API | Contrato |
| --- | --- |
| `jump(speed)` | Impulso vertical positivo, false quando não permitido. |
| `set_gravity_factor(entity,factor)` | Multiplicador finito não negativo da gravidade Jolt (-9,81), somente corpo dinâmico. Transitório: reaplicar após recriar corpo ou carregar save. |
| `is_grounded(entity)` | Sondas físicas com alcance extra de 0,06 e normal Y > 0,6. |
| `get_velocity(entity)` / `set_velocity(entity,vec3)` | Velocidade do corpo existente; valores finitos. |
| `teleport(entity,vec3)` | Sincroniza corpo/Transform e zera velocidade imediatamente. |
| `line_of_sight(from,to)` | Segmento entre centros físicos; ignora origem, aceita destino como primeiro impacto. |
| `movement_enabled(bool)` | Habilita input do controlador e salto; não pausa física. |
| `keyboard_movement_only(bool)` | Desativa eixos de gamepad para movimento desse runtime. |
| `set_camera_offset(vec3)` | Y positivo, projeção horizontal não nula; coordenadas até 100 unidades. |
| `set_rotation(entity,vec3)` | Rotação visual em radianos; não gira os boxes físicos. |
| `set_parent(child,parent)` | Hierarquia visual; null remove pai, corpo físico não pode ser filho. |
| `set_tint(entity,vec3)` | Multiplicador RGB em [0,1] por entidade, persistido. |
| `pose_part(entity,prefix,pivot,rotation)` | Pose visual rígida em unidades do GLB e radianos. Prefixo vazio/ausente é erro. |
| `get_entities(definition_id)` | Snapshot de entidades com a definição exata, incluindo inativas. |
| `damage_from(source,target,amount)` | Mesmo hook `core:damage`, com `source` informativo, e evento `on_damage_dealt(source,target,applied)`. |
| `maximum_health(entity)` | Vida máxima atual. |
| `math_floor`, `math_sin`, `math_sqrt`, `math_atan2` | Utilitários numéricos, sem substituir nomes locais comuns como `floor`. |
| `number_text(number)` | Número arredondado sem casas para apresentação. |
| `show_ui(text,map,markers)` | Painel provisório; até 4096 bytes e 64 marcadores com x/z/label/active. |

`pose_part` aplica a matriz após a transformação estática da parte e antes
da transformação da entidade. Prefixos sobrepostos usam a primeira entrada
lexicográfica; o rig do protótipo usa prefixos disjuntos. Dados/poses de uma
entidade não alteram o asset compartilhado. `show_ui` agora enquadra os marcadores automaticamente, conforme 24; ainda
não é um formato público final de mapas. Texto/UI e poses
são reconstruídos; não se serializam handles de GPU.

O conteúdo publica `adventure:melee` (hook com source/target/amount),
`adventure:attacked` (ID de arma), `adventure:travelled` (nome do destino) e
`adventure:respawned` (ouro perdido). Arco de arma é o cosseno mínimo aceito
para o ângulo ao alvo; padrões implementados: slash e thrust. O inventário genérico
continua com contagens por definição; instâncias/atributos e UID equipado ficam
no estado do pacote, conforme 24.

## Aceitação

Verificar salto/aterrissagem/parede/ausência de salto duplo, exploração livre,
ativação dos destinos, dano dado e recebido, limite exato de seis segundos,
crítico só na queda, morte/perda única, save/load, parâmetros modificados por
outro pacote, pixels reais de GPU e smoke da Application em Debug/Release.
Esse ciclo não equivale à conclusão do jogo inteiro: animações finais, UI final,
streaming, catálogo amplo e otimização representativa continuam trabalhos futuros.

## Ajuste de salto — 2026-10-09

A referência de bloco para o salto é o cubo da árvore original do autor em
`assets/core/models/tree.glb`: 16 unidades de autoria, aplicado com escala
0,038 no cenário, resulta em 0,608 unidade de mundo por bloco. O piso decorativo
de duas unidades não define essa medida. O salto deve elevar os pés em dois
blocos da árvore: 1,216 unidade de mundo.

`Actor.jump = 7.19` é a velocidade inicial vertical e
`Actor.gravity_factor = 2.03873598` aplica aproximadamente 20 unidades/s² de
gravidade ao jogador. Os valores consideram o passo físico de 60 Hz e o
amortecimento do corpo; o ciclo completo dura aproximadamente 0,7 segundo.
A gravidade dos outros corpos permanece no padrão. Os campos continuam
alteráveis por mods; `jump(speed)` mantém seu contrato anterior de velocidade.
O pacote reaplica o fator ao criar o jogador e em `on_loaded`, sem alterar o
esquema v1 do save. As restrições de apoio/vida/menus e o crítico durante a
queda permanecem iguais.
