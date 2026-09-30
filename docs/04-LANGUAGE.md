# Linguagem própria

## Status

A linguagem possui agora uma **especificação v0 implementável**.

A referência normativa da Etapa 8 é:

- [18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md)

Este documento permanece como visão geral e histórico das decisões de design.

## Objetivo

Criar uma linguagem específica para o ecossistema do jogo, simples para humanos e adequada para geração/revisão por agentes de IA.

A implementação do compilador/runtime será integrada ao core em C++.

A linguagem será a principal forma de criar lógica de mods, mas mods também poderão conter C++, JSON, GLB/GLTF, imagens, áudio, mapas e outros assets.

## Decisões v0 já fechadas

- extensão de trabalho: `.sl`;
- variáveis por atribuição, sem `var`/`let`;
- tipagem inferida/dinâmica na superfície;
- funções com `func`;
- parâmetros com default;
- retornos simples e múltiplos;
- `if` / `else`;
- `for ... in ...`;
- listas com `[]`;
- imports explícitos;
- acesso a membros com `.`;
- chamadas com `()`;
- categorias com `@`;
- `@Globals`;
- constantes globais iniciadas por `.`;
- comentários `?` e `/? ... ?/`;
- valores simbólicos como `Legendary` e `Fire`;
- blocos lineares com `:` + indentação;
- definições estruturadas com `{}`;
- `from` cria nova definição derivada sem alterar a base;
- `modify` aplica patch em definição existente;
- merge recursivo de seções estruturadas;
- conflitos de patches incompatíveis devem ser diagnosticados;
- escopo lexical;
- `null`, `true`, `false`;
- AST mínimo e critério de conclusão definidos.

## Exemplos rápidos

### Item

```text
@Items
weapon SunSword:
    .damage = 18
    .speed = 1.2
```

### Entidade

```text
@Entities
enemy Goblin {
    Attributes:
        .health = 30

    Assets:
        .model = "forest:goblin"

    AIs:
        .Melee
}
```

### Função

```text
func damage_multiplier(damage, multiplier):
    return damage * multiplier
```

### Condição

```text
if target.health < 20:
    target.damage(20)
else:
    target.damage(10)
```

### Lista

```text
enemies = [Zombie, Skeleton, Goblin]

for enemy in enemies:
    enemy.damage(10)
```

### Derivação

```text
@Entities
enemy FireZombie from Zombie {
    Attributes:
        .fire_resistance = 1.0
}
```

### Patch

```text
@Core/Entities
modify player {
    Attributes:
        .max_health = 150
}
```

## O que continua fora da v0

Não bloqueia a Etapa 8:

- generics;
- pattern matching;
- async/coroutines;
- macros;
- FFI completo;
- classes tradicionais;
- operator overloading;
- visibilidade avançada;
- exceptions complexas;
- metaprogramação;
- debugger completo.

## Guia para autores

Veja:

- [modding/04-SCRIPTING-AND-CONTENT.md](modding/04-SCRIPTING-AND-CONTENT.md)

## Regra para implementação

O Codex não deve considerar gramática, tipagem, `from` ou `modify` como indefinidos para a v0.

Para esses assuntos, seguir [18-LANGUAGE-V0-SPEC.md](18-LANGUAGE-V0-SPEC.md).
