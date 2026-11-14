# Evolução da plataforma de mods

Data: 2026-10-18. Escopo: streaming, persistência em escala, ferramentas de
desenvolvimento e conclusão dos fluxos de equipamentos, UI e navegação solicitados.
As alterações anteriores do projeto foram preservadas; não houve publicação ou commit.

Guia das APIs e operação:
[modding/08-STREAMING-AND-DEVELOPER-TOOLS.md](modding/08-STREAMING-AND-DEVELOPER-TOOLS.md).
O [relatório 29](29-MOD-PLATFORM-VALIDATION.md) registra a etapa anterior.

## Funcionalidades

| Frente | Implementação |
| --- | --- |
| Streaming | Worker de geração independente de scripts/ECS/renderer; fila e resultados limitados; prioridade para apoio físico; descarte por revisão após edição/load; publicação visual com orçamento cooperativo por frame. |
| Persistência | Páginas imutáveis por chunk, paleta de blocos, cache de até 64 páginas por store, manifesto no checkpoint e validação de integridade antes do load. Removido o teto global de 4.096 edições, conservando esse limite por transação. |
| Ferramentas | Painel F3; relatórios de sistemas com origem, chamadas e custos; histórico limitado de diagnósticos; inspeção de entidades/componentes; identificação dos autores de patches do registro; exportação JSON em diretório próprio. |
| Equipamentos | Transferência atômica de instância e inventário entre proprietários, conservando atributos e durabilidade, com hook de veto, revalidação após callbacks e evento posterior. |
| Interfaces | Cursor, seleção, clipboard UTF-8, Delete/Home/End/Shift, rolagem com recorte, foco que revela widgets, arraste de sliders e drag/drop com payloads entre widgets. |
| Navegação | Opção para considerar os corpos físicos presentes na consulta, com lista de entidades ignoradas e nova consulta refletindo seus movimentos. |

## Decisões de arquitetura e compatibilidade

A geração roda em um único worker e recebe somente o gerador imutável e as edições
da área necessária ao chunk, incluindo a borda de uma célula. Assim, não depende de
APIs gráficas ou execução concorrente de mods. Somente a thread principal publica
meshes e corpos. Resultados preparados antes de uma edição ou load são descartados.

O streaming ocorre uma vez por frame, antes dos passos de simulação. A seleção de
apoio físico percorre os corpos dinâmicos, sem enumerar todas as decorações e
entidades visuais. A fila tem até 24 solicitações atuais e oito resultados prontos;
um trabalho antigo já em execução pode terminar antes de ser descartado.

O armazenamento usa páginas 16×16×48 e um índice de referências. As transações
preparam somente as páginas alteradas, recursos de chunks carregados e seus
vizinhos; a publicação usa nós já alocados. Uma falha de preparação conserva o
terreno anterior. O histórico guarda alterações inversas limitadas por transação,
em vez de copiar o mapa inteiro a cada edição.

Saves novos continuam usando o envelope v1, com um campo opcional `core.regions`.
Saves antigos com `core.blocks` são lidos e preparados antes da troca do mundo.
Snapshots de geradores e componentes de equipamentos mantêm seus contratos.
Um runtime antigo não entende as páginas novas: a compatibilidade garantida é
carregar saves antigos neste runtime, não abrir saves novos em executáveis antigos.

Uma transferência prepara cópias de ambos os inventários e componentes, além de
seu índice, e só então os troca. A instância deve estar desequipada; seu ID é
recriado no destinatário. Hooks podem vetar e têm seus efeitos revalidados, mas
efeitos arbitrários executados pelo próprio hook não são desfeitos.

Novas opções de widgets e navegação são opcionais. Botões existentes continuam
ativando no press; botões que optam por ser arrastáveis ativam no release quando
não houve arraste. Drag/drop entrega eventos para o mod aplicar a regra de negócio.
A engine não transfere um equipamento simplesmente porque um widget foi solto.

## Problemas identificados e corrigidos

- A cópia de todas as edições e de todo o índice a cada mudança crescia com o mundo.
  A preparação agora é por página alterada; consultas de chunk copiam apenas a área
  e sua borda, em vez de nove páginas inteiras para cada trabalho.
- Contar e procurar páginas residentes percorrendo todo o índice prejudicaria
  consultas em mundos extensos. O cache mantém um conjunto separado de residentes.
- Resultados assíncronos podiam chegar depois de alterações ou troca de save.
  Revisões invalidam o trabalho antigo, com regressão verificando o mesh publicado.
- A migração de edições antigas precisava ser preparada antes do commit do save,
  inclusive alocações e possíveis escritas de cache. Agora isso ocorre na validação.
