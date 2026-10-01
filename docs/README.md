# Documentação de planejamento

Esta pasta registra a visão atual do projeto. Ela deve ser atualizada conforme decisões forem tomadas.

## Documentos

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
- [22-GAME-DIRECTION-AND-REFERENCES.md](22-GAME-DIRECTION-AND-REFERENCES.md) — preparação da etapa 13: decisões do autor, pesquisa de MCD II e limites ainda abertos.

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

Evitar transformar hipótese em decisão. Cada documento diferencia:

- **Definido** — já decidido na concepção atual.
- **Direção preferida / baseline** — escolha atual, mas substituível se houver motivo técnico.
- **Provisório** — aceito para continuar o desenho, mas precisa ser revisitado.
- **Em aberto** — precisa ser decidido.

## Regra operacional

A partir de agora, decisões técnicas tomadas durante o planejamento devem ser registradas no repositório no mesmo fluxo da conversa, em vez de ficarem apenas no chat.
