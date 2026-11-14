# Etapa 2 — Janela, input e game loop com SDL3

Este documento formaliza a segunda etapa de implementação do runtime.

## Objetivo

Integrar SDL3 como camada de janela, eventos, teclado, mouse e gamepad, mantendo SDL fora das APIs públicas de gameplay/mods.

Ao final desta etapa, o projeto deve possuir:

- janela real;
- eventos de janela;
- teclado;
- mouse;
- estrutura de gamepad;
- medição de tempo;
- update variável;
- fixed update;
- frame limiter;
- VSync configurável futuramente;
- tratamento de foco/minimização;
- encerramento limpo.

Ainda não é necessário renderizar uma cena 3D.

## Arquitetura

Baseline:

```text
Application
├─ WindowSystem
│  └─ SDL3
├─ InputSystem
│  └─ SDL3
├─ GameLoop
└─ Renderer
   └─ D3D11 (entra na etapa seguinte)
```

## Regra de isolamento

SDL3 é detalhe de implementação.

Código de gameplay não deve chamar SDL diretamente.

Evitar:

```cpp
SDL_GetKeyboardState(...)
```

dentro de código como `Player.cpp`.

Preferir API própria:

```cpp
Input::isKeyDown(Key::W);
```

ou, para gameplay, ações semânticas:

```cpp
Input::isPressed(Action::Attack);
Input::isHeld(Action::MoveForward);
Input::axis(Axis::MoveX);
```

Arquitetura:

```text
Gameplay / Mods
      ↓
Input API do projeto
      ↓
SDL3
```

## WindowSystem

Responsabilidades:

- criar/destruir janela;
- acompanhar dimensões;
- receber eventos;
- estado de foco;
- minimize/restore;
- modo de janela;
- notificar renderer sobre resize futuramente.

Baseline inicial de resolução:

```text
1280x720
```

O sistema deve ser preparado para suportar futuramente:

- Windowed;
- Borderless;
- Fullscreen;
- seleção de resolução;
- refresh rate;
- monitor;
- UI scaling.

Não é necessário implementar tudo imediatamente.

## InputSystem

Deve unificar:

- teclado;
- mouse;
- gamepad.

### Teclas físicas

A camada baixa pode expor teclas:

```text
Key::W
Key::A
Key::S
Key::D
```

### Ações de gameplay

Gameplay deve preferir ações:

```text
MoveForward
MoveBackward
MoveLeft
MoveRight
Attack
Dodge
Interact
```

Exemplo:

```text
W       → MoveForward
Mouse1  → Attack
Space   → Dodge
```

Essa separação permitirá:

- remapeamento;
- gamepad;
- acessibilidade;
- mods registrarem ações;
- UI alterar controles sem reescrever gameplay.

## Gamepad

Suporte estrutural a gamepad deve existir desde cedo, mesmo que a experiência completa de controle seja implementada posteriormente.

A mesma camada de input deve abstrair keyboard/mouse/gamepad.

## Eventos

Eventos crus do SDL não devem escapar livremente para o restante do jogo.

Eles devem ser convertidos para eventos próprios.

Exemplos:

```text
WindowClosed
WindowResized
FocusGained
FocusLost
KeyPressed
KeyReleased
MouseMoved
MouseButtonPressed
GamepadConnected
GamepadDisconnected
```

No futuro, alguns desses eventos poderão alimentar o sistema de eventos exposto a mods.

## Game loop

O jogo deve desacoplar simulação e renderização.

Baseline conceitual:

```cpp
while (running)
{
    timer.update();

    window.pollEvents();
    input.update();

    accumulator += timer.deltaTime();

    while (accumulator >= fixedDelta)
    {
        fixedUpdate(fixedDelta);
        accumulator -= fixedDelta;
    }

    update(timer.deltaTime());

    renderer.beginFrame();
    render();
    renderer.endFrame();
}
```

Na etapa 2, chamadas de renderer podem ser stubs até a etapa 3.

## Fixed timestep

Baseline:

```text
fixedDelta = 1 / 60
fixed update = 60 Hz
```

Uso esperado:

- física;
- simulação que precise de passos consistentes;
- sistemas determinísticos quando aplicável.

## Variable update

`update(deltaTime)` roda com o tempo real entre frames e é adequado para:

- lógica visual;
- câmera;
- animação não física;
- sistemas que não exigem timestep fixo.

## Renderização

Renderização deve poder rodar independentemente da frequência do fixed update:

```text
60 Hz physics
+
60 / 90 / 120 / 144+ FPS render
```

A velocidade do jogo não deve depender do FPS.

## Frame limiting

Estrutura inicial deve permitir:

```text
Unlimited
30
60
120
144
```

Os valores podem crescer depois.

## VSync

VSync deve ser configurável independentemente do frame limiter.

Modelo:

```text
VSync: On/Off
Frame Limit: 60
```

A implementação efetiva de present/VSync entra junto do backend D3D11, mas a configuração e arquitetura devem ser previstas nesta etapa.

## Resize

Fluxo futuro:

```text
SDL window resize
      ↓
WindowSystem
      ↓
WindowResized
      ↓
Renderer::resize(width, height)
      ↓
D3D11 recria backbuffer/render targets
```

A etapa 2 deve emitir corretamente o evento; a recriação gráfica entra na etapa 3.

## Foco e minimização

Tratar explicitamente:

- Alt+Tab;
- janela minimizada;
- perda de foco;
- ganho de foco.

Quando minimizado:

- evitar renderização desnecessária;
- reduzir consumo de CPU/GPU;
- permitir política futura de pausa.

## Meta de desempenho

A arquitetura deve ser leve e evitar busy loops desnecessários.

O usuário quer que o jogo rode de forma suave em seu PC, portanto desempenho e custo de CPU/GPU são requisitos de primeira classe desde o runtime inicial.

Metas numéricas exatas de hardware/FPS serão definidas separadamente.

## Critério de conclusão

A etapa 2 está concluída quando o executável consegue:

```text
abrir janela
↓
redimensionar
↓
receber teclado/mouse
↓
detectar estrutura de gamepad
↓
rodar game loop estável
↓
medir deltaTime
↓
executar fixedUpdate a 60 Hz
↓
limitar FPS
↓
tratar foco/minimização
↓
fechar corretamente
```

A janela pode permanecer visualmente vazia/preta.

A renderização 3D começa na etapa 3.
