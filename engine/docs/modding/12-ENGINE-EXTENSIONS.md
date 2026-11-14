# Extensões da engine — API 1

Esta API permite estender e substituir fases do núcleo por scripts ou DLLs. Os sistemas de gameplay, geração, widgets e shaders existentes continuam disponíveis. Não altera o executável nem expõe ponteiros internos. A ABI nativa permanece 1; a versão desta API é negociada separadamente.

## Descoberta e configuração

`engine_info()` retorna `api`, `stages`, `extensions` e os últimos 64 `diagnostics`. Cada fase informa sua versão e provedor ativo. Cada extensão informa proprietário, modo, prioridade, ativação, chamadas, erros e custos CPU em milissegundos. F3 mostra o total e os erros; o relatório de desempenho inclui `engine_at_export`.

`engine_configuration()` retorna os valores atuais de `physics`, `render` e `scheduler`. `configure_engine(servico, opcoes)` altera somente os campos enviados. Tipos, campos desconhecidos e limites são verificados antes da mudança. Configurações conflitantes exigem dependência do mod que escreveu antes. Campos independentes de objetos podem ser alterados por mods diferentes; listas são substituídas integralmente. Essa proveniência cobre `configure_engine`; APIs anteriores como `set_environment` conservam seu contrato anterior.

| Serviço | Campos |
| --- | --- |
| `physics` | `gravity`: aceleração para baixo entre 0 e 100; `max_slope`: inclinação transitável entre 1 e 80 graus. |
| `render` | `environment`: campos opcionais `enabled`, `shadows`, `sky`, `sun`, `ambient`, `fog_start`, `fog_end`; `lights`: lista completa de até 32 luzes pontuais. |
| `scheduler` | `fixed_order`: uma permutação exata de `fixed`, `movement`, `physics`. Padrão: nessa ordem. |

Cores do ambiente são `vec3` com canais de 0 a 2; fog de 0 a 10.000, com fim maior que início. Cada luz exige `position: vec3`, `color: vec3` (0 a 32), `range` (0,01 a 10.000) e `intensity` (0 a 64). Posições são locais à origem flutuante, entre ±100.000. Atualize luzes móveis usando a posição local atual da entidade.

```text
@Events
on_start():
    physics = record()
    physics.gravity = 3
    physics.max_slope = 45
    configure_engine("physics", physics)
```

As configurações pertencem à sessão. A troca de mundo restaura os padrões e os mods do próximo mundo aplicam suas regras. Carregar um snapshot mantém as regras físicas do runtime atual, inclusive durante a preparação transacional. Se uma configuração dinâmica deve sobreviver ao reinício, persista os valores no estado do mod e reaplique-os em `on_loaded`; callbacks e ponteiros nunca são salvos.

Mudar a aceleração gravitacional acorda corpos dinâmicos em repouso, inclusive os que adormeceram sob gravidade zero. Reaplicar a mesma gravidade não os acorda repetidamente.

Essas regras físicas afetam corpos do backend físico. Projéteis que declaram sua própria gravidade na API de projéteis conservam essa configuração, assim como fatores gravitacionais por corpo.

## Registro de fases

`register_engine_extension(id, fase, callback, opcoes?)` exige ID no namespace do mod e função que receba um contexto. Opções:

- `api`: 1, padrão 1; outra versão é recusada;
- `mode`: `before` (padrão), `after` ou `replace`;
- `priority`: inteiro de -100 a 100, padrão 0;
- `replaces`: obrigatório em `replace`, proibido nos outros modos.

Há até 32 extensões por mod e 256 por sessão. `before` executa antes do provedor, `after` depois; prioridade crescente e depois ID definem a ordem dentro de cada grupo. Os sistemas registrados com `register_system` continuam usando `configure_system` para dependências, intervalos e ordenação dentro de seus eventos.

| Fase | Comportamento padrão e contexto |
| --- | --- |
| `fixed` | Despacha `on_fixed_update(delta)` e sistemas de gameplay fixos. |
| `movement` | Move o jogador; contexto contém `right`, `forward` e `player_key` persistente. |
| `physics` | Avança a simulação física com `delta`. |
| `update` | Despacha `on_update(delta)` e sistemas de gameplay por quadro. |
| `streaming` | Atualiza chunks; contexto contém `distance` e `center`. |
| `render` | Desenha a cena; contexto contém `camera`, `world`, `terrain`, `effects` e `ui`. |

Todos os contextos também contêm `api: 1` e `stage`. São cópias: mudar o objeto recebido não muda o despacho. As três fases fixas respeitam `fixed_order`, fotografada no início de cada passo. O loop permanece em 60 passos fixos por segundo. `streaming` exige mundo procedural em jogo e simulação não pausada. O menu do título/pausa é desenhado pelo núcleo, fora da fase extensível. O `update` mantém a política anterior de eventos durante a pausa gerenciada pelos mods.

`engine_extension_enabled(id, booleano)` e `unregister_engine_extension(id)` só afetam extensões do próprio mod. Registros, remoções e ativação valem no próximo despacho da fase; não modificam a fotografia já em execução.

## Substituição explícita

