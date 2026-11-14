# Colisão lateral durante saltos

Data: 2026-10-19.

## Bug reproduzido

Manter o movimento contra um bloco enquanto o jogador estava no ar podia interromper o salto e deixá-lo suspenso. O teste de regressão, executado antes da correção, falhou com `Holding movement into a wall left the player suspended`.

O controlador reaplica a velocidade horizontal a cada passo da física. Contra uma parede, isso produz um impulso normal continuamente. O atrito entre os corpos transformava parte desse impulso em sustentação vertical suficiente para contrabalançar a gravidade.

## Correção

`PlayerController::move` utiliza `PhysicsSystem::setMovementVelocity`, que identifica o corpo como dirigido por movimento durante o próximo passo de simulação. Um listener de contatos do Jolt remove o atrito apenas nos contatos laterais desses corpos, tanto em contatos novos quanto persistentes. A colisão continua bloqueando a travessia da parede, enquanto o jogador pode subir, cair e deslizar ao longo dela.

O critério lateral usa a mesma separação de inclinação do teste de suporte (`abs(normal.y) < 0.6`). O atrito de chão e teto continua normal. Outros corpos e chamadas comuns de `setVelocity` mantêm seu comportamento. A identificação de movimento é descartada depois do passo completo, incluindo todos os subpassos, e ao destruir o corpo; sua capacidade de armazenamento é reutilizada entre passos.

Não foram alterados os formatos dos saves, as APIs de script ou o SDK nativo. O estado transitório não precisa ser persistido nem restaurado ao trocar de jogador ou carregar um mundo.

## Cobertura de regressão

O teste `movement` agora cobre:

- Salto contra paredes nas quatro direções, em passos de 1/30, 1/60 e 1/120 s: subida, queda, aterrissagem, bloqueio da parede, ausência de suporte falso e possibilidade de saltar novamente.
- Queda contra a lateral de um bloco de uma unidade, com as dimensões do corpo e a gravidade usadas pelo mod base.
- Movimento diagonal ao longo de uma parede, aterrissagem e afastamento da parede.
- Preservação do atrito de corpos sem movimento controlado, do atrito do chão e da restauração do comportamento normal em contatos persistentes depois de encerrar o movimento controlado.

## Validação executada

| Execução | Resultado |
|---|---|
| Reprodução antes da correção, Release | Falha esperada no teste `movement`: jogador suspenso contra a parede |
| Suíte direcionada após a correção, Release | **5/5**: movimento, simulação, procedural, desenvolvedor e regressões de integração |
| Compilação e suíte completa Debug | **45/45**, 127,17 s na suíte; zero avisos MSVC |
| Compilação e suíte completa Release | **45/45**, 49,93 s na suíte; zero avisos MSVC |

Logs: [reprodução](../build/wall-jump-before.log), [testes direcionados](../build/wall-jump-release-focused.log), [Debug](../build/wall-jump-debug-final.log) e [Release](../build/wall-jump-release-final.log). Evidências e hashes dos fontes: [wall-jump-2026-10-19.json](validation/wall-jump-2026-10-19.json). Não há falhas pendentes nessas execuções. As alterações anteriores do projeto foram preservadas, sem commit ou publicação.

## Limites

Esta correção trata o atrito lateral do movimento controlado; não acrescenta subida automática de degraus ou uma nova forma de corpo. A validação automatizada não substitui uma sessão manual com teclado e mouse físicos.
