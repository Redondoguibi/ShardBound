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

Saves v1 preservam registros de conteúdo removido como dados órfãos, sem executar
esse conteúdo. Reinstalar o mod permite recuperá-lo no próximo load. Política em
[21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md).

## C++ em mods

Implementado pela ABI C v1 para DLLs Windows x64, com SDK próprio e opção
`--allow-native`. Extensões nativas executam no processo do jogo; contratos e
lifecycle estão em [21-EXTENSIBILITY-V1.md](21-EXTENSIBILITY-V1.md).


## Registro de assets

Assets públicos do mod devem ser declarados explicitamente. Apenas colocar um arquivo em `models/`, `textures/` ou outra pasta não cria automaticamente uma API pública.

Isso permite reorganizar sources sem quebrar IDs consumidos por outros mods.

## Merge e replace

Política definida:

- operações estruturalmente combináveis podem usar merge;
- substituições destrutivas precisam ser declaradas explicitamente;
- conflitos incompatíveis devem gerar diagnóstico em vez de serem silenciosamente resolvidos por ordem de arquivos.

A regra de prioridade entre dois replaces explícitos ainda será especificada.

## Preparação no startup

Quando novos/alterados mods exigirem compilação significativa, o jogo exibirá uma tela dedicada **Preparing Mods** com progresso e diagnóstico.

Mods já totalmente cacheados não devem causar espera perceptível.


## Documentação pública para autores

A documentação orientada a modders está em [modding/README.md](modding/README.md).

Ela é o ponto de entrada recomendado para autores externos e cobre:

- primeiro mod;
- manifest;
- assets;
- GLB/GLTF;
- modelagem;
- scripting;
- compatibilidade;
- performance;
- distribuição.

## Direção de modelos

Mods podem fornecer qualquer geometria suportada pelo pipeline, mas a identidade principal do jogo usa majoritariamente cubos/cuboids.

O player segue a estrutura geométrica/proporcional blocada do player clássico de Minecraft como referência. O projeto fornecerá template próprio; autores não devem depender de arquivos internos de terceiros.

Detalhes: [17-ART-DIRECTION-MODELING.md](17-ART-DIRECTION-MODELING.md).
