# Especificação da Linguagem v0

Este documento é o **contrato implementável da Etapa 8**.

Se houver conflito entre exemplos antigos e este documento, **este documento vence para a v0**.

A linguagem continuará evoluindo, mas o Codex **não deve bloquear a implementação da v0 esperando novas decisões** sobre os itens definidos aqui.

---

# 1. Filosofia

A linguagem é:

- permissiva;
- simples para humanos;
- simples para IA gerar;
- orientada a modding;
- com tipos inferidos;
- sem necessidade de anotar tipos na sintaxe comum;
- organizada por categorias/registries;
- capaz de declarar conteúdo e executar lógica.

A implementação interna pode usar tipos fortes no runtime/compiler, mas o source v0 não exige declarações como `int`, `float` ou `string`.

---

# 2. Extensão

Para a v0, usar:

```text
.sl
```

O nome definitivo da linguagem ainda pode mudar futuramente, mas **a implementação da Etapa 8 deve usar `.sl`**.

---

# 3. Tipagem v0

## Definido

A v0 usa **tipagem inferida/dinâmica na superfície da linguagem**.

Exemplos:

```text
damage = 10
speed = 1.25
critical = false
name = "Goblin"
target = player
```

O compilador/runtime infere e acompanha o tipo do valor.

Tipos básicos mínimos:

```text
Number
Boolean
String
Symbol
List
Object/EntityRef
Null
Function
```

Números inteiros e decimais podem ser representados internamente de forma distinta se isso ajudar o runtime, mas a sintaxe não precisa exigir tipos explícitos.

## Conversões

A v0 não deve fazer conversões absurdamente implícitas.

Exemplo:

```text
"10" + 5
```

deve gerar erro de tipo em vez de adivinhar automaticamente que `"10"` significa número.

---

# 4. Variáveis

Declaração por atribuição:

```text
damage = 10
critical = false
```

Não usar `var`, `let` ou declaração de tipo na v0.

A primeira atribuição cria a variável no escopo atual.

---

# 5. Globais

```text
@Globals
difficulty = 1.5
boss_defeated = false
```

Globais pertencem ao módulo/mod em que são declarados.

---

# 6. Constantes

Constantes globais começam com `.`:

```text
@Globals
.MAX_ENEMIES = 50
.BASE_DAMAGE = 10
```

Depois de inicializadas, não podem ser reatribuídas.

O parser diferencia isso de propriedades declarativas pelo contexto.

---

# 7. Comentários

Linha única:

```text
? comentário
```

Multilinha:

```text
/?
comentário
em várias linhas
?/
```

---

# 8. Imports

```text
import "combat.sl"
```

Imports usam paths relativos ao módulo/mod atual na v0.

Imports circulares devem gerar diagnóstico claro.

---

# 9. Funções

```text
func damage_multiplier(damage, multiplier):
    return damage * multiplier
```

Parâmetros não exigem tipo explícito.

## Parâmetros com valor padrão

A v0 aceita:

```text
func damage(target, amount, type = Physical):
    target.damage(amount, type)
```

Chamadas:

```text
damage(enemy, 10)
damage(enemy, 10, Fire)
```

Parâmetros com default devem vir depois de parâmetros obrigatórios.

---

# 10. Return

Retorno simples:

```text
return damage
```

Múltiplos valores:

```text
return player.health, player.level
```

Internamente, múltiplos retornos podem ser representados como tuple/lista especial, desde que a semântica permaneça estável.

`return` sem expressão é permitido e retorna `null`.

---

# 11. Condições

```text
if target.health < 20:
    target.damage(20)
else:
    target.damage(10)
```

A v0 deve suportar:

```text
if
else
```

`else if`/equivalente pode ser adicionado depois; não bloqueia a v0.

---

# 12. Listas e loops

Lista:

```text
enemies = [Zombie, Skeleton, Goblin]
```

Loop:

```text
for enemy in enemies:
    enemy.damage(10)
```

A v0 precisa de iteração sobre listas e outras coleções expostas pela API do runtime.

---

# 13. Expressões e operadores

Mínimo v0:

```text
+
-
*
/
%
==
!=
<
<=
>
>=
and
or
not
```

Precedência deve seguir convenções comuns de linguagens modernas.

Parênteses:

```text
damage = (base + bonus) * multiplier
```

---

# 14. Acesso a propriedades e métodos

```text
target.health
target.damage(10)
player.position
```

Acesso usa `.`.

Chamadas usam parênteses.

---

