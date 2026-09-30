# Visão do projeto

## Resumo

O projeto será um jogo 3D com estética pixel-art e forte inspiração estrutural em action RPGs como Minecraft Dungeons 2, mas com foco próprio: o jogo deve funcionar como uma **plataforma altamente modificável**.

Em vez de produzir primeiro dezenas de áreas, inimigos e itens, a prioridade será criar o sistema capaz de receber esses elementos posteriormente como mods.

## Ideia central

O jogo base fornece o runtime e um núcleo mínimo de experiência.

Mods fornecem grande parte do conteúdo e podem também alterar o comportamento do próprio jogo.

```text
CORE DO JOGO
├─ runtime
├─ renderização
├─ input
├─ física/colisão
├─ áudio
├─ entidades
├─ mundo/cenas
├─ UI
├─ save
├─ linguagem
└─ mod loader

MODS
├─ código da linguagem própria
├─ código C++ opcional
├─ JSON/dados
├─ modelos 3D
├─ texturas/imagens
├─ áudio
├─ mapas
└─ outros recursos
```

## Definido

- O núcleo será desenvolvido em C++.
- Não será usada uma engine de jogos tradicional.
- Bibliotecas externas são permitidas.
- O jogo será 3D.
- A identidade visual pretendida é pixel-art/3D estilizado.
- A maior parte da geometria usa cubos/cuboids simples, em uma linguagem visual semelhante à modelagem blocada de Minecraft.
- O player usa como referência estrutural a mesma geometria/proporções blocadas do player clássico de Minecraft; o projeto terá seu próprio template/rig e assets próprios.
- O jogo base deverá ser relativamente pequeno.
- Mods poderão adicionar conteúdo e alterar conteúdo já existente.
- Mods poderão depender de outros mods.
- Mods serão carregados principalmente durante a inicialização do jogo.
- Mapas poderão ser construídos e também gerados proceduralmente.
- A linguagem própria será importante, mas não será o único formato aceito em mods.
- A linguagem deve ser utilizável tanto por pessoas quanto por IAs.

## Objetivo de longo prazo

Permitir algo próximo a:

> Uma pessoa ou IA cria um pacote de conteúdo — código, modelos, texturas, dados e mapas — coloca esse pacote na instalação do jogo e, ao iniciar, o runtime integra o conteúdo ao jogo.

O sistema deve suportar desde mods simples, como uma espada nova, até mods avançados que adicionem regiões inteiras ou substituam sistemas do jogo.

## Não-objetivos iniciais

- Editor visual completo.
- Hot reload sofisticado.
- Marketplace.
- Multiplayer.
- Ferramentas de criação para usuários finais.
- Compatibilidade com consoles.
- Grande quantidade de conteúdo base.

Esses pontos podem ser considerados depois que o runtime e o sistema de mods estiverem sólidos.
