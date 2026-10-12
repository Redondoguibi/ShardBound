# 01 — Visão, escopo e experiência do ShardBound Studio

## 1. Visão

**Definido:** criar um plugin especializado do Blockbench para construir conteúdo utilizável no ShardBound. Ele deve manter o processo de criação familiar para quem conhece **Minecraft + Blockbench + GeckoLib**; não obrigar a aprender um sistema de modelagem/animação diferente.

A meta é que criadores possam **modelar, texturizar, animar, incluir efeitos e exportar** um recurso para o jogo ou para um mod. O conteúdo exportado deve seguir os contratos oficiais do ShardBound. O plugin não substitui o runtime de animação/VFX e não precisa ser uma engine de jogo.

### O que significa "igualzinho ao GeckoLib"

- **Definido, na experiência de autoria:** árvore de pastas/grupos e cubos, pivôs, editor UV/painting do Blockbench, timeline, keyframes e clips; grupos podem ser animados.
- **Definido:** não introduzir no editor um fluxo obrigatório de esqueleto separado, skinning por influência de vértices ou pintura de pesos.
- **Proposta:** reutilizar o editor nativo e o sistema de Animate Effects do Blockbench em vez de recriar visualmente todas as ferramentas.
- **Definido:** a referência ao GeckoLib significa **mesma experiência de edição e animação**, **não** uma exigência de importar projetos GeckoLib existentes ou exportar para o formato GeckoLib.
- **Não prometido:** compatibilidade binária com todos os arquivos/recursos do GeckoLib, igualdade de APIs, redistribuição de código ou de assets de terceiros, ou equivalência absoluta de cada menu.

## 2. Conteúdos que o plugin deverá atender

| Tipo | Requisito confirmado | Ferramentas específicas propostas |
| --- | --- | --- |
| Skin do player | Criar/editar **skins no padrão Minecraft** | PNG 64×64, UV, camadas de skin e preview no humanoide de referência |
| Item genérico | Criar modelos de itens | Poses de exibição, escala e ponto de encaixe |
| Arma corpo a corpo | Criar e animar armas melee **com o player de referência visível** | Player padrão sem skin, ajuste na mão e animações completas do player **correndo, pulando e atacando com a arma equipada**, no mesmo editor |
| Arma de longa distância | Criar e animar armas ranged **com o mesmo editor contextual de player** | Animações de uso, corrida, pulo, disparo e outras ações pertinentes; pontos visuais de saída de projéteis e efeitos |
| Bloco | Criar blocos que compõem grande parte do cenário do ShardBound | Cubos e módulos de cenário, orientação, variantes e referências de colisão; **não** incluir mecânica de colocação/quebra pelo jogador |
| Armadura | Criar peças **sobrepostas** ao player, organizadas em pastas por parte do corpo | Editor com player de referência, encaixe por braço esquerdo/direito etc.; animações opcionais, sem exigir poses do player |
| Monstro | Criar mobs/criaturas animados com **hierarquia de grupos totalmente livre** | Clips e efeitos por grupo, sem exigir forma humanoide ou estrutura fixa |
| Partículas/VFX | **Criar efeitos do zero** | Editor completo de parâmetros/texturas/emissores, keyframes e **preview de aparência idêntica ao jogo** |

**Definido:** todos os tipos devem contemplar texturas; animações devem estar disponíveis sempre que fizerem sentido (não necessariamente obrigatórias para blocos/skins).

**Definido:** projetos podem conter um único modelo ou vários modelos independentes relacionados, conforme o conteúdo. A estrutura de exportação e as regras para vínculo entre modelos continuam em aberto.

**Definido:** a maior parte dos cenários do jogo será composta por blocos, tendo Minecraft Dungeons II como referência de construção visual. **Não** projetar editor de construção/mineração ou blocos colocáveis/quebráveis pelo jogador; a categoria diz respeito à autoria de **elementos do cenário**.

**Definido:** o plugin **não** oferece edição de gameplay (dano, vida, velocidade, IA, estatísticas, lógica de ataque etc.). Ele se concentra em **modelos, skins, texturas, animações, efeitos e dados visuais**. A lógica de combate e as regras permanecem nos sistemas do ShardBound. Pontos visuais de origem de projéteis podem ser preparados no editor sem configurar sua lógica de gameplay.

**Definido:** as **flechas padrão são compartilhadas** entre armas de longa distância que usam flechas; não exigir um modelo de flecha exclusivo por arma. Mods podem personalizar ou substituir essa escolha.

## 3. Fluxo de autoria desejado

