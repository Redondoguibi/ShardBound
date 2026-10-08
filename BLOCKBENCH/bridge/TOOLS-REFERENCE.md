# Referência de modelagem e MTools

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
de download públicos. O bridge também permite abrir e salvar fontes em `assets/` e exportar `.glb`, conforme a referência abaixo.

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
Por padrão, preserva arquivos existentes. Para atualizar uma fonte já salva,
envie `overwrite: true` explicitamente.

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

O bridge automatiza 19 ações do MTools 2.1.0:

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



Para instalar ou conectar ao Chat normal, use [CHATGPT-DESKTOP.md](CHATGPT-DESKTOP.md).
