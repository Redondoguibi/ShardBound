# Console de desenvolvimento e atributos

Em um mundo aberto, segure **Page Up + Page Down** para alternar o modo de desenvolvedor. O estado pertence à sessão e começa desligado em cada mundo. Repetições do teclado não alternam o modo novamente. Ao desligá-lo, a câmera normal retorna e comandos pendentes são descartados.

Com o modo ativo, `/` abre o console. Enter envia, Escape fecha, Tab/Shift+Tab percorrem sugestões e as setas verticais percorrem os últimos 128 comandos. Há seleção com Shift, Home/End, Delete/Backspace e Ctrl+A/C/X/V, respeitando pontos de código UTF-8. A roda percorre a saída. Não há avaliação de código `.sl` pelo console. O mundo continua simulando enquanto se digita, com as ações do jogador capturadas.

## Comandos internos

`<...>` indica obrigatório; `[...]`, opcional. IDs vêm dos mods carregados.

| Sintaxe | Comportamento |
|---|---|
| `/help [comando]` | Ajuda geral ou sintaxe de um comando. |
| `/list` | Todos os comandos registrados, inclusive os de mods. |
| `/list <items\|entities\|blocks>` | IDs de conteúdo registrados. `air` também aparece nos blocos. |
| `/camera <default\|thirdperson\|free>` | Câmera normal, terceira pessoa orbital ou câmera livre. |
| `/pos` | Posição global do jogador, incluindo a origem flutuante. |
| `/tp <x> <y> <z>` | Teleporte global; `~`, `~5` e `~-5` são relativos ao jogador. |
| `/give <mod:item> [quantidade]` | Adiciona cópias ao inventário; padrão 1. Não cria instâncias de equipamento automaticamente. |
| `/spawn <mod:entity> [x y z]` | Usa o `spawn` normal e emite `on_developer_spawn(entidade)` para inicialização pelo conteúdo. Sem coordenadas, cria duas unidades ao lado do jogador. |
| `/set <x1> <y1> <z1> <x2> <y2> <z2> <mod:block>` | Região inclusiva, até 4096 blocos, uma transação. `air` remove. Coordenadas inteiras; a base de `~` usa o bloco ocupado pelo jogador. |
| `/undo`, `/redo` | Histórico existente de até 16 edições de terreno. |
| `/seed` | Seed original, sem convertê-la em número. |
| `/attribute <mod:atributo> get` | Valor base e efetivo do jogador. |
| `/attribute <mod:atributo> set <valor>` | Altera a base persistente; equipamentos continuam compondo o resultado. |

Teleportes aceitam até 100000 unidades locais por operação; o limite global do terreno permanece ±10¹². A altura de edição continua 0–47. As edições e mudanças de inventário/atributos entram no salvamento normal do mundo. Câmeras, modo de desenvolvedor, histórico e saída não entram no save. Não foram adicionados comandos de cura, invulnerabilidade, velocidade, bloco individual, inspeção ou salvamento.

Terceira pessoa: segure o botão direito e mova o mouse para orbitar; a roda controla o zoom. A movimentação acompanha a orientação horizontal da câmera, e um raio com margem reduz o atravessamento de paredes. Câmera livre: WASD, Space para subir, Q para descer, Shift para acelerar; botão direito para olhar. Seu alcance local é ±100000. O streaming visual acompanha essa câmera; a física mantém suporte sob os atores. Escape continua disponível para a pausa. Perder o foco ou abrir uma interface modal libera o mouse.

## Comandos de mods

Registre no carregamento do mod ou em um evento. Cada ID precisa pertencer ao namespace do autor; nomes internos não podem ser substituídos. Até 256 comandos por sessão, com até 32 argumentos cada.

```sl
func set_energy(args, actor):
    set_attribute(actor, "example:energy", args.value)
    return "Energia alterada."

@Globals
arg = record()
arg.name = "value"
arg.type = "number"
arg.optional = false
options = record()
options.description = "Altera a energia do jogador"
options.arguments = [arg]
register_command("example:energy", options, set_energy)
```

