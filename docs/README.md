# Documentação de planejamento

Esta pasta registra a visão atual do projeto. Ela deve ser atualizada conforme decisões forem tomadas.

## Editor de mapas em C++

Planejamento da primeira versão do editor e da transição do jogo para um mundo finito.

- [Mundo e mods](editor/01-MUNDO-E-MODS.md) — biomas, catálogo de blocos, estruturas e interiores.
- [Editor e Blockbench](editor/02-EDITOR-E-BLOCKBENCH.md) — autoria de mapas, importação de saves e integração com o Blockbench instalado.
- [IA, terminal e linha do tempo](editor/03-IA-E-LINHA-DO-TEMPO.md) — fornecedores, ferramentas, modos de execução e restauração.
- [Itens, armaduras e animações](editor/04-ITENS-ARMADURAS-E-ANIMACOES.md) — autoria visual de atributos, volumes de ataque, sequências e animações equipadas.

## Documentos

- [modding/13-ASYNC-AWAIT.md](modding/13-ASYNC-AWAIT.md) — sintaxe e contratos de tarefas cooperativas da linguagem `.sl`.

- [38-ENGINE-EXTENSIONS.md](38-ENGINE-EXTENSIONS.md) — implementação e testes da API de extensão do núcleo por scripts e DLLs.
- [modding/12-ENGINE-EXTENSIONS.md](modding/12-ENGINE-EXTENSIONS.md) — contrato de fases, configurações e substituições da engine.
- [37-READINESS-REVIEW.md](37-READINESS-REVIEW.md) — correções de controles, isolamento entre mundos, perfis e apoio de colisores girados.
- [PLAYING.md](PLAYING.md) — controles, perfis de mods, configurações, medição de desempenho e pacote distribuível.
- [36-PLAYER-READINESS.md](36-PLAYER-READINESS.md) — implementação e validação das cinco frentes de preparação do jogo.

- [35-WALL-JUMP-FIX.md](35-WALL-JUMP-FIX.md) — correção e regressões do travamento no ar ao andar contra blocos.

- [34-INTEGRATION-BUGFIXES.md](34-INTEGRATION-BUGFIXES.md) — correções de salvamentos, equipamentos, atributos e captura de Escape, com regressões de integração.

- [33-DEVELOPER-MODE.md](33-DEVELOPER-MODE.md) — implementação, correções, testes e limites do modo de desenvolvedor.

- [modding/11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md](modding/11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md) — modo de desenvolvedor, câmeras, comandos e atributos.

- [32-RUNTIME-ROBUSTNESS.md](32-RUNTIME-ROBUSTNESS.md) — implementação e validação de armazenamento, compatibilidade e recuperação.

- [modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md](modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md) — contratos de compatibilidade, armazenamento e recuperação.

- [31-MOD-SHADERS.md](31-MOD-SHADERS.md) — implementação e validação de shaders extensíveis.

- [modding/09-SHADERS.md](modding/09-SHADERS.md) — contrato executável de shaders de mods.

- [27-PROCEDURAL-WORLDS.md](27-PROCEDURAL-WORLDS.md) — estado atual: menu, configurações, mundos por seed, chunks e conteúdo oficial por mods.
- [modding/06-WORLD-GENERATION.md](modding/06-WORLD-GENERATION.md) — guia implementado de biomas, solo, relevo, estruturas, herança e extensões de geração.
- [28-MOD-PLATFORM-FOUNDATIONS.md](28-MOD-PLATFORM-FOUNDATIONS.md) — sistemas, geração volumétrica, edição transacional, IA, equipamentos e widgets por mods.
- [modding/07-MOD-PLATFORM.md](modding/07-MOD-PLATFORM.md) — contratos e exemplos das novas APIs.
- [29-MOD-PLATFORM-VALIDATION.md](29-MOD-PLATFORM-VALIDATION.md) — validação, correções e limitações da plataforma.
- [modding/08-STREAMING-AND-DEVELOPER-TOOLS.md](modding/08-STREAMING-AND-DEVELOPER-TOOLS.md) — streaming, persistência paginada, diagnóstico e fluxos avançados.
- [30-MOD-PLATFORM-EXPANSION.md](30-MOD-PLATFORM-EXPANSION.md) — implementação e validação da evolução da plataforma.

