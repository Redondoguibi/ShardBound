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
VSync está conectado ao present. Shutdown destrói a janela e SDL.

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
texturas, materiais OPAQUE/MASK e faces duplas (estendidos no primeiro marco da
etapa 13). Rejeita BLEND, skins/animações e arquivos externos, com erro
claro. GLTF com data URI e GLB com bufferView são aceitos. Suporte completo a
animações, sub-recursos públicos e dependências de arquivos externos fica pendente.
O teste importa o cubo original gerado por `tools/generate-demo-model.py`, valida
textura embutida, desenha na GPU e verifica reutilização do cache compilado.

## Etapa 5 — ECS v0

EnTT é privado à implementação de World. IDs de runtime não são reutilizados;
Name/Active/Transform/Model usam API própria add/get/has/remove. Hierarquia
parent/child impede ciclos, compõe transforms e desanexa filhos ao destruir pai
(preservando transform local). RenderSystem coleta modelos ativos. O runtime
agora desenha uma entidade com GLB importado, sem tipos EnTT no gameplay.

## Etapas 6–7 — manifest e loader

Discovery ordenado, manifest `api: 0.1`, namespaces reservados/duplicados,
versões numéricas (2–3 componentes), dependências `id` ou `id >= 1.0.0`
(também `==`, `<`, `<=`, `>`), topologia e diagnóstico de ciclos. Dependências
ausentes/incompatíveis não descartam assets independentes. IDs duplicados
excluem ambos os pacotes; ciclos impedem inicialização de scripts envolvidos.

`assets.json` suporta registries `models` e `textures`; conteúdo é registrado
explicitamente no VFS/AssetManager, validado e cacheado. Falhas são isoladas por
asset. Replaces implícitos e import settings não implementados são rejeitados.
Uma tela dedicada Preparing Mods exibe pacote/progresso; logs preservam detalhes
dos erros. Scripts estão conectados nas etapas 8–9.

## Etapa 8 — linguagem v0

Lexer com localização/indentação, parser/AST, interpretador, escopos lexicais,
variáveis, @Globals/constantes, funções/defaults/retornos múltiplos, listas,
if/else/for, operadores, símbolos registrados, categorias extensíveis e eventos.
`from` copia e mescla sem alterar a base; `modify` preserva outros campos e
diagnostica writes incompatíveis de mods não relacionados. Patches com dependência
explícita podem seguir a ordem intencional.

Decisões de implementação v0: interpretação de AST; booleanos estritos em
condições; imports relativos à raiz do mod; funções e definições de imports são
publicadas no módulo, variáveis locais do arquivo ficam privadas; defaults são
avaliados no escopo de definição a cada chamada; múltiplos retornos viram List.
Recursão limitada a 64 chamadas, imports a 64 níveis e budget de 100.000 passos por execução/evento
produzem diagnóstico em vez de travar indefinidamente. A etapa 12 fornece ABI C
versionada; estruturas internas C++ não são públicas. Os testes cobrem tipos, constantes, imports
circulares, símbolos desconhecidos, escopo e conflitos de patch.
Executáveis reservam 8 MiB de stack para acomodar esses limites também nos
frames maiores do MSVC Debug; a reserva não é um heap/allocator público.

## Etapas 9–10 — bridge e primeiro mod funcional

`spawn(definition, vec3(x,y,z))`, `get_entity(name)`, `destroy(entity)` e `log`
operam através das APIs próprias. EntityRef expõe `position`, `name`, `active`;
posição permite leitura/escrita de x/y/z. Referências destruídas produzem erros
controlados. `on_start`, `on_update(delta)` e `on_fixed_update(delta)` são hooks v0.

O pacote `mods/example` declara GLB próprio e entidade, importa `.sl`, cria o
modelo no startup e o move por script. O runtime não contém spawn hardcoded
deste mod. O teste bridge verifica o ciclo até pixels reais, movimento, destruição
e uso de referência inválida. Remover a pasta do build e reiniciar remove o mod.

## Etapa 11 — gameplay v0

Adaptadores próprios de Jolt/miniaudio, sistemas separados de física/ECS,
vida/dano/cura, inventário por ID, seleção de interação e controle provisório do
player com câmera. API de scripts e pacote de demonstração em `examples/gameplay`.
Modelo estático original com seis partes nas proporções de referência documentadas.
Veja contratos, execução e limites em [19-GAMEPLAY-V0-API.md](19-GAMEPLAY-V0-API.md).

O controlador definitivo, rig/animação, áudio espacial, HUD, equipamentos e regras
finais de combate continuam pendentes. Os testes cobrem colisão, movimento,
inventário, dano, áudio offline e integração real com scripts/render.

## Etapa 12 — extensibilidade v1 implementada

Hooks/eventos com inscrições removíveis, hook real de dano, patch e replace explícito
com conflitos; componentes de dados versionados e migrações; saves do mundo com
GUIDs, física/player, estado por mod, backup e preservação de órfãos; SDK C ABI v1
com DLL real e lifecycle; CLI com inspeção de saves e manifests nativos.
Contratos, APIs, exemplos e limites: [21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md).

## Etapa 13 — primeiro marco: assets e materiais

O sandbox técnico não é o hub ou o jogo final. Progressão, áreas oficiais,
combate final e identidade artística dependem do desenho de conteúdo e dos
contratos de mundo/áreas ainda abertos. Save v1 persiste o runtime atual; não
define o formato de mapas ou as regras finais de progressão.

Decisões do autor e pesquisa pública de MCD II estão em
[22-GAME-DIRECTION-AND-REFERENCES.md](22-GAME-DIRECTION-AND-REFERENCES.md).
Hub, floresta em mundo aberto, waystones, itens e morte possuem direção
documentada; contratos técnicos e parâmetros ainda abertos estão explicitados.

O primeiro marco integra os GLBs do autor: `core:sword`, `core:tree` e
`core:waystone`, com registro próprio no módulo de assets e carga sob demanda.
Importador/cache/shader preservam `alphaMode`, `alphaCutoff`, alfa da cor-base
e `doubleSided`. Alpha-test descarta cor/profundidade; faces duplas invertem
normais no verso, e transforms espelhados corrigem o culling. O cache de
modelos foi versionado para evitar reutilizar materiais compilados pelo v1.

A [galeria executável](../examples/asset-gallery/README.md) cria os três objetos
por script, sem spawn especial na Application. Execute
`./build/debug/game.exe --mods examples/asset-gallery`. Escalas de apresentação
e câmera fixa servem à conferência dos assets. Controlador definitivo, salto,
hub/floresta, viagem rápida, combate e itens continuam como próximos marcos.

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

Validação em 2026-10-01: builds MSVC Debug/Release concluídos; 23/23 testes CTest
aprovados em cada configuração. Incluem pixels na GPU (recorte alfa, profundidade
e faces duplas), galeria dos três assets por script, scripts/mods, Jolt,
PCM offline, gameplay, componentes/migrações, saves/órfãos, DLLs reais e CLI. Input de interação foi sintetizado; gamepad físico
e qualidade audível no dispositivo não foram avaliados manualmente.

## Limites do planejamento

A especificação normativa da linguagem v0 foi publicada em
[18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md) e incorporada do remoto durante
a implementação. Ela substitui as dúvidas anteriores de gramática/tipagem.
Saves e ABI nativa têm agora contratos v1 em 21-EXTENSIBILITY-V1. Cenas, formato
de mapas e gameplay final continuam pendentes; a etapa 12 não os substitui.
