# ChatGPT normal + Blockbench (Windows)

## Caminho padrao

O ShardBound usa **OpenAI Secure MCP Tunnel** para o ChatGPT normal.

Arquitetura:

```
ChatGPT
  -> OpenAI Secure MCP Tunnel
  -> tunnel-client no Windows
  -> node mcp/server.mjs (stdio)
  -> daemon local ShardBound
  -> WebSocket local
  -> Blockbench
```

O fluxo padrao nao usa:

- Cloudflare Quick Tunnel;
- URL `trycloudflare.com`;
- OAuth proprio da bridge;
- `.local/chatgpt-password.txt`;
- pagina `/authorize` ou `/approve`.

Os scripts antigos continuam apenas como `chatgpt:legacy:*`.

## O que o Connect-ChatGPT faz

Ao executar:

```powershell
.\Connect-ChatGPT.ps1
```

o script:

1. verifica Node e dependencias;
2. executa o setup local se o plugin pareado ainda nao existir;
3. baixa a release oficial mais recente de `openai/tunnel-client`;
4. valida SHA-256 usando o `SHA256SUMS.txt` oficial;
5. na primeira execucao pede o `tunnel_id`;
6. na primeira execucao pede a Runtime API key;
7. guarda a Runtime API key com DPAPI do Windows em `.local/`;
8. chama `tunnel-client runtimes connect`;
9. conecta o runtime por stdio a `node mcp/server.mjs`;
10. consulta `runtimes status --json`.

O runtime e gerenciado pelo proprio `tunnel-client`.

## Preparacao unica na conta OpenAI

A parte local esta automatizada, mas o repositorio nao pode criar permissoes na
sua conta sozinho. Uma unica vez:

1. abra Platform > Tunnels;
2. crie ou selecione um tunnel;
3. copie o `tunnel_id` no formato `tunnel_...`;
4. crie uma Runtime API key com Tunnels Read + Use;
5. no ChatGPT, com Developer mode, crie uma conexao/app com
   **Connection = Tunnel**;
6. selecione o mesmo `tunnel_id`.

Se o tunnel nao aparecer no ChatGPT, associe-o ao workspace/conta correta na
pagina de Tunnels.

## Primeira execucao

Na pasta:

```
ShardBound\BLOCKBENCH\bridge
```

rode:

```powershell
git pull
npm ci
.\Connect-ChatGPT.ps1
```

Se ainda nao houver configuracao, o script abre as paginas oficiais de Tunnels e
Runtime API keys e pede os dois valores.

A Runtime API key nao e gravada em texto puro no repositorio.

## Blockbench

Carregue:

```
.local\shardbound_bridge.js
```

em:

```
File > Plugins > Load Plugin from File
```

Abra um **Generic Model**.

Se precisar preencher a conexao manualmente no Blockbench, abra:

```
.local\config.json
```

e use:

- `bridgePort` -> porta local;
- `token` -> codigo de pareamento.

Nao use a Runtime API key no Blockbench.

## Uso diario

Depois da configuracao inicial:

```powershell
.\Connect-ChatGPT.ps1
```

Esse comando deve reutilizar o tunnel e o runtime.

No ChatGPT, abra um chat normal e selecione a conexao/app ShardBound.

Primeiro teste:

```
Execute status no meu Blockbench.
```

Depois:

```
Use read_project para inspecionar o Generic Model aberto e capture_view para mostrar a vista atual. Nao edite nada.
```

Teste completo:

```
Inspecione meu Generic Model no Blockbench e modele uma espada simples, com textura. Mostre uma captura antes e depois.
```

## Verificacao

Estado local:

```powershell
.\Status-ChatGPT.ps1
```

Verificacao estrita:

```powershell
.\Status-ChatGPT.ps1 -Verify
```

ou:

```powershell
npm run chatgpt:verify
```

O teste exige:

```
process_running = true
healthy = true
ready = true
```

Alem disso:

```powershell
npm run doctor
```

verifica a bridge local e a conexao com o Blockbench.

## Parar

```powershell
.\Disconnect-ChatGPT.ps1
```

ou:

```powershell
npm run chatgpt:stop
```

Isso para apenas o runtime local. O tunnel remoto continua cadastrado.

## Atualizar tunnel-client

```powershell
.\scripts\Install-TunnelClient.ps1 -Force
```

O instalador baixa a release oficial mais recente para Windows e valida o
checksum publicado pela OpenAI.

## Reconfigurar tunnel ou chave

```powershell
.\Connect-ChatGPT.ps1 -Reconfigure
```

Ou informe outro tunnel diretamente:

```powershell
.\Connect-ChatGPT.ps1 -TunnelId tunnel_...
```

## Comandos npm

Fluxo atual:

```powershell
npm run tunnel:install
npm run tunnel:configure
npm run tunnel:connect
npm run tunnel:status
npm run tunnel:stop

npm run chatgpt:start
npm run chatgpt:status
npm run chatgpt:verify
npm run chatgpt:stop
```

Fluxo antigo, apenas para compatibilidade:

```powershell
npm run chatgpt:legacy:start
npm run chatgpt:legacy:status
npm run chatgpt:legacy:verify
npm run chatgpt:legacy:stop
```

## Plugin privado

O plugin privado e uma camada separada do transporte.

Primeiro faca a conexao Tunnel funcionar diretamente no ChatGPT. Depois que o
ChatGPT criar o app tecnico para essa conexao, use seu ID `plugin_asdk_app_...`
no `.app.json` e publique uma nova release do plugin privado.

Isso evita empacotar qualquer URL temporaria.

## Referencias oficiais

- Secure MCP Tunnel:
  https://developers.openai.com/api/docs/guides/secure-mcp-tunnels
- Conectar e testar plugins:
  https://developers.openai.com/plugins/deploy/connect-chatgpt
- tunnel-client:
  https://github.com/openai/tunnel-client
