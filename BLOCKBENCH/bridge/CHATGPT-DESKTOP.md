# ChatGPT comum no app Desktop

Pesquisa consultada em **2 de outubro de 2026**.

## Plugin privado criado para esta máquina

O [ShardBound Blockbench](https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53)
foi salvo como plugin pessoal privado em 2 de outubro de 2026. Abra seu link no
ChatGPT Desktop e conclua a instalação se o botão for exibido. Em um Chat novo,
selecione o plugin pelo `@` e peça uma consulta de status.

Este pacote privado usa o Node e o servidor preparados na pasta original
`BLOCKBENCH/bridge` desta máquina. Mantenha a pasta no mesmo local. O token não
foi enviado: o runtime lê a configuração local existente. A instalação da versão
0.1.0 foi observada pelo usuário; as ferramentas no Chat comum não foram expostas.
A versão 0.1.1 foi salva na conta, mas sua execução no Chat comum e a conexão
com o editor ainda não estão confirmadas.

O pacote com essa configuração é gerado por `npm run package:desktop`. O pacote
portável de código-fonte continua sendo gerado por `npm run package`.

**Selecionar o plugin no Chat comum não comprova que suas ferramentas locais
foram carregadas.** O usuário testou a versão 0.1.0 no Desktop e recebeu a skill,
mas não recebeu `status`, `read_project` ou `capture_view`. A disponibilidade de
plugins e de MCPs locais é específica de cada modalidade e conta. A documentação
geral de plugins no Desktop não valida este pacote stdio no Chat comum.

Na versão 0.1.1, o arquivo gerado inclui também `.mcp.json` e um manifesto de
compatibilidade com referência explícita ao MCP. Após salvar, a leitura do
plugin pela API mostrou que o importador preservou `mcp.json` e `.mcp.json`, mas
recriou `.codex-plugin/plugin.json` sem `mcpServers`, incluindo apenas skills.
Os dois arquivos MCP iniciam o servidor e descobrem 15 ferramentas quando usados
por um cliente local real. Esse teste não valida sua descoberta no Chat comum.
Não edite o cache para tentar corrigir: o importador pode recriá-lo.

Há dois diagnósticos distintos: ferramentas ausentes significam que o host não
as expôs; uma chamada real de `status` com `connected: false` significa que o
servidor respondeu, mas o plugin do Blockbench não está pareado. Carregar o
plugin no editor resolve apenas o segundo caso.

