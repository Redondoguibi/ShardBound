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

GLB/GLTF é uma forte opção inicial por ser moderno, aberto e adequado a pipelines automatizados.

A escolha final ainda deve considerar animação, skeletal meshes, materiais, compressão, importação rápida, geração por IA e ferramentas existentes.

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
