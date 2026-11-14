# Conteúdo e assets

## Tipos de recursos

Mods poderão conter:

- modelos 3D;
- animações;
- texturas;
- imagens;
- materiais;
- áudio;
- mapas;
- dados;
- shaders;
- scripts.

## Modelos

GLB/GLTF é o formato-fonte principal planejado para modelos 3D.

**Definido:** modelos devem poder utilizar texturas embutidas. O fluxo preferido permite que mesh, materiais, texturas, skeleton e animações viajem juntos no mesmo asset, especialmente em GLB.

O pipeline poderá extrair esses elementos para sub-recursos internos otimizados durante a compilação sem exigir que o autor do mod separe as texturas manualmente.

GLTF textual também poderá ser aceito quando suas imagens estiverem embutidas no próprio asset/formato suportado pelo importador.

## Geometria e modelagem

**Definido:** a maioria dos modelos é construída principalmente a partir de cubos/cuboids simples, mesmo sendo distribuída em GLB/GLTF.

O modelo base do player segue a mesma estrutura geométrica/proporcional blocada do player clássico de Minecraft como referência de forma.

O projeto deverá manter seu próprio template/rig e assets próprios para uso oficial e por modders.

Modelos não cúbicos continuam permitidos quando a direção artística exigir.

Veja [17-ART-DIRECTION-MODELING.md](17-ART-DIRECTION-MODELING.md).

## Pixel-art 3D

**Definido:** a estética pixel-art vem principalmente dos assets e materiais, enquanto a imagem final continua sendo uma renderização 3D normal.

A direção visual combina:

- geometria 3D;
- texturas de baixa resolução;
- filtragem nearest/point onde aplicável;
- materiais estilizados;
- iluminação 3D;
- efeitos e partículas coerentes com pixel-art;
- resolução final não deliberadamente pixelada.

O render scale poderá reduzir resolução interna por performance, mas isso não é parte obrigatória da identidade visual.

## Perfis visuais de área

Cada área poderá definir sua própria identidade cromática sem eliminar o ciclo dia/noite.

Um perfil de área poderá influenciar futuramente:

- ambient color;
- fog;
- color grading;
- exposição;
- céu;
- intensidade/temperatura de iluminação;
- matiz geral.

Esses perfis devem ser data-driven e futuramente acessíveis a mods.

## VFX

O jogo pretende suportar combates com muitos efeitos e partículas.

Assets de VFX poderão incluir:

- sprites/atlases;
- texturas emissivas;
- meshes simples;
- parâmetros de partículas;
- materiais additive/alpha;
- referências opcionais a dynamic lights.

O formato definitivo ainda será especificado.

## Mapas

O sistema deve suportar ambos:

### Mapas construídos

Um mod fornece uma cena/mapa preparado previamente.

### Geração procedural

Scripts e dados constroem o mapa em runtime ou durante uma etapa de compilação.

Os dois modelos podem coexistir.

## Pipeline conceitual

```text
source asset
   ↓
import
   ↓
validate
   ↓
convert/optimize
   ↓
cache
   ↓
runtime representation
```

Exemplos:

```text
.glb → mesh/material/animation runtime
.png → texture runtime
.wav/.ogg → audio runtime
map source → scene representation
script → bytecode/IR
```

## Cache

O compilador/toolchain pode evitar trabalho repetido mantendo cache de conteúdo processado.

Ao iniciar:

- asset inalterado → usa cache;
- asset alterado → recompila/converte;
- mod removido → deixa de ser montado.

Isso preserva a ideia de "jogar um mod no compilador e ele virar parte do jogo" sem alterar permanentemente o executável base.


## Arquitetura do pipeline

A especificação detalhada da etapa está em [14-ASSET-PIPELINE.md](14-ASSET-PIPELINE.md).

Decisões atuais:

- arquivos de mod são fontes amigáveis para criação;
- o runtime poderá usar representações compiladas/cacheadas;
- todo recurso exposto recebe ID estável `namespace:path`;
- gameplay/mods referenciam IDs, não paths físicos;
- VFS separa localização física de identidade;
- cache é incremental e externo aos sources do mod;
- Asset Manager retorna handles controlados;
- assets devem poder ser descarregados por área/lifetime;
- GLB/GLTF são formatos-fonte principais de modelos;
- texturas de modelos podem e preferencialmente poderão estar embutidas;
- PNG continua disponível para texturas independentes/UI/VFX;
- assets públicos precisam ser declarados explicitamente;
- merges seguros são permitidos; replaces destrutivos precisam ser explícitos;
- compilação relevante de mods usa uma tela "Preparing Mods".
