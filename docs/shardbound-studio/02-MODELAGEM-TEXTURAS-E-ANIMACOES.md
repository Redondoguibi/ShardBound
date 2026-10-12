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

## 5. Armaduras e equipamentos

**Definido:** armaduras e armas devem ser criáveis no plugin, com texturas/animações.

**Proposta:** armaduras por grupo ou peça de corpo (cabeça, tronco, braços, pernas) e pontos de encaixe nomeados para itens; ao equipar, a peça acompanha a pasta correspondente do player. Esse processo deve continuar visualmente baseado em grupos, não em pintura de pesos.

**Proposta:** presets distintos para arma corpo a corpo e arma de longa distância, com grupo de empunhadura e marcadores visuais para efeitos/projéteis. Marcadores não são ossos editados separadamente; são auxiliares de posição.

**Em aberto:** como compatibilizar diferentes modelos de jogador, proporções alternativas, trocas de skin, armaduras e animações de armas.

## 6. Monstros, itens e blocos

- **Monstros:** grupos próprios, animações de locomoção/ataque/idle e eventos sincronizados. Comportamento de IA é responsabilidade do runtime/scripts.
- **Itens/armas:** geometria, textura, origem, animações e pontos de encaixe. Estatísticas e regras devem permanecer integradas ao sistema de definições do jogo.
- **Blocos:** faces/UV, orientação, possíveis estados e animações de grupos; colisão deve ser validada no jogo. Criar modelos de blocos **não** significa que existe um sistema de mundo voxel editável no jogo.

## 7. Validação desejada (proposta)

Conferir grupos sem nomes, pivôs inconsistentes, animações que referenciam grupos removidos, nomes duplicados, textura ausente, UV fora da imagem, escala inesperada, clips sem duração válida e frames de efeito associados a pontos inexistentes.

Um exportador não deve aceitar silenciosamente dados que o runtime não reproduz.
