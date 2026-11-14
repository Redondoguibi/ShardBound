# ShardBound Blockbench bridge

Integração privada para **ChatGPT normal -> Secure MCP Tunnel OpenAI -> MCP stdio no Windows -> daemon local -> Blockbench**, com autoria Generic Model e ShardBound Studio, texturas, exportação e ações adaptadas do MTools.

Comece pelo [guia de instalação e uso no ChatGPT Desktop](CHATGPT-DESKTOP.md):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-ChatGPT-Desktop-Plugin.ps1 -OpenPlugin
```

Depois de reiniciar o PC: `.\Connect-ChatGPT.ps1`. O Blockbench deve estar aberto com `.local/shardbound_bridge.js` e um Generic Model ou projeto Studio. O plugin de conta é [ShardBound Blockbench](https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53).

Para o Studio, siga [Compatibilidade e uso completo](STUDIO-COMPATIBILITY.md). Carregue também `../studio/dist/shardbound_studio.js`.

## Implementação

`mcp/server.mjs` oferece stdio e inicia o daemon se necessário. `mcp/daemon.mjs` escuta somente em loopback, autentica pelo token local e encaminha chamadas serializadas por WebSocket à bridge do Blockbench. `plugin-src/controller.mjs` valida projeto/revisão antes de alterações. Generic Model (`free`) e Studio (`shardbound_studio`, Studio 1.1+) podem ser editados. `shared/tools.mjs` e `shared/studio-tools.mjs` definem 34 ferramentas e o conjunto explícito de adaptações MTools; `mtools_list` informa quais estão automatizadas. Não há execução arbitrária de scripts de plugins.

O pareamento e a chave DPAPI do túnel ficam em `.local/` ignorado pelo Git. O pacote privado inclui `.app.json` com o app registrado e as referências nos manifests portatil e compatível. Não inclui segredos ou node_modules. O pacote da conta não tenta iniciar um comando local através do upload de arquivos.

Consulte a [referência de ferramentas, arquivos, cubos, UVs e MTools](TOOLS-REFERENCE.md) para os detalhes do contrato e da seleção.

## Validação

```powershell
npm test
.\Status-ChatGPT-Desktop-Plugin.ps1
.\Status-ChatGPT-Desktop-Plugin.ps1 -RequireChatHost
```

O diagnóstico faz handshake MCP, lista todas as ferramentas, chama status e verifica as demais camadas. Uma prova do cliente independente, do cache ou de descoberta não equivale a uma chamada em um Chat normal. Os eventos reais são registrados sem argumentos/resultados em `.local/mcp-audit.jsonl`.

## Desenvolvimento

```powershell
npm ci
npm run build
npm run build:paired
npm run package
```

`Install.ps1 -RegisterCodex` configura opcionalmente o cliente Codex local, separado da integração da conta e sem depender dele como host final. O HTTP autenticado e sua implementação OAuth permanecem disponíveis para testes/outros clientes (`npm run http`); o fluxo Chat normal usa o túnel oficial. Os antigos scripts de transporte temporário foram desativados.

Veja [a auditoria da integração](INTEGRATION-AUDIT.md) para causa raiz, evidências e limitações dos testes.
