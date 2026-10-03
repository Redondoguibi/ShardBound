# Conectar ao ChatGPT Desktop (Windows, Plus)

## Estado verificado em 3 de outubro de 2026

A ponte oferece 18 ferramentas MCP e 19 adaptadores MTools. O transporte local
stdio atende ao Codex. O transporte HTTPS com OAuth foi testado com um cliente
MCP real: descoberta das ferramentas, autenticação PKCE e chamada de `status`.
A chamada indicou que o Blockbench ainda não estava conectado.

Isso não comprova que as ferramentas já estão disponíveis no Chat comum. A
validação final depende da conexão adicionada e autorizada na sua conta.

## Caminho observado na sua interface

A imagem fornecida pelo usuário mostra **Configurações → Integrações → Plugins**.
Dentro de Plugins, o usuário confirmou **Adicionar → Adicionar servidor MCP**.
O mesmo menu também contém Criar plugin e Adicionar Marketplace.

A segunda imagem, `conectando_mcp_plugin.png`, confirma que o formulário tem
**Nome** e **Tipo**, com as opções **STDIO** e **HTTP com streaming**. Para a
conexão HTTPS preparada aqui, selecione **HTTP com streaming**. Os campos
Comando para iniciar, Argumentos, Variáveis do ambiente, Encaminhamento de
variáveis do ambiente e Diretório de trabalho mostrados na imagem pertencem
à opção STDIO.

A terceira imagem, `https.png`, confirma os campos da opção HTTP: **URL**,
**Variável de ambiente de token do portador**, **Cabeçalhos** e **Cabeçalhos a
partir de variáveis de ambiente**. Preencha Nome e URL, mantendo os campos de
token e cabeçalhos vazios para testar a descoberta OAuth. `MCP_BEARER_TOKEN`
é o exemplo de nome de uma variável de ambiente, não uma senha a preencher.
A imagem não mostra um seletor OAuth; ainda é preciso verificar se o host
oferece autenticação depois de salvar. Não cole a senha de consentimento nem
o token de pareamento nesses campos.

O usuário confirmou que salvar o servidor não abriu autenticação. Portanto,
esse cadastro ainda não comprova uma conexão OAuth nem a disponibilidade no
Chat comum. Testes externos autenticados não substituem esse teste no host.

A documentação oficial usa outros nomes em algumas interfaces. Não procure
“Segurança e login” quando essa seção não aparece na sua instalação.
Se o formulário tiver transporte remoto/URL, use o endereço HTTPS abaixo.
Se só aceitar comandos locais, ele não comprova instalação para o Chat comum;
é preciso verificar o registro remoto em chatgpt.com e sua disponibilidade no Desktop.

## Iniciar a conexão de teste

Na pasta `BLOCKBENCH/bridge`, após o setup normal:

```powershell
./Connect-ChatGPT.ps1
```

Esse comando instala `cloudflared` em `.local/bin/` se necessário, verificando
SHA-256 contra a publicação oficial, e inicia os processos em segundo plano.
Não instala um serviço do Windows nem altera a inicialização do computador.
Não precisa de chave da API OpenAI, conta Cloudflare ou domínio para este teste.

Alternativamente, com o binário já instalado:

```powershell
npm run chatgpt:start
npm run chatgpt:status
npm run chatgpt:verify
```

O campo `mcpUrl` mostra o endereço atual, terminado em `/mcp`.
Ele também fica em `.local/chatgpt-session.json`. Cadastre:

- Nome: **ShardBound Blockbench**.
- Transporte: HTTP remoto/Streamable HTTP, se houver seletor.
- URL: copie `mcpUrl` da sessão atual.
- Autenticação: **OAuth**; o servidor suporta registro dinâmico (DCR).
- Client ID/secret: não invente valores; use descoberta/registro dinâmico quando disponível.

