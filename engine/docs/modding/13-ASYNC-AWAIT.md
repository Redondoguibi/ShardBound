# Async/await em `.sl`

Extensão aditiva à linguagem v0, sem alteração do manifest, da ABI nativa ou do formato de saves. `async func` declara uma função cooperativa; chamá-la devolve um `Task` imediatamente. Seu corpo começa em uma atualização posterior do jogo. `await` obtém o resultado de uma tarefa, suspendendo a função enquanto o resultado não estiver disponível.

```text
@Globals
job = null
completed = 0

async func delayed_steps(count = 3):
    for index in number_range(count):
        await sleep(0.25)
        completed = completed + 1
    return completed

@Events
on_ready():
    job = delayed_steps()
```

`await` só é permitido no corpo de uma função `async`. Uma função comum aninhada não herda essa permissão. Parâmetros padrão e campos de definições de conteúdo continuam síncronos. Fora do novo contexto, identificadores existentes chamados `async` e `await` continuam utilizáveis; `async func` introduz a nova declaração. Dentro de uma função assíncrona, `await` é operador.

## Expressões e escopos

São aceitos parâmetros padrão, funções aninhadas, `if`/`else`, `for`, retornos simples/múltiplos e `await` em argumentos, listas, condições e expressões. Variáveis e o ponto exato da execução sobrevivem à suspensão. `for` mantém a mesma semântica de snapshot da lista do interpretador síncrono. Atribuições continuam procurando uma variável existente nos escopos externos antes de criar uma local.

```text
async func calculate():
    first = delayed_steps(2)
    second = delayed_steps(4)
    return (await first) + (await second)
```

As duas tarefas são agendadas antes da primeira espera. Não há threads de script. `await` tem precedência superior a multiplicação e inferior ao acesso a membros/chamadas; para acessar um membro do resultado, use `(await job).campo`. `and`/`or` preservam o curto-circuito: `false and await funcao()` não chama a função.

## Esperas e controle

| API/propriedade | Contrato |
| --- | --- |
| `sleep(segundos)` | Retorna uma tarefa de espera. Número finito entre 0 e 3.600, medido em tempo de jogo. |
| `next_frame()` | Retorna uma tarefa que só pode concluir em uma atualização posterior. Equivale à espera de duração zero. |
| `task_cancel(tarefa)` | Cancela somente tarefas do próprio mod/sessão. Retorna `true` se cancelou; `false` se já terminou. |
| `tarefa.id` | Identificador da sessão, útil para diagnóstico. |
| `tarefa.status` | `queued`, `running`, `waiting`, `completed`, `failed` ou `cancelled`. |
| `tarefa.done` | Verdadeiro para qualquer estado terminal. |
| `tarefa.failed` / `tarefa.cancelled` | Consulta o estado correspondente. |
| `tarefa.result` | Resultado concluído; gera diagnóstico se pendente, falho ou cancelado. |
| `tarefa.error` | Mensagem da falha; string vazia nos demais estados. |

Propriedades são somente leitura. Resultados contendo listas/registros são snapshots, copiados na conclusão e na leitura/`await`; entidades continuam referências da engine. Resultados não podem conter handles de outras tarefas; use `return await outra_tarefa`. O retorno sem valor conclui com `null`. `await` de uma tarefa já concluída continua sem espera adicional.

Cancelar uma tarefa não desfaz efeitos já realizados nem cancela automaticamente suas tarefas filhas. Ao cancelar a própria tarefa em execução, ela para na próxima fronteira segura de instrução. Guardar handles permite cancelar explicitamente o trabalho filho. A linguagem ainda não possui `try/catch`: falhas são inspecionadas por `done`, `failed` e `error` antes de ler o resultado.

## Integração e limites

- O jogo avança o agendador uma vez por atualização ativa, antes da fase extensível `update`. Pausa, minimização e ausência de mundo ativo suspendem o avanço e o relógio das tarefas.
- Tarefas criadas durante um avanço começam no próximo avanço. A ordem de visita gira entre tarefas para evitar inanição. Dependências prontas podem liberar uma tarefa neste avanço ou no seguinte, conforme a ordem de visita; não dependa dessa ordem para sincronizar efeitos.
- Cada tarefa recebe até 256 instruções da máquina de continuação por avanço; o conjunto recebe até 4.096. Expressões e chamadas comuns permanecem protegidas pelo orçamento compartilhado de 100 mil passos do interpretador por avanço. Um trecho síncrono que excede esse orçamento falha com `SCRIPT304`; ele não é interrompido e retomado no meio.
- Há até 64 tarefas por mod e 256 na sessão. Esperas também contam. Estados terminais são retirados da fila ao final do avanço; handles retidos continuam consultáveis. O código de uma função assíncrona tem até 65.536 instruções; resultados têm profundidade máxima de 64 e sua validação consome o orçamento de execução.
- Ciclos de espera, tarefas de outra sessão, valores que não sejam `Task` e parâmetros inválidos geram diagnósticos. Falhas mantêm arquivo/linha/mod e identificam a tarefa no relatório do desenvolvedor; `developer_report().tasks` mostra as tarefas pendentes.
- Sistemas recorrentes, hooks e extensões de fases da engine exigem callbacks síncronos. Eles podem iniciar uma função assíncrona explicitamente. `on_event` aceita uma função assíncrona para agendar uma tarefa por emissão, sujeito aos limites normais. Não use isso em eventos de alta frequência sem controlar tarefas pendentes.
- As APIs da engine continuam na thread principal. Async não oferece paralelismo CPU, rede, acesso irrestrito a arquivos ou execução de shaders `.sl`. Uma chamada nativa longa não pode ser preemptada pelo agendador.
- Tarefas são transitórias: não são serializáveis e não entram em saves. Salve dados de progresso pelo mecanismo de estado do mod e reinicie o trabalho em `on_loaded`/`on_ready`. Fechar/recarregar o mundo ou destruir o runtime cancela tarefas restantes; não executa suas continuações.

Para distribuir trabalho pesado, coloque `await next_frame()` entre lotes e mantenha funções síncronas curtas. Laços diretamente no corpo `async` também cedem automaticamente ao atingir a fatia de instruções. Exemplo instalável em [examples/async-tasks](../../examples/async-tasks/main.sl).

## Host C++

`Runtime::advanceTasks(delta)` avança tarefas cooperativas e retorna os diagnósticos daquele avanço. Não deve ser chamado de dentro de um callback/dispatch, nem concorrer em outra thread. `Runtime::cancelTasks()` cancela e libera a fila; o destrutor também o faz. `Runtime::emit` continua síncrono e não avança tarefas implicitamente, preservando a semântica de eventos e hooks existente.
