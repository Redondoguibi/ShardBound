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

- **Definido:** criar conteúdo usando cubos/cuboids, com textura e UV via ferramentas familiares do Blockbench.
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

**Proposta:** incluir template próprio do ShardBound com esses grupos e suporte visual a layers/UV no padrão de skin definido futuramente. Resolução padrão, formato aceito, variante de braço fino, camadas externas e regras de troca de skin estão **em aberto**. Criar/editar a skin não implica trocar automaticamente o personagem do jogo sem suporte do runtime.

## 5. Armas: editor com player de referência (definido)

**Definido:** enquanto modela uma espada ou outra arma, o criador deverá ter um **local no Blockbench com um modelo padrão do player, sem skin**, no qual poderá:

- Ajustar a posição da arma na mão e o modo de empunhadura;
- Ajustar posições/poses de partes do player — **inclusive partes que não estão diretamente envolvidas com a arma**;
- Criar e revisar **animações de ataque e outras ações no mesmo ambiente** de edição.

**Regra:** o player de referência é uma ferramenta de autoria/preview; não deve ser copiado como geometria adicional para dentro do arquivo final de cada espada. **Em aberto:** o contrato para dividir/exportar animações do personagem versus animações da arma, aplicar overrides e combinar com animações base do player. Não inventar uma semântica definitiva antes do projeto técnico.

**Proposta:** usar um painel contextual ou ambiente de preview com template oficial de player e pastas editáveis durante a edição da arma. A UI específica ainda não foi decidida.

**Proposta coerente com o requisito geral:** suportar no mesmo fluxo armas corpo a corpo e de longa distância, com marcadores auxiliares de empunhadura/posição de efeitos/projéteis. Esses marcadores não constituem ossos separados.

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

**Ainda em aberto:** blocos colocáveis/removíveis por jogador, edição de terreno em runtime, mineração, quebra, grade de encaixe e eventuais estados interativos. Não afirmar que esses sistemas já existem.

## 8. Compatibilidade com GeckoLib — escopo fechado

**Definido:** o plugin deve reproduzir **a experiência de autoria** de pastas, cubos, pivôs e timeline que o criador conhece. **Não há requisito de importar/exportar arquivos no formato GeckoLib**. A exportação é voltada para os contratos do ShardBound.

## 9. Validação desejada (proposta)

Conferir grupos sem nomes, pivôs inconsistentes, animações que referenciam grupos removidos, nomes duplicados, textura ausente, UV fora da imagem, escala inesperada, clips sem duração válida e frames de efeito associados a pontos inexistentes.

Um exportador não deve aceitar silenciosamente dados que o runtime não reproduz.
