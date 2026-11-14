# Personalizar a geração de mundos por mods

O runtime lê `world.json` na raiz de cada mod. Você pode criar blocos, biomas,
camadas de solo, estruturas, geradores e extensões de geradores existentes.
Tudo abaixo está implementado. Não é necessário recompilar C++ para mudar
os dados de um mod instalado em `build/<preset>/content/`.

O exemplo executável fica em `examples/worldgen-addons/gardens/`. Copie a pasta
`gardens` para o `content/` do executável, ao lado de `foundation` e `plains`,
reinicie e escolha **Jardins tranquilos** ao criar um mundo. Ele também acrescenta
flores ao gerador de planícies, sem alterar os arquivos do mod `plains`.
Execute `modcheck content` a partir da pasta do executável para validar os mods.

## Biomas e camadas

Cada definição usa um ID com o namespace do seu mod. Assets de outro mod
continuam usando seus IDs originais; declare a dependência em `mod.json`.

```json
{
  "version": 1,
  "biomes": {
    "gardens:flower_field": {
      "name": "Campo de flores",
      "rock": "plains:stone",
      "layers": [
        {"block": "plains:grass", "depth": 1},
        {"block": "plains:dirt", "depth": 3}
      ]
    }
  }
}
```

As camadas são aplicadas **de cima para baixo**. `rock` preenche o restante da
coluna. Camadas que ultrapassem a profundidade da coluna são truncadas no fundo.
Blocos do solo precisam ser sólidos. Cada bioma admite até oito camadas, com
profundidade individual de 1–32 e soma de até 48 blocos.

Um gerador seleciona biomas com uma lista como:

```json
"biomes": [
  {"biome": "plains:meadow", "weight": 0.65},
  {"biome": "plains:grove", "weight": 0.35}
],
"biome_period": 192,
"biome_salt": 7919
```

`biome_period` (16–4096) controla a escala das manchas de bioma; `biome_salt`
altera o sorteio sem mudar o relevo. Pesos positivos dividem os intervalos do
ruído suave, na ordem da lista; **não são uma garantia de porcentagem de área**.
Os biomas mudam o solo e filtram conteúdo. A altura é contínua e compartilhada
pelo gerador, evitando degraus artificiais na fronteira entre biomas.

Sem biomas, o gerador usa suas próprias `layers`, ou o formato original
`surface`, `fill`, `rock` se `layers` estiver vazio. Nesse formato original,
há um bloco de superfície, dois de preenchimento e rocha abaixo.

## Relevo e presets herdados

Um novo preset pode herdar um gerador de uma dependência:

```json
"generators": {
  "gardens:quiet": {
    "extends": "plains:gentle",
    "name": "Jardins tranquilos",
    "safe_radius": 1,
    "noise": [
      {"period": 128, "amplitude": 0.75, "salt": 101},
      {"period": 32, "amplitude": 0.25, "salt": 102}
    ],
    "biomes": [{"biome": "gardens:flower_field", "weight": 1}],
    "structures": []
  }
}
```

Campos omitidos são herdados. Um campo fornecido substitui o campo completo:
listas não são concatenadas por herança; `structures: []` remove as estruturas
herdadas. A herança é resolvida antes das extensões. Ciclos são rejeitados.
O novo ID aparece como uma opção independente na criação de mundos.

Sem `extends`, os campos base são `name`, `surface`, `fill`, `rock`,
`base_height`, `relief`, `period` e `structures`. Os demais são opcionais.

`noise` substitui o ruído simples quando não está vazio. Cada camada tem período
16–4096, amplitude 0–4 e salt inteiro 0–2147483647. São aceitas até oito camadas;
a soma das amplitudes não pode ultrapassar 4. O resultado é somado e arredondado
uma vez, depois adicionado a `base_height` (2–24). A base precisa exceder o relevo
total. Sem essas camadas, continuam valendo `relief` (0–4) e `period` (16–1024).
As planícies oficiais mantêm relevo 1 e altura de 3–5 blocos, sem cavernas.

## Estruturas e distribuição

Estruturas continuam sendo arrays `[x, y, z, "namespace:bloco"]`. No modo novo,
x/z podem ir de -6 a 6; y de 0 a 15, com até 512 células por estrutura.
As posições são relativas à âncora. Células duplicadas são erro.

Ative `"structure_mode": "scatter"` no gerador para usar regras como:

```json
"structures": [
  {
    "id": "gardens:ruins",
    "structure": "plains:ruin",
    "chance": 0.08,
    "attempts": 2,
    "salt": 12,
    "biomes": ["gardens:flower_field"],
    "min_height": 3,
    "max_height": 6,
    "max_slope": 0,
    "spacing": 6,
    "rotations": [0, 90, 180, 270],
    "alignment": "anchor",
    "y_offset": 0
  }
]
```

| Campo | Efeito e limites |
| --- | --- |
| `id` | ID estável e único da regra no gerador; obrigatório em `scatter`. |
| `structure` | Estrutura registrada, inclusive de uma dependência. |
| `chance` | Probabilidade de cada tentativa, de 0 a 1. |
| `attempts` | 1–8 tentativas por chunk; padrão 1. Não garante quantidade: filtros podem rejeitar tentativas. |
| `salt` | Varia o sorteio da regra; inteiro 0–2147483647, padrão 0. |
| `biomes` | Biomas aceitos na âncora; vazio aceita todos. Devem constar na seleção do gerador. |
| `min_height`, `max_height` | Altura do chão na âncora; intervalo 1–47, padrão completo. |
| `max_slope` | Diferença máxima entre alturas do chão na âncora e nas colunas ocupadas; 0–8. Zero exige chão plano; padrão 8. |
| `spacing` | Distância mínima entre âncoras aceitas no mesmo chunk, de 0 a 32; padrão 0. Vale o maior espaçamento das duas regras. |
| `rotations` | Lista não vazia, sem repetições, de 0, 90, 180 e/ou 270 graus. |
| `alignment` | `anchor` mantém a estrutura rígida; `surface` posiciona cada coluna sobre seu chão, útil para plantas. |
| `y_offset` | Deslocamento vertical adicional de 0 a 8; padrão 0. |

