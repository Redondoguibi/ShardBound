# ShardBound: ChatGPT normal -> Blockbench no Windows

A integração privada usa o plugin da conta e o Secure MCP Tunnel da OpenAI. O runtime Node e o Blockbench continuam neste PC. A conta do ChatGPT recebe as ferramentas pela conexão registrada, referenciada por `.app.json`. O marketplace local não é o mecanismo de instalação desse plugin de conta.

## Instalar e iniciar

Na pasta `BLOCKBENCH/bridge`, execute:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-ChatGPT-Desktop-Plugin.ps1 -OpenPlugin
```

O instalador preserva o pareamento existente, instala dependências se faltarem, configura o tunnel-client oficial, inicia um único runtime, migra somente as configurações antigas do ShardBound e executa o diagnóstico. Runtime API keys são guardadas com DPAPI para o usuário Windows em `.local/`; nenhum segredo entra no pacote ou no Git. Na primeira configuração, informe o ID de um túnel associado à mesma conta/workspace e uma Runtime API key com Tunnels Read + Use. Esses dados são pedidos localmente, nunca no chat.

No Blockbench, carregue `.local/shardbound_bridge.js` em **File -> Plugins -> Load Plugin from File** e abra um **Generic Model**. Instale MTools separadamente. A bridge oferece adaptações explícitas das ações listadas por `mtools_list`; não executa qualquer plugin arbitrário.

## Conexão na conta

O plugin privado existente é [ShardBound Blockbench](https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53).

Se a conexão ainda não existir, na interface atual: **Plugins -> Adicionar -> Criar servidor MCP personalizado**, escolha **Túnel**, selecione o túnel e use **Sem autenticação**. Isso se refere à autenticação adicional do MCP; o acesso ao túnel é controlado pela OpenAI e pela Runtime API key local. No Desktop, o ponto de entrada informado pelo usuário é **Configurações -> Plugins -> Adicionar -> Adicionar servidor MCP**. Os rótulos podem variar entre web e Desktop. Não procure uma seção inexistente chamada “Segurança e login”.

O ID técnico criado pelo ChatGPT (`asdk_app_...`) precisa estar em `.app.json`; os dois manifests precisam apontar para esse arquivo. Trocar o ID exige verificar a conexão real e publicar uma nova release privada. Uma URL `api.openai.com/v1/tunnel/...` não deve ser cadastrada como URL pública do MCP.

## Uso diário

Depois de reiniciar o Windows:

```powershell
.\Connect-ChatGPT.ps1
```

Deixe o Blockbench aberto e a bridge carregada. No ChatGPT Desktop, abra um **novo Chat normal**, selecione **ShardBound Blockbench** e, quando aparecer, conecte sua dependência MCP. Para validar sem editar:

> Execute status no meu Blockbench usando a ferramenta real do ShardBound. Não edite nada.

A resposta deve incluir a versão do Blockbench, o formato atual e MTools. Um texto dizendo que a skill existe não comprova uma chamada MCP. Para editar, o formato precisa ser `free` (Generic Model). O fluxo da skill é status -> read_project -> capture_view -> mostrar a imagem -> editar.

## Diagnóstico e limites da prova

```powershell
.\Status-ChatGPT-Desktop-Plugin.ps1
.\Status-ChatGPT-Desktop-Plugin.ps1 -Json
.\Status-ChatGPT-Desktop-Plugin.ps1 -RequireChatHost
```

O relatório fica em `.local/chatgpt-diagnostic.json`. Ele verifica manifests, cache, instalação/processos/logs Desktop, configuração de pareamento, Node absoluto, handshake stdio, as 18 ferramentas, status, daemon, controlador e estado do túnel. `.local/mcp-audit.jsonl` registra somente metadados do protocolo, nomes de ferramentas e sucesso; não contém argumentos, modelos, imagens ou credenciais.

O cliente de diagnóstico é independente do ChatGPT. Eventos `openai-mcp` pelo processo filho do túnel provam tráfego da OpenAI; initialize/tools/list sozinhos não provam execução no Chat normal. Cache baixado não prova que uma conversa carregou aquela versão. `normal_chat_host` permanece pendente até haver uma observação real da conversa; não edite o arquivo de evidência para forjar sucesso.

Código de saída: 0 = camadas locais prontas; 1 = falha local; 2 com `-RequireChatHost` = falta comprovar o Chat normal no Desktop. O relatório distingue uma prova no Chat web de uma prova no Desktop.

Para parar: `.\Disconnect-ChatGPT.ps1`. Para atualizar a bridge, pare o túnel, atualize os arquivos e inicie novamente. Não execute dois tunnel-client para o mesmo túnel stdio.

## Outros clientes MCP

O servidor stdio continua disponível para clientes locais compatíveis, inclusive Codex, por `.local/codex-config.toml` produzido por `Install.ps1 -RegisterCodex`. Essa opção é independente do fluxo do Chat normal. O pacote da conta neutraliza `mcp.json` e `.mcp.json` para evitar descobrir a mesma integração duas vezes. `${PLUGIN_ROOT}` pertence ao carregamento de plugins locais por clientes compatíveis; não substitui um app registrado na conta.

Os antigos atalhos de Cloudflare foram desativados. As conexões antigas na conta não são utilizadas pelo novo vínculo; não selecione aquelas conexões. O instalador faz backup antes de remover a entrada ShardBound do marketplace pessoal e desativar o MCP antigo `shardbound_blockbench-2`, preservando os outros plugins/servidores.

## Fontes oficiais consultadas em 03/10/2026

- [Plugins e manifests](https://developers.openai.com/plugins/build/plugins)
- [Secure MCP Tunnel](https://developers.openai.com/api/docs/guides/secure-mcp-tunnels)
- [Plugins no ChatGPT](https://learn.chatgpt.com/docs/plugins)
- [Limites de stdio e instâncias do tunnel-client](https://github.com/openai/tunnel-client/blob/master/docs/configuration.md)
