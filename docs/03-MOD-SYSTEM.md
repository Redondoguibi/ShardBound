# Sistema de mods

## Filosofia

Mods não são apenas pacotes de conteúdo. Eles são módulos capazes de adicionar, remover, substituir, estender e reagir ao jogo.

## Estrutura conceitual

```text
my_mod/
├─ mod.json
├─ scripts/
├─ data/
├─ models/
├─ textures/
├─ audio/
├─ maps/
├─ shaders/
└─ native/
```

Essa estrutura não é definitiva.

## Manifest

Conceitualmente:

```json
{
  "id": "example_mod",
  "name": "Example Mod",
  "version": "1.0.0",
  "entry": "scripts/main.<ext>",
  "dependencies": [],
  "optional_dependencies": []
}
```

## Escala de mods

### Conteúdo simples
Item, arma, NPC, inimigo, habilidade, efeito.

### Conteúdo composto
Boss, dungeon, área, questline, conjunto de equipamentos.

### Sistemas
Sistema mágico, inventário, árvore de habilidades, geração de mundo.

### Alterações profundas
Substituir combate, player controller, câmera ou regras centrais.

Essas alterações serão possíveis, mas não necessariamente fáceis.

## Extensão vs substituição

Sempre que possível, APIs devem favorecer extensão por hooks, eventos, componentes, registries e overrides.

Substituição completa continua como opção avançada.

## Ciclo de vida

```text
discover
→ inspect manifest
→ resolve dependency graph
→ mount assets
→ compile/load scripts
→ register definitions
→ initialize systems
→ start game
```

## Remoção de mods

Ao retirar um mod e reiniciar, seu código e assets deixam de ser carregados.

Saves que referenciem conteúdo removido exigirão política específica, ainda não definida.

## C++ em mods

Será possível em princípio, mas extensões nativas devem ser tratadas separadamente porque possuem riscos maiores de estabilidade e segurança.
