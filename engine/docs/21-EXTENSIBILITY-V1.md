# Etapa 12 — extensibilidade v1

Contrato da primeira implementação completa dos itens da etapa 12. Consolida as
decisões que estavam abertas para componentes, persistência e C++ nativo, sem
alterar a semântica normativa de `from`/`modify` da linguagem v0. Não representa
conclusão do jogo da etapa 13 nem de todos os recursos futuros da plataforma.

## Componentes customizados

Um componente é registrado por ID universal, versão inteira positiva e objeto
de defaults. O armazenamento pertence à entidade, sob EnTT privado. Não existem
tipos EnTT, Jolt ou ponteiros de bibliotecas nas APIs de modding.

```text
@Components
component Counter:
    .version = 2
    Defaults:
        .count = 0

register_component(Counter)

func migrate_v1(old):
    old.count = old.count + 10
    return old

register_migration(Counter, 1, migrate_v1)
```

Registro de tipos e migrações exige o namespace do mod que executa a chamada.
Versões vão de 1 a 1.000.000. Defaults e estado são objetos de dados: null, bool,
número finito, string, lista ou objeto. Símbolos são convertidos em strings de
IDs. Funções, EntityRefs/HostObjects e ciclos não são dados persistíveis. Limites:
64 níveis de dados e 16 MiB por documento serializado.

| API | Efeito |
|---|---|
| `record()` | Cria um objeto vazio, preenchido por atribuições de campos. |
| `register_component(Type)` | Registra definição de `@Components`, com `version` e `Defaults`. |
| `register_migration(Type, from, function)` | Registra conversão de `from` para `from + 1`. |
| `add_component(entity, Type)` | Copia defaults; duplicação é erro. |
| `get_component(entity, Type)` | Migra se necessário e retorna cópia dos dados. |
| `set_component(entity, Type, data)` | Valida/migra o estado anterior e substitui os dados. |
| `has_component(entity, Type)` | Consulta presença, inclusive de componente órfão. |
| `remove_component(entity, Type)` | Remove explicitamente e retorna bool. |

As alterações de uma cópia retornada por `get_component` só passam a valer após
`set_component`. As migrações são sequenciais e executadas no primeiro acesso.
Uma cadeia inteira só é publicada se todas as conversões forem válidas. Falha,
migração ausente ou versão futura produz diagnóstico e preserva o registro
original. Um componente de mod ausente continua serializado sem executar código.
Não há inferência de migrações por versão comercial do pacote, nem tipagem rígida
de campos além do domínio de dados serializáveis.

## Eventos e hooks

`@Events` continua funcionando. APIs adicionais:

| API | Efeito |
|---|---|
| `on_event(name, function)` | Inscreve função e retorna token. |
| `emit_event(name, args)` | Emite com lista de argumentos. |
| `add_hook(name, function)` | Inscreve transformação e retorna token. |
| `apply_hook(name, value)` | Passa valor pelas transformações registradas. |
| `unsubscribe(token)` | Remove inscrição de evento ou hook. |

A ordem é a ordem de registro, derivada da ordem de dependências dos mods e da
execução de cada arquivo. O despacho usa uma cópia da lista: inscrições e remoções
durante um callback afetam o próximo despacho. Eventos isolam falhas entre
handlers, retornando diagnósticos ao host. `emit_event` entrega todos e propaga
o primeiro erro ao script emissor. Hooks propagam falha sem aplicar a operação
de gameplay que os chamou. A transformação deve retornar o novo valor.

O hook `core:damage` recebe `{target: EntityRef, amount: Number}` antes do dano.
A API valida o resultado, aplica dano e emite `on_damage(target, applied)`.
Isso permite ajustar ou cancelar dano retornando amount=0 sem editar HealthSystem.
Hooks adicionais podem ser definidos por sistemas/mods com `apply_hook`.

Lifecycle: `on_start`, `on_fixed_update(delta)`, `on_update(delta)`, `on_shutdown`,
`on_before_save`, `on_saved`, `on_loaded`. Dispatch e chamadas possuem limites de
recursão e budget de interpretação. Callbacks nativos são código confiável e não
podem ser interrompidos pelo budget do interpretador.

## Patch e override explícito

