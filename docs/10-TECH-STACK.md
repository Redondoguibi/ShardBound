# Stack técnica atual

Este documento registra a fundação técnica escolhida até agora.

## Plataforma

**Definido:**

- Windows somente inicialmente.
- Arquitetura alvo principal: x64.

A compatibilidade com outras plataformas não é prioridade nesta fase.

## Core

**Baseline atual:**

- C++20
- MSVC
- CMake
- Ninja
- vcpkg

## Renderização

**Baseline atual:**

- Direct3D 11 como backend inicial.
- SDL3 para janela/input.
- O renderer deve possuir uma abstração própria para evitar acoplamento de gameplay/mods diretamente ao D3D11.

Arquitetura desejada:

```text
Game / Mods
    ↓
Renderer API do projeto
    ↓
D3D11 backend
```

Um backend D3D12 poderá ser criado futuramente sem exigir reescrever os sistemas de gameplay.

## Entidades

**Baseline atual:**

- EnTT como implementação ECS de baixo nível.
- O jogo expõe sua própria API de entidades/componentes.

Mods não devem depender diretamente de EnTT.

## Física

**Baseline atual:**

- Jolt Physics.

Mods acessam física através da API própria do jogo, não diretamente pela biblioteca.

## Áudio

**Baseline atual:**

- miniaudio.

## Modelos 3D

**Baseline atual:**

- glTF/GLB como formato principal de intercâmbio.
- fastgltf para importação.

## Texturas

**Baseline atual:**

- DirectXTex para operações relevantes de textura no pipeline Windows/DirectX.

## Matemática

**Baseline atual:**

- DirectXMath internamente onde fizer sentido.
- Tipos próprios do runtime devem impedir que APIs públicas fiquem acopladas diretamente à biblioteca.

## Regra arquitetural

Bibliotecas externas são detalhes de implementação.

Nunca:

```text
Mod → EnTT
Mod → Jolt
Mod → DirectX
```

Sempre:

```text
Mod
 ↓
APIs estáveis do jogo
 ↓
implementação/bibliotecas
```

Essa separação é essencial para permitir trocar bibliotecas no futuro sem quebrar o ecossistema de mods.

## Desempenho

O projeto priorizará eficiência desde o início.

A meta exata de hardware/FPS ainda deve ser formalizada, mas a arquitetura deverá suportar presets gráficos e evitar depender de recursos de GPU caros para o estilo visual principal.

## Status

Essas escolhas formam a **baseline atual**, não uma promessa de que nenhuma biblioteca jamais poderá mudar.

Se uma substituição ocorrer, a camada pública do runtime deve proteger mods e sistemas superiores da mudança.
