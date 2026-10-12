# ShardBound Studio — documentação do plugin Blockbench

> **Estado:** especificação e planejamento; não significa implementação pronta.  
> **Registro inicial:** 2026-10-11. **Decisões complementares:** 2026-10-12.  
> **Objetivo:** criar conteúdo visual especializado para ShardBound, com a experiência de modelagem e animação do Blockbench/GeckoLib.

## Decisões centrais

- **Definido:** modelar com cubos/cuboids, pastas/grupos hierárquicos e pivôs; animar esses grupos na timeline, como no Blockbench usado com GeckoLib.
- **Definido:** não exigir criação de esqueletos separados, edição manual de bones, skinning ou pintura de pesos. Para o autor, as pastas são as partes animáveis.
- **Definido:** suportar autoria de skins de player, itens, armas corpo a corpo, armas de longa distância, blocos, armaduras, monstros, partículas e efeitos.
- **Definido:** configurar partículas e efeitos na **mesma sessão de animação do modelo**, disparados por keyframes em tempos/frames escolhidos.
- **Definido (armas melee e ranged):** editor/preview com **player padrão sem skin**, ajustes da arma na mão, poses do player e animações completas do jogador **correndo, pulando e atacando/disparando com a arma equipada**, no mesmo ambiente/timeline.
- **Definido (armaduras):** sobrepor as peças ao modelo do player, vinculadas a **pastas/grupos** por parte do corpo (braço esquerdo/direito etc.); editor análogo de encaixe, **sem exigir poses do player**, com animações opcionais.
- **Definido (monstros):** hierarquia de grupos totalmente livre, sem estrutura corporal predefinida obrigatória.
- **Definido (projetos):** aceitar tanto um objeto isolado quanto vários modelos independentes relacionados, conforme o tipo de criação.
- **Definido (GeckoLib):** replicar a **experiência de autoria**, não exigir importação/exportação compatível com formatos específicos do GeckoLib.
- **Definido (sequência):** implementar primeiro as ferramentas do plugin no Blockbench; a integração funcional completa ao runtime do jogo vem depois.
- **Definido (blocos):** grande parte do cenário do ShardBound será construída com blocos, como na referência visual de Minecraft Dungeons II; **não** incluir mecânica de jogador colocar, quebrar, minerar ou construir com blocos como requisito do plugin.
- **Definido (escopo):** ferramentas visuais **sem editor de dados de gameplay** (dano, vida, IA etc.); criar **novo plugin**, não evoluir obrigatoriamente o antigo.
- **Definido (VFX):** criar partículas do zero no Blockbench; preview com **aparência idêntica à do jogo** como meta técnica a validar; exportar **GLB acompanhado de arquivos auxiliares** de eventos e efeitos.
- **Definido (player):** animações gerais do jogador são criadas **no próprio projeto de criação do player**, sem uma categoria/projeto separado exclusivamente para animações.
- **Definido (som sincronizado):** inserir **sons nos keyframes da timeline**, assim como partículas, com eventos de reprodução durante animações.
- **Definido (efeitos avançados):** incluir **rastros de armas, flashes, efeitos de brilho** e outros VFX, mesmo quando a implementação técnica não usar partículas tradicionais. Para o criador, todos pertencem às ferramentas integradas de efeitos.
- **Definido (skins):** skins no formato **Minecraft**, inclusive PNG 64×64, camadas e UV.
- **Definido (flechas):** flecha padrão compartilhada pelas armas que usam flechas, com possibilidade de personalização por mods.
- **Preferência ainda não congelada:** o autor inclinou-se a **formato editável próprio** para o projeto do Studio.
- **Alinhamento com o ShardBound:** modelos GLB/GLTF, preferência por GLB com texturas embutidas, direção artística blocada/pixel-art, mods e IDs `namespace:path`.
- **Importante:** suporte do editor/exportador não equivale a suporte pronto do runtime; faltam contratos e implementação.

## Índice

1. [01-VISAO-E-ESCOPO.md](01-VISAO-E-ESCOPO.md) — intenção, tipos de conteúdo e fluxo de trabalho.
2. [02-MODELAGEM-TEXTURAS-E-ANIMACOES.md](02-MODELAGEM-TEXTURAS-E-ANIMACOES.md) — pastas/grupos, cubos, pivôs, skins, rig e timeline.
3. [03-PARTICULAS-EVENTOS-E-EXPORTACAO.md](03-PARTICULAS-EVENTOS-E-EXPORTACAO.md) — efeitos em frames, locators, formatos e exemplos.
4. [04-INTEGRACAO-E-IMPLEMENTACAO.md](04-INTEGRACAO-E-IMPLEMENTACAO.md) — necessidades do runtime, compatibilidade, validação e fases.
5. [05-DECISOES-EM-ABERTO.md](05-DECISOES-EM-ABERTO.md) — decisões consolidadas, preferências provisórias e pendências técnicas legítimas.

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
