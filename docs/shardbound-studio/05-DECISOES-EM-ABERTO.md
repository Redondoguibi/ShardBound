# 05 — Registro de decisões e questões ainda em aberto

> Revisado em 2026-10-11 com base nas **sete respostas do autor** sobre o ShardBound Studio.
>
> **Regra:** `Definido` significa requisito decidido, **não** implementação realizada. Manter escolhas não respondidas explicitamente abertas. Não voltar a perguntar sobre requisitos já respondidos sem motivo técnico concreto.

## A. Decisões confirmadas nesta rodada

### D1 — Editor de armas e animação com player de referência — DEFINIDO

O autor rejeitou o enquadramento anterior de animar **somente a arma** ou de escolher um modo rígido.

Ao modelar uma espada, deve existir **um local de edição/pré-visualização** com um **modelo padrão de player, sem skin**, permitindo:

1. Ajustar **onde e como a espada fica no player**, incluindo sua posição na mão;
2. Ajustar posições/poses do player, **inclusive partes que não envolvem diretamente a espada**;
3. **Animar o ataque e outras ações ali mesmo**, no contexto combinado de player e arma.

Este requisito vale para o fluxo exemplificado por armas corpo a corpo. **Proposta, ainda sujeita a detalhamento:** aplicar o mesmo conceito ao trabalho com armas de longa distância.

**Não decidido:** a divisão dos dados exportados entre animação do player, animação da arma, overrides e clips compartilháveis. O mannequin do player é uma referência de autoria; incluir sua geometria em cada arquivo de arma **não** é exigido.

### D2 — Projeto de um ou de vários modelos — DEFINIDO

**Ambos os modos** deverão existir, **conforme o tipo de criação**: projetos com um modelo e projetos com múltiplos modelos independentes relacionados. A divisão técnica dos recursos exportados e a identificação dos submodelos permanecem em aberto.

### D3 — Monstros com hierarquia totalmente livre — DEFINIDO

O autor exige **liberdade completa para inventar qualquer hierarquia de pastas/grupos**. Não exigir anatomia padrão, hierarquia humanoide ou grupos de nomes fixos para os monstros. Templates opcionais são **proposta**, jamais obrigação.

### D4 — Armaduras como sobreposição organizada em pastas — DEFINIDO

As peças de armadura ficam **por cima do modelo do player** e **acompanham os movimentos** das partes do corpo correspondentes. O autor quer organizá-las por **pastas/grupos**, inclusive braços esquerdo e direito e demais regiões.

Deve existir um contexto de posicionamento/preview do player análogo ao das armas, **mas sem exigir poses do player para editar armaduras**. Animações próprias das armaduras são **opcionais**. Não presumir substituição total do corpo como comportamento padrão.

**Não decidido:** compatibilidade com variantes corporais, offsets, camadas múltiplas e a combinação das animações próprias da armadura com as animações do player.

### D5 — Blocos como parte fundamental dos cenários — DIREÇÃO DEFINIDA; INTERAÇÃO EM ABERTO

Resposta do autor: **"Pense em MCD2, o mundo é literalmente blocos. A maior parte dos cenários do ShardBound também será composto por blocos."**

Portanto, o plugin deve apoiar a **criação de blocos e componentes modulares usados extensivamente em cenários**. Não reduzir a categoria a meros props decorativos.

O autor **não decidiu** que jogadores devem colocar, remover, minerar ou modificar esses blocos. O suporte a construção/edição de mundo em runtime continua **em aberto**; referência a Minecraft Dungeons II expressa linguagem de construção do cenário, não necessariamente mecânicas de Minecraft.

### D6 — Relação com GeckoLib — DEFINIDO

**Somente a mesma experiência de edição e animação**: grupos/pastas, cubos, pivôs, timeline, eventos etc. **Não exigir** importação de modelos/animações de GeckoLib nem exportação para GeckoLib. A ausência desse requisito não proíbe futura compatibilidade opcional.

### D7 — Ordem de desenvolvimento — DEFINIDO

**Primeiro desenvolver todas as ferramentas do plugin no Blockbench; depois integrar ao ShardBound.**

