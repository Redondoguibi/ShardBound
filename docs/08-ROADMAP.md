# Roadmap inicial

O objetivo é validar a arquitetura antes de criar um jogo completo.

## Etapa 1 — Skeleton C++

Detalhes: [11-RUNTIME-SKELETON.md](11-RUNTIME-SKELETON.md)

- C++20 / MSVC;
- CMake + Ninja;
- vcpkg;
- estrutura modular do repositório;
- `Application`;
- loop mínimo;
- logging;
- lifecycle;
- builds Debug/Release.

**Resultado:** executável C++ funcional com fundação modular, ainda sem janela real.

## Etapa 2 — Janela, input e game loop

Detalhes: [12-WINDOW-INPUT-GAMELOOP.md](12-WINDOW-INPUT-GAMELOOP.md)

- SDL3;
- janela;
- keyboard/mouse;
- estrutura de gamepad;
- eventos próprios;
- ações de input;
- delta time;
- fixed update a 60 Hz;
- frame limiter;
- foco/minimização.

**Resultado:** runtime com janela e loop estável, ainda sem renderização 3D.

## Etapa 3 — Renderer D3D11

Detalhes: [13-RENDERER-D3D11.md](13-RENDERER-D3D11.md)

- forward renderer;
- device/context;
- swap chain;
- backbuffer;
- render targets;
- resize;
- clear/present;
- shaders;
- buffers;
- primeira geometria;
- integração com VSync;
- abstração própria sobre D3D11;
- flip-model swap chain;
- render scale;
- frustum/distance culling;
- instancing;
- material system mínimo;
- iluminação e sombras básicas;
- métricas de performance.

**Resultado:** primeira cena/gráfico desenhado pela engine.

## Etapa 4 — Asset manager e pipeline mínimo

Detalhes: [14-ASSET-PIPELINE.md](14-ASSET-PIPELINE.md)

- IDs universais `namespace:path`;
- VFS/mounts;
- cache compilado incremental;
- handles/lifetime;
- dependências entre assets;
- metadata/import settings;
- IDs de recursos;
- filesystem virtual/mounts;
- GLB/GLTF;
- texturas;
- cache;
- diagnósticos.

**Resultado:** conteúdo externo vira recurso de runtime.

## Etapa 5 — ECS

- EnTT encapsulado;
- Entity IDs;
- componentes básicos;
- Transform;
- lifecycle;
- systems.

**Resultado:** objetos de jogo existem como entidades/componentes.

## Etapa 6 — Mod manifest e discovery

- estrutura mínima de mod;
- `mod.json`;
- descobrir mods;
- IDs/namespaces;
- versões;
- dependências.

**Resultado:** runtime identifica pacotes externos.

## Etapa 7 — Mod loader

- resolver ordem;
- montar assets;
- registries;
- erros;
- dependências ausentes;
- ciclo de vida.

**Resultado:** um mod consegue registrar conteúdo externo.

## Etapa 8 — Linguagem v0

Especificação normativa: [18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md)

Esta etapa **não está bloqueada** por gramática, tipagem, `from` ou `modify`; esses contratos já estão definidos para v0.

- lexer;
- parser;
- AST;
- variáveis;
- funções;
- `if`;
- `for`;
- listas;
- `import`;
- categorias `@`;
- `from`;
- `modify`;
- chamadas de API do jogo.

**Resultado:** um mod executa lógica sem C++.

## Etapa 9 — Bridge linguagem ↔ runtime

- `spawn(...)`;
- entidades;
- registries;
- eventos;
- assets;
- chamadas do runtime;
- diagnósticos estruturados.

**Resultado:** scripts controlam conteúdo real do jogo.

## Etapa 10 — Primeiro mod funcional

Vertical slice:

- um modelo próprio;
- uma entidade simples;
- registro via mod;
- script;
- spawn;
- asset carregado;
- conteúdo visível no runtime.

**Resultado:** provar o workflow completo de mod.

## Etapa 11 — Gameplay framework

- player;
- câmera;
- movimento;
- health;
- dano;
- item;
- inventário;
- interação;
- física Jolt;
- áudio miniaudio.

Tudo exposto através das APIs próprias do runtime.

## Etapa 12 — Extensibilidade avançada

- hooks;
- eventos;
- patch/override;
- componentes customizados;
- C++ nativo;
- saves compatíveis com mods;
- ferramentas CLI.

## Etapa 13 — Jogo

Somente depois da plataforma estar funcional:

- hub;
- combate real;
- progressão;
- conteúdo oficial;
- identidade artística final;
- áreas oficiais.

A arquitetura nasce antes do conteúdo.
