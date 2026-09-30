# Estado da implementação

Este arquivo distingue código executável de intenções do roadmap. Os documentos
específicos 10–17 refinam os documentos conceituais anteriores; decisões abertas
continuam abertas. Namespace C++ temporário: `game`.

## Etapa 1

Fundação C++20/MSVC x64, targets CMake, presets Ninja Debug/Release, manifest
vcpkg, logging, Application com shutdown idempotente e loop com limite de frames.
`--frames N` permite encerrar smoke tests sem intervenção; sem essa opção o loop
continua até receber uma solicitação de encerramento.

## Etapa 2

SDL3 encapsulado em WindowSystem, janela 1280×720, eventos próprios, teclado,
mouse e conexão/desconexão de gamepads. InputSystem aceita ações registradas por
nome e bindings substituíveis. Foco perdido limpa botões; minimização suspende
simulação/render e aguarda 50 ms por iteração.

GameLoop separa poll/fixed/update/render: fixed update de 60 Hz, delta limitado
a 250 ms para evitar recuperação ilimitada após travamentos, interpolação e
frame limiter (`--fps 0|30|60|120|144|...`). `--hidden` atende testes de startup.
VSync será conectado ao present na etapa 3. Shutdown destrói a janela e SDL.

## Build e execução

Requisitos: Visual Studio 2022 com C++ x64 e Windows SDK, CMake >= 3.25, Ninja,
vcpkg inicializado e variável `VCPKG_ROOT` apontando para ele.

```powershell
./tools/build.ps1 debug
./tools/build.ps1 release
./build/debug/game.exe --frames 120
```

Em um Developer PowerShell x64 também é possível usar diretamente:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Builds, dependências locais e caches são ignorados pelo Git. Bibliotecas serão
adicionadas somente na etapa em que forem utilizadas.

## Limites do planejamento

A etapa 8 exige formalizar blocos/indentação, tipos, execução, `from` e `modify`.
Os exemplos publicados não definem essas regras e não serão substituídos por uma
linguagem incompatível. Saves, ABI nativa, cenas e gameplay final também precisam
de especificações adicionais antes de serem considerados implementados.
