# Robustez de mundos e mods — implementação e validação

Data: 2026-10-18. Contratos e exemplos:
[10-SAVES-COMPATIBILITY-AND-RECOVERY.md](modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md).

## Cinco frentes implementadas

1. **Trabalho bloqueante:** autosave/F5 capturam snapshots e transferem serialização,
   índices e publicação para um executor de I/O limitado a 32 trabalhos. Expulsão
   de páginas sujas usa o worker; geração visual lê páginas frias a partir de
   janelas imutáveis. Eventos de sucesso só ocorrem após a publicação. As APIs
   síncronas e o salvamento antes de sair preservam seus contratos.
2. **Armazenamento e recuperação:** o descritor de terreno cabe no checkpoint e
   aponta para um índice externo em fluxo, com checksum e até 10 milhões de páginas.
   Há backup completo, seleção de restauração e limpeza explícita na seleção de
   mundos. Backups têm inventário SHA-256, validação antes da troca e cópia preventiva
   do mundo atual válido. Journals recuperam trocas de pastas interrompidas.
3. **Compatibilidade:** manifests declaram versão mínima e schema. Saves antigos
   continuam sendo lidos, e migrações consecutivas transformam somente estado e
   entidades do proprietário em um Runtime isolado, sem APIs do mundo vivo. O
   checkpoint é a autoridade quando os metadados estiverem atrasados. Atualizações
   incompatíveis/downgrades falham sem regravar os dados.
4. **Preparação transacional:** cada pacote usa VFS/AssetManager temporários,
   preflight sintático/imports e validação dos assets, incluindo áudio. Falhas e
   cancelamentos descartam os registros; dependentes obrigatórios são excluídos e
   conteúdo independente com dependências opcionais pode carregar.
5. **Escala e interrupções:** novo teste `robustness`, com execução prolongada
   configurável, relatório da GPU real, memória e tempos do workload. Exercita mods
   combinados, falhas de disco, gravação, recarga, recuperação e índice grande.

## Correções encontradas no ciclo

- Dados de páginas expulsas podiam permanecer retidos depois de concluir a escrita.
  O estado compartilhado libera o payload no sucesso, sem perder os dados de uma
  escrita que falhou. O callback usa referência fraca para evitar ciclo com seu
  future; o teste verifica liberação após drenar o executor e nova tentativa após
  falha de armazenamento.
- Arquivos de áudio existentes, mas inválidos, ainda podiam entrar no registro;
  agora a preparação valida seu formato antes da publicação do pacote.
- O save podia publicar mais entidades do que o loader aceita ou referenciar um
  pai excluído. A captura recusa esses casos antes de substituir o checkpoint.
- Metadados antigos poderiam exigir novamente uma migração já publicada no
  checkpoint. A abertura confere a versão/schema do checkpoint, e a publicação
  seguinte atualiza os metadados.
- A carga inicial de 400 mil entradas produziu 16.688.890 bytes, abaixo de 16 MiB.
  O teste foi ampliado para 500 mil entradas; a exigência de cruzar o limite foi
  mantida. Essa correção alterou somente o volume da modalidade soak do teste.

## Validação final

| Configuração | Suíte completa | Tempo do CTest | Avisos de compilação |
|---|---|---|---|
| Debug | 43/43 aprovados | 137,44 s | 0 |
| Release | 43/43 aprovados | 69,80 s | 0 |

Depois das suítes completas, o ajuste de 400 para 500 mil páginas foi recompilado
nas duas configurações e `robustness` passou novamente em ambas. Os sources do
engine não mudaram depois das suítes completas. A modalidade soak final foi
executada em Release, sem outra compilação/suíte concorrente.

[Resultados individuais e hashes dos sources](validation/runtime-robustness-2026-10-18.json).
Logs: `build/robustness-debug.log`, `build/robustness-release.log`,
`build/robustness-size-debug.log`, `build/robustness-size-release.log` e
`build/robustness-soak-release.log`.

O teste cobre falha/cancelamento de pacotes sem vazamento de registros/mounts,
propagação de dependências, isolamento de scripts, retry de disco sem perder
edições, snapshots imutáveis, recusa de dados inválidos, migrações, backup/restore/GC,
seleção de backups e bloqueio da interface enquanto restaura. Dois subprocessos
são terminados à força em limites anteriores/posteriores à publicação de arquivo;
o arquivo antigo ou o novo, e a geração anterior, permanecem conforme o ponto.
A recuperação de troca de pastas interrompida também é exercitada.

## Ensaio de escala

Comando: `build/release/robustness_tests.exe --soak 4096`, a partir da pasta
`build/release` usando `./robustness_tests.exe --soak 4096`.

| Medida | Resultado |
|---|---|
| GPU efetivamente usada | Intel(R) UHD Graphics 630 |
| Iterações de streaming/edição/saves | 4.096 |
| Pacotes independentes aceitos no conjunto de teste | 25 |
| Sistemas de mods ativos por iteração | 24 |
| Atualizações de sistemas conferidas | 98.304 |
| Páginas no índice grande gravado/restaurado | 500.000 |
| Índice externo | 20,888,890 bytes |
| Memória privada inicial/final na fase de streaming | 68,50 / 75,20 MiB |
| Maior memória privada amostrada nessa fase | 83,82 MiB |
| P95 da atualização do workload | 4.8004 ms |

[Medições brutas](validation/runtime-robustness-soak-2026-10-18.json).
Os sistemas salvam/recarregam estados por mod. Novos pedidos de save respeitam a
operação pendente; cargas explícitas ocorrem periodicamente. O índice grande
compartilha uma página imutável entre muitas coordenadas: valida o volume do
índice, não a criação de 500 mil arquivos físicos. Os valores de memória são da
fase de streaming antes dessa etapa do índice. O cronômetro exclui render/present
e inclui operações explícitas de recarga; não é benchmark de FPS ou de GPU.
Os números não demonstram ausência universal de vazamentos.

## Executável real e recuperação de versões

Três execuções adicionais de `game.exe --hidden --no-vsync` em um diretório de
usuário isolado validaram criar o mundo, reabri-lo com metadados intencionalmente
atrasados e recusar um checkpoint com versão futura incompatível. Hashes confirmam
que a recusa preservou checkpoint e metadados.
[Resultado do smoke](validation/runtime-compatibility-smoke-2026-10-18.json).

## Limites restantes

- Captura de ECS/metadados e operações síncronas explícitas continuam na thread
  principal. Suporte físico após teleporte, edição transacional e validação no
  carregamento ainda podem bloquear. A fila aplica backpressure quando cheia.
- O índice em memória cresce com o mundo. A quota de entidades e o envelope de
  16 MiB para estados permanecem; foi removida a dependência do índice de terreno
  desse envelope. A limpeza é uma ação explícita e conserva slots e `.bak` válidos.
- Autores devem declarar compatibilidade corretamente e manter assets/IDs usados
  por snapshots antigos. Migrações não substituem o gerador nem remapeiam voxel IDs.
- A transação de carregamento não é rollback de efeitos arbitrários de callbacks
  ou DLLs nativas. O cache derivado de pacotes rejeitados pode permanecer em disco.
- A máquina disponível usa uma GPU Intel. Outras GPUs, horas de jogo real e perda
  física de energia não foram validadas. Interrupção de processo não equivale a
  queda de energia/controladora. O comando de soak permite repetir em outros PCs.

Não restaram falhas nas verificações finais executadas. Isso não declara encerrada
a evolução do runtime nem a validação em hardware ainda não testado.
