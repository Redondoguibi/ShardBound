# Decisões em aberto

Este arquivo evita fechar prematuramente decisões importantes.

## Core

- Windows-only inicialmente?
- C++20, C++23 ou outra versão?
- CMake, Meson ou outro build system?
- arquitetura de módulos;
- sistema de jobs/threads.

## Rendering

- Direct3D 12, Vulkan ou camada de abstração?
- biblioteca de janela/input;
- formato de shaders;
- pipeline de materiais;
- iluminação;
- estética pixel-art exata.

## Física

- Jolt;
- PhysX;
- outra biblioteca;
- solução própria para partes específicas.

## Entidades

- ECS puro;
- composição por componentes;
- objetos tradicionais;
- arquitetura híbrida.

## Linguagem

- nome;
- extensão de arquivo;
- sintaxe;
- tipagem;
- gerenciamento de memória;
- VM/bytecode vs interpretação;
- classes/componentes;
- módulos;
- erros;
- async/coroutines;
- FFI.

## Mods

- extensão/pacote;
- estrutura obrigatória;
- assinatura/verificação;
- permissões;
- load order;
- patch/override;
- isolamento;
- dependências quebradas;
- versionamento de API.

## C++ nativo

- DLLs?
- ABI estável?
- SDK?
- permissões especiais?
- avisos ao usuário?
- compatibilidade entre versões?

## Assets

- GLB/GLTF oficial?
- formatos de textura;
- formatos de áudio;
- formato de mapas;
- compressão;
- cache;
- shaders de mods.

## Mundo

- cenas;
- transição entre áreas;
- streaming;
- mapas construídos;
- procedural;
- persistência.

## Save

- IDs de conteúdo;
- mod removido;
- mod atualizado;
- migrações;
- dados órfãos;
- rollback.

## Ferramentas

- CLI;
- debugger da linguagem;
- profiler;
- validador;
- test runner;
- editor visual futuro.

---

A próxima etapa deve atacar essas decisões por dependência: primeiro **runtime/plataforma**, depois **asset model**, depois **mod model**, e só então os detalhes completos da linguagem.
