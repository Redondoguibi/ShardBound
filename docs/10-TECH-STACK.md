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

## Janela e input

**Baseline atual:**

- SDL3.

SDL3 será usado para:

- criação de janela;
- eventos de janela;
- teclado;
- mouse;
- gamepad.

Gameplay e mods não devem depender diretamente de SDL3.

A camada pública deve usar abstrações próprias de janela, eventos, teclas, e principalmente ações de gameplay.

## Game loop

**Definido como baseline:**

- update variável com `deltaTime`;
- fixed update separado;
- fixed update inicial de 60 Hz;
- renderização desacoplada da simulação;
- frame limiter configurável;
- estrutura preparada para VSync;
- tratamento explícito de foco e minimização.

A velocidade do jogo não pode depender do FPS.

## Renderização

**Baseline atual:**

- Direct3D 11 como backend inicial.
- O renderer deve possuir uma abstração própria para evitar acoplamento de gameplay/mods diretamente ao D3D11.

Arquitetura desejada:

```text
Game / Mods
    ↓
Renderer API do projeto
    ↓
D3D11 backend
```

Um backend D3D12 poderá ser criado futuramente sem exigir reescrever sistemas superiores.

### Pipeline gráfico

Baseline atual:

- forward renderer;
- HLSL;
- feature level 11_0 ou superior;
- DXGI flip-model swap chain;
- double buffering ou superior;
- resolução interna separada da resolução da janela;
- frustum culling;
- distance culling;
- instancing;
- render queues com sorting;
- material system pequeno;
- iluminação principal simples;
- shadow map configurável;
- pós-processamento leve.

Deferred rendering, TAA pesado, SSR, volumetria cara e ray tracing não fazem parte da baseline inicial.

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
Mod → SDL3
Mod → EnTT
Mod → Jolt
Mod → DirectX
```

Sempre:

```text
Mod / Gameplay
      ↓
APIs estáveis do jogo
      ↓
implementação/bibliotecas
```

Essa separação permite trocar bibliotecas no futuro sem quebrar o ecossistema de mods.

## Desempenho

O projeto priorizará eficiência desde o início.

Requisitos atuais:

- evitar busy loops;
- reduzir trabalho quando a janela estiver minimizada;
- fixed timestep estável;
- render desacoplado;
- suporte a frame limiter;
- presets gráficos futuramente;
- evitar depender de efeitos de GPU caros para a identidade visual principal.

A meta numérica de hardware/FPS ainda será formalizada.

## Status

Essas escolhas formam a **baseline atual**, não uma promessa de que nenhuma biblioteca jamais poderá mudar.

Se uma substituição ocorrer, a camada pública do runtime deve proteger mods e sistemas superiores da mudança.