Os requisitos de autoria, preview, validação e exportação podem ser implementados e testados no Blockbench antes do funcionamento in-game. Planejar um contrato técnico de exportação desde cedo, mas **não** exigir que cada feature do editor já rode no jogo no momento da entrega da fase de plugin.

## B. Decisões anteriores que permanecem definidas

- Plugin para Blockbench, especializado no ShardBound.
- Modelagem/animação com **pastas/grupos animáveis, cubos, pivôs, timeline e keyframes**, como no fluxo conhecido do autor em Minecraft/GeckoLib.
- **Sem** workflow separado obrigatório de ossos, deformação por skinning ou pintura de pesos.
- Suporte desejado a **skins de player, itens, blocos, armaduras, monstros, armas melee e ranged, partículas e efeitos**; texturas e animações quando aplicáveis.
- Efeitos/partículas na **mesma sessão e timeline do modelo**, associados a frames específicos.
- Modelos em GLB/GLTF (preferência por GLB com texturas embutidas) e arte predominantemente em cubos/cuboids, alinhados ao ShardBound.

## C. Questões realmente ainda em aberto

Essas perguntas **não foram respondidas** pelo autor, nem são resolvidas automaticamente pelas sete decisões anteriores.

### Prioridade alta de produto

1. **Dados de gameplay no editor:** apenas dados visuais/efeitos ou também formulários de vida, dano, velocidade, comportamento, projéteis e geração de `.sl`?
2. **Autoria de partículas:** editor de efeitos integralmente dentro do Blockbench desde o começo ou primeiro biblioteca com parâmetros, para depois expandir? A associação a frames já está definida.
3. **Formato físico da exportação:** um `.glb` com arquivos auxiliares gerados juntos ou literalmente um `.glb` único com metadados customizados? Exportar efeitos **associados ao modelo** é definido; bytes num arquivo único, não.
4. **Formato de skin:** aceitar especificamente PNG 64×64 e camadas/variantes do Minecraft ou usar template/formato próprio do ShardBound?
5. **Mistura de animações de armas e player:** como animações de golpe que alteram partes do player coexistem com idle, walk, corrida e outras ações? Que dados são armazenados no projeto de arma?
6. **Construção de cenários por blocos:** blocos serão apenas elementos de composição de mapas construídos ou também objetos que o player poderá colocar/remover/interagir? Escala, grade, rotação e variante de bloco também exigem contrato.

### Prioridade média de produto

7. **Templates e categorias:** projeto novo por categoria ou reclassificação de um projeto existente sem perder dados?
8. **Destino do export:** gerar pasta/pacote de arquivos ou editar diretamente o mod de destino? Como proteger alterações manuais em `.sl` e `assets.json`?
9. **Preview VFX:** fidelidade próxima ao renderer do ShardBound ou preview aproximado aceitável antes da integração?
10. **Plugin já existente no repositório:** evoluir o exportador do Blockbench já documentado ou construir plugin separado com reutilização seletiva?
11. **Projetos compostos:** como representar, identificar e exportar múltiplos modelos independentes que pertencem ao mesmo projeto?
12. **Armaduras avançadas:** compatibilidade com skins/variantes de player, sobreposição de partes e animações opcionais.
13. **Experiência de ranged:** editor contextual com player igual ao de espadas para arcos, bestas, cajados etc., inclusive poses e ponto de saída de efeitos/projéteis?

## D. Decisões técnicas para o desenvolvimento resolver

- Estrutura dos grupos transformáveis, pivôs, clips, interpolação, blending e escalas de exportação sem impor esqueleto editável separado.
- Separação entre player de preview e assets exportados; dados de animação combinada da arma.
- Schema versionado para cenas, submodelos, animações e eventos por frame.
- Eventos de partículas com locators, emissor contínuo ou burst, pause/loop/cancelamento.
- Empunhadura, offsets e hierarquia de armadura associada a grupos do player.
- Ferramentas de teste/validação do plugin independentes de uma engine completamente integrada.
- Evolução do runtime para reproduzir depois o resultado dos recursos exportados.

## E. Próxima ação documental

Quando o autor tomar novas decisões, atualizar este registro e os documentos temáticos `01`–`04` na mesma rodada; **não converter propostas técnicas em decisões do autor**. Evitar reabrir D1–D7 sem necessidade.
