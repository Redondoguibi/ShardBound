# APIs da plataforma de mods

Complementa [06-WORLD-GENERATION.md](06-WORLD-GENERATION.md) e os contratos de
[extensibilidade](../21-EXTENSIBILITY-V1.md). Os exemplos usam records e listas `.sl`.
Registre sistemas e callbacks a cada sessão; grave apenas dados em componentes ou
`set_save_state`. Funções e referências de entidades não são serializáveis: use
`entity_key`/`get_entity_by_key` para referências persistentes.

Evolução de streaming, saves por chunk, F3, transferência e edição de widgets:
[08-STREAMING-AND-DEVELOPER-TOOLS.md](08-STREAMING-AND-DEVELOPER-TOOLS.md).

## Sistemas

```text
func tick(delta):
    ? delta é o tempo acumulado desde a última execução deste sistema.
    update_my_simulation(delta)
register_system("meumod:simulation", "fixed", tick, 10)
options = record()
options.after = ["meumod:prepare"]
options.interval = 0.1
options.disable_on_error = true
configure_system("meumod:simulation", options)
```

Registre `meumod:prepare` antes de configurar a dependência. `after` aceita até 64
IDs distintos da mesma fase; ciclos são erro. `interval` fica entre 0 e 3.600 s:
zero executa a cada despacho. Mantém o resto do intervalo, mas executa no máximo
uma vez por despacho, passando todo o delta acumulado. `disable_on_error` é opt-in.
Não substitua sistemas de outro mod sem uma API de cooperação definida por ele.

## Geração volumétrica

Campo opcional dentro de um gerador em `world.json`:

```json
"voxel_rules": [{
  "id": "meumod:deposito",
  "block": "meumod:minerio",
  "replace": ["plains:stone"],
  "min_y": 1,
  "max_y": 12,
  "min_surface": -48,
  "max_surface": -2,
  "period": 16,
  "threshold": 0.4,
  "salt": 31,
  "protect_spawn": true,
  "biomes": []
}]
```

Declare o bloco e a dependência de `plains`. Regras executam na ordem do array,
depois dos solos e antes das estruturas. `block: ""` remove; `replace: [""]`
preenche ar. A regra vale quando o ruído 3D é maior ou igual ao limiar. Y absoluto
é 0..47; distâncias relativas à altura original são -48..48; `period` 4..256;
`threshold` -1..1; `salt` inteiro 0..2.147.483.647. Biomas vazios aceitam todos.
`protect_spawn` usa o `safe_radius` do gerador e é verdadeiro por padrão. Limite de
oito regras, inclusive as acrescentadas por `generator_extensions.voxel_rules`.

## Edição e navegação

| API | Resultado e regras |
| --- | --- |
| `terrain_edit(edits)` | Número de células alteradas. Lista de records `{x,y,z,block}` em coordenadas **locais inteiras**; duplicatas são erro. Lote inteiro ou nenhuma mudança. |
| `terrain_stamp(structure_id, origin, quarter_turns)` | Aplica a estrutura como lote; origem `{x,y,z}` inteira; rotação 0..3 em quartos de volta. |
| `terrain_undo()` / `terrain_redo()` | Booleano: houve transação a restaurar. Histórico global da sessão, não privado por mod. |
| `terrain_surface(x,z)` | Y acima do bloco sólido mais alto, considerando estruturas/edições; zero se vazio. X/Z locais inteiros. |
| `terrain_path(start,goal,options)` | `{status,points,expanded}`. Start/goal são células dos pés, com `{x,y,z}` inteiros. |

Opções obrigatórias do caminho: `max_nodes` 1..4096, `clearance` 1..8, `step` 0..4 e
`drop` 0..8. Goal deve ficar a até 128 blocos em X/Z do início. Estados: `found`,
`unreachable`, `invalid_endpoint`, `budget`. Somente `found` contém pontos; cada ponto
usa X/Z no centro da célula e Y nos pés. Caminho inclui início e fim. Não reutilize
pontos locais depois de mudar `world_origin` sem corrigir suas coordenadas.
O limite de expansão evita buscas ilimitadas; cache de colunas evita gerar cada
bloco individualmente. Solicite caminhos em intervalos e recalcule quando necessário.

## Comportamentos e fases

