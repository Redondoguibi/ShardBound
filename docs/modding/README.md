# Guia oficial de modding

Este diretório é a documentação voltada a **autores de mods**.

A documentação em `docs/` explica como o jogo é construído internamente. Esta pasta explica como **usar a plataforma de modding** sem precisar conhecer o renderer, EnTT, Jolt ou outros detalhes do runtime.

> Estado atual: partes dos exemplos conceituais abaixo são antigas. Para APIs executáveis, consulte os documentos 18, 21, 27, 28 e o código. O loader, scripts, mods nativos e Studio já estão operacionais; recursos futuros continuam identificados como planejamento.

## Comece aqui

- [13-ASYNC-AWAIT.md](13-ASYNC-AWAIT.md) — funções assíncronas cooperativas, esperas, cancelamento, desempenho e ciclo de vida.

1. [01-GETTING-STARTED.md](01-GETTING-STARTED.md) — criar o primeiro mod e entender o workflow.
2. [02-MOD-STRUCTURE-AND-MANIFEST.md](02-MOD-STRUCTURE-AND-MANIFEST.md) — estrutura profissional, `mod.json`, `assets.json`, IDs e dependências.
3. [03-ASSETS-AND-MODELS.md](03-ASSETS-AND-MODELS.md) — GLB/GLTF, texturas embutidas, modelos cúbicos, player, materiais, animações e sub-recursos.
4. [04-SCRIPTING-AND-CONTENT.md](04-SCRIPTING-AND-CONTENT.md) — linguagem própria, entidades, itens, eventos, imports e alterações do core.
5. [05-PROFESSIONAL-MOD-PRACTICES.md](05-PROFESSIONAL-MOD-PRACTICES.md) — compatibilidade, organização, diagnósticos, performance, versionamento e distribuição.
6. [../21-EXTENSIBILITY-V1.md](../21-EXTENSIBILITY-V1.md) — APIs implementadas de componentes customizados, hooks, saves e SDK C++.
7. [../28-MOD-PLATFORM-FOUNDATIONS.md](../28-MOD-PLATFORM-FOUNDATIONS.md) — sistemas, mundos, IA, equipamentos e interfaces extensíveis.
8. [06-WORLD-GENERATION.md](06-WORLD-GENERATION.md) — geração determinística por mods e snapshots.
9. [07-MOD-PLATFORM.md](07-MOD-PLATFORM.md) — contratos das APIs avançadas e seus limites.
10. [08-STREAMING-AND-DEVELOPER-TOOLS.md](08-STREAMING-AND-DEVELOPER-TOOLS.md) — streaming, saves por chunk, F3, inspeção, transferência, navegação dinâmica e widgets.

11. [09-SHADERS.md](09-SHADERS.md) — HLSL de mods, materiais, blocos, parâmetros e pós-processamento.

12. [10-SAVES-COMPATIBILITY-AND-RECOVERY.md](10-SAVES-COMPATIBILITY-AND-RECOVERY.md) — gravação, migrações, preparação transacional e recuperação de mundos.

13. [11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md](11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md) — console, câmeras, comandos de mods e atributos persistentes.

14. [12-ENGINE-EXTENSIONS.md](12-ENGINE-EXTENSIONS.md) — fases substituíveis, física, câmera, iluminação, ordem da simulação e acesso nativo versionado.

## Filosofia do ecossistema

Geração procedural implementada: [06-WORLD-GENERATION.md](06-WORLD-GENERATION.md)
— biomas, camadas de solo, ruído, distribuição de estruturas, herança e addons.

Um mod pode ser pequeno:

```text
nova espada
```

ou enorme:

```text
nova área
+ inimigos
+ bosses
+ loot
+ VFX
+ sistemas
+ alterações profundas no jogo
```

O objetivo do projeto é permitir os dois sem exigir que o autor modifique o executável base.

## Regra de ouro

Use as APIs e IDs do jogo.

Evite depender de:

- caminhos físicos internos do jogo;
- objetos D3D11;
- EnTT;
- Jolt;
- SDL3;
- detalhes privados do cache.

Esses elementos são implementação interna e podem mudar sem aviso.
