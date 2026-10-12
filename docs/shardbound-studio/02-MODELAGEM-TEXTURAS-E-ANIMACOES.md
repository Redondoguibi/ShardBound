# 02 — Modelagem, texturas, hierarquia e animação

## 1. Regra inegociável: pastas são as partes animáveis

**Definido pelo autor:** animação como no Blockbench para Minecraft com GeckoLib. O usuário cria **pastas/grupos**, insere cubos, ajusta o pivô do grupo e cria keyframes na timeline.

Exemplo conceitual:

```text
Monstro
├── corpo/              (grupo, pivô e animação)
│   ├── cubo_01
│   └── cubo_02
├── cabeca/             (grupo)
│   └── cubo_03
├── braco_direito/      (grupo)
│   └── cubo_04
├── braco_esquerdo/     (grupo)
├── perna_direita/      (grupo)
└── perna_esquerda/     (grupo)
```

O grupo `braco_direito` gira em torno do **pivô da pasta**. Pastas podem ser aninhadas, preservando transformações relativas. Não apresentar ao usuário outro editor para "construir ossos".

### Vocabulário interno versus interface

Em documentação de importadores glTF, "bone", "joint" ou "node" podem designar nós de transformação hierárquicos. **Isso não autoriza adicionar um workflow separado de ossos ao plugin.** Para o autor, são **grupos/pastas animáveis**.

**Proposta técnica:** exportar os grupos como nós hierárquicos de transformação com cubos associados, realizando animações rígidas por grupo, sem depender de deformação por pesos. Se alguma limitação técnica impedir preservar essa semântica na exportação, ela precisa ser resolvida no exportador/runtime, não repassada ao modelador como obrigação de skinning.

## 2. Modelagem e pintura

- **Definido:** criar conteúdo usando cubos/cuboids, com textura e UV via ferramentas familiares do Blockbench. **Definido:** a criação de efeitos/partículas do zero também deve ocorrer dentro do plugin e seu preview deve corresponder visualmente ao jogo.
- **Definido por padrão do jogo:** estética blocada/pixel art; geometria especial não cúbica pode existir como exceção visual.
- **Definido:** modelos exportáveis em GLB/GLTF com preferência por GLB e texturas embutidas.
- **Proposta:** presets de UV/tamanhos de textura sem fixar universalmente uma única resolução.
- **Proposta:** opção de filtros `nearest`, transparência recortada e materiais simples em conformidade com o importador.

Não confundir **textura do modelo embutida no GLB** com **textura de partícula**, que pode ser um recurso VFX independente.

## 3. Timeline e clips

**Definido:** timeline e animação como no fluxo GeckoLib do Blockbench.

Comportamentos mínimos desejados:
- Clips independentes com nomes definidos pelo autor (ex.: `idle`, `walk`, `attack`, `cast`, `reload`).
- Keyframes de **posição, rotação e escala dos grupos**.
- Pivôs e transformações locais previsíveis.
- Duração, looping e reprodução de preview.
- Trilhas/eventos para partículas, sons e outros efeitos sincronizados (ver documento 03).

**Proposta:** reutilizar o editor de animação existente do Blockbench e suas trilhas de efeitos quando compatíveis. Não prometer suporte a todos os formatos de expressão/interpolação do GeckoLib sem validação. Definir um subconjunto executável e diagnosticar o resto.

## 4. Skin de player

**Definido:** o plugin deve permitir criar skins de player.

**Fatos do projeto atual:** `assets/core/models/player.bbmodel` contém seis grupos `Head`, `Body`, `LeftArm`, `RightArm`, `LeftLeg`, `RightLeg`. O jogo já referencia a geometria blocada do player clássico de Minecraft, mas usa assets próprios. O documento `assets/core/models/README.md` registra exportação em escala 1/16 **para aqueles personagens**, não uma regra geral já fechada para todo asset.

**Definido (2026-10-12):** criar skins **iguais às skins do Minecraft** quanto ao formato de autoria: suporte a **PNG 64×64**, layout UV, **camadas externas** e variantes de braço padrão/fino. O modelo de referência do editor pode ser próprio do ShardBound, sem redistribuir texturas de outros jogos. A aplicação da skin ao player em runtime será integrada posteriormente.

## 5. Armas: editor com player de referência (definido)

**Definido:** enquanto modela uma espada **ou uma arma de longa distância**, o criador deverá ter um **local no Blockbench com um modelo padrão do player, sem skin**, no qual poderá:

- Ajustar a posição da arma na mão e o modo de empunhadura;
- Ajustar posições/poses de partes do player — **inclusive partes que não estão diretamente envolvidas com a arma**;
- Criar e revisar **animações de corrida, pulo, ataque, disparo e outras ações com a arma equipada** no **mesmo ambiente/timeline**. As ações pertinentes dependem do tipo de arma, mas o editor contextual é compartilhado entre melee e ranged.

**Regra:** o player de referência é ferramenta de autoria/preview; não precisa ser copiado como geometria adicional para o arquivo visual final de cada arma. **Já definido:** o animador cria as ações do player segurando aquela arma; não se trata apenas de animar a espada isoladamente. **A resolver tecnicamente:** contrato para dividir/exportar os canais do personagem versus os da arma, aplicar sobrescritas e combinar clips quando necessário. Não substituir esse requisito por animações procedurais automáticas sem consentimento.

