# Streaming, persistência e ferramentas de desenvolvimento

Complementa [07-MOD-PLATFORM.md](07-MOD-PLATFORM.md). As APIs anteriores continuam
disponíveis; as opções novas são opt-in quando alteram o comportamento de um mod.

## Streaming do mundo

Chunks visuais distantes são gerados por um worker de CPU. Esse worker recebe uma
janela imutável de referências às edições e lê páginas frias no worker. Usa somente o gerador determinístico: nunca chama
scripts, renderer, assets ou ECS. Uploads e criação de corpos continuam na thread
principal. A fila mantém até 24 solicitações, com até oito resultados preparados.

Cada frame publica no máximo dois resultados e verifica um orçamento cooperativo
de quatro milissegundos entre publicações. Uma chamada ao driver não pode ser
interrompida no meio: isso não constitui garantia de frame abaixo de 4 ms.
Chunks necessários para apoiar atores físicos têm prioridade e fallback síncrono.
Spawn, teleporte e recuperação de terreno podem exceder o orçamento visual.
O carregamento inicial do jogo prepara o apoio físico e preenche a distância de
visão em segundo plano. `update(..., true)` permanece disponível em C++ para
carregamento imediato determinístico, usado também nos testes.

Edições e carregamentos de save invalidam a fila por revisão. Um resultado antigo
não pode substituir um chunk editado. Deslocamentos de origem não alteram as
coordenadas globais do trabalho. Solicitações fora da distância de visão são
descartadas; encerrar a sessão aguarda o worker antes de destruir seu gerador.

`world_streaming_stats()` retorna:

| Campo | Significado |
| --- | --- |
| `queued` | Solicitações pendentes, em execução ou prontas. |
| `ready`, `loaded` | Resultados preparados e chunks publicados. |
| `resident_pages`, `pages` | Páginas de edições em RAM e páginas indexadas. |
| `generated`, `discarded` | Gerações terminadas e trabalhos/resultados descartados. |
| `generation_ms`, `commit_ms` | Tempos acumulados de geração no worker e publicação visual na thread principal. Não representam o tempo total de frame. |

## Edições e saves por chunk

O limite global de 4.096 blocos editados foi removido. O limite de **4.096 células
por transação** permanece para conter o custo de uma chamada. Uma página corresponde
a um chunk 16×16×48 e armazena somente diferenças do mundo gerado.

O runtime mantém até 64 páginas de dados residentes e carrega outras sob demanda.
O índice de páginas cresce conforme as regiões modificadas. Edições preparam apenas
as páginas alteradas, sem copiar todas as edições do mundo; undo/redo guardam até
16 transações inversas, não snapshots completos do mapa.

Arquivos imutáveis em `terrain/` usam uma paleta de IDs e células locais. O nome
deriva do conteúdo; a leitura verifica seu hash e tamanho. Isso detecta corrupção
acidental, não é uma assinatura criptográfica. O checkpoint contém um descritor
compacto de índice externo, publicado somente após escrever os arquivos necessários. Alterar o
terreno depois de salvar não modifica as páginas de checkpoints anteriores.

Páginas e manifestos são validados antes da troca do mundo ativo. Arquivos ausentes,
corrompidos, IDs desconhecidos, coordenadas inválidas e caminhos impróprios fazem o
load falhar conservando o mundo anterior. Saves antigos com `core.blocks` continuam
carregando; o próximo save usa páginas. Saves novos requerem este runtime.

**Para copiar ou fazer backup de um mundo, copie sua pasta inteira**, incluindo
`terrain/`, arquivos de mundo e checkpoints. Copiar somente o JSON do checkpoint
não inclui suas páginas externas. Arquivos antigos são conservados para não romper
outros slots e backups. A seleção de mundos oferece backup completo, restauração
por snapshot e limpeza explícita de arquivos sem referência.

O índice de terreno versão 2 é externo ao envelope de 16 MiB do checkpoint. Esse
envelope continua limitando entidades/estados dos mods; metadados do índice crescem
na memória. Autosave e F5 usam snapshots com publicação no worker de I/O. Leituras
para suporte físico, operações síncronas explícitas e edições ainda podem bloquear.
Consulte [10-SAVES-COMPATIBILITY-AND-RECOVERY.md](10-SAVES-COMPATIBILITY-AND-RECOVERY.md)
para formatos, migrações, limites, recuperação e transações de carregamento de mods.

## Diagnósticos e inspeção

**F3** abre/fecha o painel de desenvolvimento durante a sessão. Mostra entidades,
fila de chunks, páginas residentes, os primeiros sistemas registrados e diagnósticos
recentes. A apresentação atualiza a cada 250 ms. É um painel de leitura e não pausa
o jogo nem captura seus controles.

- `developer_report()` retorna um record serializável com `systems`, `profiles`,
  `diagnostics` e `registry`.
- `systems` informa ID, owner, fase, habilitação, intervalo, prioridade, dependências,
  arquivo/linha de registro, chamadas, erros e custos `last_ms`, `peak_ms`, `total_ms`.
- `profiles` mantém os acumuladores por mod de eventos, hooks, erros e milissegundos.
- `diagnostics` conserva os últimos 256 erros registrados, com código, mensagem,
  mod, recurso, arquivo, linha e coluna. Inclui erros de carga/execução e dispatch.
