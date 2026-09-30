# Roadmap inicial

O objetivo é validar a arquitetura antes de criar um jogo completo.

## Fase 0 — Especificação

- definir requisitos do runtime;
- escolher bibliotecas fundamentais;
- escolher sistema de build;
- definir estrutura do repositório;
- decidir arquitetura de entidades;
- especificar formato de mod;
- desenhar linguagem v0.

**Resultado:** especificação mínima implementável.

## Fase 1 — Runtime mínimo

Aplicação C++ capaz de:

- abrir janela;
- processar input;
- renderizar uma cena 3D simples;
- carregar uma câmera;
- importar um modelo;
- carregar textura;
- mover um objeto.

**Resultado:** sandbox 3D própria.

## Fase 2 — Asset pipeline

- IDs de recursos;
- filesystem virtual/mounts;
- importação GLB/GLTF;
- texturas;
- áudio;
- cache;
- diagnósticos.

**Resultado:** conteúdo externo vira recurso de runtime.

## Fase 3 — Mod loader v0

- descobrir mods;
- ler manifest;
- resolver ordem;
- registrar assets;
- dependências simples;
- mensagens de erro.

**Resultado:** um pacote externo modifica o conteúdo carregado.

## Fase 4 — Linguagem v0

Começar pequeno:

- lexer;
- parser;
- AST;
- tipos básicos;
- funções;
- módulos;
- chamadas para APIs do jogo;
- compilação/interpretação mínima.

**Resultado:** um mod executa lógica sem C++.

## Fase 5 — Gameplay framework

Criar apenas o mínimo:

- player;
- câmera;
- movimento;
- entidade;
- health;
- dano;
- item;
- inventário;
- interação.

Tudo exposto ao sistema de mods.

## Fase 6 — Mod vertical slice

Criar um mod oficial de teste com:

- pequena área;
- inimigo;
- item;
- equipamento;
- evento;
- assets próprios;
- lógica própria.

**Resultado:** provar o workflow completo.

## Fase 7 — Extensibilidade avançada

- hooks;
- eventos;
- patch/override;
- componentes customizados;
- extensões nativas C++;
- save compatível com mods;
- ferramentas CLI completas.

## Fase 8 — Jogo

Somente depois da plataforma funcional:

- hub;
- combate real;
- progressão;
- conteúdo oficial;
- identidade artística final;
- áreas oficiais.

A arquitetura nasce antes do conteúdo.
