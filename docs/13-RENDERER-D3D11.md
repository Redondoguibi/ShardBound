# Etapa 3 — Renderer Direct3D 11

Este documento formaliza a direção atual do renderer.

## Objetivo

Criar um renderer 3D próprio, leve e modular, usando Direct3D 11 como backend inicial.

O objetivo não é competir com engines generalistas. O renderer deve servir muito bem ao jogo: 3D estilizado/pixel-art, áreas de escala moderada, muitos objetos relativamente simples e foco em desempenho em hardware modesto.

## Princípios

1. **Renderer simples antes de renderer sofisticado.**
2. **Poucos passes e custo previsível.**
3. **Gameplay e mods nunca dependem diretamente de D3D11.**
4. **Recursos gráficos são acessados por handles/IDs próprios.**
5. **Escalabilidade de qualidade entra na arquitetura desde o início.**
6. **Profiling e debug fazem parte do renderer, não são tarefas finais.**

## Backend

Baseline:

- Direct3D 11;
- feature level alvo: 11_0 ou superior;
- HLSL;
- DXGI flip-model swap chain;
- pelo menos double buffering;
- formato SDR inicialmente;
- suporte estrutural a VSync e tearing quando disponível.

A Microsoft recomenda flip model para melhor desempenho em aplicações Win32 modernas. O backend deve usar esse modelo em vez das swap chains antigas de cópia/blit quando possível.

## Arquitetura

```text
Game / Mods
    ↓
Render API própria
    ↓
Renderer
├─ RenderDevice
├─ SwapChain
├─ RenderContext
├─ ResourceManager
├─ ShaderManager
├─ MaterialSystem
├─ MeshSystem
├─ Camera
├─ Lighting
├─ RenderQueue
└─ D3D11 Backend
```

Nenhuma API pública de gameplay deve expor:

- ID3D11Device;
- ID3D11DeviceContext;
- ID3D11Texture2D;
- outros objetos COM do Direct3D.

## Pipeline inicial

A escolha atual é um **forward renderer**.

Fluxo conceitual:

```text
collect visible objects
↓
frustum culling
↓
build render queues
↓
shadow pass
↓
opaque pass
↓
transparent/alpha pass
↓
particles
↓
post-process leve
↓
UI
↓
present
```

### Por que forward?

Para este projeto:

- é mais simples de implementar;
- consome menos memória;
- combina com número moderado de luzes;
- funciona bem em GPU integrada;
- facilita MSAA caso seja desejado futuramente;
- reduz a quantidade de render targets e bandwidth.

Deferred rendering não é baseline inicial.

## Render targets

Inicialmente:

- color target;
- depth buffer;
- shadow map;
- render target intermediário opcional para resolução interna/pós-processamento.

Evitar uma cadeia grande de buffers até existir necessidade real.

## Resolução interna

O renderer deve separar:

```text
window resolution
≠
internal render resolution
```

Isso permitirá, por exemplo:

```text
janela: 1920×1080
render scale: 75%
render 3D: 1440×810
UI: 1920×1080
```

Essa é uma ferramenta importante para manter FPS alto em GPUs integradas.

O método final de upscale ainda será decidido.

## Pixel-art 3D

A identidade visual ainda precisa de uma decisão importante:

### Opção A — pixel-art nos assets

- modelos 3D;
- texturas pixeladas/baixa resolução;
- point/nearest filtering onde aplicável;
- imagem final continua em resolução normal.

### Opção B — imagem 3D também pixelada

- cena renderizada em resolução interna deliberadamente baixa;
- upscale controlado;
- pixels visíveis na imagem final.

O renderer deve conseguir suportar ambos, mas precisamos escolher qual será a identidade principal do jogo antes de finalizar o pipeline de pós-processamento.

## Materiais

Começar com um material pequeno e previsível.

Baseline conceitual:

```text
Material
├─ base color / albedo
├─ base texture
├─ normal map opcional
├─ roughness
├─ metallic
├─ emissive
├─ alpha mode
└─ flags
```

Porém, materiais simples não devem pagar o custo de recursos que não usam.

O sistema deve permitir variantes leves:

- unlit;
- lit;
- alpha-test;
- transparent;
- emissive.

Não criar um sistema de materiais gigantesco no primeiro renderer.

## Iluminação

Baseline recomendada:

- uma luz direcional principal;
- ambient/hemisphere light simples;
- número controlado de point/spot lights;
- luzes dinâmicas limitadas por área/objeto visível.

Não implementar centenas de luzes dinâmicas no renderer inicial.

A necessidade de ciclo dia/noite ainda está em aberto.

## Sombras

Baseline:

