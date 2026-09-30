# Etapa 4 — Asset Manager e Asset Pipeline

Este documento formaliza a arquitetura inicial de assets do projeto.

## Objetivo

Transformar arquivos amigáveis para autores de mods em recursos eficientes para o runtime.

O sistema deve aceitar fontes simples como:

- GLB/GLTF;
- PNG;
- WAV/OGG;
- JSON/dados;
- mapas;
- shaders;
- scripts;

e produzir/armazenar representações otimizadas para o jogo.

A ideia central é:

```text
Mod / Core
  ↓
source assets
  ↓
Asset Compiler
  ↓
validation + dependency analysis
  ↓
compiled cache
  ↓
Asset Manager
  ↓
runtime handles/resources
```

## Princípio fundamental

Arquivos-fonte não são necessariamente a representação final usada em gameplay.

Exemplos:

```text
model.glb
→ validação/importação
→ mesh/material/animation compilados
→ cache
→ GPU/runtime
```

```text
texture.png
→ validação
→ mipmaps/compressão/conversão
→ textura compilada/cacheada
→ GPU
```

O arquivo original continua pertencendo ao mod. O compilador não precisa modificar os arquivos-fonte.

## IDs universais

Todo recurso exposto ao jogo deve possuir um ID estável independente do caminho físico.

Baseline:

```text
namespace:path
```

Exemplos:

```text
core:player
core:hub
desert:scarab
desert:sun_sword
frozen_realm:ice_castle
```

O namespace normalmente corresponde ao ID do mod.

O tipo do recurso é conhecido pelo registry/API que está sendo usado, portanto não é obrigatório codificar o tipo dentro do ID.

Exemplo conceitual:

```text
Model("desert:scarab")
Texture("desert:scarab")
Entity("desert:scarab")
```

Se permitir IDs iguais entre registries diferentes causar ambiguidade real durante implementação, a regra poderá ser refinada.

## Não depender de paths em gameplay

Evitar APIs como:

```text
load_model("../../mods/desert/models/scarab.glb")
```

Preferir:

```text
model = "desert:scarab"
```

O Asset Manager resolve o ID para o recurso correto.

Isso permite:

- mover arquivos internamente;
- cache;
- override controlado;
- packs;
- ferramentas;
- dependências;
- diagnósticos melhores.

## Asset registries

O runtime manterá registries por tipo, por exemplo:

```text
Models
Textures
Materials
Animations
Audio
Maps
Shaders
VFX
Data
```

Cada entrada associa:

- Asset ID;
- origem/mod;
- tipo;
- source path;
- compiled/cache path;
- hash/versão;
- dependências;
- estado de carregamento;
- metadata/import settings.

## Handles

Gameplay e mods não devem manter ponteiros crus para recursos gráficos ou objetos internos.

Usar handles/referências controladas:

```text
TextureHandle
MeshHandle
MaterialHandle
AudioHandle
```

O Asset Manager controla criação, compartilhamento e destruição.

Isso permite descarregar áreas e recursos sem espalhar ownership manual pelo jogo.

## Virtual File System

O jogo deve possuir uma camada de filesystem virtual/mounts.

Conceitualmente:

```text
Core Assets
   +
Installed Mods
   +
Compiled Cache
   ↓
Virtual File System
   ↓
Asset Manager
```

O VFS deve esconder detalhes de onde um arquivo está fisicamente armazenado.

No futuro isso também permitirá mods distribuídos como pastas ou pacotes/arquivos sem alterar as APIs de gameplay.

## Registro explícito de assets

**Definido:** assets públicos de um mod não são registrados automaticamente apenas porque um arquivo foi colocado em uma pasta.

O autor declara explicitamente o Asset ID e seu source.

Exemplo conceitual:

```json
{
  "models": {
    "desert:scarab": {
      "source": "models/scarab.glb"
    }
  }
}
```

Isso evita que nomes/pastas virem API pública por acidente e permite reorganizar arquivos sem quebrar outros mods.

A forma exata do arquivo de declaração ainda será definida; pode ser parte do manifest ou um arquivo de assets separado.

Arquivos internos que pertencem a um asset composto não precisam necessariamente ser registrados como assets públicos independentes.

## Pipeline de descoberta

Ao iniciar:

```text
discover core + mods
↓
read manifests
↓
mount sources
↓
read explicit asset declarations
↓
register asset metadata
↓
compare fingerprints/cache
↓
compile only changed/missing assets
↓
validate dependency graph
↓
mount compiled resources
↓
start runtime
```

