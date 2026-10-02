# ShardBound Blockbench Bridge

Bridge MCP + plugin do Blockbench para controlar o projeto Generic Model aberto a partir de clientes MCP, com integração adaptada ao MTools 2.1.0.

A arquitetura é:

```
Codex / cliente MCP
        |
        | stdio ou Streamable HTTP
        v
BLOCKBENCH/bridge/mcp
        |
        | WebSocket somente em 127.0.0.1
        v
ShardBound Bridge (plugin Blockbench)
        |
        +--> Blockbench Generic Model
        +--> MTools 2.1.0
```

## Estado

Versão do bridge: **0.2.0**

Compatibilidade validada pelo projeto:

- Node.js 22+
- Blockbench 5.2+
- MTools 2.1.0
- Generic Model do Blockbench
- Windows é o ambiente principal do projeto

## 1. Instalação

No diretório do repositório:

```powershell
cd BLOCKBENCH\bridge
npm ci
npm run setup
```

O setup:

- escolhe uma porta loopback livre;
- gera um pairing token local;
- gera a senha de autorização remota;
- grava `.local/config.json`;
- compila o plugin já pareado em `.local/shardbound_bridge.js`;
- gera `.local/codex-mcp.toml`.

A pasta `.local/` contém segredos locais e nunca deve ser commitada.

### Reconfigurar

Rodar `npm run setup` novamente gera novas credenciais. Depois disso, recarregue o plugin pareado no Blockbench e atualize qualquer configuração externa que dependa dele.

Variáveis opcionais:

```powershell
$env:BLOCKBENCH_BRIDGE_PORT="30123"
$env:BLOCKBENCH_BRIDGE_PUBLIC_URL="https://seu-endpoint-https.example"
npm run setup
```

Não configure `BLOCKBENCH_BRIDGE_PUBLIC_URL` para um endereço HTTP ou com path. O bridge aceita somente uma origem HTTPS.

## 2. Blockbench

Abra o Blockbench Desktop e carregue:

```
BLOCKBENCH/bridge/.local/shardbound_bridge.js
```

O plugin aparece como **ShardBound Bridge**.

No menu Tools existe **ShardBound Bridge: conexão**, onde é possível ver/alterar:

- conexão ativada;
- porta local;
- código de pareamento.

O plugin reconecta automaticamente quando o daemon reaparece.

### MTools

O adaptador foi escrito especificamente para **MTools 2.1.0**.

A cópia usada como referência está em:

```
BLOCKBENCH/MTools/mesh_tools.js
```

Instale/ative o MTools antes de usar `mtools_run`. Uma versão diferente de 2.1.0 é recusada de propósito até o adaptador ser revisado.

## 3. Diagnóstico

Execute:

```powershell
npm run doctor
```

O doctor verifica:

- Node 22+;
- configuração;
- plugin pareado;
- snippet do Codex;
- presença do MTools;
- dependências;
- diretório de saída;
- raiz do projeto;
- daemon;
- conexão com o Blockbench;
- configuração de MCP remoto.

O daemon não precisa estar iniciado manualmente para Codex/stdio: `mcp/server.mjs` o inicia em background quando necessário.

Logs locais:

```
BLOCKBENCH/bridge/.local/bridge.log
```

## 4. Codex

`npm run setup` gera:

```
BLOCKBENCH/bridge/.local/codex-mcp.toml
```

Copie a seção desse arquivo para:

```
~/.codex/config.toml
```

A configuração usa um MCP stdio local e aponta para o executável Node usado durante o setup.

Depois, reinicie o Codex e confira a lista de MCPs configurados.

O MCP stdio é a forma recomendada para este projeto no Codex porque o Blockbench e o bridge estão na mesma máquina.

## 5. ChatGPT

ChatGPT não acessa `localhost` diretamente.

Para manter o bridge privado, use o **Secure MCP Tunnel** da OpenAI apontando para o MCP local. Também é possível usar uma origem HTTPS remota controlada por você e definir `BLOCKBENCH_BRIDGE_PUBLIC_URL` antes de rodar o setup.

O endpoint HTTP do bridge é:

```
/mcp
```

Quando uma origem HTTPS pública é configurada, o bridge ativa OAuth para o MCP remoto. A senha local de aprovação é gerada em:

```
.local/chatgpt-password.txt
```

Nunca faça commit desse arquivo.

> Observação de produto: disponibilidade de MCP com ações de escrita/modificação no ChatGPT depende do plano e do workspace. O código do bridge pode estar pronto mesmo quando a conta atual não permite ferramentas MCP de escrita.

## 6. Fluxo seguro de arquivos

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

## 7. Cubes estilo Minecraft

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

## 8. MTools

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

## 9. Segurança e consistência

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

## 10. Testes

Execute:

```powershell
npm test
npm run build
```

Os testes verificam schemas, segurança de paths, Cubes, GLB, MTools e o bundle do plugin.

O repositório também contém CI em:

```
.github/workflows/blockbench-bridge.yml
```

que executa `npm ci`, `npm test` e `npm run build` em Node 22.

## 11. Fluxo recomendado para um agente

1. `status`
2. `read_project`
3. criar/abrir o modelo quando necessário
4. `put_texture`
5. criar/editar Cubes ou Meshes
6. `select_elements` antes de MTools
7. `mtools_list` e `mtools_run`
8. `capture_view` para inspeção visual
9. corrigir se necessário
10. `save_project` para preservar a fonte editável
11. `export_project` para gerar `.glb`/ `.gltf` do jogo

Sempre reutilize o `project_id` e, em operações de edição, o `expected_revision` mais recente.