Para uma conexão customizada documentada no Chat comum, registre um MCP remoto
via HTTPS ou Secure MCP Tunnel, conforme
[Connect and test](https://developers.openai.com/plugins/deploy/connect-chatgpt).
O servidor local deste projeto já possui a alternativa HTTP/OAuth descrita
abaixo. Ela ainda não tem URL HTTPS ou túnel cadastrado.

Fontes oficiais:

- [ChatGPT Learn — Plugins](https://learn.chatgpt.com/docs/plugins): plugins
  funcionam em Chat e Work, incluindo Desktop; plugins “Desktop only” exigem
  o app para instalação e uso.
- [Help Center — Plugins in ChatGPT](https://help.openai.com/en/articles/20001256-plugins-in-chatgpt):
  na seção “Choose what your plugin contains”, documenta MCPs locais executados
  no computador pelo ChatGPT Desktop; salvar na conta não os torna executáveis
  na web/mobile. Também documenta criação pelo Plugin Creator e restrições de
  importação/instalação por conta ou workspace.
- [Formato de pacote](https://developers.openai.com/plugins/build/plugins):
  manifests e fontes de plugins; marketplaces locais têm disponibilidade
  específica por superfície.

## Teste do plugin local em um host compatível

O pacote deste projeto contém `plugin.json`, `mcp.json` com transporte `stdio`,
skill e servidor Node. Assim, o host pode iniciar o MCP na própria máquina,
que por sua vez conversa com o plugin carregado no Blockbench.

1. Conclua a preparação e o pareamento descritos no [README](README.md).
2. No ChatGPT Desktop, abra **Plugins**. Use o fluxo disponível de criação ou
   instalação local do **Plugin Creator** com a pasta deste projeto. Solicitação
   sugerida: “Instale como plugin local o ShardBound Blockbench desta pasta,
   usando seu plugin.json e mcp.json; quero usá-lo no Chat comum do Desktop.”
3. Se o host copiar o pacote para outra pasta, nessa cópia precisam existir as
   dependências Node e uma configuração válida. Execute `npm ci` e `npm run
setup` ali e carregue o plugin pareado gerado ali. Alternativamente, configure
   o servidor instalado com `BLOCKBENCH_BRIDGE_CONFIG` apontando para a
   configuração já existente; assim os dois clientes reutilizam o mesmo
   pareamento e daemon. Não copie credenciais para um ZIP compartilhado.
4. Inicie um chat novo em um host compatível com processos locais, selecione o
   plugin pelo `@`, pelo `+` ou pelo seletor de Plugins e confirme a descoberta
   das ferramentas. Se estiver testando o Chat comum, não presuma suporte apenas
   porque o plugin aparece na lista.
5. Peça `status`, `read_project` e `capture_view`. Só depois teste uma pequena
   edição e Undo em um modelo de teste.

O ZIP é um pacote de código-fonte; não instala Node nem executa `npm ci`
automaticamente. Importação de ZIP pela conta/workspace não é um botão universal:
use-a apenas se estiver disponível e se a conta permitir. A instalação local
no Codex e a publicação privada na conta são etapas distintas. O pacote privado
foi salvo na conta; não foi publicado no diretório público.

**Estado verificado:** servidor, Blockbench, MTools e cliente MCP funcionando;
MCP registrado no Codex desta máquina. **Ainda não verificado:** o pacote
selecionável e executando ferramentas em um Chat comum da sua conta. A pesquisa
confirma suporte do produto, não confirma permissões ou implantação na conta.

O caminho local não exige chave da API OpenAI nem túnel. O assistente usa a
autenticação e os limites do produto onde você abriu a conversa.

## Alternativa: MCP remoto autenticado por OAuth

Útil se você também quiser acesso pela web ou se o fluxo de plugins locais não
estiver disponível na sua conta. O servidor já implementa Streamable HTTP,
OAuth, descoberta de metadados, registro dinâmico de clientes e PKCE S256.

É necessário um endereço **HTTPS estável** encaminhando as requisições para
`http://127.0.0.1:30123`. O computador e o Blockbench precisam permanecer ligados.
O provedor/túnel não foi configurado automaticamente neste projeto.

```powershell
npm run stop
npm run setup -- --public-url https://SEU-DOMINIO
npm run http
```

Encaminhe o host inteiro, incluindo `/mcp`, endpoints OAuth e `/.well-known/`,
preservando o caminho. Cadastre `https://SEU-DOMINIO/mcp` como MCP remoto no fluxo
de desenvolvimento de plugins do ChatGPT e escolha OAuth. Autorize usando a
senha de `.local/chatgpt-password.txt` na página de consentimento da própria
ponte. Não use o token interno do Blockbench como senha.

O fluxo de conexão customizada e seu acesso dependem da conta/workspace:
[Connect and test](https://developers.openai.com/plugins/deploy/connect-chatgpt)
e [Developer mode](https://developers.openai.com/api/docs/guides/developer-mode).
Não basta colocar `localhost` como endereço de um MCP executado pela nuvem.

Tokens e autorizações OAuth ficam em memória: reiniciar o daemon requer uma
nova autorização. O servidor continua vinculado a loopback; o túnel/proxy fornece
o HTTPS. O cliente local do Codex continua usando o token local, simultaneamente.
Arquivos exportados continuam no computador; essa opção não publica os modelos.

Para retornar ao modo local: pare o serviço, execute `npm run setup --
--local-only`, encerre o túnel e reinicie a conexão. O Secure MCP Tunnel da OpenAI
é outra opção documentada, mas exige credenciais/acesso próprios e não está
pré-configurado aqui:
[Secure MCP Tunnel](https://developers.openai.com/api/docs/guides/secure-mcp-tunnels).
