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

## Regra de documentação

Evitar transformar hipótese em decisão. Cada documento diferencia:

- **Definido** — já decidido na concepção atual.
- **Direção preferida / baseline** — escolha atual, mas substituível se houver motivo técnico.
- **Provisório** — aceito para continuar o desenho, mas precisa ser revisitado.
- **Em aberto** — precisa ser decidido.

## Regra operacional

A partir de agora, decisões técnicas tomadas durante o planejamento devem ser registradas no repositório no mesmo fluxo da conversa, em vez de ficarem apenas no chat.