`modify` mantém seu merge recursivo e diagnóstico de conflitos. Agora também há:

```text
data = get_definition("Entities", "my_mod:Enemy")
data.Attributes.health = 200
replace_definition("Entities", "my_mod:Enemy", data)
```

`get_definition` retorna uma cópia. `replace_definition` substitui explicitamente
o objeto inteiro. Writes anteriores sobre o recurso, mesmo em campos diferentes,
conflitam com o replace quando feitos por outro mod sem dependência explícita do
substituidor. Após replace, patches posteriores também respeitam esse conflito.
Proveniência de writes iguais de mods distintos é preservada, evitando que um
terceiro overwrite escape da verificação. Não há prioridade silenciosa por nome
de arquivo. Alterar definições não reconfigura automaticamente instâncias existentes.

## Saves compatíveis com mods

`save_game("slot")` e `load_game("slot")` agendam uma operação para um ponto seguro,
após todos os callbacks do startup/frame. Só uma operação pode ficar pendente;
não se permite encadear novas operações pelos próprios hooks de save/load.

Saves ficam em `user/saves/<slot>.json`, separados dos sources dos mods. Slot usa
letras minúsculas, dígitos, `_` e `-`, até 64 caracteres. O formato tem identificador
`shardbound-save` e versão 1. Limites de leitura: 16 MiB e 4.096 entidades.

São persistidos:

- GUID persistente, mod proprietário, definition ID, nome, Active e hierarquia;
- Transform, model ID, Health, inventário e marcador de interação;
- componentes customizados com sua versão e dados;
- box físico, estado dinâmico/estático, velocidade e seleção/velocidade do player;
- versões dos mods e estado de módulo fornecido pelo autor.

`set_save_state(data)` e `get_save_state()` acessam exclusivamente o namespace do
mod que executa a função. O autor registra seu estado no `on_before_save`, incluindo
seu próprio campo de versão quando precisar migrá-lo. Falhas nesse evento cancelam
a gravação. Não se serializam closures, globais automaticamente, ponteiros, áudio
em reprodução ou recursos de GPU.

Cada gravação usa um temporário no mesmo diretório e substituição atômica do
arquivo. Quando o slot já existe, a versão anterior fica em `<slot>.json.bak`.
Não há fallback automático para backup: ele pode ser inspecionado pela CLI e
restaurado explicitamente pelo usuário. A garantia é de substituição do arquivo,
não de recuperação universal após falha física de disco.

Load constrói um World e PhysicsSystem temporários, valida entidades, hierarquia,
componentes nativos, colisores e player, e só então troca o estado ativo. Save
corrompido ou de formato futuro gera erro sem substituir o mundo atual. Falha
posterior de um handler `on_loaded` é reportada; não desfaz um load já concluído.

Política de conteúdo indisponível:

- mod removido ou definition ausente: entidade e descendentes ficam como registros
  órfãos, sem execução/render/física, preservados nas próximas gravações;
- reinstalar o mod e carregar novamente permite restaurar esses registros;
- componente desconhecido ou futuro e itens indisponíveis permanecem nos dados;
- modelo ausente vira handle vazio, com ID preservado e diagnóstico;
- versão de pacote alterada gera aviso; migrações são dirigidas pela versão de
  cada componente, não executadas implicitamente pelo número do pacote;
- estado de módulo ausente permanece no envelope, sem código daquele mod rodar.

EntityId de runtime muda após load; GUID persistente não muda. `entity_key(entity)`
retorna o GUID, e `get_entity_by_key(key)` resolve a nova instância ou null. Guarde
essas chaves como strings no estado do módulo e recupere EntityRefs em `on_loaded`.
Referências antigas são inválidas e produzem diagnóstico. `get_entity(name)` é
adequado apenas quando o nome for único. O estado transitório de systems novos
deve ser reconstruído por seus hooks; futuros tipos de física exigirão extensão
versionada do formato.

## C++ nativo e SDK

O manifest aceita `"native": "native/my_mod.dll"`. Somente DLL Windows x64 é
suportada. O módulo é carregado após os scripts do seu pacote, na ordem resolvida
de mods, antes de `on_start`. Ciclos de dependências não inicializam DLLs.