Na página “Conectar ao seu Blockbench”, use a senha de
`.local/chatgpt-password.txt`. Não envie essa senha no chat nem use o token de
pareamento do editor. Autorize a conexão e confira a descoberta das ferramentas.
Se o host criar uma conexão com ID `plugin_asdk_app...`, esse ID pode ser
associado ao plugin privado existente pelo Plugin Creator.

## Conectar o editor

No Blockbench, carregue `.local/shardbound_bridge.js` em **File → Plugins →
Load Plugin from File** e abra um Generic Model. Instale MTools 2.1.0 para seus
adaptadores. O arquivo pareado mantém as credenciais do setup existente.

Em um Chat novo, selecione a conexão autorizada e peça:

> Execute status, read_project e capture_view. Mostre a captura antes de editar.

Uma chamada real com `connected: false` indica falta de conexão do editor.
Ferramentas ausentes indicam que o host não as expôs. São problemas diferentes.

## Duração e encerramento

O Quick Tunnel usa uma URL temporária que muda ao reiniciar o túnel. Manter o
processo ativo conserva a URL durante a sessão, sem garantia de disponibilidade.
Ao reiniciá-lo, atualize a URL no ChatGPT e autorize novamente.

```powershell
npm run chatgpt:stop
```

Isso encerra o túnel e o daemon associado. O Codex pode iniciar novamente seu
daemon local na próxima conexão. A configuração local, porta e credenciais
continuam preservadas; a URL temporária só existe no estado da sessão.

O gateway público encaminha apenas MCP e OAuth. `/rpc`, `/shutdown`, `/health`
e o WebSocket de pareamento não são expostos por esse túnel. MCP exige OAuth;
segredos e modelos não são incluídos no pacote do plugin.

Para uso permanente, substitua o Quick Tunnel por um hostname estável em um
provedor configurado por você, ou pelo Secure MCP Tunnel da OpenAI. Este último
exige `tunnel_id`, chave de runtime e permissões da organização apropriada.
A escolha depende do acesso que sua conta oferece; não foi contratada infraestrutura.

## Plugin privado e Codex

O plugin privado existente é
[ShardBound Blockbench](https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53).
A versão 0.1.1 usa stdio e foi selecionada no Chat comum sem expor ferramentas.
Reinstalar a mesma configuração não resolve esse diagnóstico por si só.

Em 3 de outubro foi publicada a versão **0.2.0**, mantendo a mesma identidade
e visibilidade privada. A leitura de volta confirmou `mcp.json` com transporte
`streamable-http` e a referência `mcpServers` no manifesto de compatibilidade.
Release: `pluginrel_6ac0704ef428819182a109c48473241b`.
O endpoint temporário dessa publicação é
`https://badly-operated-addressing-mortality.trycloudflare.com/mcp`.
O pacote ainda não tem um vínculo `.app.json` com um app registrado na conta.
Não invente esse ID: confirme o registro remoto e a autorização no ChatGPT,
depois use o ID técnico verificado caso esse vínculo seja necessário.

O código-fonte mantém `mcp.json` local para Codex. Um pacote remoto pode ser
gerado com `scripts/package.ps1 -RemoteUrl https://ENDERECO/mcp` sem modificar
o registro local. Esse pacote contém a URL, mas nunca senhas ou tokens.
Uma publicação privada não substitui a autorização OAuth nem o teste no Chat.

## Referências oficiais

- [Developer mode](https://developers.openai.com/api/docs/guides/developer-mode):
  inclui Plus na disponibilidade web. A interface do Desktop deve ser verificada.
- [Conectar e testar](https://developers.openai.com/plugins/deploy/connect-chatgpt):
  registro de MCP remoto e autenticação.
- [Pacotes e associação a uma conexão](https://developers.openai.com/plugins/build/plugins):
  referência a `plugin_asdk_app` para empacotar uma conexão registrada.
- [Quick Tunnels](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/):
  temporários, sem garantia de disponibilidade e sem SSE. A ponte usa Streamable
  HTTP com respostas JSON, validado pelo teste externo.
- [Secure MCP Tunnel](https://developers.openai.com/api/docs/guides/secure-mcp-tunnels).
