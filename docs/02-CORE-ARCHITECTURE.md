# Arquitetura conceitual do core

Este documento descreve uma arquitetura inicial, não uma especificação final.

## Camadas propostas

```text
Platform Layer
   ↓
Core Runtime
   ↓
World / Entity Runtime
   ↓
Mod Runtime + Language VM
   ↓
Game Systems
   ↓
Loaded Mods
```

## Platform Layer

Responsável por integrar o jogo ao sistema operacional: janela, input, filesystem, threads, timers e carregamento de módulos nativos.

## Rendering

O projeto controla sua abstração, pipeline e recursos mesmo usando APIs/bibliotecas de baixo nível.

Responsabilidades: meshes, materiais, texturas, câmera, luzes, sombras, partículas, pós-processamento e UI.

A API gráfica ainda não foi definida.

## Physics / Collision

Pode utilizar biblioteca dedicada.

Responsabilidades: colisões, rigid bodies quando necessário, triggers, raycasts, movimentação de personagens e queries espaciais.

## Asset System

Todo recurso deve possuir identidade estável.

Exemplo conceitual:

```text
core:player
core:hub
desert:scarab
desert:sun_sword
ice:citadel
```

O formato `namespace:id` é apenas uma direção sugerida.

Mods devem poder registrar, referenciar, sobrescrever quando permitido e estender recursos.

## Entity / Object Model

Ainda não foi decidido se a implementação será ECS, componentes, objetos tradicionais ou híbrida.

Requisitos:

- extensibilidade;
- hooks/eventos;
- dados serializáveis;
- IDs estáveis;
- possibilidade de adicionar comportamento sem editar o core.

## Game Systems

Combate, inventário, equipamentos, atributos, habilidades, loot, quests, IA, interação e câmera devem ser modulares.

Quanto mais alto o nível do sistema, mais fácil deve ser alterá-lo por mods.

## Mod Runtime

Responsável por descobrir mods, validar manifests, resolver dependências, registrar assets, compilar/carregar scripts, inicializar módulos e expor APIs.

## Native Extensions

Mods avançados poderão conter C++.

Direção possível:

```text
Normal mod
  → linguagem própria + assets

Native mod
  → linguagem própria + assets + módulo C++
```

A política exata ainda está em aberto.