- Páginas ausentes, corrompidas e manifestos com caminhos indevidos são rejeitados
  antes da troca do mundo. Arquivos imutáveis preservam checkpoints anteriores.
- Transferências precisam revalidar a instância depois do hook e verificar overflow
  do destinatário antes de aplicar qualquer uma das duas mudanças.
- Um corpo continua colidindo mesmo se sua entidade estiver marcada como inativa.
  A navegação considera sua presença física e permite ignorá-lo explicitamente.
- O relatório de desenvolvimento não pode compartilhar o nome de um slot.
  A exportação usa `diagnostics/report.json`, preservando `diagnostics.json`.
- O histórico de diagnósticos e as escritas repetidas do mesmo autor no registro
  poderiam crescer indefinidamente. O journal é limitado; patches repetidos atualizam
  sua entrada. Emitir eventos/hooks desconhecidos não cria entradas vazias no runtime.
- Inputs passaram a conservar seleção ao colar um clipboard vazio ou inválido.
  Corte, exclusão e truncamento respeitam os limites de codepoints UTF-8.
- Hit testing com recorte e camadas impede cliques/drops em widgets invisíveis ou
  sob painéis desabilitados. Labels comuns deixam os controles inferiores receberem
  entrada. Perda de foco cancela o arraste e libera sua captura.
- Escape de uma tela de widgets deixa de abrir simultaneamente o menu de pausa.
  O encaminhamento é validado com `GameUi` e `FrontEnd` juntos; sem widgets abertos,
  a tecla volta a controlar a pausa normalmente.
- Uma falha no provedor do painel F3 apresenta uma mensagem em vez de encerrar o
  frame de gameplay. Recursos de medição de texto usam liberação por escopo.

Também foram corrigidos avisos de sombreamento no código novo, a codificação de
trechos do guia de geração e uma seção idêntica duplicada nesse guia.

O ensaio de 4.096 transições revelou ainda um encerramento nativo `0xc0000409`
ausente dos testes curtos. Uma reprodução isolada criou e removeu lotes de corpos
sem avançar a simulação e apresentou a mesma falha. O Jolt adia a reciclagem de nós
da árvore de colisão até sua manutenção; substituições sucessivas podem esgotar
esses nós mesmo com poucos corpos vivos. O wrapper agora realiza manutenção após
um número limitado de inserções entre passos, mantendo o limite original de corpos.
Mensagens nativas do Jolt também são encaminhadas ao log e descarregadas antes de
um possível abort. A reprodução permanece em `SimulationTests.cpp`, com 2.048
lotes de 16 corpos, sem inserir passos de simulação para esconder a condição.

## Cobertura automatizada

O novo alvo `mod_expansion_tests`, registrado como `mod_expansion` no CTest, cobre:

- 6.144 edições, 96 páginas, cache limitado, coordenadas negativas, expulsão e reload;
  snapshots imutáveis, bordas de consultas e manifesto com caminho indevido.
- Mais de 4.096 edições no mundo, undo de transação esparsa que ultrapassa o cache,
  save/load paginado, migração do formato antigo e página corrompida sem alterar a
  sessão ativa.
- Transferência de item/durabilidade, reservas e índice de componentes, item equipado,
  veto por hook e overflow sem aplicação parcial.
- Obstáculos físicos em movimento, entidade inativa ainda física e lista de ignore.
- Origem de diagnóstico de script e de exceção nativa, custos e arquivo de sistema,
  limite do journal, inspeção de entidade e exportação sem sobrescrever slot.
- Seleção/copiar/colar/cortar/delete UTF-8, clipboard vazio, medição de caracteres
  suplementares, rolagem, recorte, slider arrastado, drag/drop, bloqueio por camada
  desabilitada, cancelamento por foco e renderização/falha do painel F3.
  Também verifica Escape com a interface de mods e o menu real integrados.
- Preenchimento da distância de visão em segundo plano, invalidação de mesh antigo,
  deslocamentos de origem, limites de chunks/fila/cache/entidades e medições de memória.

Os testes usam uma janela oculta, renderer D3D11 e física reais. Eventos de UI são
sintéticos; o clipboard é injetado no teste para não alterar o clipboard do usuário.
A integração padrão de clipboard usa SDL, mas não foi testada por uma sessão manual
de copiar/colar entre aplicativos. A imagem renderizada de rolagem foi inspecionada,
incluindo recorte e posição da barra.

## Limitações e trabalho restante

- O orçamento visual é cooperativo: até dois resultados e verificação de 4 ms entre
  publicações. Uma operação do driver pode exceder esse tempo. Apoio físico após
  spawn/teleporte tem fallback síncrono. Não há promessa de FPS constante.
