# Etapa 5 — ECS e sistema de entidades

## Objetivo

Criar a camada de entidades do runtime usando EnTT internamente, sem expor EnTT diretamente ao gameplay ou aos mods.

## Regra principal

```text
Gameplay / Mods
      ↓
Entity API própria
      ↓
EnTT
```

## Entidade

Uma entidade é apenas um identificador estável de runtime associado a componentes.

Exemplos:

- player;
- inimigo;
- NPC;
- projétil;
- item dropado;
- objeto interativo;
- efeito temporário.

## Componentes mínimos v0

Começar apenas com:

- TransformComponent;
- Name/IDComponent;
- ModelComponent;
- ActiveComponent.

Logo depois:

- PhysicsComponent;
- HealthComponent;
- AnimationComponent;
- AudioEmitterComponent.

## Transform

Todo objeto espacial usa Transform:

```text
position
rotation
scale
```

O sistema deve suportar hierarquia parent/child para casos como:

- arma na mão;
- acessórios;
- VFX presos a entidades;
- partes compostas.

## API pública

Exemplos conceituais:

```text
entity = create_entity()
destroy_entity(entity)

entity.add(Transform)
entity.get(Health)
entity.has(Model)
entity.remove(AudioEmitter)
```

A sintaxe final da linguagem própria poderá adaptar essa API.

## Systems

Comportamento não deve ficar concentrado em uma mega-classe Entity.

Exemplos:

```text
TransformSystem
RenderSystem
AnimationSystem
PhysicsSystem
HealthSystem
```

## IDs

Distinguir:

- Asset/Definition ID: estável, como `desert:scarab`;
- Runtime Entity ID: uma instância específica daquele conteúdo.

Exemplo:

```text
Definition: desert:scarab
Runtime entity: #4812
```

## Critério de conclusão

O v0 está pronto quando for possível criar uma entidade, adicionar Transform + Model, mover a entidade e fazer o renderer desenhá-la.
