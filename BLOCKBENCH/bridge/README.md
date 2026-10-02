# ShardBound Blockbench Bridge

Ponte local entre Blockbench Desktop, MCP e assistentes. Suporta **Generic Model**
(`free`), edição de malhas, grupos, cubos, UVs, texturas, capturas e 14 operações
adaptadas do MTools 2.1.0. O plugin do Blockbench e o plugin do assistente são
componentes diferentes: ambos usam o mesmo serviço local.

## Começar no Windows

Requisitos: Node.js 22 ou posterior, npm, Blockbench Desktop. Validado com
Node 26.4.0 e Blockbench 5.2.1. MTools é opcional para as ferramentas básicas.

Nesta pasta, execute:

```powershell
npm ci
npm run setup -- --register-codex
```

Alternativa: `./Install.ps1 -RegisterCodex`. O registro exige o comando `codex`
disponível; sem ele, rode `npm run setup` e importe os valores de
`.local/codex-config.toml` nas configurações MCP do Codex.

1. Abra o Blockbench e vá a **File > Plugins > Load Plugin from File** (ou os
   equivalentes traduzidos).
2. Carregue **`.local/shardbound_bridge.js`** desta pasta. Esse arquivo já contém
   o pareamento. `plugin/shardbound_bridge.js` é a versão distribuível sem token.
3. Para MTools, instale a versão 2.1.0. Neste repositório, o arquivo fornecido
   pelo usuário está em `../MTools/mesh_tools.js`; o ZIP não inclui MTools.
4. Abra um Generic Model. O plugin aceita uma única instância conectada do
   Blockbench. Veja a conexão em **Tools > ShardBound Bridge: conexão**.
5. Reinicie a conexão MCP do Codex ou abra um novo chat. O cliente inicia o
   serviço local automaticamente. Se necessário, use `npm run http` em um
   terminal separado, desde que o serviço ainda não esteja rodando.
6. Rode `npm run doctor`. Para operação completa, ele deve informar a ponte e
   o Blockbench conectados. Um editor fechado resulta em saída 1.

Prompt inicial: “Use o ShardBound Blockbench para inspecionar meu Generic Model,
listar as ações MTools disponíveis e mostrar uma captura antes de editar.”

O MCP foi registrado nesta máquina como `shardbound_blockbench`. O registro não
injeta ferramentas retroativamente no chat já aberto. A instalação no Chat
comum tem um fluxo próprio: veja [CHATGPT-DESKTOP.md](CHATGPT-DESKTOP.md).

## Ferramentas

| Ferramenta                           | Função                                                    |
| ------------------------------------ | --------------------------------------------------------- |
| `status`, `read_project`             | Conexão, projeto, revisão, seleção e geometria opcional   |
| `create_project`                     | Nova aba Generic Model                                    |
| `create_elements`, `update_elements` | Grupos, cubos e malhas; transformações e propriedades     |
| `edit_mesh`                          | Vértices, faces e UVs com validação de topologia          |
| `delete_elements`, `select_elements` | Remoção e seleção de elementos/componentes                |
| `capture_view`                       | Imagem da vista do editor                                 |
| `undo_redo`                          | Histórico nativo do Blockbench                            |
| `mtools_list`, `mtools_run`          | Descoberta de ações e execução dos adaptadores            |
| `put_texture`, `get_texture`         | Criação/substituição e leitura de textura PNG             |
| `export_project`                     | Novos arquivos `.bbmodel`, `.gltf` e `.obj` em `outputs/` |

As mutações usam `project_id`; `expected_revision` permite rejeitar uma edição
baseada em estado antigo. Antes de editar, leia o projeto e use seus UUIDs reais.
UVs usam pixels da textura. Crie os grupos antes de referenciá-los como pais.

O `.bbmodel` inclui bitmaps e mantém o projeto editável. O `.gltf` é estático,
com buffers e imagens embutidos. O `.obj` contém geometria sem materiais.
Os arquivos recebem nomes únicos; o MCP retorna caminhos locais, não links
de download públicos. Não há importação arbitrária de arquivos, execução de
JavaScript fornecido pelo modelo nem API de animações nesta versão.

## Integração MTools

Adaptadores testados: `bridge_edge_loops`, `uv_turnaround_projection`,
`subdivide`, `split_edges`, `triangulate`, `poke`, `tris_to_quad`,
`laplacian_smooth`, `to_sphere`, `expand_selection`, `shrink_selection`,
`array_elements`, `polyhedron` e `torusknot`.

`mtools_list` retorna os parâmetros em JSON Schema. Apenas ações marcadas
`automated: true` podem ser executadas. Boolean, terrain, text, scatter e demais
ações sem adaptador continuam disponíveis na interface do próprio MTools.
Para Laplacian Smooth, selecione um componente conectado completo: o adaptador
evita uma falha de cache de vizinhos do MTools 2.1.0 com seleção parcial.
O código original fornecido do MTools não foi alterado.

## Serviço e manutenção

```powershell
npm run doctor
npm run stop
npm run setup
npm run build
npm test
npm run test:blockbench
npm run package
```

`setup` preserva os segredos existentes e recompila os dois plugins. `build`
recompila apenas a versão distribuível. Após atualizar o código, pare o serviço,
rode `setup` e recarregue o plugin no Blockbench. O próximo cliente stdio inicia
o daemon atualizado. O daemon permanece ativo quando um cliente encerra, para
atender Codex e ChatGPT simultaneamente. `stop` o encerra explicitamente.

O ZIP de código-fonte fica em `.local/package-<id>/shardbound-blockbench.zip`.
Inclui `plugin.json`, `mcp.json`, skill, instalador e código. Não inclui
`node_modules`, modelos do usuário, MTools nem credenciais. Ao extrair em outra
pasta/máquina, execute `npm ci` e `npm run setup` nessa pasta e carregue seu novo
plugin pareado. Evite executar duas configurações independentes na mesma porta.

O serviço escuta somente `127.0.0.1:30123` por padrão. O token autentica o
pareamento e as rotas locais. Configuração, senha OAuth e plugin pareado ficam
em `.local/`; não compartilhe essa pasta. Requisições são serializadas e têm
prazo de 45 segundos incluindo a fila. Após timeout/desconexão, inspecione o
modelo antes de repetir uma mutação: ela pode ter sido executada.

Para trocar a porta: pare a ponte, execute `npm run setup -- --port 30124`,
recarregue o plugin pareado e reinicie a conexão. Se o Blockbench já guardou
configuração manual, ajuste também o diálogo de conexão do plugin.

## Validação realizada

- Testes automatizados de validação de entrada, autenticação, fluxo OAuth HTTP,
  PKCE, códigos de uso único, refresh, MCP HTTP e serialização.
- Cliente MCP stdio real: descoberta das 15 ferramentas e consulta de status.
- Blockbench 5.2.1 real em perfil temporário: geometria, topologia, seleção,
  revisões, proteção de aba errada, Undo/Redo, todas as 14 ações MTools adaptadas,
  texturas, capturas e três formatos de exportação.

O teste Electron usa perfil descartável em `.local/e2e/`, sem abrir os modelos
do usuário. Nessa instalação, o Blockbench gera dois erros da tela inicial
antes do carregamento dos plugins; não houve novos erros nas operações da
ponte. A instalação/seleção no Chat comum da conta ainda requer validação no
host: testes do servidor não equivalem a testar a interface do ChatGPT.