# 15. Funções globais do runtime

Chamadas como:

```text
spawn(Goblin, position)
```

são funções normais expostas pelo runtime.

Também deve ser possível:

```text
enemy = spawn(Goblin, player.position)
```

A lista concreta de funções pertence à Bridge da Etapa 9.

---

# 16. Símbolos

Identificadores não citados podem representar valores simbólicos registrados:

```text
.rarity = Legendary
.damage_type = Fire
```

`Legendary` e `Fire` não são strings.

Eles são símbolos/valores registrados e devem ser resolvidos pelo contexto/registry.

Strings continuam sendo:

```text
"Legendary"
```

e são semanticamente diferentes.

---

# 17. Categorias

Categorias começam com `@`.

Exemplo:

```text
@Items
```

```text
@Entities
```

```text
@Events
```

```text
@Globals
```

Categorias podem ser extensíveis/registradas por mods; o parser não deve hardcodar que somente essas quatro existem.

---

# 18. Declarações simples

Exemplo:

```text
@Items
weapon SunSword:
    .damage = 18
    .speed = 1.2
```

Regra v0:

- `tipo Nome:` inicia uma declaração simples;
- o bloco após `:` usa indentação;
- propriedades declarativas começam com `.`.

---

# 19. Declarações estruturadas

Exemplo:

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

Regra v0:

- `{}` delimita uma definição estruturada;
- subseções usam `Name:`;
- conteúdo de subseções usa indentação;
- subseções são dados estruturados e podem ser entendidas por registries/sistemas.

A linguagem aceita esse formato porque entidades e sistemas complexos precisam de organização visual melhor que uma lista plana.

---

# 20. Regra de blocos

## Definido para v0

Usar:

- `:` + indentação para fluxo e blocos lineares;
- `{}` para definições declarativas estruturadas maiores.

Exemplos com `:`:

```text
func test():
    return 1

if ready:
    spawn(Goblin, position)

for enemy in enemies:
    enemy.damage(1)
```

Exemplo com `{}`:

```text
enemy Goblin {
    Attributes:
        .health = 30
}
```

Tabs ou espaços podem ser lexicamente normalizados, mas mistura inconsistente no mesmo bloco deve gerar warning/erro claro.

---

# 21. `from` — derivação

`from` cria uma **nova definição derivada** sem modificar a original.

Exemplo:

```text
@Entities
enemy FireZombie from Zombie {
    Attributes:
        .fire_resistance = 1.0
}
```

Semântica v0:

1. localizar a definição base `Zombie`;
2. criar uma nova definição `FireZombie`;
3. copiar/herdar a estrutura da base;
4. aplicar apenas as alterações declaradas no filho;
5. manter `Zombie` intacto.

## Merge

Objetos/seções estruturadas são mesclados recursivamente.

Exemplo:

Base:

```text
Attributes:
    .health = 30
    .speed = 1.0
```

Filho:

```text
Attributes:
    .health = 50
```

Resultado:

```text
Attributes:
    .health = 50
    .speed = 1.0
```

Valores escalares declarados pelo filho substituem o valor herdado.

Listas, registries especiais e coleções podem fornecer semântica específica no futuro. Na ausência de regra específica, uma atribuição de lista substitui a lista herdada.

---

# 22. `modify` — patch

`modify` altera uma definição já existente.

Exemplo:

```text
@Core/Entities
modify player {
    Attributes:
        .max_health = 150
}
```

Semântica v0:

1. localizar o alvo;
2. manter todos os campos não mencionados;
3. aplicar somente os campos fornecidos;
4. fazer merge recursivo de seções estruturadas;
5. substituir valores escalares explicitamente atribuídos.

Exemplo:

Antes:

```text
Attributes:
    .max_health = 100
    .speed = 1.0
```

Patch:

```text
Attributes:
    .max_health = 150
```

Depois:

```text
Attributes:
    .max_health = 150
    .speed = 1.0
```

## Diferença entre `from` e `modify`

```text
from
→ cria algo novo baseado em algo existente

modify
→ altera o alvo existente
```

---

# 23. Conflitos de `modify`

Patches em campos diferentes podem coexistir.

Exemplo:

```text
Mod A → muda max_health
Mod B → muda speed
```

podem ser mesclados.

Se dois mods não relacionados tentarem escrever valores incompatíveis no mesmo campo:

```text
Mod A → max_health = 150
Mod B → max_health = 200
```

o loader/compiler deve gerar um **diagnóstico de conflito explícito**.

