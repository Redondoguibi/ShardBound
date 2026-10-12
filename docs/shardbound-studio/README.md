# ShardBound Studio — documentação do plugin Blockbench

> **Estado:** especificação e planejamento; não significa implementação pronta.  
> **Registro:** 2026-10-11.  
> **Objetivo:** criar conteúdo visual especializado para ShardBound, com a experiência de modelagem e animação do Blockbench/GeckoLib.

## Decisões centrais

- **Definido:** modelar com cubos/cuboids, pastas/grupos hierárquicos e pivôs; animar esses grupos na timeline, como no Blockbench usado com GeckoLib.
- **Definido:** não exigir criação de esqueletos separados, edição manual de bones, skinning ou pintura de pesos. Para o autor, as pastas são as partes animáveis.
- **Definido:** suportar autoria de skins de player, itens, armas corpo a corpo, armas de longa distância, blocos, armaduras, monstros, partículas e efeitos.
- **Definido:** configurar partículas e efeitos na **mesma sessão de animação do modelo**, disparados por keyframes em tempos/frames escolhidos.
- **Alinhamento com o ShardBound:** modelos GLB/GLTF, preferência por GLB com texturas embutidas, direção artística blocada/pixel-art, mods e IDs `namespace:path`.
- **Importante:** suporte do editor/exportador não equivale a suporte pronto do runtime; faltam contratos e implementação.

## Índice

1. [01-VISAO-E-ESCOPO.md](01-VISAO-E-ESCOPO.md) — intenção, tipos de conteúdo e fluxo de trabalho.
2. [02-MODELAGEM-TEXTURAS-E-ANIMACOES.md](02-MODELAGEM-TEXTURAS-E-ANIMACOES.md) — pastas/grupos, cubos, pivôs, skins, rig e timeline.
3. [03-PARTICULAS-EVENTOS-E-EXPORTACAO.md](03-PARTICULAS-EVENTOS-E-EXPORTACAO.md) — efeitos em frames, locators, formatos e exemplos.
4. [04-INTEGRACAO-E-IMPLEMENTACAO.md](04-INTEGRACAO-E-IMPLEMENTACAO.md) — necessidades do runtime, compatibilidade, validação e fases.
5. [05-DECISOES-EM-ABERTO.md](05-DECISOES-EM-ABERTO.md) — perguntas e contratos pendentes.

## Vocabulário de decisão

- **Definido:** preferência/requisito explicitamente confirmado pelo autor.
- **Proposta:** recomendação do assistente, ainda sem aprovação final.
- **Em aberto:** depende de decisão ou protótipo técnico.
- **Estado atual:** suporte relatado pelos documentos existentes do projeto, não intenção futura.

## Referências internas

- [Conteúdo e assets](../05-CONTENT-AND-ASSETS.md)
- [Asset pipeline](../14-ASSET-PIPELINE.md)
- [Direção de arte](../17-ART-DIRECTION-MODELING.md)
- [Equipamentos](../24-EQUIPMENT-AND-WORLD-CONTENT.md)
- [Guia de assets de mods](../modding/03-ASSETS-AND-MODELS.md)
- [Modelos atuais](../../assets/core/models/README.md)

**Regra:** exemplos deste conjunto não são APIs do ShardBound já implementadas nem decisões irrevogáveis.