`behavior_tick(tree,context,memory)` retorna `{status,memory}`. Contexto é passado
às ações; cada ação retorna exatamente `success`, `failure` ou `running`.

```text
action = record()
action.kind = "action"
action.run = my_action
sequence = record()
sequence.kind = "sequence"
sequence.children = [action]
sequence.reactive = false
result = behavior_tick(sequence, actor, previous_memory)
```

Tipos: `action` com `run`; `sequence` e `selector` com lista não vazia de `children`;
`invert` com exatamente um filho. Máximo de 128 nós e 16 níveis. Sequências/seletores
normais retomam o filho em execução; `reactive: true` reavalia desde o início.
A memória contém cursores por caminho; comece com `record()`. Armazene a memória
retornada se quiser continuar no próximo tick ou depois de um save. Ao mudar a
estrutura da árvore, migre/limpe sua memória. Erros de callbacks não desfazem seus
efeitos no mundo; árvores não são transações de gameplay.

`state_tick(states,current,context)` retorna o nome do próximo estado. Cada estado
é um record com função `tick(context)` obrigatória e `enter(context)`/`exit(context)`
opcionais. Tick retorna null para ficar no estado ou o nome do destino. Em mudança,
executa `exit` do antigo e `enter` do novo. A entrada inicial é responsabilidade do
mod. Máximo de 64 estados. O conteúdo decide limiares, transições e fases de chefes.

## Raios e projéteis

`physics_raycast(from,to,ignored_entities)` retorna null ou
`{entity,fraction,position,normal}`. Ignore até 16 entidades. O segmento atravessa
os corpos físicos e o terreno procedural, mesmo fora do streaming. Uma colisão
com terreno sem corpo carregado tem `entity = null`. No terreno: coordenadas locais
até ±100.000; soma das distâncias absolutas por eixo até 4.096 células. Um início
dentro de bloco sólido retorna fração zero, podendo ter normal zero.

`launch_projectile(entity,source_or_null,velocity,lifetime,gravity)` usa uma entidade
raiz com transform e **sem corpo físico**. Velocidade por eixo: ±1.000; duração
0,001..60 s; gravidade 0..100. Máximo de 512 projéteis. A origem usa identidade
persistente; sua destruição deixa a referência de origem null.

Eventos, antes da destruição automática da entidade do projétil:

- `on_projectile_hit(projectile,source,target,position,normal)`;
- `on_projectile_expired(projectile,source)`.

O mod aplica dano/efeitos nesses eventos. `stop_projectile(entity)` interrompe o
movimento e conserva a entidade. Colisão é pontual, não uma esfera ou corpo rígido.
Projéteis inativos não avançam. O componente persistente contém velocidade, tempo
restante, gravidade e origem; posição acompanha o floating origin normalmente.

## Equipamentos

`create_item_instance(actor,item_id,options)` converte uma cópia **livre** do
inventário em instância e retorna seu ID. Não aumenta a quantidade possuída.

```text
bonus = record()
bonus.add = 5
bonus.multiply = 1.2
stats = record()
stats.damage = bonus
options = record()
options.maximum = 100
options.stats = stats
options.affixes = record()
instance = create_item_instance(actor, "meumod:espada", options)
equip_instance(actor, "weapon", instance)
```

`maximum` é a durabilidade máxima (1..1e9). Stats usam IDs de até 64 caracteres:
letras minúsculas, dígitos, `_`, `-`, `:`. Cada bônus requer `add` (-1e6..1e6) e
`multiply` (0..100). `affixes` mapeia IDs com namespace para records de bônus.

| API | Contrato |
| --- | --- |
| `item_instances(actor)` | Lista de cópias dos records, cada um com `id,item,slot,durability,maximum,stats,affixes`. |
| `item_instance(actor,id)` | Uma cópia do record; erro se não pertence ao ator. |
| `equipped_instance(actor,slot)` | ID da instância ou null. |
| `available_item_count(actor,item)` | Cópias livres após reservas antigas e únicas. |
| `equip_instance(actor,slot,id_or_null)` | Troca/move a instância; null libera o slot; retorna se mudou. Slot antigo de pilha deve ser liberado antes. |
| `item_durability(actor,id,value)` | Define durabilidade entre zero e máximo; serve para desgaste e reparo. |
| `release_item_instance(actor,id)` | Exige desequipar; descarta os atributos da instância e libera sua cópia para a pilha. |
| `equipment_stat(actor,stat,base)` | Agrega fontes equipadas não quebradas. Falha se o resultado não for finito ou exceder ±1e12. |
| `attach_socket(visual,parent,node,offset)` | Visual sem física; nome/UUID único do nó, offset Vec3; preserva rotação/escala. `set_parent` remove o vínculo de socket. |

