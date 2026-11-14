# Carregamento e dependências

## Momento de carregamento

Inicialmente, mods serão resolvidos durante a inicialização do jogo.

Hot reload não é prioridade.

## Dependências

Mods poderão depender de outros mods.

Exemplo conceitual:

```json
{
  "dependencies": [
    "core_api >= 1.0",
    "magic_system >= 2.1"
  ]
}
```

## Dependência ausente

Decisão atual:

> O jogo deve avisar claramente quando uma dependência estiver ausente, mas não bloquear automaticamente o carregamento do mod inteiro.

Isso exige carregamento tolerante a falhas.

Possível fluxo:

1. detectar dependência ausente;
2. emitir diagnóstico claro;
3. tentar carregar partes válidas;
4. referências indisponíveis viram erros controlados;
5. impedir crash do runtime sempre que possível.

## Grafo de dependências

O loader deve construir um grafo para:

- ordenar inicialização;
- detectar ciclos;
- identificar versões incompatíveis;
- descobrir dependências opcionais;
- produzir mensagens compreensíveis.

## Conflitos

Dois mods podem tentar alterar o mesmo recurso.

Precisaremos definir:

- prioridade;
- load order;
- replace;
- extend;
- patch;
- merge;
- erro de conflito.

O comportamento deve ser explícito, não silencioso.

## Diagnósticos

Objetivo:

```text
[ModLoader]
Mod: desert_expansion
Missing optional dependency: weather_plus >= 1.2

Loaded with reduced functionality:
- sandstorm event disabled
- desert region loaded
- items loaded
- enemies loaded
```

em vez de um erro genérico sem contexto.

Isso ajuda tanto humanos quanto IAs.
