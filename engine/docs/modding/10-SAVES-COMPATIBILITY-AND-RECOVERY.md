# Saves, compatibilidade e recuperação

O runtime possui snapshots de gravação, índice externo de terreno, carregamento
transacional de assets e contratos explícitos para atualização dos mods.

## Atualizações compatíveis

O manifest pode declarar:

```json
"save_compatibility": {
  "minimum_version": "1.0.0",
  "schema": 2
}
```

A versão instalada precisa ser maior ou igual à versão salva, e a versão salva
precisa ser maior ou igual a `minimum_version`. Não há downgrade implícito.
`schema` é inteiro de 1 a 1000000. Campos desconhecidos são erros. Sem esse bloco,
o comportamento continua estrito: versão exata e schema 1.

Declarar a faixa é uma responsabilidade do autor: mantenha os assets/IDs que os
snapshots de mundos antigos ainda referenciam. O engine continua usando a definição
de mundo guardada no snapshot, preservando a geração. A declaração não remapeia
blocos nem troca automaticamente o gerador pelo conteúdo de uma versão nova.

Mundos/checkpoints antigos sem `mod_schemas` são considerados schema 1.
O jogo confere o contrato e a existência de todos os passos de migração antes de
abrir um mundo salvo. Versões e schemas atuais são registrados no próximo
checkpoint bem sucedido e nos metadados do mundo. O checkpoint é a autoridade
para a abertura: se uma interrupção deixar os metadados uma geração atrás, eles
não obrigam a repetir uma migração já publicada.

## Migração de schema

No script principal do mod, registre cada passo consecutivo:

```text
register_save_migration(1, "migrations/schema1.sl")
```

O arquivo fornece `migrate(bundle)`:

```text
func migrate(bundle):
    if bundle.state == null:
        bundle.state = record()
        bundle.state.gold = 0
    bundle.state.schema = 2
    return bundle
```

O registro acima transforma schema 1 em 2. Para chegar ao 3, registre também o
passo que recebe 2. Duplicatas e passos ausentes são rejeitados.

`bundle.state` contém somente o estado persistente do mod proprietário.
`bundle.entities` contém os registros salvos das entidades desse proprietário.
Uma migração pode atualizar seus campos, referências e componentes; deve preservar
os IDs persistentes, o número de entidades e o proprietário. Entidades de outros
mods e o estado `core` não entram no pacote.

O arquivo e seus imports são resolvidos dentro da pasta do mod e verificados pelo
checker. A execução ocorre em um Runtime novo com os utilitários de dados/listas,
`record()` e `script_error()`. Não recebe APIs de entidades, física, áudio, UI,
assets, saves ou acesso ao runtime vivo. Alterar globals só afeta esse runtime
isolado. O orçamento de instruções e profundidade da linguagem continua ativo.

O resultado passa pelas validações normais do save. Falhas deixam o mundo ativo
e o checkpoint original intactos. A migração não regrava o arquivo durante load;
o próximo save publica a versão migrada. Mudanças nos schemas de componentes
customizados continuam usando `register_migration`, que é uma API separada.

## Gravação e trabalho em segundo plano

O autosave e F5 do jogo padrão usam `SaveGame::requestSave`. O ponto seguro na
thread principal captura ECS, estados e metadados de terreno. O worker de I/O
serializa, publica páginas/índice e grava o checkpoint. `on_saved` só é emitido
depois da conclusão bem sucedida; falhas são apresentadas no menu de pausa.

No C++, `requestSave` retorna um `SaveRequest` que identifica o resultado daquela operação. `finishSave(wait, request)` permite observar esse resultado mesmo se outra API já aguardou a gravação. Ignorar o retorno e usar `finishSave(wait)` continua válido. A conclusão de um pedido antigo não consome outro pedido mais recente. O jogo mantém o ticket do checkpoint até confirmar sucesso ou apresentar a falha; salvar pelo menu e encerrar usam o mesmo caminho de conclusão.

Há no máximo uma gravação pendente por SaveGame e uma fila global limitada a
32 trabalhos de disco. Ela drena no encerramento. Sair do mundo, salvar pelo
menu e as APIs C++/scripts síncronas existentes aguardam a conclusão para preservar
seus contratos. Os snapshots não mantêm referências ao mundo ou ao Runtime vivo.

A cache de terreno mantém até 64 páginas residentes. Páginas sujas podem ser
persistidas pelo worker ao serem expulsas; o dado permanece disponível enquanto
a escrita está pendente, inclusive para uma tentativa posterior se o disco falhar.
O worker visual recebe uma janela imutável de referências e carrega as páginas
frias necessárias sem fazê-lo na thread principal.

