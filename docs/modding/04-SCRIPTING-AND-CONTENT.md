# 4. Scripting e conteúdo

## Estado da linguagem

A linguagem própria ainda está sendo implementada. Este documento registra a sintaxe definida até agora para que autores possam estruturar conteúdo.

A especificação normativa está em [../18-LANGUAGE-V0-SPEC.md](../18-LANGUAGE-V0-SPEC.md).
As assinaturas executáveis de gameplay e seus limites estão em
[../19-GAMEPLAY-V0-API.md](../19-GAMEPLAY-V0-API.md). Exemplos conceituais abaixo
não substituem esses contratos; por exemplo, o runtime atual usa `damage(target, 10)`.

## Entidades

Exemplo:

```text
@Entities
enemy Goblin {
Attributes:
.health = 30

Assets:
.model = "my_mod:goblin"

AIs:
.Melee
}
```

## Itens

Exemplo:

```text
@Items
weapon SunSword:
.damage = 18
.speed = 1.2
```

## Eventos

```text
@Events
on_hit(target):
target.damage(10)
```

A localização física dos eventos nos arquivos é decisão do autor do mod.

## Funções

```text
func damage_multiplier(damage, multiplier):
return damage * multiplier
```

## Variáveis

```text
damage = 10
critical = false
```

## Globais

```text
@Globals
difficulty = 1.5
boss_defeated = false
```

## Constantes

Constantes globais começam com ponto:

```text
@Globals
.MAX_ENEMIES = 50
.BASE_DAMAGE = 10
```

## Condições

```text
if target.health < 20:
    target.damage(20)
else:
    target.damage(10)
```

## Listas e loops

```text
enemies = [Zombie, Skeleton, Goblin]

for enemy in enemies:
    enemy.damage(10)
```

## Spawn

```text
spawn(Goblin, position)
```

ou:

```text
enemy = spawn(Goblin, player.position)
```

## Imports

```text
import "combat.sl"
```

## Herança/derivação

```text
@Entities
enemy FireZombie from Zombie {
Attributes:
.fire_resistance = 1.0
}
```

A semântica de `from` está definida na especificação normativa v0: cópia profunda
com merge recursivo das estruturas e substituição de escalares/listas.

## Modificando o core

```text
@Core/Entities
modify player {
Attributes:
.max_health = 150
}
```

Mods avançados poderão modificar sistemas profundos, mas APIs públicas e hooks devem ser preferidos a dependências de implementação.

## Valores simbólicos

```text
.rarity = Legendary
.damage_type = Fire
```

A linguagem permitirá valores registrados pelo core ou por mods sem exigir strings para tudo.

## Comentários

Linha única:

```text
? comentário
```

Múltiplas linhas:

```text
/?
comentário
de várias linhas
?/
```

## Categorias novas

Mods não ficam limitados para sempre a `@Items` e `@Entities`.

A arquitetura prevê registries/categorias extensíveis para sistemas novos.

## Organização profissional de scripts

Prefira separar por responsabilidade:

```text
scripts/
├─ main.sl
├─ entities/
│  ├─ scarab.sl
│  └─ boss.sl
├─ items/
│  └─ weapons.sl
├─ systems/
│  └─ desert_heat.sl
└─ shared/
   └─ math.sl
```

Evite arquivos gigantes que registram todo o mod.

## APIs do runtime

A direção planejada inclui APIs equivalentes a:

```text
spawn(...)
destroy(...)
damage(...)
play_sound(...)
get_entity(...)
add_component(...)
remove_component(...)
register_item(...)
register_entity(...)
register_area(...)
```

A assinatura final dessas APIs deve ser consultada na documentação da versão do SDK quando o runtime estiver implementado.
