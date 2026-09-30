# Linguagem própria

## Objetivo

Criar uma linguagem específica para o ecossistema do jogo, pensada para ser simples de escrever por humanos e simples de gerar/revisar por agentes de IA.

A implementação do compilador/runtime da linguagem será integrada ao core em C++.

A linguagem será a principal forma de criar lógica de mods, mas não será o único formato aceito: mods também poderão conter C++, JSON, GLB, imagens, áudio, mapas e outros assets.

---

## Estado deste documento

A sintaxe ainda está em fase de desenho.

As regras abaixo são separadas em:

- **Definido** — preferência já escolhida.
- **Provisório** — aceito como direção, mas ainda pode mudar.
- **Em aberto** — ainda não decidido.

---

# Sintaxe atualmente definida

## 1. Categorias com `@`

Declarações podem ser organizadas por categorias explícitas.

Exemplos:

```text
@Items
weapon SunSword:
.damage = 18
.speed = 1.2
```

```text
@Entities
enemy Goblin {
Attributes:
.health = 30

Assets:
.model = "forest:goblin"

AIs:
.Melee
}
```

Outras categorias poderão existir, inclusive categorias criadas por mods.

A intenção é que o sistema de categorias não fique totalmente hardcoded no compilador.

---

## 2. Funções

Funções usam `func`.

```text
func damage_multiplier(damage, multiplier):
return damage * multiplier
```

Não é necessário declarar explicitamente o tipo dos parâmetros na sintaxe básica.

---

## 3. Eventos

Eventos podem ser declarados em uma categoria própria.

```text
@Events
on_hit(target):
target.damage(10)
```

A organização física dos eventos entre arquivos é decisão do autor do mod; a linguagem não deve obrigar todos os eventos a ficarem em um único arquivo.

---

## 4. Variáveis

A linguagem deve ser permissiva e permitir variáveis sem palavras-chave como `var`.

```text
damage = 10
critical = false
```

---

## 5. Acesso a propriedades e métodos

O operador `.` é usado normalmente para acessar propriedades e métodos de objetos.

```text
target.health
target.damage(10)
player.position
```

Dentro de blocos declarativos, propriedades também podem aparecer iniciadas por `.`.

```text
Attributes:
.health = 30
```

---

## 6. Condições

Sintaxe atual:

```text
if target.health < 20:
    target.damage(20)
else:
    target.damage(10)
```

---

## 7. Spawn/criação dinâmica

A forma preferida é uma função explícita.

```text
spawn(Goblin, position)
```

Exemplo:

```text
enemy = spawn(Goblin, player.position)
```

---

## 8. Listas

Listas usam colchetes.

```text
enemies = [Zombie, Skeleton, Goblin]
```

Loops seguem o estilo:

```text
for enemy in enemies:
    enemy.damage(10)
```

---

## 9. Imports

Arquivos são importados explicitamente.

```text
import "combat.sl"
```

A extensão `.sl` é apenas a extensão usada nos exemplos atuais e ainda não é definitiva.

---

## 10. Globais

A linguagem terá uma categoria para valores globais do mod.

```text
@Globals
difficulty = 1.5
boss_defeated = false
```

---

## 11. Constantes

Constantes globais são identificadas por um `.` no início do nome.

```text
@Globals
.MAX_ENEMIES = 50
.BASE_DAMAGE = 10
```

O contexto resolve a diferença entre uma constante global iniciada por `.` e uma propriedade declarativa como `.health`.

---

## 12. Comentários

Comentário de uma linha:

```text
? Este é um comentário inline
```

Comentário de múltiplas linhas:

```text
/?
Este é um comentário
com várias linhas
?/
```

---

## 13. Herança / derivação

A palavra `from` representa derivação de uma definição existente.

```text
@Entities
enemy FireZombie from Zombie {
Attributes:
.fire_resistance = 1.0
}
```

O comportamento exato de herança, cópia, override e composição ainda precisa ser especificado.

---

## 14. Modificação do Core

Mods poderão modificar definições existentes do jogo base.

Exemplo:

```text
@Core/Entities
modify player {
Attributes:
.max_health = 150
}
```

O caminho após `@Core/` identifica a área do core sendo modificada.

Mods avançados também poderão alterar ou substituir sistemas mais profundos, embora isso deva exigir APIs mais avançadas.

---

## 15. Valores reconhecidos pela linguagem

A direção aceita é permitir valores simbólicos em vez de obrigar strings para tudo.

Exemplo:

```text
.rarity = Legendary
.damage_type = Fire
```

`Legendary` e `Fire` podem representar valores registrados pelo core ou por mods.

A especificação de enums/registries ainda está em aberto.

---

# Sintaxe provisória

## Parâmetros opcionais

A ideia foi aceita provisoriamente, mas ainda precisa ser revisitada porque o comportamento não foi discutido em profundidade.

Possível forma:

```text
func damage(target, amount, type = Physical):
    target.damage(amount, type)
```

Chamadas:

```text
damage(enemy, 10)
damage(enemy, 10, Fire)
```

Não tratar esta sintaxe como definitiva ainda.

---

# Estilo de blocos

Os exemplos atuais usam dois formatos:

Declarações lineares:

```text
weapon SunSword:
.damage = 18
.speed = 1.2
```

Estruturas maiores com subseções:

```text
enemy Goblin {
Attributes:
.health = 30

Assets:
.model = "forest:goblin"
}
```

A gramática formal de `:`, `{}` e indentação ainda precisa ser definida. Não assumir regras adicionais além dos exemplos já escolhidos.

---

# Filosofia da linguagem

## Permissiva para escrever

A linguagem deve evitar cerimônia desnecessária:

```text
damage = 10
name = "Sword"
```

em vez de exigir:

```text
int damage = 10
string name = "Sword"
```

## Estruturada para ferramentas

Apesar da sintaxe permissiva, o compilador deve fornecer diagnósticos claros e estruturados sempre que possível.

Ainda está em aberto o grau de rigidez de erros, inferência de tipos e sugestões automáticas.

---

# Integração com mods

A linguagem deve permitir:

- novos itens;
- novas entidades;
- eventos;
- novas AIs;
- novas categorias;
- novos sistemas;
- alterações no Core;
- referências entre arquivos;
- reutilização de funções;
- extensão de conteúdo de outros mods;
- integração com assets e JSON;
- APIs do runtime.

A organização dos arquivos é responsabilidade do autor do mod, dentro das regras mínimas que o manifest/toolchain vier a exigir.

---

# Ainda em aberto

- nome da linguagem;
- extensão oficial;
- gramática formal;
- tipagem;
- inferência de tipos;
- gerenciamento de memória;
- bytecode/VM vs interpretação;
- semântica exata de `from`;
- semântica de `modify`;
- namespaces;
- registries;
- tratamento de erros;
- módulos avançados;
- async/coroutines;
- FFI com C++;
- visibilidade/public/private;
- generics;
- pattern matching;
- operadores customizados;
- sistema de permissões;
- debugger.

---

# Divisão de responsabilidades

```text
C++ → core e extensões nativas
Linguagem própria → lógica principal dos mods
JSON/dados → conteúdo declarativo
Assets → representação visual/sonora
```