Uma tentativa é rejeitada por inteiro se enterrar células no chão, sair da
altura disponível, colidir com células já colocadas ou violar os filtros.
Assim uma ruína não perde metade das paredes. Estruturas ficam dentro do chunk,
com margem de um bloco; não há construções que atravessem chunks nesta versão.
Espaçamento entre chunks e sustentação de células elevadas não são simulados.

São permitidas até 64 regras e 128 tentativas totais por gerador, com até 512
células de estruturas aceitas por chunk. Regras anteriores têm prioridade
quando há disputa por espaço/orçamento. O sorteio de cada regra usa ID, salt,
seed e coordenadas: acrescentar uma regra não muda as amostras das anteriores,
embora novas colisões possam mudar quais tentativas serão aceitas.

`safe_radius` protege um quadrado de chunks ao redor de (0,0), incluindo-o.
O padrão 0 protege somente o chunk inicial; 1 protege 3×3 chunks; -1 desativa
a proteção e 8 é o máximo. Essa proteção se aplica a estruturas, não aos
encontros, que são controlados pelo mod de gameplay.

## Acrescentar conteúdo a outro gerador

```json
"generator_extensions": {
  "gardens:plains_flowers": {
    "generator": "plains:gentle",
    "structures": [
      {"id":"gardens:extra_flowers", "structure":"gardens:flower_patch",
       "chance":0.15, "attempts":2, "alignment":"surface"}
    ]
  }
}
```

A extensão acrescenta `structures` e/ou escolhas `biomes` ao gerador alvo.
Não substitui campos nem modifica o arquivo original. O alvo deve usar
`scatter`; declare seu mod como dependência instalada com versão compatível.
IDs da extensão e das regras acrescentadas pertencem ao mod autor.
Extensões são aplicadas em ordem alfabética de ID, após resolver a herança.
IDs duplicados e referências inexistentes são rejeitados pelo loader/modcheck.
Para alterar outros parâmetros, crie um preset com `extends`.

## Consultar o mundo nos scripts

| API | Resultado |
| --- | --- |
| `terrain_biome(x, z)` | ID do bioma; string vazia nos geradores antigos sem biomas. |
| `terrain_info(x, z)` | Record com `height`, `biome`, `surface` do terreno base gerado, sem árvores/estruturas nem edições posteriores. |
| `terrain_block(x, y, z)` | ID do bloco, considerando estruturas e edições; `""` representa ar. |
| `world_noise(global_x, global_z, period, salt)` | Ruído suave determinístico entre -1 e 1, usando a seed atual. |

Consultas `terrain_*` recebem coordenadas locais, como as posições das entidades;
`world_noise` recebe coordenadas globais inteiras. Some `world_origin().x/.z`
às coordenadas locais para manter uma distribuição estável após longas viagens.
Período/salt de `world_noise` seguem os mesmos limites das camadas de ruído.

O mod `foundation` aceita `biomes` opcional nas definições `Encounters`:

```text
@Encounters
encounter GardenScout:
    .generator = "gardens:quiet"
    .biomes = ["gardens:flower_field"]
    .enemy = "foundation:Scout"
    .chance = 0.12
    .salt = 2401
```

Sem `biomes`, mantém o comportamento anterior. Uma lista vazia não permite
encontros dessa definição. O filtro consulta o bioma do ponto de surgimento.

## Saves, compatibilidade e limites

Mantemos `version: 1` com campos opcionais. Quando `structure_mode` é omitido,
o modo `legacy` conserva o sorteio original: uma estrutura por chunk e células
x/z limitadas a -3…3. Campos avançados de distribuição nesse modo são rejeitados,
para evitar uma configuração aparentemente aceita que não tem efeito.

O mundo salva uma cópia completa das definições, já com herança e extensões
resolvidas. Mudanças posteriores nos mods afetam mundos novos; os existentes
preservam terreno, biomas e distribuição. Assets e scripts continuam vindo dos
mods instalados, sujeitos à conferência de versões existente. Essas opções não
implementam migração de mundos, rios, líquidos ou cavernas procedurais.

Limites globais de conteúdo: 128 blocos, 64 estruturas, 64 biomas e 32 geradores;
até 32 escolhas de bioma por gerador. As estruturas são voxels; modelos GLB/Studio
e entidades podem ser posicionados pelos scripts consultando estas APIs, mas
não são células do `world.json`.


## Extensões volumétricas e edição

Geradores também aceitam `voxel_rules`, inclusive via `generator_extensions`.
O contrato de ruído 3D, filtros, edição em lote, histórico e navegação está em
[07-MOD-PLATFORM.md](07-MOD-PLATFORM.md). As regras são opcionais e entram no snapshot;
as planícies oficiais continuam sem cavernas. Blocos não sólidos não implicam
simulação de fluidos.

Streaming, edições paginadas e ferramentas de desenvolvimento estão em
[08-STREAMING-AND-DEVELOPER-TOOLS.md](08-STREAMING-AND-DEVELOPER-TOOLS.md).
