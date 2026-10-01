# 3. Assets, modelos e direção visual

## Formato principal de modelos

**Definido:** GLB/GLTF é o formato-fonte principal para modelos 3D.

**Preferência prática:** use GLB quando você quiser um asset autocontido.

Um único GLB pode carregar:

```text
model.glb
├─ meshes
├─ materials
├─ embedded textures
├─ skeleton
└─ animations
```

## Texturas embutidas

O pipeline foi projetado para trabalhar naturalmente com texturas embutidas em modelos.

Você não precisa separar:

```text
model.glb
model_albedo.png
model_normal.png
```

se o seu workflow preferir:

```text
model.glb
└─ texturas embutidas
```

Durante a compilação, o jogo pode extrair as imagens para sub-recursos internos otimizados. Isso é transparente para o autor.

## Sub-recursos endereçáveis

Mods avançados poderão alterar partes de um modelo sem substituir todo o GLB.

Exemplo conceitual:

```text
desert_expansion:scarab/mesh/body
desert_expansion:scarab/material/body
desert_expansion:scarab/texture/Body
desert_expansion:scarab/animation/attack
```

Isso permite, por exemplo, um compatibility mod trocar só a textura `Body`.

## Linguagem visual: cubos/cuboids

**Definido:** apesar de os modelos serem armazenados como GLB/GLTF, a maior parte dos modelos do jogo seguirá uma linguagem geométrica simples baseada em **cubos e cuboids**, semelhante à modelagem visual associada a Minecraft.

Isso vale especialmente para:

- personagens;
- inimigos humanoides;
- criaturas simples;
- props;
- partes de equipamento;
- elementos de cenário estilizados.

GLB é o contêiner/formato técnico; ele não implica modelos arredondados, high-poly ou realistas.

Um modelo profissional para este jogo deve priorizar:

- silhueta legível;
- poucos elementos geométricos;
- cuboids limpos;
- pivôs bem colocados;
- UVs simples;
- texturas de baixa resolução;
- consistência de escala;
- animação clara.

## Player

**Definido como direção de design:** a geometria e as proporções do player seguem o mesmo modelo humanoide blocado clássico de Minecraft.

Para modding profissional, trate o player como um humanoide composto essencialmente por:

```text
Head
Body
LeftArm
RightArm
LeftLeg
RightLeg
```

com pivôs/articulações previsíveis.

Quando o projeto publicar um template oficial de player/rig, **use esse template** como referência técnica.

Não dependa de extrair ou redistribuir texturas/arquivos proprietários de outros jogos; o ecossistema deve usar assets próprios, mesmo quando a geometria/proporções de referência forem equivalentes.

## Pixel-art

A estética vem principalmente de:

- texturas pequenas;
- nearest/point filtering;
- materiais simples;
- cuboids;
- paleta;
- iluminação;
- VFX coerente.

A tela inteira não é deliberadamente renderizada em low resolution.

## Escala

Todos os autores devem trabalhar com a escala oficial do projeto assim que ela for congelada.

Até o template oficial existir:

- mantenha escala consistente entre assets do mesmo mod;
- não aplique escalas absurdas no Transform para corrigir um modelo exportado errado;
- mantenha origem/pivôs previsíveis.

## Texturas independentes

PNG continua apropriado para:

- UI;
- ícones;
- VFX;
- atlases;
- decals;
- texturas que não pertencem naturalmente a um GLB.

Import settings poderão definir:

- sRGB/linear;
- nearest/linear;
- mipmaps;
- wrap/clamp;
- normal map;
- compressão;
- alpha.

## Animações

GLB/GLTF poderá armazenar animações junto do modelo.

Boas práticas:

- nomes de clips estáveis;
- pivôs corretos;
- skeleton consistente;
- não renomear bones publicados sem necessidade;
- não duplicar animações idênticas entre dezenas de arquivos se puder reutilizá-las.

## Materiais

Comece simples.

**Suporte executável atual:** GLB/GLTF estático autocontido com materiais
`OPAQUE` e `MASK`. O importador preserva `alphaCutoff` e `doubleSided`, e o
shader aplica recorte também ao alfa de `baseColorFactor`. `BLEND` e animação
continuam não suportados. Veja a [galeria de assets](../../examples/asset-gallery/README.md).

Tipos esperados:

- lit;
- unlit;
- alpha-test;
- transparent;
- emissive.

Para folhas, grama e recortes pixel-art, prefira alpha-test quando visualmente adequado em vez de transparência real.

## VFX

Combate pode usar muitos efeitos.

Um VFX pode conter:

- sprites;
- atlas;
- mesh simples;
- material emissivo;
- partículas;
- luz dinâmica opcional.

Não associe uma luz real a cada partícula. O renderer usa orçamento de luzes dinâmicas.

## Performance de modelos

Mesmo com estética low-poly/cuboid, performance ainda importa.

Evite:

- milhares de materiais únicos;
- centenas de texturas minúsculas quando um atlas faz mais sentido;
- skeletons excessivamente complexos;
- transparência em superfícies que poderiam usar cutout;
- objetos repetidos com meshes diferentes sem motivo.

O renderer foi projetado para aproveitar instancing de objetos repetidos.