## Cache incremental

O jogo não deve recompilar todos os assets em todo startup.

Cada asset deve possuir fingerprint suficiente para detectar mudança.

Fingerprint pode incluir:

- conteúdo/hash do source;
- import settings;
- versão do asset compiler/importer;
- dependências relevantes.

Se nada mudou:

```text
CACHE HIT
→ usa resultado compilado
```

Se algo mudou:

```text
CACHE MISS
→ recompila somente o necessário
```

Se uma dependência muda, assets derivados dela podem ser invalidados.

## Local do cache

O cache compilado deve ficar fora dos arquivos-fonte dos mods.

Conceitualmente:

```text
mods/desert/
    sources...

user/cache/desert/
    compiled...
```

Remover um mod impede seu mount e faz seu conteúdo desaparecer do runtime.

Cache órfão pode ser limpo posteriormente sem afetar o mod original.

## Modelos 3D

Formato-fonte principal atual:

- GLB/GLTF;
- fastgltf para importação.

A geometria mais comum será simples e baseada em cubos/cuboids. O formato GLB/GLTF é escolhido pelo pipeline e não por exigir geometria complexa.

O player base segue a estrutura geométrica/proporcional blocada do player clássico de Minecraft como referência.

### Texturas embutidas

**Definido:** o pipeline de modelos deve trabalhar naturalmente com texturas embutidas dentro de GLB/GLTF.

A intenção principal é que um asset de modelo possa carregar em um único source:

```text
model.glb
├─ meshes
├─ materials
├─ embedded textures
├─ skeleton
└─ animations
```

O autor não deve ser obrigado a manter texturas do modelo em arquivos externos separados.

Para GLTF textual, o importador deverá aceitar imagens realmente embutidas (por exemplo via buffer/data URI) quando o arquivo utilizar essa forma. GLB é o formato mais conveniente para um pacote único.

O compilador poderá extrair:

- vertex/index data;
- submeshes;
- materiais;
- skeleton;
- skinning;
- animações;
- bounds;
- texturas/imagens embutidas.

No cache compilado, essas texturas podem virar sub-recursos independentes para upload, streaming e deduplicação interna, mesmo que para o modder continuem pertencendo ao modelo.

### Sub-recursos do modelo

Direção provisória:

```text
desert:scarab
├─ mesh/body
├─ material/body
├─ texture/albedo
├─ texture/emissive
└─ animation/attack
```

**Definido:** sub-recursos de modelos poderão ser endereçados individualmente por mods avançados.

Exemplos conceituais:

```text
desert:scarab/mesh/body
desert:scarab/material/body
desert:scarab/texture/Body
desert:scarab/animation/attack
```

Isso permite que outro mod altere apenas uma textura, material, mesh ou animação embutida sem substituir o GLB/GLTF inteiro.

O asset principal continua sendo um único modelo do ponto de vista do autor, enquanto o compilador expõe sub-recursos internos quando necessário.

O runtime não precisa parsear todo GLB/GLTF novamente a cada carregamento se existir cache compilado válido.

## Texturas

PNG é um bom formato-fonte para criação/modding.

O pipeline poderá gerar formatos GPU-friendly/cacheados.

Import settings devem conseguir controlar:

- sRGB vs linear;
- point/nearest vs linear filtering;
- mipmaps;
- wrap/clamp;
- normal map;
- compressão;
- alpha.

Para pixel-art, point/nearest é importante, mas não deve ser assumido para absolutamente toda textura.

## Áudio

Fontes iniciais podem incluir:

- WAV;
- OGG.

O pipeline/runtime deve distinguir pelo menos:

- sons curtos carregáveis em memória;
- música/áudio longo que pode usar streaming.

O formato interno exato será decidido quando a etapa de áudio for implementada.

## Metadata/import settings

Assets precisam ser declarados explicitamente e podem receber configuração além do próprio arquivo.

Exemplo conceitual:

```json
{
  "id": "desert:scarab",
  "type": "model",
  "source": "models/scarab.glb",
  "import": {
    "embedded_textures": true,
    "texture_filter": "nearest",
    "mipmaps": true
  }
}
```

Outro asset pode ser uma textura independente:

```json
{
  "id": "desert:ui_icon",
  "type": "texture",
  "source": "textures/ui/icon.png",
  "import": {
    "filter": "nearest",
    "srgb": true
  }
}
```