- I/O de cache, validação de páginas e edições ainda podem bloquear a thread
  principal. Geração assíncrona não equivale a todo o armazenamento ser assíncrono.
- O índice de páginas cresce com as regiões alteradas, e o manifesto continua no
  envelope de save de 16 MiB. Dados residentes são limitados; armazenamento físico
  não é literalmente ilimitado.
- Páginas antigas são conservadas para proteger slots e backups; ainda não há
  coleta automática de arquivos sem referência. Backups devem copiar a pasta inteira
  do mundo, incluindo `terrain/`, e não somente o JSON do checkpoint.
- Permanecem altura de 48 blocos, lote de 4.096 células, histórico de 16 transações
  por sessão, limites de física e demais quotas da plataforma.
- Navegação usa uma célula de largura e a posição dos obstáculos no momento da
  consulta. Mods devem recalcular caminhos; não há coordenação temporal de multidões.
- UI não implementa leitor de tela, editor visual, listas virtualizadas ou seleção
  por graphemes combinados. O menu anterior à criação do runtime continua em C++.
- O ensaio de streaming usa terreno controlado, teletransportes e edições periódicas.
  Ele não substitui horas de jogo com todos os mods, testes em outras máquinas,
  perfil de GPU, testes de queda de energia ou balanceamento de gameplay.

## Como reproduzir

```powershell
./tools/build.ps1 -Preset debug
./tools/build.ps1 -Preset release
# Para iterar somente nos testes pertinentes:
./tools/build.ps1 -Preset debug -TestRegex 'mod_expansion|mod_platform|procedural$'
```

Na pasta `build/release`, o ensaio adicional é executado com
`./mod_expansion_tests.exe --soak 4096`. Ele gera
`user/expansion-soak-4096.json`; a execução padrão da suíte gera o arquivo de 24
iterações. As medições usam bytes privados do processo, não uso total da máquina.
Os tempos `update_pair_*` medem duas chamadas de streaming, com teleporte e edições
periódicas. Render/present ocorre a cada oito iterações fora desse cronômetro.

## Resultado das suítes completas

Rodada final com compilação incremental dos alvos alterados e execução de toda a
suíte CTest, incluindo a regressão da manutenção de nós do Jolt e a redução da cópia
de edições para a borda do chunk:

| Configuração | Compilação | CTest | Tempo real do CTest |
| --- | --- | --- | --- |
| Debug | Aprovada | 39/39, zero falhas | 116,95 s |
| Release | Aprovada | 39/39, zero falhas | 65,59 s |

Um aviso de sombreamento de variável no teste de integração foi corrigido depois
dessas suítes; o alvo `mod_expansion_tests` foi recompilado e executado novamente
em Debug e Release, sem avisos ou erros. Essa última mudança apenas renomeou a
variável local do teste. Artefatos das suítes completas: `build/expansion-debug.log`
e `build/expansion-release.log`.
`git diff --check` passou; links locais e UTF-8 dos documentos novos também foram
verificados. Não restou falha conhecida nas suítes executadas.

## Ensaio de 4.096 transições após a correção de física

O comando `mod_expansion_tests.exe --soak 4096` terminou com exit code 0. Preservamos
as [medições brutas](validation/mod-platform-expansion-soak-2026-10-18.json), incluindo
257 amostras periódicas. O ensaio foi executado depois dos testes, sem outra
compilação ou suíte concorrente durante a medição.

| Medida | Resultado |
| --- | --- |
| Transições com teleportes, streaming e edições periódicas | 4.096 |
| Gerações concluídas pelo worker, incluindo resultados obsoletos | 39.020 |
| Duas chamadas de streaming por iteração: mediana / P95 / máximo | 2,43 / 3,81 / 13,54 ms |
| Memória privada inicial / final | 74,66 / 64,06 MiB |
| Maior memória privada amostrada ao final de uma iteração | 76,30 MiB |
| Páginas residentes ao final | 64 |

Os limites de chunks, solicitações, resultados preparados, páginas residentes e
entidades foram verificados a cada iteração. Antes do stress de teleporte, o teste
também aguardou os 49 chunks da distância de visão usada e verificou que uma edição
não foi sobrescrita por trabalho antigo. Não houve crescimento progressivo observado
nas amostras finais; isso não prova ausência de vazamentos em todo conteúdo ou hardware.
As medidas são de um workload sintético de streaming, não benchmarks do jogo completo.

## Evolução posterior de armazenamento e compatibilidade

Este relatório preserva a validação daquela etapa. O runtime ganhou índice externo, autosave em worker, preparação transacional e recuperação de mundos; os contratos atuais estão em [10-SAVES-COMPATIBILITY-AND-RECOVERY.md](modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md).