Para substituir o padrão, use `mode="replace"` e `replaces="core:default"`. Para substituir outro provedor ativo, indique seu ID e declare dependência do mod responsável. Não há substituição silenciosa. Ao remover/desativar o último provedor, o anterior volta a ser selecionado; se não houver outro ativo, volta o padrão.

O callback de substituição retorna `true` quando tratou a fase. `false` ou `null` mantém o comportamento padrão. Para executar o padrão com parâmetros modificados, chame `engine_default(opcoes?)`. Ele só pode ser chamado pelo proprietário do callback de substituição em execução, no máximo uma vez por despacho. Sempre delega ao núcleo, não ao provedor anterior. `before`/`after` não podem chamá-lo.

```text
@Globals
func air_control(context):
    actor = get_entity_by_key(context.player_key)
    options = record()
    if actor != null and not is_grounded(actor):
        options.right = context.right * 0.6
        options.forward = context.forward * 0.6
    engine_default(options)
    return true

@Events
on_start():
    options = record()
    options.api = 1
    options.mode = "replace"
    options.replaces = "core:default"
    register_engine_extension("meumod:air_control", "movement", air_control, options)
```

Parâmetros substituíveis:

- `fixed`/`update`: `delta` de 0 a 0,25 segundo; `physics`: de 0,000001 a 0,25. Isso altera o tempo daquela fase, não a frequência do loop.
- `movement`: `right`/`forward` entre -1 e 1. `player_key`, `stage` e `api` são somente leitura. Um controlador totalmente próprio pode usar a referência persistente e as APIs de movimento/teleporte existentes, retornando `true`.
- `streaming`: `distance` inteira entre 2 e 5; `center` nulo ou `vec3` local. Os limites de streaming e memória permanecem ativos.
- `render`: flags booleanas `world`, `terrain`, `effects`, `ui`; `camera` completa com `eye`, `target`, `near`, `far`, `fov`. Vetores são locais, near ≥ 0,001, far ≤ 100.000 e maior que near; fov em radianos entre 0,1 e 3. A direção não pode ser nula ou paralela ao eixo vertical usado como up.

Uma substituição de render que retorna `true` sem delegar omite a cena. O núcleo ainda produz um quadro válido com HUD/ferramentas e apresentação, evitando imagem e estatísticas GPU antigas. Para mudar o que é desenhado, use os filtros, câmera, entidades, modelos e shaders já disponíveis. `after` executa depois do desenho; alterações de iluminação nele afetam o próximo quadro.

## Falhas e limites de confiança

Uma exceção, retorno inválido ou orçamento de script esgotado desativa a extensão e registra diagnóstico. Outras extensões continuam. Uma substituição que falha antes de executar o padrão recebe o padrão no mesmo despacho. Se já o executou, a engine não repete física ou desenho. Falhas do próprio núcleo são propagadas, inclusive se uma DLL tentar ocultá-las. Reentrância entre fases é recusada.

O orçamento existente do interpretador também cobre chamadas aninhadas feitas por uma substituição. Custos CPU de wrappers incluem o tempo delegado ao núcleo; não são medições exclusivas nem atribuições de GPU por mod. Não há rollback geral de efeitos arbitrários já realizados pelo callback. Remover uma extensão não desfaz configurações ou entidades que ela criou; o mod deve restaurá-las quando necessário.

A API não substitui SDL/D3D11/Jolt por outro backend binário, não muda a ABI em memória e não entrega acesso irrestrito ao sistema operacional aos scripts. Persistência, apresentação, ciclo da janela e tarefas essenciais do host continuam no núcleo. DLLs continuam sendo código confiável dentro do processo, sem sandbox e sem interrupção pelo orçamento do interpretador.

## DLLs — ABI 1 preservada

As operações novas usam `SbHost::call`, com JSON UTF-8. `engine.info` e `engine.configuration` são leituras com consulta de tamanho e `SB_BUFFER_TOO_SMALL`. Mutações exigem pelo menos 32 bytes antes de qualquer efeito e retornam `null`:

| Operação | Requisição |
| --- | --- |
| `engine.info` / `engine.configuration` | `null` |
| `engine.configure` | `{"service":"physics","options":{"gravity":3}}` |
| `engine.register` | `{"id":"meumod:physics","stage":"physics","handler":"meumod:physics_hook","options":{"api":1,"mode":"replace","replaces":"core:default"}}` |
| `engine.default` | `{}` ou parâmetros válidos da fase atual |
| `engine.enabled` | `{"id":"meumod:physics","enabled":false}` |
| `engine.remove` | `{"id":"meumod:physics"}` |

Para registrar, primeiro inscreva `handler` pelo `subscribe` existente, com `hook=1`. O nome deve ser do próprio namespace. O callback recebe o contexto JSON e deve devolver `true`, `false` ou `null` em JSON terminado em NUL. Em observadores, o retorno é ignorado. Os handlers são resolvidos na execução; ausentes ou inválidos produzem diagnóstico e fallback. Toda chamada permanece na thread do runtime. As operações da engine deixam de estar disponíveis quando o descarregamento nativo começa.

O exemplo nativo executado pelos testes está em [native_probe.cpp](../../examples/engine-extensions/native_probe.cpp). Um mod de exemplo por scripts está em [examples/engine-extensions](../../examples/engine-extensions/README.md).