1. Escolher um template de conteúdo do ShardBound no Blockbench.
2. Modelar com cubos/cuboids e organizar em pastas/grupos.
3. Pintar texturas e configurar UVs, preferencialmente em baixa resolução/pixel art.
4. Criar clips com keyframes para grupos na timeline.
5. No mesmo projeto e sessão, criar/selecionar partículas e associá-las a frames, com sons e eventos quando desejado.
6. Validar escala, IDs, referências e compatibilidade.
7. Exportar o conjunto para o ShardBound, sem separar manualmente texturas do modelo.

**Preferência ainda não final:** o autor inclinou-se a um **formato de projeto próprio** do ShardBound Studio, editável no Blockbench, em vez de obrigar um formato específico por categoria. Extensão e preservação de interoperabilidade com `.bbmodel` serão escolhas técnicas. **Definido:** a exportação usa **GLB com arquivos acompanhantes** para efeitos/eventos; não precisa colocar todos os dados dentro de um único GLB. Caminhos, schemas e nomes concretos ainda exigem contrato.

## 4. Editor contextual de armas e armaduras (definido)

**Armas (melee e ranged):** ao modelar uma espada, arco ou outra arma, oferecer um local de edição/pré-visualização com **modelo padrão do player sem skin**. Nele o autor posiciona a arma na mão, ajusta como ela é segurada e pode animar **corrida, pulo, ataque, disparo e ações pertinentes com a arma equipada**, movendo pastas do jogador inclusive as não diretamente ligadas à arma. Tudo ocorre no **mesmo editor/timeline**. O player é referência de autoria; não significa embutir sua geometria em cada arma exportada.

**Armaduras:** fornecer um contexto semelhante para verificar como as peças se encaixam sobre o player. As peças acompanham os grupos correspondentes do corpo, organizados por pastas (inclusive braços esquerdo e direito). **Não exigir edição de poses do player no fluxo de armadura**; permitir animações da armadura quando o criador desejar.

**A definir tecnicamente:** quais canais/clips do player pertencem ao pacote exportado da arma e como serão combinados com animações base. **Já está decidido:** o editor contextual também vale para armas de longa distância.

## 5. Requisitos de experiência do usuário

- Familiaridade com o Outliner do Blockbench: grupos, subgrupos, cubos e pivôs.
- A timeline visual não deve exigir escrever código para declarar um efeito em certo frame.
- Ao animar uma espada, por exemplo, o autor precisa poder adicionar partículas **criadas do zero** a **essa mesma animação**, sem abrir um projeto desconectado.
- Presets/templates para reduzir erros repetitivos ao criar mods.
- Mensagens de erro claras para referências/formatos não suportados.
- **Definido:** pré-visualização de efeitos e partículas com **aparência idêntica** à execução no ShardBound. A compatibilidade visual ainda precisará ser construída e testada tecnicamente, sobretudo após integração.
- Exportação reprodutível, sem alterações inesperadas nos originais.

## 6. Direção artística e integração

- **Definido no ShardBound:** modelos predominantemente cuboides, proporções blocadas e texturas pixel-art.
- **Definido no ShardBound:** GLB/GLTF como formatos-fonte e preferência por textura embutida em GLB.
- **Definido no ShardBound:** identificação pública de assets por `namespace:path`, com declaração explícita no sistema de mods.
- **Alinhamento:** não importar texturas ou outros assets proprietários de Minecraft/Minecraft Dungeons como conteúdo oficial.
- **Proposta:** nomes como "ShardBound Studio", "ShardBound Model" e "Particle Editor" são de trabalho, não marca final confirmada.

## 7. Independência do plugin anterior e destino da exportação

**Definido:** construir **um novo plugin ShardBound Studio**, e não usar o plugin/exportador Blockbench anterior como produto a ser evoluído. Isso não impede usar os contratos existentes do ShardBound como referência.

**Não prioritário:** instalar automaticamente arquivos na pasta de um mod. **Proposta conservadora:** um comando de exportação salva o GLB e acompanhantes em uma pasta escolhida, sem editar mods ou scripts automaticamente. O autor não tomou decisão final sobre a automação da instalação e não precisa ser perguntado novamente agora.

## 8. Ordem de implementação (definida)

Primeiro implementar **todas as ferramentas de criação do plugin dentro do Blockbench**; depois integrar o conteúdo produzido ao runtime do ShardBound. O desenvolvimento pode definir schemas de exportação, criar exemplos e testes do editor antes da integração, mas **não deve afirmar** que animações, armaduras e VFX já funcionam no jogo nessa etapa.

## 9. Fora do escopo já confirmado

Não foi solicitado: editor de mapas completo, lógica de IA de monstros dentro do Blockbench, linguagem de programação visual completa ou substituição do sistema `.sl`. Esses recursos não devem ser presumidos.
