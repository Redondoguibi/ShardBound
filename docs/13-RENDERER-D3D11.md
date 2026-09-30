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

**Definido:** a identidade pixel-art ficará principalmente nos **assets**, não em uma renderização final deliberadamente pixelada.

Direção visual:

- modelos 3D;
- texturas pixeladas/baixa resolução;
- point/nearest filtering onde aplicável;
- materiais estilizados;
- imagem final renderizada normalmente na resolução interna escolhida;
- UI e efeitos podem manter linguagem visual pixel-art sem exigir que a tela inteira seja low-res.

O render scale continua existindo como ferramenta de performance, não como parte obrigatória da estética.

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

Baseline:

- uma luz direcional principal representando o Sol/Lua;
- ambient/hemisphere light simples;
- point/spot lights controladas;
- luzes dinâmicas limitadas por importância, distância e visibilidade.

### Ciclo diurno

**Definido:** áreas possuem ciclo dia/noite normal, com iluminação variando ao longo do tempo.

Cada área poderá, porém, definir sua própria identidade cromática. Exemplos:

- floresta com matiz mais quente/esverdeada;
- área congelada com matiz mais fria/azulada;
- região corrompida com ambiente violeta;
- deserto com tons quentes e saturação própria.

Isso não significa travar a área em uma hora do dia. A área fornece parâmetros de ambiente que modulam o ciclo diurno.

Conceitualmente:

```text
Global/Area Day Cycle
      +
Area Lighting Profile
      ↓
Sun direction/intensity
Ambient color
Fog color
Exposure
Color grading
Sky parameters
```

Perfis de iluminação devem ser dados do mapa/área e, futuramente, modificáveis por mods.

Não implementar centenas de luzes dinâmicas reais no renderer inicial.

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

Inicialmente leve, mas preparado para a identidade forte de cada área.

Baseline:

- color grading por área;
- exposição;
- bloom simples;
- fog;
- vignette opcional.

O color grading é particularmente importante porque cada área poderá ter uma matiz/identidade cromática diferente sem precisar alterar manualmente todos os assets.

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

## VFX e partículas

**Definido:** combates poderão ter muitos efeitos e partículas.

O renderer deve ser projetado para isso desde cedo.

### Estratégia

Nem todo efeito visual deve criar uma luz real.

Separar:

```text
visual particle
emissive particle
dynamic light
decal/effect
```

Uma explosão pode ter centenas de partículas emissivas, mas apenas uma ou poucas luzes reais.

### Particle System

O sistema deve suportar:

- emissão em bursts;
- emissão contínua;
- lifetime;
- velocidade/aceleração;
- escala;
- rotação;
- cor ao longo da vida;
- alpha;
- emissive;
- billboard particles;
- meshes simples quando necessário;
- additive e alpha blending;
- atlas/sprite sheets;
- limites por preset;
- distance culling;
- pooling/reutilização.

A primeira versão pode simular partículas na CPU e renderizá-las de forma instanciada/batched.

GPU particles podem ser adicionadas futuramente caso profiling real mostre necessidade.

### Dynamic Light Budget

Luzes de combate devem usar orçamento controlado.

Quando houver efeitos demais, o renderer pode priorizar luzes por:

- distância da câmera;
- intensidade;
- tamanho aparente;
- prioridade do efeito;
- relevância para o player.

Efeitos visuais continuam aparecendo mesmo quando uma luz secundária for descartada.

Isso permite combate visualmente intenso sem transformar cada partícula em uma light source.

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

Ainda precisam ser refinados futuramente:

- modelo exato do céu;
- duração/escala do ciclo dia-noite;
- transições climáticas;
- número concreto de dynamic lights por preset;
- formato dos perfis de iluminação das áreas;
- formato final dos efeitos/partículas para mods;
- anti-aliasing definitivo.

Esses detalhes não bloqueiam o renderer v0.
