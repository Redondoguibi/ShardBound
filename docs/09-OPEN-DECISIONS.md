# Decisões em aberto

Este arquivo evita fechar prematuramente decisões importantes.

Para decisões já tomadas sobre a fundação técnica, veja [10-TECH-STACK.md](10-TECH-STACK.md). Para a sintaxe atual da linguagem, veja [04-LANGUAGE.md](04-LANGUAGE.md).

## Core

Já definido:

- Windows-only inicialmente;
- C++20;
- MSVC;
- CMake + Ninja;
- vcpkg.

Ainda em aberto:

- arquitetura detalhada dos módulos internos;
- sistema de jobs/threads;
- política de alocação de memória;
- logging;
- profiling.

## Rendering

Baseline definida:

- Direct3D 11;
- SDL3;
- camada própria de abstração do renderer.

Ainda em aberto:

- formato de shaders;
- pipeline de materiais;
- iluminação;
- sombras;
- pós-processamento;
- estética pixel-art exata;
- estratégia de batching/instancing;
- resolução dinâmica/upscaling;
- eventual backend D3D12.

## Física

Baseline definida:

- Jolt Physics através de uma API própria.

Ainda em aberto:

- política de layers;
- character controller;
- integração física/ECS;
- determinismo;
- parâmetros de timestep.

## Entidades

Baseline definida:

- EnTT por baixo;
- API própria de entidades/componentes por cima.

Ainda em aberto:

- modelo exato de componentes;
- registries;
- lifecycle;
- serialização;
- systems/schedulers;
- componentes nativos arbitrários além dos componentes de dados v1 da etapa 12;
- fronteira entre ECS e objetos tradicionais.

## Linguagem

A Etapa 8 possui especificação implementável em [18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md).

**Já definido para v0:**

- extensão de trabalho `.sl`;
- gramática/sintaxe mínima;
- tipagem inferida/dinâmica na superfície;
- tipos básicos mínimos;
- variáveis, globais e constantes;
- funções e parâmetros com default;
- múltiplos retornos;
- `if`, `else`, `for`, listas;
- imports;
- categorias `@`;
- símbolos;
- escopo lexical;
- semântica de `from`;
- semântica de `modify`;
- merge recursivo de estruturas;
- conflitos explícitos para patches incompatíveis;
- AST mínimo;
- diagnósticos mínimos.

**Ainda em aberto, mas não bloqueia a v0:**

- nome definitivo da linguagem;
- gerenciamento de memória interno;
- VM/bytecode vs interpretação direta;
- namespaces avançados;
- sistema avançado de módulos/export;
- async/coroutines;
- FFI completo;
- visibilidade avançada;
- generics;
- pattern matching;
- macros/metaprogramação;
- debugger completo.

## Mods

- extensão/pacote;
- estrutura mínima obrigatória;
- assinatura/verificação;
- permissões;
- load order;
- patch/override;
- isolamento;
- conflitos;
- comportamento de dependências quebradas;
- versionamento de API.

## C++ nativo

Definido na etapa 12: DLL Windows x64, ABI C versionada, SDK próprio,
habilitação explícita por `--allow-native`, callbacks na thread do runtime,
negociação de versão/tamanho e unload em ordem reversa. Contrato em
[21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md).

Ainda abertos: assinatura/distribuição, isolamento fora do processo, hot reload,
permissões granulares e novas versões da ABI.

## Assets

Baseline atual:

- GLB/GLTF;
- fastgltf;
- DirectXTex.

Ainda em aberto:

- formatos finais de textura;
- formatos de áudio;
- formato de mapas;
- compressão;
- cache;
- shaders de mods;
- formato interno compilado.

## Mundo

Direção de gameplay definida: hub/aldeia segura, primeira região de floresta,
mundo aberto, waystones ativadas por proximidade e viagem pelo mapa a partir
de qualquer lugar, bloqueada até completar 6 segundos sem causar/receber dano.
Viagem gratuita e instantânea; ativações persistem após morte e save/load.
Veja [22-GAME-DIRECTION-AND-REFERENCES.md](22-GAME-DIRECTION-AND-REFERENCES.md).

Contratos técnicos ainda abertos:

- cenas;
- transição entre áreas;
- streaming;
- mapas construídos;
- geração procedural;
- persistência;
- formato de scene/map.

## Jogo e conteúdo oficial

Definido pelo autor: referência principal MCD II, conteúdo centrado em mods,
fontes/UI próprias, espada de ferro inicial, padrões de ataque por arma,
movimento somente WASD no PC, crítico durante a queda de 1,7x,
níveis numéricos/raridades e retorno ao hub com perda de 15% do ouro ao morrer,
arredondada para baixo. Renascer restaura toda a vida e mantém itens/equipamentos.
Sem lore, campanha, fim ou sequência
obrigatória de progressão definidos.

Ainda abertos: outros bindings/câmera, detalhes técnicos de detecção do acerto
durante a queda, slots/raridades/demais fórmulas, balanceamento,
raio de ativação, casos especiais de dano, escala e rig.
Pesquisa de referência não resolve automaticamente esses contratos.

## Save

Definido na etapa 12: formato v1, IDs de conteúdo e GUIDs de entidades, estado
por mod, preservação de órfãos, migrações versionadas de componentes, backup
anterior e restauração transacional. Veja [21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md).

Ainda abertos: futuros formatos de mundo/área, save distribuído/cloud, migrações
entre versões futuras do formato e UI de gerenciamento/recuperação.

## Ferramentas

- CLI;
- debugger da linguagem;
- profiler;
- validador;
- test runner;
- editor visual futuro.

---

A sequência recomendada agora é:

```text
Runtime mínimo
→ Asset model
→ Entity/component API
→ Mod model
→ Mod loader
→ Linguagem formal
→ Gameplay
```
