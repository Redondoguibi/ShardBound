# Estado da implementação

Este arquivo distingue código executável de intenções do roadmap. Os documentos
específicos 10–17 refinam os documentos conceituais anteriores; decisões abertas
continuam abertas. Namespace C++ temporário: `game`.

## Etapa 1

Fundação C++20/MSVC x64, targets CMake, presets Ninja Debug/Release, manifest
vcpkg, logging, Application com shutdown idempotente e loop com limite de frames.
`--frames N` permite encerrar smoke tests sem intervenção; sem essa opção o loop
continua até receber uma solicitação de encerramento.

## Build

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
