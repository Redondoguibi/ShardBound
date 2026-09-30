# Direção de arte e modelagem 3D

Este documento registra regras visuais/técnicas importantes para modelos oficiais e mods.

## Princípio geral

O jogo usa GLB/GLTF como formato técnico principal, mas isso **não significa uma direção de modelagem realista ou high-poly**.

**Definido:** a maior parte dos modelos seguirá uma linguagem geométrica baseada em cubos e cuboids, visualmente próxima da construção de modelos de Minecraft.

A identidade combina:

- geometria blocada;
- poucos volumes;
- silhuetas claras;
- texturas pixel-art;
- iluminação 3D;
- animações simples e legíveis;
- VFX relativamente ricos.

## Modelos comuns

A maioria de personagens, inimigos, props e equipamentos deve poder ser construída principalmente com:

```text
cube
cuboid
simple planes quando necessário
```

Não é obrigatório que absolutamente todo asset seja cúbico.

Bosses, VFX, objetos especiais e partes orgânicas podem usar geometria mais livre quando isso melhorar o resultado.

A regra é estética, não uma limitação do formato.

## Player

**Definido:** o modelo base do player deve usar literalmente a mesma estrutura geométrica/proporcional blocada do player clássico de Minecraft como referência de forma.

Estrutura conceitual:

```text
Player
├─ Head
├─ Body
├─ LeftArm
├─ RightArm
├─ LeftLeg
└─ RightLeg
```

Isso facilita:

- skins;
- armaduras;
- equipamentos;
- animações;
- criação de conteúdo por modders;
- geração de modelos por IA;
- ferramentas simples de edição.

O projeto deverá fornecer seu próprio template/rig oficial para modders quando a implementação alcançar essa etapa.

A referência à geometria/proporções não significa que o projeto deve redistribuir texturas ou arquivos proprietários de terceiros. Assets oficiais do jogo devem ser próprios.

## Rig

O rig oficial deverá manter nomes/pivôs estáveis para permitir:

- troca de skins/modelos;
- equipamentos anexados;
- armas na mão;
- VFX presos a bones;
- animações reutilizáveis.

A nomenclatura final de bones ainda será congelada.

## Escala

Uma escala oficial de exportação/importação será definida antes da produção de grande quantidade de conteúdo.

Quando definida, deverá ser usada por:

- conteúdo oficial;
- templates;
- mods;
- ferramentas de validação.

## Texturas

Direção principal:

- baixa resolução;
- nearest/point filtering quando adequado;
- UVs legíveis;
- paleta controlada;
- texturas embutidas em GLB/GLTF por padrão quando pertencem naturalmente ao modelo.

PNG continua disponível para recursos independentes.

## Materiais

Materiais devem permanecer simples quando possível:

- lit;
- unlit;
- alpha-test;
- transparent;
- emissive.

O visual deve vir mais de:

- textura;
- forma;
- iluminação;
- VFX;

do que de materiais fisicamente complexos.

## Modelos complexos

Geometria não cúbica é permitida.

Exemplos onde pode fazer sentido:

- bosses;
- criaturas especiais;
- objetos mágicos;
- armas ornamentadas;
- efeitos;
- cenário específico.

Mesmo nesses casos, o asset deve continuar coerente com a identidade estilizada do jogo.

## Performance

A direção cúbica também é vantajosa para performance:

- baixa contagem de vértices;
- meshes reutilizáveis;
- instancing;
- bounds simples;
- animações leves.

Autores devem evitar complexidade geométrica sem ganho visual correspondente.

## Workflow recomendado

```text
modelar
↓
UV/textura
↓
rig/animação se necessário
↓
embutir texturas
↓
exportar GLB
↓
declarar asset
↓
asset compiler
↓
validar
↓
testar no jogo
```