**Proposta:** usar um painel contextual ou ambiente de preview com template oficial de player e pastas editáveis durante a edição da arma. A UI específica ainda não foi decidida.

**Definido:** usar o **mesmo editor de referência com player** também para armas de longa distância. **Proposta técnica:** marcadores de empunhadura e origem visual de efeitos/projéteis; esses marcadores não são ossos separados.

**Definido:** armas de longa distância **que usam flechas** compartilham uma **flecha padrão**, sem exigir a criação de um modelo de flecha para cada arma. Mods poderão contornar/personalizar/substituir esse comportamento. A lógica do projétil permanece no jogo; o plugin não configura dano, velocidade de projétil ou combate.

## 6. Armaduras: sobreposição por pastas (definido)

**Definido:** as peças de armadura são **colocadas por cima do modelo do jogador** e seguem os movimentos das partes correspondentes do corpo; não substituem visualmente braços, pernas ou o corpo base no fluxo definido.

**Definido:** a armadura é **organizada por pastas/grupos** para as regiões do corpo, inclusive braço esquerdo e direito. Exemplos de grupos, **não nomes finais obrigatórios**:

```text
Armadura
├── cabeça/
├── tronco/
├── braço_esquerdo/
├── braço_direito/
├── perna_esquerda/
└── perna_direita/
```

**Definido:** oferecer ferramenta de encaixe/preview semelhante à de armas usando o modelo do player de referência. **Ao contrário do editor de armas, não é necessário criar poses do player para armaduras.** O autor pode optar por animações da armadura.

**Em aberto:** como essas animações opcionais se somam ao movimento corporal, visuais de peças individuais/conjuntos, espessura/offset de sobreposição, compatibilidade com skins e variantes corporais.

**Proposta de implementação:** atrelar grupos da armadura aos grupos correspondentes do personagem por nomes/mapeamento estável, com offsets locais. O autor continua manipulando pastas e cubos, sem skinning por pesos.

## 7. Monstros, modelos compostos e blocos

### Monstros: hierarquia sem restrições de anatomia

**Definido:** **liberdade completa** para o autor montar qualquer hierarquia de grupos. Não exigir humanoide, seis membros ou outro rig obrigatório. Pastas podem representar caudas, asas, várias cabeças, tentáculos, estruturas abstratas etc.

**Proposta:** oferecer templates de conveniência, mas nunca tornar esses templates obrigatórios nem limitar o número/tipo de grupos por anatomia fixa. Restrições técnicas de performance e limites de tamanho poderão existir, mas não podem proibir hierarquias arbitrárias por tipo de monstro.

IA, física e comportamento de combate permanecem no ShardBound, não surgem automaticamente a partir de uma pasta.

### Projetos de um ou vários modelos relacionados

**Definido:** suportar **ambos** — projeto com um único objeto e projeto com vários modelos independentes relacionados, conforme o tipo de criação.

Exemplos **ilustrativos**: arco + flecha; criatura + objeto segurado; cajado + projétil visual. **Em aberto:** o agrupamento final, IDs, divisão em GLBs e modo de carregar submodelos. Um conjunto de modelos independentes **não** é o mesmo que um único modelo com muitas pastas.

### Blocos de composição de cenários

**Definido como direção de conteúdo:** a maior parte dos cenários do ShardBound será construída de **blocos**, com Minecraft Dungeons II como referência para a linguagem de cenários.

O editor precisa favorecer autoria de blocos/cubos e elementos modulares usados em composição de cenários; não restringir a categoria a meros objetos decorativos isolados.

**Definido:** **não** incluir como requisito deste plugin blocos colocáveis/quebráveis pelo jogador, mineração ou construção em gameplay. O objetivo é **produzir blocos usados para montar o mundo/cenários**. Detalhes técnicos como tamanho, orientação e grade de encaixe podem ser projetados internamente; não reabrir a questão de mecânicas Minecraft com o autor.

## 8. Compatibilidade com GeckoLib — escopo fechado

**Definido:** o plugin deve reproduzir **a experiência de autoria** de pastas, cubos, pivôs e timeline que o criador conhece. **Não há requisito de importar/exportar arquivos no formato GeckoLib**. A exportação é voltada para os contratos do ShardBound.

## 9. Formato editável — preferência provisória

O autor manifestou preferência por **um formato próprio de projeto** do ShardBound Studio, sem decisão definitiva da extensão ou do schema. A ferramenta deverá continuar funcionando dentro do Blockbench, preservando grupos, cubos, animações e metadados ao salvar/reabrir. A exportação para o jogo utilizará GLB **acompanhado de arquivos de efeitos/eventos**.

## 10. Validação desejada (proposta)

Conferir grupos sem nomes, pivôs inconsistentes, animações que referenciam grupos removidos, nomes duplicados, textura ausente, UV fora da imagem, escala inesperada, clips sem duração válida e frames de efeito associados a pontos inexistentes.

Um exportador não deve aceitar silenciosamente dados que o runtime não reproduz.