`register_command(id, options, handler)` recebe `description` e `arguments`. Cada argumento tem `name`, `type` e `optional`. Opcionais devem estar no final. Tipos: `string`, `number`, `integer`, `item`, `entity`, `block`, `attribute`, `enum`. Para `enum`, forneça `choices`, uma lista de 1–64 valores. Tipos de conteúdo validam o ID e fornecem sugestões automaticamente. Argumentos ausentes chegam como `null` no record do callback. Números chegam como Number; outros argumentos como String. Strings entre aspas aceitam espaços e escapes de aspas e barra invertida.

O handler recebe `(argumentos, jogador)` e retorna texto ou `null`. Roda na thread principal em um ponto seguro, com o orçamento e o namespace normais do runtime. Falhas aparecem no console; não escapam para o loop do jogo. `unregister_command(id)` remove apenas comandos do próprio mod, inclusive durante o callback. A linha aceita 1024 bytes; o retorno de mods, 16384 bytes. A saída é limitada a 2048 linhas. Registro e sugestões não executam os handlers.

`on_developer_spawn(entidade)` permite aplicar física, componentes, saúde e visuais. O mod `foundation` inicializa seus inimigos dessa forma. Entidades genéricas continuam seguindo seu contrato normal de `spawn`: o engine não deduz políticas de IA a partir de campos arbitrários. Se um handler falhar, o console informa que a entidade foi criada, mas a inicialização falhou; efeitos arbitrários de scripts não têm rollback geral.

## Atributos

```sl
@Globals
energy = record()
energy.default = 20
energy.min = 0
energy.max = 100
register_attribute("example:energy", energy)
```

APIs:

- `register_attribute(id, {default,min,max})`: namespace do autor, até 1024 definições; ID de até 64 bytes.
- `get_base_attribute(entity,id)`: base salva ou default registrado.
- `get_attribute(entity,id)`: `(base + soma de bônus) × produto de multiplicadores`, limitado ao intervalo registrado.
- `set_attribute(entity,id,value)`: valida intervalo e overflow dos bônus antes de publicar a base.
- `reset_attribute(entity,id)`: remove o override e volta ao default.
- `on_attribute_changed(entity,id,base_anterior,base_nova)`: notificação depois da mudança. Falhas de callbacks são diagnosticadas; a mudança publicada permanece.
- `set_maximum_health(entity,maximum)`: ajusta o máximo e limita a vida atual sem curar. Intervalo `(0,1000000]`.

As bases ficam no componente reservado `core:attributes`, versão 1, com até 64 overrides por entidade. Saves antigos sem esse componente continuam válidos. IDs de mods ausentes são preservados. A carga valida os dados antes de trocar o mundo vivo. Não escreva diretamente nesse componente; use as APIs.

O campo `default` também pode ser uma referência `{category,id,field}` a um campo numérico de uma definição. Essa referência é resolvida no uso, depois das alterações dos mods dependentes, com os mesmos limites de validação. Não há resolução recursiva de referências. O mod base usa referências a `Entities/foundation:Actor` para velocidade, salto e vida máxima. Um mod pode substituir `default` por um número para fixar seu próprio valor; uma base explicitamente salva por entidade continua tendo prioridade sobre qualquer default. Defaults numéricos anteriores mantêm seu comportamento.

Bônus de instâncias e afixos usam o mesmo ID do atributo. `equipment_stat(entity,id,fallback)` também respeita uma base explicitamente salva para esse ID, preservando o comportamento dos stats antigos sem override. A aplicação de atributos a regras concretas continua pertencendo aos mods.

O mod base registra `foundation:movement_speed`, `foundation:jump_speed`, `foundation:max_health`, `foundation:damage_multiplier` e `foundation:armor_bonus`. Os dois últimos compõem os cálculos de dano e armadura existentes, preservando os bônus antigos `damage` e `armor`. Para modificar defaults desse contrato, altere as definições da categoria `Attributes`; os limites são revalidados no uso. Mudar vida máxima não recupera vida.

Exemplos: `/attribute foundation:movement_speed set 9`, `/attribute foundation:max_health get`.
