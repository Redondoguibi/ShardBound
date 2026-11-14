# IA, terminal e linha do tempo

**Estado de 13 de novembro de 2026.** Requisitos da primeira versão. A IA faz parte do fluxo completo de autoria e consegue executar tarefas com várias etapas.

## Implementação disponível

Abra **Assistente IA**. O Codex local usa a instalação e o login do usuário. Deixe o modelo em branco para usar o padrão do CLI ou informe um ID aceito pela conta. Para os quatro fornecedores por API, escolha o modelo e salve a chave no Gerenciador de Credenciais do Windows. O assistente pode ler o catálogo, consultar colunas, preencher terrenos, colocar estruturas ou modelos, ligar interiores, mover o ponto inicial, chamar ferramentas do Blockbench, executar PowerShell e rodar uma prévia do mapa no jogo. O painel mostra o comando e o resultado. No modo por risco, comandos de terminal e ações sensíveis passam por revisão. O modo autônomo executa depois da autorização inicial do autor.

O mapa e uma cópia de `content` são preparados em `%LOCALAPPDATA%\ShardBound\Editor\Revisions` por tarefa. **Aplicar rascunho** valida os mods e inclui as revisões na linha do tempo. Voltar a um comando seleciona o mapa e a cópia dos mods daquele ponto. **Testar no jogo** usa o conjunto de mods da revisão selecionada. O painel mantém a saída e o caminho do rascunho. Efeitos fora da pasta da tarefa exigem recuperação própria. Se o editor portátil estiver fora do repositório, use **Bridge do Blockbench…** para selecionar o `server.mjs` da instalação local da ponte.

**Salvar** grava a posição selecionada e as edições manuais e da IA, inclusive as que ainda podem ser refeitas. O mapa aponta para um arquivo `.shardbound-timeline-*.json` na mesma pasta. Guarde os dois arquivos juntos. Para navegar entre revisões de mods, mantenha também as pastas de conteúdo referenciadas. Se o histórico estiver indisponível, o editor abre a versão salva do mapa e informa o problema.

A suíte local cobre captura de saída, código de retorno, aprovação, recusa, cancelamento e troca de revisão de mapa e mods. O Codex autenticado foi exercitado ao vivo com uma ação de mapa e uma rodada de confirmação do resultado. As conexões com OpenRouter, OpenAI, Anthropic e Google Gemini precisam de credenciais próprias para a verificação ao vivo.

## Capacidades

- Inspecionar mapa, seleção, catálogo, dependências e diagnósticos. Criar e editar terreno, blocos, texturas, estruturas, portas, interiores, itens, armaduras e scripts `.sl` pelas mesmas operações que a interface usa.
- Abrir, inspecionar, editar e exportar assets pelo bridge local do Blockbench. Vincular os resultados ao projeto, validar alterações, preparar um teste no jogo e usar os resultados para corrigir o trabalho.
- **Executar comandos reais no terminal**, inclusive scripts de build, validação, testes e outras ferramentas de desenvolvimento. A IA recebe saída padrão, saída de erro e código de retorno para tomar a próxima decisão. O autor consegue ver o comando, o diretório de execução, o estado e a saída, e pode cancelar a execução pelo editor.
- Oferecer Codex local pelo CLI autenticado e conexões por chave para OpenRouter, OpenAI, Anthropic e Google Gemini. O autor escolhe o fornecedor e o modelo de API. As chaves ficam no armazenamento de credenciais do Windows. As conexões compartilham as ferramentas do editor e a política de execução. [OpenRouter](https://openrouter.ai/docs/api/api-reference/chat/create-a-chat-completion) · [OpenAI](https://developers.openai.com/api/reference/resources/responses/methods/create) · [Anthropic](https://platform.claude.com/docs/en/api/messages/create) · [Google Gemini](https://ai.google.dev/api/generate-content)

## Modos configuráveis

- **Autônomo.** A IA executa as ações dentro das permissões da sessão, incluindo comandos de terminal, sem pedir confirmação a cada passo.
- **Por risco.** A IA avalia a ação e explica o impacto. Se classificar a ação como arriscada ou incerta, o editor pausa, mostra o que seria feito e aguarda a decisão do autor. O executor também identifica efeitos observáveis, como exclusão em lote, alterações fora do projeto e operações com consequências externas. O processo só começa depois da revisão quando esta for exigida pelo modo ou pelas permissões da sessão.

## Linha do tempo e restauração

- Registrar ações manuais e da IA, agrupadas por tarefa quando aplicável. Cada ação de edição tem autor, resultado e revisão do projeto. Ações de terminal mostram comando, diretório, estado, código de saída e mudanças capturadas, com credenciais omitidas do histórico.
- Antes de um comando que possa editar arquivos do projeto, criar um ponto recuperável. Capturar as mudanças após o comando, inclusive quando ele falhar ou for cancelado. Aplicar o mesmo contrato a alterações de scripts, texturas e modelos e às exportações do Blockbench. Comandos apenas de leitura também aparecem no histórico.
- Clicar numa ação restaura o estado **logo após aquela ação**, desfazendo as alterações posteriores no projeto. Primeiro encerrar tarefas que ainda possam escrever, depois guardar a revisão atual para recuperação e só então restaurar. Se a ação escolhida for apenas de leitura, restaurar a última revisão de projeto correspondente àquele ponto.
- O histórico restaura dados do projeto sob controle do editor. Ações com efeitos fora do projeto, como publicar um pacote ou alterar uma instalação do sistema, são identificadas no histórico com seu alcance, pois exigem recuperação própria.

## Aceite

Com uma única tarefa, pedir à IA para criar uma ruína com interior e um item, produzir ou editar um asset, rodar a validação pelo terminal e corrigir um erro identificado na saída. Exercitar o fluxo com Codex e verificar a execução de ferramentas com os quatro fornecedores. Confirmar autonomia, pausa por risco e cancelamento de um comando. Restaurar o projeto para antes da publicação dos assets e verificar o mapa, os arquivos editados pelo terminal e os resultados mostrados na linha do tempo.
