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
- componentes definidos por mods;
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

- DLLs?
- ABI estável?
- SDK?
- permissões especiais?
- avisos ao usuário?
- compatibilidade entre versões?

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

- cenas;
- transição entre áreas;
- streaming;
- mapas construídos;
- geração procedural;
- persistência;
- formato de scene/map.

## Save

- IDs de conteúdo;
- mod removido;
- mod atualizado;
- migrações;
- dados órfãos;
- rollback;
- versionamento de save.

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
