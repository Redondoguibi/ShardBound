# Guia oficial de modding

Este diretório é a documentação voltada a **autores de mods**.

A documentação em `docs/` explica como o jogo é construído internamente. Esta pasta explica como **usar a plataforma de modding** sem precisar conhecer o renderer, EnTT, Jolt ou outros detalhes do runtime.

> Estado atual: esta documentação descreve o contrato planejado/v0 do sistema de mods. O runtime ainda está em implementação. Regras marcadas como **Definido** devem ser tratadas como decisões do projeto; itens marcados como **Provisório** ainda podem mudar.

## Comece aqui

1. [01-GETTING-STARTED.md](01-GETTING-STARTED.md) — criar o primeiro mod e entender o workflow.
2. [02-MOD-STRUCTURE-AND-MANIFEST.md](02-MOD-STRUCTURE-AND-MANIFEST.md) — estrutura profissional, `mod.json`, `assets.json`, IDs e dependências.
3. [03-ASSETS-AND-MODELS.md](03-ASSETS-AND-MODELS.md) — GLB/GLTF, texturas embutidas, modelos cúbicos, player, materiais, animações e sub-recursos.
4. [04-SCRIPTING-AND-CONTENT.md](04-SCRIPTING-AND-CONTENT.md) — linguagem própria, entidades, itens, eventos, imports e alterações do core.
5. [05-PROFESSIONAL-MOD-PRACTICES.md](05-PROFESSIONAL-MOD-PRACTICES.md) — compatibilidade, organização, diagnósticos, performance, versionamento e distribuição.

## Filosofia do ecossistema

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