Por padrão DLLs ficam desabilitadas, com diagnóstico, mantendo scripts/assets
independentes disponíveis. Use `--allow-native` para habilitar código confiável.
Native executa dentro do processo com acesso de código C++; não há sandbox nem
isolamento de access violations. A busca de dependências da DLL é restrita ao
diretório da DLL e System32, sem busca pelo diretório corrente.

O SDK está em `sdk/native/shardbound.h`: ABI C versão 1, structs com tamanho/versão,
inteiros de largura fixa, callbacks `__cdecl`, JSON UTF-8 e buffers pertencentes
ao chamador. Export obrigatório: `sb_mod_init`. Negociação incompatível é rejeitada.
Não trafegam STL, exceções, objetos EnTT/Jolt ou memória que precise ser liberada
em outro allocator. A DLL é responsável por não lançar exceções pela ABI e por
liberar seus recursos se a inicialização falhar.

`SbHost.call` oferece operações `log`, `entity.find`, `entity.create`,
`entity.destroy`, `component.define/add/get/set/remove` e `state.get/set`.
Entidades são IDs de runtime codificados como strings JSON. Definição de componente
exige namespace próprio. Estado de módulo é limitado ao próprio módulo.
Operações mutáveis exigem buffer de pelo menos 32 bytes antes de qualquer efeito;
as respostas dessas operações cabem nesse tamanho. Leituras aceitam consulta de
tamanho via `SB_BUFFER_TOO_SMALL` e `required` (inclui terminador NUL).

`subscribe`/`unsubscribe` conectam eventos e hooks ao mesmo dispatcher dos scripts.
Eventos recebem array JSON; hooks recebem/retornam valor JSON. EntityRefs em
payloads usam `{"$entity":"runtime-id"}` e são validadas ao retornar de um hook.
Output de hook está limitado a 1 MiB; payloads são emprestados durante a chamada.
Todas as chamadas devem ocorrer na thread do runtime. SDK v1 não oferece worker
threads, hot reload nem compatibilidade binária com estruturas internas C++.

Shutdown ocorre em ordem reversa de carga: callback da DLL, remoção de inscrições,
depois FreeLibrary, enquanto serviços do runtime ainda existem. Inscrições de uma
inicialização rejeitada também são removidas. O core não desfaz mutações arbitrárias
que uma DLL já tenha feito durante init; a extensão é responsável por seu lifecycle.

## Exemplos e CLI

```powershell
./tools/build.ps1 debug
./build/debug/game.exe --mods examples/extensions
./build/debug/game.exe --mods examples/native --allow-native
./build/debug/modcheck.exe ./build/debug/examples/native --json
./build/debug/modcheck.exe --save ./build/debug/user/saves/checkpoint.json --json
```

No exemplo `extensions`, E salva e mouse esquerdo carrega. Ele registra componente,
hook de dano e estado de módulo. O exemplo C++ é compilado pelo CMake em DLL real,
registra componente e cria uma entidade pelo SDK. A CLI verifica existência e
contenção da DLL, sem executar código nativo. `--save` inspeciona envelope/versão
e mostra mods/contagem de entidades; não executa migrações nem restaura o mundo.

## Critérios executáveis

Extensões aditivas usadas pela Etapa 13: tint RGB opcional por modelo,
`player.camera` opcional e evento `on_validate_load(state_por_mod)` antes
da troca do mundo. Validadores não devem mutar o runtime; podem abortar com
`script_error(message)`. Saves antigos usam tint branco e câmera histórica.
`damage_from` mantém `core:damage` e acrescenta o evento `on_damage_dealt`;
`damage(target,amount)` e `on_damage(target,applied)` continuam compatíveis.
Detalhes e limites em [23-PLAYABLE-SLICE.md](23-PLAYABLE-SLICE.md).

O teste `extensions` cobre hooks de dano/eventos, unsubscribe, componentes e
migração, save/load com mundo/física/player, backup, remoção/reinstalação de mod,
preservação de dados desconhecidos/futuros, rollback de load inválido, conflitos de
override, rejeição de ciclos/referências persistíveis, DLL real, ABI inválida e
remoção de callbacks no unload. Há smoke tests separados com DLL habilitada e
desabilitada, exemplo de checkpoint, modcheck nativo e inspeção de save.
