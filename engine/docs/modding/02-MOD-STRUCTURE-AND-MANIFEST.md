# 2. Estrutura, manifest, IDs e dependências

## Separação entre manifest e conteúdo

### `mod.json`

Descreve o próprio mod:

- ID;
- nome;
- versão;
- versão de API;
- entrypoint;
- dependências;
- dependências opcionais.

### `assets.json`

Declara assets públicos e suas configurações de importação.

Essa separação evita transformar `mod.json` em um arquivo enorme.

## Estrutura profissional recomendada

```text
my_mod/
├─ mod.json
├─ assets.json
├─ README.md
├─ CHANGELOG.md
├─ scripts/
│  ├─ main.sl
│  ├─ entities/
│  ├─ items/
│  ├─ systems/
│  └─ shared/
├─ models/
│  ├─ enemies/
│  ├─ items/
│  └─ props/
├─ textures/
│  ├─ ui/
│  └─ vfx/
├─ audio/
│  ├─ sfx/
│  └─ music/
├─ maps/
├─ data/
├─ vfx/
└─ licenses/
```

Isso é recomendação, não obrigação rígida.

## IDs

Formato:

```text
namespace:path
```

Exemplos:

```text
core:player
core:hub
desert_expansion:scarab
desert_expansion:weapons/sun_sword
```

O namespace normalmente é o `mod.json.id`.

### Estabilidade

Considere IDs como parte da API pública do seu mod.

Depois que outro mod ou save depende de:

```text
desert_expansion:scarab
```

renomear esse ID pode quebrar compatibilidade.

## Registries

O mesmo texto após o namespace pode existir em registries diferentes quando não houver ambiguidade.

Conceitualmente:

```text
Model("desert_expansion:scarab")
Entity("desert_expansion:scarab")
```

O tipo da API/registry informa o que está sendo procurado.

## Dependências

Uma dependência obrigatória significa:

> Meu mod espera que esse outro mod exista.

Uma dependência opcional significa:

> Se existir, eu posso integrar com ele; se não existir, meu mod ainda deve funcionar.

A política atual do loader para dependência ausente é tentar recuperar quando possível:

- mostrar diagnóstico claro;
- carregar partes independentes;
- falhar isoladamente onde a dependência for realmente necessária;
- evitar derrubar o jogo inteiro.

## Merge vs replace

### Merge

Use quando múltiplos mods podem contribuir de forma segura.

Exemplos:

```text
adicionar entrada de loot
registrar event listener
adicionar metadata
registrar componente
```

### Replace

Use quando você realmente quer substituir algo.

Exemplos:

```text
trocar modelo
trocar shader
substituir definição inteira
```

Replaces devem ser explícitos.

Dois replaces incompatíveis no mesmo alvo devem gerar conflito diagnosticável, não uma escolha silenciosa baseada apenas na ordem dos arquivos.

## Conteúdo do core

O jogo base usa namespace reservado:

```text
core:
```

Exemplos:

```text
core:player
core:hub
```

Mods podem referenciar e, pelas APIs apropriadas, modificar conteúdo do core.

## Load order

A ordem final será derivada principalmente do grafo de dependências e das regras do loader.

Não crie mods que funcionem apenas porque "a pasta X vem alfabeticamente depois da pasta Y".

## Entry point

O entrypoint atual é conceitualmente:

```json
"entry": "scripts/main.sl"
```

A extensão oficial da linguagem ainda não foi fechada; `.sl` é a extensão de trabalho atual.