Hook `core:equip_instance` recebe `{actor,slot,instance,allow}`; somente `allow`
controla o veto. Eventos `on_instance_equipped(actor,slot,id_or_null)` e
`on_item_durability(actor,id,old,new)` ocorrem após gravar. `core:equip` continua
atendendo slots antigos. IDs são únicos por ator, não globais.

`roll_affixes(seed,pool,count)` retorna o record pronto para `options.affixes`.
Seed é texto de até 256 bytes; pool tem até 64 entradas com `id`, `weight` (.001..1000)
e `stats`. Cada stat contém `add_min`, `add_max`, `multiply_min`, `multiply_max`,
nos mesmos limites dos bônus. Sorteia até oito entradas sem reposição, usando ordem
do pool e seed; persista o resultado na instância para preservar o loot após mudanças
no pool. Não depende do relógio ou de um RNG global.

No mod oficial `foundation`, `damage` altera dano causado e `armor` reduz o recebido.
A arma única perde um ponto de durabilidade por acerto que cause dano; quebrada não
causa dano. Instâncias em outros slots também podem contribuir bônus. A engine não
atribui significado fixo a outros nomes de atributos.

## Widgets

`show_widgets("meumod:tela", specification)` cria/substitui uma tela do próprio mod.
Especificação: `layout` (`row`, `column`, `absolute`), `modal` booleano, `x,y,w,h`
como frações do viewport lógico, `gap` opcional em pixels e lista `widgets`.

Cada widget tem `id`, `kind`, `text` opcional e `enabled` opcional. Tipos:

- `button`, `label`, `panel`;
- `toggle`: `value` booleano;
- `input`: `value` texto UTF-8;
- `slider`: `min,max,step,value` numéricos (intervalo dentro de ±1e9).

Layouts row/column dividem igualmente a área; absolute exige `x,y,w,h` relativos à
tela em cada widget. IDs distintos por tela; ordem de inserção define pintura e
Tab. A última tela fica por cima; modal impede entrada nas inferiores.

`on_ui(screen_id,widget_id,event,value)` entrega `activate`, `change`, `submit` ou
`cancel`. Filtre pelo ID da sua tela. Enter ativa; Tab muda foco; setas esquerda/direita
ajustam sliders; Escape cancela. Mod decide o que fazer com cancel. `widget_value`
consulta o estado, `focus_widget` altera foco e `hide_widgets` remove a tela.
Reenviar `show_widgets` substitui valores; envie somente quando quiser atualizar a
especificação, não a cada quadro. O foco é conservado quando o ID ainda é interativo.

Widgets são registrados no runtime da sessão. O menu de escolher/criar mundo ainda
não executa scripts, e não é substituído por estas APIs. Não há suporte de leitor
de tela. Seleção, clipboard, scroll e arraste estão documentados no guia 08.

## Movimento, preferências e áudio

`set_player_step_height(altura)` configura a subida e descida automática em suporte estático, entre 0 e 2 unidades. O padrão é zero para conservar o comportamento anterior; o mod `foundation` usa 1,05 nos mundos novos. O valor é persistido com o jogador. A tentativa de degrau verifica teto e espaço para o volume inteiro do colisor; saltos não recebem encaixe automático no chão.

`add_box(entidade, meia_extensao, dinamico, rotacao?)` aceita uma rotação física opcional em radianos, na mesma composição da transformação visual. O argumento ausente mantém a caixa alinhada aos eixos, inclusive se o modelo estiver girado. A rotação física é persistida independentemente da visual.

`play_sound(id)` mantém o canal `effects`. `play_sound(id, "music")` e `play_sound(id, "interface")` escolhem os canais controlados pelas configurações correspondentes. O volume geral multiplica todos os canais.

As personalizações de input do jogador têm prioridade sobre `bind` de mods. A escala de UI aplica-se ao desenho, ponteiro e projeções das APIs existentes; continue usando as coordenadas lógicas dessas APIs. Não multiplique novamente pela escala das configurações.
