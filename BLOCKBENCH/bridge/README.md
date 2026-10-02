# ShardBound Blockbench Bridge

Ponte local entre Blockbench Desktop, MCP e assistentes. Suporta **Generic Model**
(`free`), edição de malhas, grupos, cubos, UVs, texturas, capturas e 19 operações
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
| `list_model_files`, `open_project_file`, `save_project` | Lista, abre e salva fontes em `assets/` |
| `export_project`                     | `.bbmodel`, `.gltf`, `.glb` e `.obj` em `outputs/` ou `assets/` |

As mutações usam `project_id`; `expected_revision` permite rejeitar uma edição
baseada em estado antigo. Antes de editar, leia o projeto e use seus UUIDs reais.
UVs usam pixels da textura. Crie os grupos antes de referenciá-los como pais.

O `.bbmodel` inclui bitmaps e mantém o projeto editável. O `.gltf` é estático,
com buffers e imagens embutidos. O `.obj` contém geometria sem materiais.
Os arquivos recebem nomes únicos; o MCP retorna caminhos locais, não links
de download públicos. A versão 0.2.0 também permite abrir e salvar fontes em `assets/` e exportar `.glb`, conforme a referência abaixo.

## Fluxo seguro de arquivos

O MCP **não recebe acesso arbitrário ao computador**.

Por padrão, operações de arquivo ficam limitadas a:

```
<repo>/assets/
```

Caminhos absolutos, `..`, `.` e escapes da raiz permitida são rejeitados.

Ferramentas:

### `list_model_files`

Lista fontes/exports em `assets/`.

Formatos reconhecidos:

- `.bbmodel`
- `.gltf`
- `.glb`

### `open_project_file`

Abre uma fonte editável `.bbmodel` em uma nova aba do Blockbench.

Somente Generic Models são aceitos.

### `save_project`

Salva o projeto editável como `.bbmodel` em `assets/`, com texturas embutidas.

### `export_project`

Exporta:

- `bbmodel`
- `obj`
- `gltf`
- `glb`

`gltf` e `glb` usam o codec nativo do Blockbench com texturas embutidas.

Se `path` for informado, o destino deve estar em `assets/`. Sem `path`, um arquivo único é criado em `BLOCKBENCH/bridge/outputs/`.

## Cubes estilo Minecraft

Cubes são uma parte de primeira classe do contrato MCP.

`create_elements` e `update_elements` suportam:

- `from` / `to`;
- origin/pivot;
- rotation;
- inflate;
- shade;
- mirror UV;
- box UV;
- UV offset;
- as seis faces;
- texture UUID por face;
- UV por face;
- rotação UV 0/90/180/270;
- face enabled/disabled.

Exemplo conceitual:

```json
{
  "type": "cube",
  "name": "stone",
  "from": [0, 0, 0],
  "to": [16, 16, 16],
  "faces": {
    "north": {
      "uv": [0, 0, 16, 16],
      "texture": "TEXTURE_UUID",
      "rotation": 0,
      "enabled": true
    }
  }
}
```

Use `put_texture` primeiro para obter o UUID de uma textura criada pelo MCP.

## MTools

Use `mtools_list` para descobrir disponibilidade e parâmetros.

A versão 0.2.0 automatiza 19 ações do MTools:

- Subdivide
- Split Edges
- Triangulate
- Poke
- Triangles to Quads
- Laplacian Smooth
- To Sphere
- Expand Selection
- Shrink Selection
- Array
- Bridge Edge Loops
- Cubic Projection
- Scatter
- Boolean
- Polyhedron
- Torus Knot
- Text Mesh
- Terrain
- XYZ Math Surface

Não automatizadas propositalmente:

- **Project From View** — o próprio MTools marca como deprecated; depende do estado de câmera/viewport.
- **Terrain Style Editor** — altera configuração persistente do MTools, não é uma operação de modelagem do projeto.

### Boolean

Boolean é experimental no MTools 2.1.0. O MCP exige:

```json
{
  "acknowledge_experimental": true
}
```

e exatamente dois meshes selecionados.

### Seleção

Use `select_elements` antes de ações que dependem de componentes.

O bridge valida o modo esperado, por exemplo:

- object;
- vertex;
- edge;
- face;
- edge ou face para Bridge Edge Loops.

## Segurança e consistência

O bridge inclui:

- listener HTTP/WebSocket em loopback para transporte local;
- pairing token;
- validação de Host/Origin;
- OAuth quando `publicUrl` está habilitado;
- schemas Zod para ferramentas;
- limites de tamanho;
- `project_id`;
- `expected_revision`;
- fila serial de operações;
- timeouts;
- Undo;
- rollback de operações MTools quando possível;
- restrição de arquivos a asset roots;
- rejeição de versão MTools não validada;
- nenhum recurso de execução arbitrária de JavaScript pelo MCP.

Se uma chamada atingir timeout, **leia o projeto novamente antes de repetir**, porque a operação pode ter terminado dentro do Blockbench depois que o cliente desistiu.


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

`npm run doctor -- --offline` verifica a instalação sem exigir o editor aberto e é usado no CI.
O setup aceita também `BLOCKBENCH_BRIDGE_PORT` e `BLOCKBENCH_BRIDGE_PUBLIC_URL`.
Na primeira instalação, escolhe uma porta livre; nas seguintes, preserva a porta e as credenciais.

## Validação realizada

- Testes automatizados de validação de entrada, autenticação, fluxo OAuth HTTP,
  PKCE, códigos de uso único, refresh, MCP HTTP e serialização.
- Cliente MCP stdio real: descoberta das 18 ferramentas e consulta de status.
- Blockbench 5.2.1 real em perfil temporário: geometria, topologia, seleção,
  revisões, proteção de aba errada, Undo/Redo, todas as 14 ações MTools adaptadas,
  texturas, capturas e quatro formatos de exportação, além de salvar/reabrir a fonte.

O teste Electron usa perfil descartável em `.local/e2e/`, sem abrir os modelos
do usuário. Nessa instalação, o Blockbench gera dois erros da tela inicial
antes do carregamento dos plugins; não houve novos erros nas operações da
ponte. A instalação/seleção no Chat comum da conta ainda requer validação no
host: testes do servidor não equivalem a testar a interface do ChatGPT.
