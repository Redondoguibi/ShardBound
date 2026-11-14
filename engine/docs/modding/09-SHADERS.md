# Shaders de mods — ABI 1

Mods podem registrar programas HLSL de superfície e pós-processamento em
`assets.json`, aplicá-los a entidades, texturas e blocos, e atualizar parâmetros
por script. O renderer continua responsável por geometria, instancing, iluminação,
sombras, estados de transparência e composição da interface.

Exemplo executável: [shader_lab](../../examples/shaders/shader_lab/).
Na raiz do projeto, após compilar:

```powershell
./build/debug/modcheck.exe examples/shaders --json
./build/debug/game.exe --mods examples/shaders
```

## Registro e compilação

```json
{
  "shaders": {
    "meumod:brilho": {"version": 1, "kind": "surface", "source": "shaders/brilho.hlsl"},
    "meumod:cor": {"version": 1, "kind": "postprocess", "source": "shaders/cor.hlsl"}
  }
}
```

Os IDs novos pertencem ao namespace do mod. `version`, `kind` e `source` são
obrigatórios; campos desconhecidos são rejeitados. Não há substituição implícita
de IDs de outro mod. Para alterar a aparência existente, aplique explicitamente
um programa por uma das APIs abaixo. Para reutilizar shaders de outro mod,
declare a dependência no manifest.

O loader compila HLSL para `ps_5_0` e cria os recursos D3D11 durante a preparação.
Programas ficam compartilhados em memória: alterar parâmetros não recompila HLSL.
Uma falha de compilação não publica o shader. `modcheck` usa o mesmo compilador,
sem precisar criar janela ou dispositivo gráfico, e verifica referências de
shaders nos blocos. Erros carregam código, arquivo, mod, recurso e, quando o
compilador fornece, linha/coluna. Avisos HLSL também impedem a compilação.

Includes com aspas são relativos ao arquivo que os inclui. A resolução pelo VFS
recusa arquivos fora da pasta do mod, inclusive escapes por links/junctions.
Includes `<...>` e includes de outros mods não são aceitos.
Limites por compilação: 256 KiB por arquivo, 2 MiB lidos, 64 aberturas e 16 níveis
adicionais de inclusão. O processo registra até 256 shaders.

## Shader de superfície

O arquivo fornece esta função, sem redeclarar os recursos do engine:

```hlsl
void modSurface(VertexOutput v, inout float4 base,
                inout float3 normal, inout float3 emission) {
    base.rgb *= modParameters[0].rgb;
    emission = modParameters[0].rgb * modParameters[0].w;
}
```

- `base` começa com textura × cor do material × cor do vértice; altere RGBA.
- `normal` está em espaço do mundo, já invertida para a face traseira de um
  material de duas faces no passe de cor. O engine normaliza a saída.
- `emission` começa em zero; soma luz emitida antes da conversão de cor/fog.
- `v.uv`, `v.world`, `v.normal` e `v.color` permitem efeitos por UV e posição.
  `v.world` usa a origem local da cena; mundos infinitos podem recentralizá-la.
- `modParameters[0..3]` são quatro `float4`, preenchidos pelos 16 números do script.
- `modFrame.x` é o tempo da sessão em segundos; `yz` é a resolução interna;
  `w` é reservado. O relógio pausa com o jogo e reinicia em uma nova sessão.
- `modTexture.Sample(modSampler, uv)` amostra a textura adicional opcional.
  Sem textura adicional, a amostra é branca. O filtro vem do registro da textura.

A função também participa do passe de sombras. Recortes por alfa em materiais
`Mask` devem depender de UV/posição do mundo para coincidirem nos dois passes;
`v.position` representa coordenadas da câmera ou do mapa de sombras, conforme o
passe. Retorne uma normal válida e não nula. A função pode usar `clip()`.

O modo de transparência continua sendo o do material: escrever alfa não transforma
um material opaco em translúcido. `Blend` e `Additive` respeitam o alfa produzido,
inclusive com iluminação. Superfícies opacas e recortadas mantêm o pipeline de
profundidade. Materiais sem iluminação usam `base.rgb + emission` diretamente.

### Entidades

No runtime completo, `record()` constrói o objeto de opções:

```text
fx = record()
fx.parameters = [0.2, 0.8, 1.0, 0.15]
set_entity_shader(entity, "meumod:brilho", fx)
info = entity_shader(entity)
set_entity_shader(entity, null)
```

`set_entity_shader(entity, shader_id_or_null, options?)` aplica a todas as partes
do modelo, inclusive modelos animados/Studio. A entidade precisa ter um modelo.
Opções: `parameters` (0..16 floats finitos) e `texture` (ID de textura registrada).
Valores omitidos viram zero; a textura omitida vira branca. Cada chamada substitui
o vínculo inteiro. Um erro mantém o vínculo anterior. Trocar o modelo por
`set_model` preserva o shader da entidade.