- shadow map para a luz direcional principal;
- qualidade configurável;
- distância de sombra configurável;
- resolução configurável;
- sombras podem ser desativadas nos presets mais baixos.

Cascaded Shadow Maps podem ser adicionados, mas não devem bloquear o primeiro renderer funcional.

## Transparência

Separar pelo menos:

- opaque;
- alpha-test/cutout;
- transparent.

Alpha-test deve ser preferido para vegetação/objetos estilizados quando for visualmente adequado, pois transparência real custa mais e exige ordenação.

## Culling

Obrigatório desde cedo:

### Frustum culling

Não enviar para a GPU objetos fora da câmera.

### Distance culling

Objetos pequenos/distantes podem ser omitidos conforme regras do asset/área.

### Occlusion culling

Não é requisito da primeira versão. Pode ser adicionado depois quando cenas reais justificarem sua complexidade.

## Instancing

Instancing deve fazer parte do design inicial.

Exemplo:

```text
200 árvores usando o mesmo mesh/material
→ poucas draw calls instanciadas
```

Isso é especialmente relevante para áreas com:

- árvores;
- pedras;
- vegetação;
- decoração repetida;
- mobs do mesmo tipo.

## Render queues e sorting

Objetos devem ser agrupados/sortidos para reduzir trocas caras de:

- shader;
- material;
- textura;
- mesh/pipeline state.

O renderer não deve simplesmente desenhar entidades na ordem em que elas existem no ECS.

## Shaders

HLSL será usado no backend D3D11.

O sistema deve:

- compilar shaders em desenvolvimento;
- armazenar diagnósticos claros;
- cachear resultados;
- permitir variantes controladas;
- futuramente pré-compilar shaders para builds de distribuição.

Mods não recebem acesso irrestrito ao dispositivo D3D11 apenas por fornecer shaders.

A política de shaders de mods será definida separadamente.

## Post-processing

Inicialmente leve.

Possíveis efeitos:

- color grading;
- exposição;
- bloom simples;
- fog;
- vignette opcional.

Evitar como baseline:

- TAA pesado;
- SSR;
- volumetria cara;
- ray tracing;
- cadeias grandes de efeitos.

Efeitos devem ser opcionais por preset.

## Anti-aliasing

Ainda não fechado.

Para estética pixel-art, TAA pode borrar detalhes e não deve ser adotado automaticamente.

Possíveis caminhos:

- nenhum AA;
- FXAA/SMAA opcional;
- MSAA em partes do pipeline;
- upscale/pixelização que reduza a necessidade de AA.

A escolha depende de como a identidade pixel-art final será definida.

## Performance

O renderer deve medir:

- FPS;
- frame time;
- CPU render time;
- GPU frame time quando possível;
- draw calls;
- triangles;
- visible objects;
- culled objects;
- uso de memória de recursos quando possível.

Não esperar o jogo ficar pesado para adicionar métricas.

## Debug

Debug builds devem usar recursos de diagnóstico do Direct3D quando disponíveis.

O renderer deve produzir logs claros para:

- falha na criação do device;
- feature level;
- swap chain;
- shader compilation;
- resource creation;
- resize;
- device removed/reset;
- invalid assets.

## Presets

A arquitetura deve permitir presets equivalentes a:

```text
Potato
Low
Medium
High
```

Eles poderão controlar:

- render scale;
- shadow resolution;
- shadow distance;
- shadow enable;
- light limits;
- particles;
- post-processing;
- vegetation distance;
- object distance;
- texture quality.

## Resize

Fluxo:

```text
WindowResized
↓
Renderer::resize(width, height)
↓
release/rebind size-dependent targets
↓
ResizeBuffers
↓
recreate backbuffer/depth/render targets
```

## Critério de conclusão da etapa 3

O renderer v0 está concluído quando consegue:

```text
criar D3D11 device/context
↓
criar flip-model swap chain
↓
criar backbuffer + depth
↓
limpar a tela
↓
responder a resize
↓
compilar/carregar shader
↓
criar vertex/index buffer
↓
desenhar primeiro triângulo/mesh
↓
usar câmera básica
↓
apresentar frame
↓
VSync/frame limiting integrar corretamente
↓
shutdown sem leaks óbvios
```

## Decisões ainda necessárias

Antes de fechar a identidade visual do renderer:

1. Pixel-art será principalmente dos assets ou a imagem final também deverá ser deliberadamente pixelada?
2. O jogo precisa de ciclo dia/noite e iluminação mudando continuamente, ou a iluminação pode ser majoritariamente definida por área?
3. Qual o grau desejado de luzes dinâmicas e efeitos visuais durante combate?

Estas decisões afetam pós-processamento, iluminação e sombras, mas não impedem a implementação do renderer base.
