# 1. Começando um mod

## Objetivo

Este guia mostra o formato mínimo de um mod profissional.

## Estrutura mínima recomendada

```text
my_mod/
├─ mod.json
├─ assets.json
└─ scripts/
   └─ main.sl
```

Um mod com conteúdo 3D pode crescer para:

```text
my_mod/
├─ mod.json
├─ assets.json
├─ scripts/
│  ├─ main.sl
│  ├─ entities.sl
│  ├─ items.sl
│  └─ combat.sl
├─ models/
├─ textures/
├─ audio/
├─ maps/
├─ data/
└─ vfx/
```

A organização interna é do autor. O jogo só exige os arquivos e referências definidos pelo contrato do loader.

## 1. Escolha um ID de mod

Exemplo:

```text
desert_expansion
```

Esse ID será seu namespace padrão:

```text
desert_expansion:scarab
desert_expansion:sun_sword
desert_expansion:pyramid
```

Use IDs:

- em minúsculas;
- estáveis;
- curtos;
- sem depender do nome comercial do mod se você pretende renomeá-lo depois.

## 2. Crie o manifest

Exemplo atual:

```json
{
  "id": "desert_expansion",
  "name": "Desert Expansion",
  "version": "1.0.0",
  "api": "0.1",
  "dependencies": [],
  "optional_dependencies": [],
  "entry": "scripts/main.sl"
}
```

## 3. Declare assets públicos

Arquivos não viram assets públicos automaticamente.

Exemplo:

```json
{
  "models": {
    "desert_expansion:scarab": {
      "source": "models/scarab.glb"
    }
  }
}
```

Isso permite reorganizar `models/` depois sem mudar o ID usado por scripts e por outros mods.

## 4. Crie a lógica

Exemplo seguindo a sintaxe atualmente definida:

```text
@Entities
enemy Scarab {
Attributes:
.health = 30

Assets:
.model = "desert_expansion:scarab"

AIs:
.Melee
}
```

## 5. Teste

Workflow planejado:

```text
colocar mod em mods/
↓
iniciar jogo
↓
Preparing Mods
↓
validar manifest
↓
resolver dependências
↓
compilar/cachear assets alterados
↓
compilar/carregar scripts
↓
registrar conteúdo
↓
iniciar jogo
```

## Ferramentas planejadas

O toolchain deverá permitir futuramente:

```text
game-tool mods validate my_mod
game-tool assets validate my_mod
game-tool assets build my_mod
game-tool assets inspect desert_expansion:scarab
```

Essas ferramentas são especialmente importantes para CI e agentes de IA.

## O que um mod não deve fazer

Evite:

```text
../../core/models/player.glb
```

ou dependências diretas de arquivos internos.

Use IDs:

```text
core:player
```

ou APIs públicas documentadas.

## Próximo passo

Leia [02-MOD-STRUCTURE-AND-MANIFEST.md](02-MOD-STRUCTURE-AND-MANIFEST.md).