`entity_shader(entity)` retorna `null` ou uma cópia com `shader`, `parameters`,
`texture` e `available`. Os campos não expõem objetos D3D11.

Vínculos de entidades são salvos junto ao modelo. Saves antigos continuam válidos.
Se o shader ou o registro de sua textura adicional estiver ausente, o carregamento
registra `SAVE205`, mantém os IDs/parâmetros e desenha com o material padrão.
Salvar novamente preserva o vínculo; reinstalar o recurso e recarregar restaura
o efeito. Parâmetros malformados impedem a troca do mundo vivo.

### Texturas existentes

```text
set_texture_shader("plains:grass", "meumod:brilho", fx)
set_texture_shader("plains:grass", null)
```

A regra afeta materiais que usam exatamente a textura registrada, incluindo
chunks já publicados. Um vínculo explícito de entidade ou bloco tem precedência.
Texturas embutidas em GLB não têm automaticamente IDs próprios: use a API da
entidade nesses modelos.

Só o mod que criou a regra pode atualizá-la/removê-la enquanto ela existir.
Uma tentativa de outro mod gera conflito identificando o proprietário. Até 256
regras podem existir. Elas pertencem à sessão; configure em `on_start` e, para
estado dependente do save, reaplique em `on_loaded` usando seu estado persistente.

### Blocos e snapshots de mundo

No bloco de `world.json`, o campo opcional `shader` segue este formato:

```json
{
  "texture": "meumod:cristal",
  "solid": true,
  "shader": {
    "id": "meumod:brilho",
    "parameters": [0.2, 0.8, 1.0, 0.15],
    "texture": "meumod:ruido"
  }
}
```

`parameters` e `texture` são opcionais. O snapshot do mundo guarda esse vínculo;
chunks novos e reconstruídos usam os mesmos parâmetros. Os recursos GPU são
resolvidos na thread principal, antes do worker de geração. A ausência de um shader
exigido pelo snapshot impede abrir o mundo, como os demais assets obrigatórios.
Blocos existentes sem `shader` preservam sua renderização.

## Pós-processamento

```hlsl
float4 modPost(float2 uv, float4 color) {
    return float4(color.rgb * modParameters[0].rgb, color.a);
}
```

`color` é a amostra do passe anterior. Para filtros de vizinhança, amostre
`baseTexture` com `baseSampler` usando UV deslocada; `1 / modFrame.yz` fornece o
passo em pixels. O sampler da cena é linear com clamp. `modTexture` e parâmetros
funcionam como no shader de superfície.

```text
grade = record()
grade.parameters = [1.05, 1.0, 0.95]
grade.priority = 10
set_post_effect("meumod:grade", "meumod:cor", grade)
set_post_effect("meumod:grade", null)
```

`set_post_effect(pass_id, shader_id_or_null, options?)` cria, atualiza ou remove
um passe. O ID do passe pertence ao mod chamador; o ID do shader pode vir de uma
dependência. Além de `parameters` e `texture`, aceita `priority`, inteiro entre
-10000 e 10000, padrão zero.

Até oito passes ativos são executados por prioridade crescente e, em empate,
por ID lexicográfico. O engine alterna dois alvos, evitando ler e escrever a
mesma textura. A interface é composta depois dos passes. Redimensionamento e
escala de renderização recriam os alvos na resolução correta. A configuração
pertence à sessão e deve ser recriada pelo script; não é salva implicitamente.
F3 mostra a quantidade de passes ativos; C++ `RenderStats.postDraws` conta os
passes efetivamente desenhados.

## Contratos e limites atuais

- ABI 1 oferece hooks de pixel/superfície e pós-processamento. Não permite trocar
  vertex/geometry/compute shaders, deslocar a geometria, criar render passes
  arbitrários nem substituir a arquitetura de iluminação.
- São 16 floats e uma textura auxiliar por vínculo. Não há uniforms nomeados,
  listas dinâmicas de samplers, editor de grafos ou hot reload de HLSL.
- A cena e os passes usam RGBA8; não há HDR nem acesso ao depth buffer para SSAO.
  Não redeclare `Frame`, `ModParameters`, samplers ou texturas do engine.
  A reflexão rejeita bindings usados fora do contrato. O pós-processamento
  não pode consumir o mapa de sombras.
- Compilação é síncrona na preparação, com cache em memória, sem cache persistente
  de bytecode. Reinicie o jogo após editar arquivos HLSL.
- O código HLSL executa na GPU; o contrato de recursos não é um sandbox contra
  programas deliberadamente caros. Custos dependem do shader, resolução e hardware.
- Shaders iguais e parâmetros/texturas iguais continuam usando instancing.
  Parâmetros distintos dividem lotes; evite um efeito exclusivo por objeto quando
  uma regra compartilhada bastar.
- O formato de save continua na versão 1 com campos opcionais. O runtime atual lê
  saves antigos; um executável antigo não preservará campos de shader novos.
