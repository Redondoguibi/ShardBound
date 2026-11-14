# Correções da auditoria de integração

Data: 2026-10-18. Escopo: os quatro bugs reproduzidos na revisão posterior ao modo de desenvolvedor.

## Salvamento assíncrono

Uma falha ao aguardar o autosave podia deixar a aplicação com uma conclusão pendente. Uma consulta posterior, sem a operação original, interpretava a ausência de trabalho como sucesso, disparava `on_saved` e apagava o aviso de erro.

`SaveGame::requestSave` agora devolve um ticket com o resultado daquela operação. A aplicação guarda esse ticket e usa o mesmo caminho de conclusão no polling, no salvamento manual e no encerramento. A falha encerra a pendência antes de ser propagada e não emite sucesso. Mesmo que uma API de script já tenha aguardado a operação, o resultado original continua disponível; observar um ticket antigo não consome um salvamento mais recente.

As chamadas existentes que ignoram o retorno de `requestSave` ou usam `finishSave(wait)` continuam válidas. O formato dos saves não mudou. O contrato está em [saves e recuperação](modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md).

## Equipamentos mistos

`equip_slot` contava apenas slots de itens empilhados. Com instâncias equipadas, aceitava um 33º slot que a validação de persistência posteriormente rejeitava.

A operação agora conta os slots dos dois modelos, depois dos hooks e antes de publicar os dados. Rejeitar o excesso preserva o equipamento anterior. Substituir um slot existente no limite continua permitido; liberar um slot de instância permite equipar um item empilhado.

## Defaults de atributos definidos por mods

O mod base capturava velocidade, salto e vida máxima antes das alterações dos mods dependentes. Na criação do jogador, esses defaults antigos sobrescreviam os valores finais de `Entities/foundation:Actor`.

O campo `default` de um atributo aceita uma referência `{category,id,field}` a um campo numérico de definição. A consulta resolve o valor atual e valida seus limites. O mod base utiliza essa referência para os três atributos. Defaults numéricos explícitos continuam disponíveis e bases salvas por entidade continuam tendo prioridade. Não há resolução recursiva de referências. Ver [console e atributos](modding/11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md).

## Escape e interfaces

Uma interface visível, mesmo sendo um HUD não modal composto apenas por texto, interceptava Escape e impedia a pausa.

A captura agora depende de uma janela modal, um widget interativo com foco ou uma operação de arrastar. HUDs sem foco permitem pausar e não recebem um cancelamento indevido. Escape cancela a edição com foco em uma interface não modal e libera esse foco; o próximo Escape pode pausar. Janelas modais continuam recebendo cancelamento. Arrastar pode ser cancelado sem deixar a captura do ponteiro retida.

## Regressões automatizadas

O novo teste `integration_regression` utiliza a própria `Application`, compartilhada com o executável pela biblioteca `game_application`. O teste cria um mundo e mods isolados, usa uma janela oculta e verifica:

- Alterações de um mod dependente em velocidade, salto e vida máxima; substituição por default numérico; prioridade de bases salvas após fechar e reabrir o mundo.
- Equipamentos mistos no limite, rejeição sem publicação parcial, substituição de slot, liberação de espaço e salvamento do resultado.
- Falha real de gravação, conclusão manual e por outra API, ticket antigo junto a uma operação nova, nova tentativa e entrega única do evento de sucesso.
- HUD persistente, pausa, janela modal, campo não modal com foco e segundo Escape após cancelar a edição.

A falha de gravação é injetada colocando um diretório no lugar do checkpoint dentro da pasta exclusiva do teste. Saves do jogador não são utilizados.

## Resultado executado

| Validação final | Resultado |
|---|---|
| Compilação e CTest Debug | **45/45**, 97,73 s na suíte; zero avisos MSVC |
| Compilação e CTest Release | **45/45**, 59,59 s na suíte; zero avisos MSVC |
| Nova regressão de integração | Passou nos dois presets: Debug 2,63 s; Release 2,26 s |

As suítes completas também exercitam mods, geração, streaming, física, equipamentos, shaders, Studio, scripts, persistência e extensões nativas. Logs finais: [Debug](../build/four-fixes-debug-final.log) e [Release](../build/four-fixes-release-final.log). Evidências e hashes dos arquivos: [integration-bugfixes-2026-10-18.json](validation/integration-bugfixes-2026-10-18.json).

Uma execução inicial do teste novo falhou na contagem de cancelamentos: o teste não havia inscrito seu callback em `on_ui`. A inscrição foi corrigida, a regressão foi repetida e ambas as suítes completas finais passaram. Não há falhas pendentes nessas execuções.

## Limites da validação

A cobertura automatizada não demonstra ausência de todos os bugs. Esta rodada não inclui uma sessão manual com teclado e mouse físicos nem validação em outra GPU ou plataforma. Os limites existentes de 32 slots e dos valores dos atributos foram preservados. As demais alterações atuais do projeto foram mantidas, sem commit ou publicação.