O formato final pode ser JSON, parte do manifest ou outro arquivo declarativo.

A meta é que o que vira API pública do mod seja explícito, e não consequência acidental da organização de pastas.

## Dependências entre assets

O pipeline deve conhecer relações como:

```text
Material
 ├─ albedo texture
 ├─ normal texture
 └─ shader

Model
 ├─ mesh
 ├─ material
 └─ animations

VFX
 ├─ texture atlas
 ├─ material
 └─ optional light profile
```

Isso permite:

- validar referências;
- recompilar dependentes;
- preload;
- detectar recursos ausentes;
- produzir erros úteis.

## Carregamento e memória

A API deve ser projetada para não exigir que todo conteúdo de todos os mods permaneça na RAM/VRAM.

Objetivo:

```text
Hub assets loaded
Area A loaded
Area B unloaded
unused resources released
```

O Asset Manager deve compartilhar recursos idênticos/referenciados e controlar lifetime centralmente.

## Loading assíncrono

A API deve ser preparada para carregamento assíncrono/background, mas a primeira implementação pode começar de forma simples.

Não devemos complicar o v0 antes de ter assets reais.

Quando necessário:

- I/O;
- decompression;
- parsing;
- CPU conversion

podem acontecer fora da thread principal, enquanto criação/upload final de certos recursos respeita as regras do renderer.

## Conflitos, merge e replace

**Definido:** o sistema seguirá uma política híbrida de **B + C**:

1. substituição destrutiva precisa ser declarada explicitamente;
2. alterações naturalmente combináveis podem ser mescladas quando a semântica for segura e definida.

Exemplos:

```text
append loot entry       → merge possível
add event listener      → merge possível
add component metadata  → merge possível

replace model            → explícito
replace shader           → explícito
replace complete entity  → explícito
```

O loader não deve escolher silenciosamente um vencedor apenas por ordem de arquivos.

Se dois mods fizerem replaces incompatíveis no mesmo alvo, o loader deve detectar o conflito e mostrar diagnóstico claro. A regra final de resolução/prioridade ainda será definida.

## Preparing Mods

**Definido:** quando houver compilação/validação relevante no startup, o jogo mostrará uma tela explícita de preparação de mods em vez de esconder o trabalho.

Exemplo:

```text
Preparing Mods

Desert Expansion
154 / 320 assets

Compiling scarab.glb...
```

A tela deverá conseguir mostrar:

- mod atual;
- etapa atual;
- progresso total;
- assets em processamento;
- warnings;
- erros que impedem determinada parte do mod;
- cache hits/misses em modo detalhado.

Se não houver trabalho relevante, essa etapa pode ser praticamente instantânea.

## Development vs distribution

Durante desenvolvimento, mods em pastas normais são a forma mais simples.

No futuro, o mesmo VFS poderá aceitar um formato empacotado para distribuição sem alterar a API de assets.

O formato de pacote ainda não foi definido.

## Diagnósticos

Erros devem apontar:

- mod;
- Asset ID;
- source file;
- tipo;
- etapa do pipeline;
- dependência ausente;
- possível correção quando conhecida.

Exemplo:

```text
[ASSET204 MissingDependency]
Mod: desert_expansion
Asset: desert:scarab_material
Missing: desert:scarab_albedo
Source: materials/scarab.json
```

## Tooling

O asset pipeline deverá futuramente poder ser executado sem abrir o jogo.

Exemplos conceituais:

```text
game-tool assets validate <mod>
game-tool assets build <mod>
game-tool assets inspect desert:scarab
game-tool assets clean-cache
```

Isso é importante para desenvolvimento por IA.

## Critério de conclusão da etapa 4

O v0 está concluído quando:

```text
Asset ID existe
↓
VFS localiza source
↓
compiler detecta cache/mudança
↓
GLB ou textura é importado
↓
resultado compilado/cacheado é produzido
↓
Asset Manager retorna handle
↓
renderer consegue usar recurso
↓
asset pode ser descarregado
↓
erro de referência gera diagnóstico claro
```

## Decisões ainda em aberto

- formato binário interno exato;
- arquivo/sintaxe exata das declarações explícitas de assets;
- formato final de pacote de mod;
- estratégia exata de compressão;
- prioridade quando dois mods declaram replaces incompatíveis;
- carregamento assíncrono v1 vs etapa posterior;
- formato interno de áudio;
- formato de mapas/scenes.