A captura ainda tem custo proporcional aos dados vivos/metadados. Backpressure,
consultas síncronas explícitas, suporte físico após teleporte, edição transacional
e carregamento de mundos ainda podem esperar por disco ou realizar trabalho na
thread principal. Não há promessa de frame time constante.

## Índice externo de terreno

Checkpoints novos usam este descritor compacto em `state.core.regions`:

```json
{"version": 2, "index": "hash-tamanho.index", "count": 1234}
```

O índice imutável em `terrain/` possui uma entrada JSON por linha, com coordenadas
de chunk e nome da página. Ele é escrito e lido em fluxo, com checksum, validação
de linhas/coordenadas e limite de 10 milhões de páginas por índice. Não depende
do envelope de 16 MiB do checkpoint; estados de entidades/mods continuam sujeitos
às quotas existentes. O índice de metadados em memória cresce com as páginas.

Manifestos de páginas versão 1 e os blocos inline antigos continuam sendo lidos.
A próxima gravação do mundo produz o descritor externo. Índices/páginas são
publicados antes do checkpoint; uma interrupção pode deixar arquivos sem
referência, mas não faz o checkpoint anterior apontar para um arquivo incompleto.

## Backup, restauração e limpeza pelo jogo

Na seleção de mundos há três ações por mundo:

- **Backup:** copia a pasta completa, incluindo checkpoints, suas gerações `.bak`,
  metadados e arquivos de terreno, para `user/world-backups/<id>/<backup>/`.
- **Restaurar:** abre a seleção de backups, mais recentes primeiro. Um backup
  completo do mundo atual válido é criado antes de substituí-lo.
- **Limpar:** valida todos os checkpoints/gerações anteriores e remove somente
  páginas/índices que nenhum deles referencia.

As operações acontecem em segundo plano com o mundo fechado. A interface impede
outra ação enquanto uma delas está em andamento. Um mundo com metadados ilegíveis
mas backups disponíveis permanece listado para permitir recuperação.

Backups têm inventário externo, tamanhos e hashes SHA-256. Arquivos adicionais,
ausentes, corrompidos, caminhos que escapam da pasta e reparse points são
rejeitados antes da restauração. Arquivos temporários incompletos não entram na
cópia. Backups incompletos não são oferecidos na lista.

A restauração prepara uma pasta nova e registra um journal antes de trocar as
pastas. No próximo acesso/listagem, um journal interrompido recupera a pasta
anterior ou conclui a limpeza após uma publicação já realizada. A limpeza só
começa a excluir arquivos depois de validar todas as referências; um checkpoint
corrompido impede a operação.

As APIs C++ são `WorldStorage::backup`, `backups`, `restoreBackup` e `collect`.
Use-as com o mundo fechado. Há uma barreira do executor para trabalhos de disco
anteriores; essa barreira não autoriza editar o mundo durante a manutenção.
Não há expiração automática de backups do usuário.

## Preparação transacional de mods

O loader verifica scripts/imports e prepara um VFS e um AssetManager temporários
para cada pacote. Todos os registros do pacote precisam passar antes da publicação.
Uma falha/cancelamento descarta seus registros e mount. Os mods que exigem um
pacote rejeitado também ficam excluídos; dependências opcionais não impedem o
conteúdo independente de carregar. Caches derivados em disco podem permanecer.

Essa transação cobre preparação de assets e preflight sintático. Callbacks de
scripts e DLLs nativas em execução mantêm seus contratos de efeitos colaterais;
não existe rollback geral de operações arbitrárias realizadas por eles. Scripts
com sintaxe válida ainda podem falhar por erros semânticos durante inicialização;
o modo procedural descarta a sessão que falhar.

## Reproduzir a validação de escala

```powershell
./tools/build.ps1 -Preset debug
./tools/build.ps1 -Preset release
cd build/release
./robustness_tests.exe --soak 4096
```

O programa testa interrupções reais de processo na publicação de arquivo,
combinações de mods, migrações isoladas, falhas de disco, backup/restore/GC,
interface de recuperação, streaming e snapshots assíncronos. A modalidade soak
cria/restaura também um índice com 500 mil páginas, maior que 16 MiB. O relatório
JSON em `user/robustness/<execucao>/result.json` identifica a GPU efetivamente usada,
tempos do workload e amostras de memória privada. As páginas repetidas do índice
grande compartilham conteúdo; não representam 500 mil arquivos diferentes.

Rodar esse comando em outras máquinas é necessário para obter evidência em outros
hardwares. Encerrar um processo não simula perda física de energia; os testes não
comprovam durabilidade contra todas as falhas de disco/controladora.
