# Linguagem própria

## Objetivo

Criar uma linguagem específica para o ecossistema do jogo.

Ela deve ser simples o bastante para criação rápida de mods, mas poderosa o suficiente para lógica real. O compilador/runtime poderá ser implementado em C++.

## Público

A linguagem deve equilibrar dois usuários:

1. humanos;
2. agentes de IA.

Isso significa evitar tanto uma DSL limitada demais quanto uma linguagem desnecessariamente complexa.

## Requisitos desejados

- sintaxe previsível;
- erros claros;
- tipagem que ajude ferramentas e IA;
- módulos;
- funções;
- estruturas/tipos;
- reutilização de código;
- importação entre arquivos;
- namespaces;
- eventos/hooks;
- integração direta com APIs do jogo;
- referências seguras a assets;
- interoperabilidade com JSON/dados;
- suporte a dependências entre mods.

## Exemplo conceitual

A sintaxe abaixo **não está decidida**.

```text
enemy ScarabGuard {
    health: 120
    model: asset("desert:scarab_guard")

    on_spawn() {
        add_component(MeleeCombat)
    }

    on_death(ctx) {
        drop("desert:scarab_shell", chance: 0.35)
    }
}
```

Também deve existir lógica geral:

```text
fn calculate_damage(base, multiplier) {
    return base * multiplier
}
```

## O que evitar

- exigir C++ para conteúdo comum;
- APIs enormes para ações simples;
- comportamento implícito demais;
- sintaxe difícil de gerar;
- mensagens de erro genéricas;
- dependência excessiva de arquivos mágicos.

## Compilada ou interpretada?

Ainda em aberto.

Opções:

- interpretação direta;
- bytecode + VM;
- representação intermediária própria;
- transpiler;
- modelo híbrido.

Uma VM/bytecode próprio é uma direção interessante porque permitiria controle, validação e APIs estáveis sem recompilar o jogo para cada script.

## Divisão de responsabilidades

```text
C++ → core e extensões nativas
Linguagem própria → lógica principal dos mods
JSON/dados → conteúdo declarativo
Assets → representação visual/sonora
```
