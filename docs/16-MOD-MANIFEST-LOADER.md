# Etapa 6 — Manifest e Mod Loader v0

## Objetivo

Definir o mínimo necessário para o runtime descobrir e carregar mods de forma previsível.

## Estrutura inicial

```text
mods/
└─ desert_expansion/
   ├─ mod.json
   ├─ assets.json
   ├─ scripts/
   ├─ models/
   ├─ textures/
   ├─ audio/
   └─ maps/
```

Pastas adicionais são permitidas. A estrutura não deve ser excessivamente rígida.

## mod.json

Baseline conceitual:

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

## assets.json

Assets públicos são declarados explicitamente.

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

O formato exato poderá evoluir, mas separar manifest de lista de assets é a baseline atual por organização.

## Loader

Fluxo:

```text
scan mods/
↓
read mod.json
↓
validate IDs/versions
↓
build dependency graph
↓
determine load order
↓
mount mod in VFS
↓
read assets.json
↓
compile/cache assets
↓
load scripts
↓
register content
```

## Dependências ausentes

Regra já definida:

- mostrar erro/aviso claro;
- ainda tentar carregar o mod quando possível;
- partes que dependem do conteúdo ausente podem falhar isoladamente;
- evitar crash global quando houver recuperação possível.

## IDs

`mod.json.id` define o namespace padrão do mod.

Exemplo:

```text
id = desert_expansion
→ desert_expansion:scarab
```

## Core

O conteúdo oficial deve participar dos mesmos registries e APIs sempre que possível.

Namespace reservado:

```text
core:
```

Isso reduz diferenças artificiais entre conteúdo oficial e conteúdo de mods.

## Critério de conclusão

O loader v0 está pronto quando um diretório em `mods/` é descoberto, validado, montado, tem seus assets registrados e aparece nos logs sem precisar recompilar o executável base.


## Guia público

Autores de mods devem começar por [modding/README.md](modding/README.md).

A documentação pública traduz este contrato técnico em workflow prático de criação, teste e distribuição.