- `registry` lista definições, suas origens e caminhos modificados por cada mod.
  Escritas conflitantes continuam sendo rejeitadas pelo contrato de dependências;
  o diagnóstico identifica os mods e o caminho em conflito.
- `inspect_entity(entity)` retorna cópias de identidade, componentes customizados
  com versão/dados, inventário e os componentes de apresentação/gameplay presentes.
  Referências a pais usam identidade persistente.
- `write_debug_report()` grava o relatório com as entidades persistentes em
  `diagnostics/report.json`, dentro da pasta de saves da sessão, e retorna o caminho.
  Não substitui um slot de save chamado `diagnostics`.

```text
@Globals
report = developer_report()
hero_snapshot = inspect_entity(hero)
report_file = write_debug_report()
streaming = world_streaming_stats()
```

Execute a inspeção quando necessário. Exportar todos os componentes é uma operação
explícita e pode ser caro em mundos com muitas entidades; não exporte a cada frame.
Os tempos medem CPU do host, não GPU. O relatório não reexecuta nem recarrega mods.

## Transferência de equipamentos

`transfer_item_instance(source, target, instance_id)` transfere uma instância
**desequipada**, junto com uma cópia de seu item no inventário. Preserva durabilidade,
bônus e afixos. Retorna o novo ID no destinatário, ou null em caso de veto. IDs são
locais ao proprietário e não devem ser tratados como identidade global.

Hook `core:transfer_instance`: `{source,target,instance,allow}`. Após o hook, o
runtime verifica novamente os proprietários, instância, reservas, capacidade e
overflow. Inventários, componentes e seu índice são publicados em uma transação.
Um erro de validação não aplica metade da transferência. Efeitos que o próprio hook
executar não são revertidos por essa transação.

Evento posterior: `on_instance_transferred(source,target,old_id,new_id)`.
Desequipe com `equip_instance(source, slot, null)` antes de transferir. O destinatário
decide quando e onde equipar a instância recebida.

## Navegação com obstáculos móveis

Opções novas de `terrain_path`, sem mudar os valores padrão anteriores:

```text
options.avoid_entities = true
options.ignore = [hero]
path = terrain_path(start, goal, options)
```

`avoid_entities` considera os volumes dos corpos físicos presentes, além do terreno,
durante essa consulta. `ignore` aceita até 16 entidades; normalmente inclui o
próprio agente e, conforme o gameplay, seu alvo. Sem isso, um endpoint ocupado pode
ser inválido. Movimentos posteriores não mudam um caminho já retornado: recalcule-o
com uma frequência adequada. Agentes continuam tendo uma célula de largura; não é
um sistema de multidões nem uma reserva temporal de caminhos.

## Edição de texto, rolagem e arraste

Escape entrega `cancel` à tela de widgets, sem abrir simultaneamente a pausa do
frontend. O mod decide como fechar sua tela; sem widgets, a pausa normal é retomada.

Campos `input` agora têm cursor e seleção por mouse/arraste, setas, Home/End e Shift;
Ctrl+A seleciona tudo, Ctrl+C copia, Ctrl+X recorta e Ctrl+V cola. Backspace/Delete
removem codepoints inteiros de UTF-8. A seleção não implementa segmentação de
graphemes combinados. Texto colado é validado, controles são removidos e o limite
de 1.024 bytes é respeitado sem cortar uma sequência UTF-8. Há rolagem horizontal
interna para manter o cursor visível. A integração padrão usa o clipboard do SDL.

Telas aceitam `content_height`, em pixels lógicos, entre a altura visível e 16.384.
Layouts em coluna distribuem seus widgets nessa altura; posições verticais de
layout absoluto também são relativas a ela. A roda do mouse rola o conteúdo,
com recorte de renderização e hit testing. Tab/Shift+Tab revelam o widget focado.
`scroll_widgets(screen_id, pixels)` define o deslocamento vertical do próprio mod
e retorna o valor limitado ao conteúdo. A barra lateral indica a posição.

Sliders acompanham o ponteiro enquanto o botão permanece pressionado, inclusive
fora de sua área. Botões e painéis podem usar `draggable: true`, `payload` com dados
serializáveis e `drop_target: true` para receber um arraste. A origem envia
`drag_start` após mover quatro pixels; um simples clique em botão arrastável ativa
no release. Durante o arraste aparece um visual acompanhando o ponteiro.

O destino recebe `on_ui(screen,widget,"drop",value)` com
`{source_screen,source_widget,payload,x,y}`. A origem recebe `drag_end` com os mesmos
campos e `delivered` indicando se houve um destino habilitado. Isso confirma a
entrega do evento, não a aplicação de uma regra de gameplay. Perder o foco da janela
gera `drag_cancel`; remover a tela de origem encerra o arraste. Modais e widgets
sobrepostos bloqueiam destinos inferiores.

Para inventários, use chaves persistentes e IDs de instância no payload, resolva as
entidades no handler e chame `transfer_item_instance`. O handler deve validar o
item atual: o payload é um snapshot, e outros sistemas podem ter alterado o inventário
durante o arraste. Os widgets não movem equipamentos automaticamente.

O menu inicial continua anterior ao runtime de mods. Leitores de tela, editor visual
e widgets de lista virtualizada não foram acrescentados nesta etapa.