A v0 não deve resolver silenciosamente esse caso apenas por ordem alfabética.

Dependências explícitas podem permitir uma ordem intencional; políticas avançadas de prioridade/override pertencem ao sistema de mods.

---

# 24. `@Core/... `

Categorias de core usam path:

```text
@Core/Entities
@Core/Items
```

Isso seleciona um registry/área do conteúdo oficial.

A sintaxe não implica acesso irrestrito a memória interna; ela opera através dos registries/APIs do runtime.

---

# 25. Escopo

Regras v0:

- variável criada dentro de função é local à função;
- parâmetro pertence ao escopo da função;
- variável criada dentro de bloco `if`/`for` pertence ao escopo lexical daquele bloco;
- `@Globals` cria bindings de módulo;
- constantes globais são read-only após inicialização;
- imports não devem despejar símbolos privados arbitrariamente no escopo sem controle.

A implementação pode começar simples, mas deve preservar escopo lexical.

---

# 26. Null

Valor nulo:

```text
null
```

APIs que podem não encontrar um recurso/entidade podem retornar `null`.

Exemplo:

```text
target = get_entity("boss")

if target != null:
    target.damage(10)
```

---

# 27. Booleanos

```text
true
false
```

---

# 28. Diagnósticos

A linguagem deve priorizar erros úteis.

Exemplo:

```text
[SCRIPT102 UnknownSymbol]
File: scripts/combat.sl
Line: 18
Symbol: Firee
Suggestion: Fire
```

Erros devem incluir sempre que possível:

- arquivo;
- linha/coluna;
- código do diagnóstico;
- mensagem;
- símbolo/asset envolvido;
- sugestão.

---

# 29. AST mínimo da Etapa 8

O parser deve ser capaz de representar pelo menos:

```text
Module
Import
Category
Declaration
StructuredDeclaration
Section
Property
Assignment
Constant
Function
Parameter
Return
If
For
ListLiteral
Call
MemberAccess
BinaryExpression
UnaryExpression
Literal
Symbol
FromClause
ModifyDeclaration
```

---

# 30. O que NÃO bloqueia a v0

Não esperar decisões sobre:

- generics;
- classes tradicionais;
- pattern matching;
- async/coroutines;
- operator overloading;
- macros;
- FFI completo;
- public/private avançado;
- exceptions complexas;
- metaprogramação.

Esses recursos podem entrar depois.

---

# 31. EBNF orientativa v0

Esta gramática é orientativa; o parser pode organizá-la internamente de outra forma desde que aceite a sintaxe definida.

```text
module          = { import_stmt | category_block | function_decl | statement } ;

import_stmt     = "import" string ;

category_block  = category_header { category_item } ;
category_header = "@" identifier { "/" identifier } ;

category_item   = declaration
                | structured_decl
                | modify_decl
                | assignment ;

declaration     = identifier identifier [ from_clause ] ":" indented_block ;
structured_decl = identifier identifier [ from_clause ] "{" { section | statement } "}" ;
from_clause     = "from" reference ;

modify_decl     = "modify" reference "{" { section | statement } "}" ;

section         = identifier ":" indented_block ;

function_decl   = "func" identifier "(" [ parameters ] ")" ":" indented_block ;
parameters      = parameter { "," parameter } ;
parameter       = identifier [ "=" expression ] ;

statement       = assignment
                | if_stmt
                | for_stmt
                | return_stmt
                | expression ;

assignment      = target "=" expression ;
return_stmt     = "return" [ expression { "," expression } ] ;

if_stmt         = "if" expression ":" indented_block
                  [ "else" ":" indented_block ] ;

for_stmt        = "for" identifier "in" expression ":" indented_block ;

expression      = logical_or ;
list_literal    = "[" [ expression { "," expression } ] "]" ;
call            = expression "(" [ expression { "," expression } ] ")" ;
member          = expression "." identifier ;
```

Lexer/parser deve também reconhecer propriedades declarativas iniciadas por `.` dentro dos contextos apropriados.

---

# 32. Critério de conclusão da Etapa 8

A Etapa 8 está concluída quando o runtime consegue:

```text
ler .sl
↓
tokenizar
↓
parsear
↓
produzir AST
↓
resolver imports
↓
resolver escopos/símbolos
↓
executar variáveis/funções/if/for/listas
↓
processar categorias
↓
processar from
↓
processar modify
↓
emitir diagnósticos úteis
↓
chamar funções stub da futura Bridge
```

A Etapa 9 conecta essas chamadas ao runtime real.
