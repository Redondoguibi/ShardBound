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

## Etapa 3 — renderer v0

D3D11 com feature level 11+, flip-model double buffer, debug layer quando
disponível, resize, color/depth, HLSL, câmera e mesh indexado. Handles próprios
encapsulam COM. Material lit/unlit, textura RGBA com nearest/linear e sRGB,
render scale independente da janela e `--no-vsync`. Contadores de draws,
triângulos e tempo CPU. Um cubo técnico valida a renderização inicial.

O teste de GPU verifica pixels desenhados e resize com scale de 50%. Os critérios
v0 do documento 13 são atendidos; sombras, instancing, culling, partículas,
transparência, cache de shaders e perfis de área permanecem trabalho posterior
da direção completa do renderer, sem alegação de implementação.

## Etapa 4 — assets v0

AssetId, VFS com mounts e contenção de paths, diagnóstico estruturado, cache
SHA-256 (source + versão do compilador + settings), publicação atômica do cache,
handles compartilhados e descarregamento quando o último usuário libera o handle.
PNG é convertido por DirectXTex e cacheado em DDS; GLB/GLTF autocontido é importado
por fastgltf e cacheado em CBOR com vértices, índices, transforms, materiais e
pixels embutidos. O cache não altera sources. nlohmann-json é dependência privada
para serialização/validação, não integra a API de gameplay.

Escopo do importador atual: triângulos estáticos, POSITION/NORMAL, TEXCOORD_0 para
texturas, material opaco. Rejeita skins/animações e arquivos externos, com erro
claro. GLTF com data URI e GLB com bufferView são aceitos. Suporte completo a
animações, sub-recursos públicos e dependências de arquivos externos fica pendente.
O teste importa o cubo original gerado por `tools/generate-demo-model.py`, valida
textura embutida, desenha na GPU e verifica reutilização do cache compilado.

## Build e execução

## Etapa 5 — ECS v0

EnTT é privado à implementação de World. IDs de runtime não são reutilizados;
Name/Active/Transform/Model usam API própria add/get/has/remove. Hierarquia
parent/child impede ciclos, compõe transforms e desanexa filhos ao destruir pai
(preservando transform local). RenderSystem coleta modelos ativos. O runtime
agora desenha uma entidade com GLB importado, sem tipos EnTT no gameplay.

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

A especificação normativa da linguagem v0 foi publicada em
[18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md) e incorporada do remoto durante
a implementação. Ela substitui as dúvidas anteriores de gramática/tipagem.
Saves, ABI nativa, cenas e gameplay final ainda precisam de especificações
adicionais antes de serem considerados implementados.
