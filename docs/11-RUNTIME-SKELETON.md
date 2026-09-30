# Etapa 1 — Skeleton C++ do projeto

Este documento formaliza a primeira etapa de implementação do projeto.

## Objetivo

Criar a fundação C++ do runtime antes de implementar renderização, física, áudio, gameplay ou a linguagem de mods.

Ao final desta etapa, o projeto deve:

- configurar e compilar em Windows x64;
- gerar builds Debug e Release;
- inicializar o runtime;
- executar um loop mínimo;
- registrar logs;
- encerrar corretamente;
- já possuir uma estrutura modular preparada para as próximas etapas.

## Estrutura inicial do repositório

Baseline:

```text
Jogo-MCD2.5/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ vcpkg.json
│
├─ src/
│  ├─ main.cpp
│  │
│  ├─ core/
│  │  ├─ Application.h
│  │  ├─ Application.cpp
│  │  ├─ GameLoop.h
│  │  ├─ GameLoop.cpp
│  │  ├─ Log.h
│  │  └─ Log.cpp
│  │
│  ├─ platform/
│  │  └─ windows/
│  │
│  ├─ renderer/
│  ├─ assets/
│  ├─ ecs/
│  ├─ physics/
│  ├─ audio/
│  ├─ mods/
│  ├─ scripting/
│  └─ game/
│
├─ include/
├─ assets/
│  └─ core/
├─ mods/
├─ tests/
├─ tools/
└─ docs/
```

A estrutura poderá crescer, mas estas responsabilidades devem continuar separadas.

## Responsabilidades dos módulos

### `core/`

Infraestrutura genérica do runtime:

- lifecycle;
- `Application`;
- game loop;
- tempo;
- logging;
- configuração;
- erros;
- coordenação de subsistemas.

### `platform/`

Código específico do sistema operacional.

Inicialmente:

```text
platform/windows/
```

Deve concentrar o máximo possível de detalhes específicos do Windows.

### `renderer/`

Abstração gráfica própria e backend de renderização.

Na primeira etapa, permanece vazio ou com interfaces/stubs mínimos.

### `assets/`

Gerenciamento, identificação, importação e cache de assets.

### `ecs/`

Camada própria de entidades/componentes, encapsulando EnTT.

### `physics/`

Camada própria de física, encapsulando Jolt.

### `audio/`

Camada própria de áudio, encapsulando miniaudio.

### `mods/`

Descoberta, manifest, dependências, carregamento e ciclo de vida de mods.

### `scripting/`

Compilador/runtime da linguagem própria.

### `game/`

Sistemas específicos do jogo:

- player;
- hub;
- combate;
- itens;
- áreas;
- gameplay.

A separação entre `game/` e o restante do runtime é intencional.

## `main.cpp`

O ponto de entrada deve ser mínimo.

Exemplo conceitual:

```cpp
#include "core/Application.h"

int main()
{
    game::Application app;

    if (!app.initialize())
        return 1;

    app.run();
    app.shutdown();

    return 0;
}
```

O ponto de entrada não deve conter renderer, física, SDL, Jolt, mod loader ou gameplay diretamente.

## `Application`

`Application` coordena o lifecycle do processo.

Interface conceitual:

```cpp
class Application
{
public:
    bool initialize();
    void run();
    void shutdown();

private:
    bool running_ = true;
};
```

Ordem conceitual de inicialização futura:

```text
Logging
↓
Platform
↓
Window
↓
Renderer
↓
Assets
↓
Audio
↓
Physics
↓
ECS
↓
Mod Loader
↓
Scripting
↓
Game
```

O shutdown deve ocorrer, em geral, na ordem inversa.

## Game loop mínimo

Na etapa 1, o loop ainda não precisa de janela nem renderização.

Conceitualmente:

```cpp
while (running)
{
    timer.tick();
    update(timer.deltaTime());
}
```

A implementação real de eventos, fixed timestep e renderização entra na etapa 2.

## Logging

Logging faz parte da fundação e deve existir no primeiro commit funcional.

API desejada:

```text
LOG_INFO(...)
LOG_WARN(...)
LOG_ERROR(...)
```

Exemplo de saída:

```text
[CORE] Application starting
[CORE] Runtime initialized
[CORE] Running
[CORE] Shutting down
```

Falhas devem carregar contexto suficiente para diagnosticar o subsistema responsável.

Evitar falhas silenciosas como:

```cpp
if (!thing)
    return false;
```

sem log ou diagnóstico.

## Build

Baseline definida:

- Windows x64;
- C++20;
- MSVC;
- CMake;
- Ninja;
- vcpkg.

Builds esperados:

```text
Debug
Release
```

Diretórios de build devem ficar fora de `src/`, por exemplo:

```text
build/debug/
build/release/
```

## Dependências na etapa 1

Não integrar todas as bibliotecas imediatamente.

A estratégia é adicionar cada dependência quando sua etapa chegar.

Ordem aproximada:

```text
Etapa 2 → SDL3
Etapa 3 → Direct3D 11
ECS → EnTT
Physics → Jolt
Assets → fastgltf / DirectXTex
Audio → miniaudio
```

Isso mantém integrações pequenas e testáveis.

## Namespaces

O nome final do projeto ainda não foi definido.

Enquanto isso, o código deve evitar espalhar `MCD2.5` como identidade técnica permanente.

Usar um namespace neutro temporário, como `game`, é aceitável até o nome definitivo existir.

## Critério de conclusão

A etapa 1 está concluída quando for possível:

```text
clone
↓
configure
↓
build Debug/Release
↓
executar
↓
inicializar runtime
↓
rodar loop mínimo
↓
encerrar corretamente
```

Nenhuma renderização 3D, física, áudio, mod ou gameplay é necessária para considerar esta etapa concluída.