- [01-VISION.md](01-VISION.md) — visão, objetivos e princípios.
- [02-CORE-ARCHITECTURE.md](02-CORE-ARCHITECTURE.md) — arquitetura conceitual do runtime.
- [03-MOD-SYSTEM.md](03-MOD-SYSTEM.md) — formato e capacidades dos mods.
- [04-LANGUAGE.md](04-LANGUAGE.md) — objetivos e sintaxe atualmente definida da linguagem própria.
- [05-CONTENT-AND-ASSETS.md](05-CONTENT-AND-ASSETS.md) — mapas, assets e pipeline de conteúdo.
- [06-LOADING-AND-DEPENDENCIES.md](06-LOADING-AND-DEPENDENCIES.md) — inicialização, dependências e conflitos.
- [07-AI-FIRST-DESIGN.md](07-AI-FIRST-DESIGN.md) — regras para tornar o ecossistema amigável a IA.
- [08-ROADMAP.md](08-ROADMAP.md) — ordem de implementação sugerida.
- [09-OPEN-DECISIONS.md](09-OPEN-DECISIONS.md) — decisões ainda não fechadas.
- [10-TECH-STACK.md](10-TECH-STACK.md) — baseline técnica atual do runtime.
- [11-RUNTIME-SKELETON.md](11-RUNTIME-SKELETON.md) — etapa 1: skeleton C++, estrutura de pastas, lifecycle, logging e build.
- [12-WINDOW-INPUT-GAMELOOP.md](12-WINDOW-INPUT-GAMELOOP.md) — etapa 2: SDL3, janela, input, eventos e game loop.
- [13-RENDERER-D3D11.md](13-RENDERER-D3D11.md) — etapa 3: renderer D3D11, pipeline, materiais, iluminação, culling e performance.
- [14-ASSET-PIPELINE.md](14-ASSET-PIPELINE.md) — etapa 4: IDs, VFS, importação, cache compilado e lifetime de assets.
- [15-ECS-ENTITIES.md](15-ECS-ENTITIES.md) — etapa 5: ECS, componentes, runtime entity IDs e systems.
- [16-MOD-MANIFEST-LOADER.md](16-MOD-MANIFEST-LOADER.md) — etapa 6: `mod.json`, `assets.json`, dependências e fluxo do loader.
- [17-ART-DIRECTION-MODELING.md](17-ART-DIRECTION-MODELING.md) — direção de modelagem, cuboids, player, rig e regras visuais.
- [18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md) — especificação normativa e implementável da linguagem v0.
- [19-GAMEPLAY-V0-API.md](19-GAMEPLAY-V0-API.md) — APIs executáveis de gameplay e sandbox.
- [20-MODCHECK-CLI.md](20-MODCHECK-CLI.md) — validação de manifests, sintaxe e imports.
- [21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md) — etapa 12: componentes, hooks, overrides, saves e SDK nativo.
- [25-COMBATANTS-LOOT-AND-RUNTIME-CACHES.md](25-COMBATANTS-LOOT-AND-RUNTIME-CACHES.md) — modelos originais, inimigos por definição, loot persistente e caches.
- [24-EQUIPMENT-AND-WORLD-CONTENT.md](24-EQUIPMENT-AND-WORLD-CONTENT.md) — instâncias, inventário, combate visual e mundo extensível.
- [22-GAME-DIRECTION-AND-REFERENCES.md](22-GAME-DIRECTION-AND-REFERENCES.md) — preparação da etapa 13: decisões do autor, pesquisa de MCD II e limites ainda abertos.

## ShardBound Studio (plugin Blockbench)

- [shardbound-studio/README.md](shardbound-studio/README.md) — especificação do editor de modelos, skins, equipamentos, monstros, animações por grupos/pastas, partículas em keyframes, exportação e decisões pendentes.

## Documentação para modders

O progresso verificável do código e as instruções de build estão em
[18-IMPLEMENTATION-STATUS.md](18-IMPLEMENTATION-STATUS.md).

Autores externos devem começar em:

- [modding/README.md](modding/README.md) — índice do guia profissional de modding.
- [modding/01-GETTING-STARTED.md](modding/01-GETTING-STARTED.md) — primeiro mod.
- [modding/02-MOD-STRUCTURE-AND-MANIFEST.md](modding/02-MOD-STRUCTURE-AND-MANIFEST.md) — estrutura, IDs e manifest.
- [modding/03-ASSETS-AND-MODELS.md](modding/03-ASSETS-AND-MODELS.md) — GLB/GLTF, texturas embutidas e modelagem.
- [modding/04-SCRIPTING-AND-CONTENT.md](modding/04-SCRIPTING-AND-CONTENT.md) — scripting.
- [modding/05-PROFESSIONAL-MOD-PRACTICES.md](modding/05-PROFESSIONAL-MOD-PRACTICES.md) — práticas profissionais.

## Regra de documentação

O primeiro ciclo jogável está descrito em
[23-PLAYABLE-SLICE.md](23-PLAYABLE-SLICE.md), com controles, persistência e APIs.

Evitar transformar hipótese em decisão. Cada documento diferencia:

- **Definido** — já decidido na concepção atual.
- **Direção preferida / baseline** — escolha atual, mas substituível se houver motivo técnico.
- **Provisório** — aceito para continuar o desenho, mas precisa ser revisitado.
- **Em aberto** — precisa ser decidido.

## Regra operacional

A partir de agora, decisões técnicas tomadas durante o planejamento devem ser registradas no repositório no mesmo fluxo da conversa, em vez de ficarem apenas no chat.

A revisão da experiência jogável (visual, interface, combate e câmera) está em
[26-PLAYABLE-QUALITY-PASS.md](26-PLAYABLE-QUALITY-PASS.md).
