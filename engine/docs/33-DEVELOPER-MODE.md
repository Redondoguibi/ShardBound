# Modo de desenvolvedor — implementação e validação

Validação realizada em 18/10/2026, Windows x64, MSVC, SDL3 e D3D11.

## Funcionalidades

- Page Up + Page Down alterna o modo uma vez por combinação, começando desligado em cada sessão. Sair restaura a câmera normal e descarta comandos ainda não executados.
- Console por `/`, com histórico, ajuda, sugestões tipadas por Tab/Shift+Tab, rolagem, edição UTF-8 e clipboard. Ações de gameplay ficam capturadas enquanto se digita. Escape, foco e interfaces modais têm tratamento próprio.
- Câmera orbital em terceira pessoa, zoom e prevenção de atravessamento por raio; câmera livre com WASD/Space/Q/Shift. Mira e projeções da interface usam a mesma câmera da renderização.
- Streaming visual acompanha a câmera livre e preserva suporte físico sob os atores. As câmeras acompanham as mudanças de origem flutuante.
- Comandos escolhidos: `help`, `list`, `camera`, `pos`, `tp`, `give`, `spawn`, `set`, `undo`, `redo`, `seed`, `attribute`. `/list` sozinho lista os comandos. `/attribute mod:id get` consulta; `/attribute mod:id set valor` modifica.
- Mods podem registrar comandos com argumentos tipados, documentação e sugestões, em seu próprio namespace. Não existe avaliação de scripts pelo texto do console.
- Atributos registrados por mods têm bases persistentes por entidade e usam os bônus de equipamentos/afixos existentes. O mod `foundation` aplica velocidade, salto, vida máxima, multiplicador de dano e bônus de armadura ao gameplay e inicializa seus inimigos criados pelo console.

O contrato, exemplos e sintaxes estão em [Console e atributos](modding/11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md).

## APIs e compatibilidade

Adições de script: `register_command`, `unregister_command`, `register_attribute`, `get_attribute`, `get_base_attribute`, `set_attribute`, `reset_attribute`, `set_maximum_health`, `on_attribute_changed` e `on_developer_spawn`.

As bases usam `core:attributes` versão 1, opcional nos saves; dados de atributos desconhecidos são preservados. Valores inválidos e versões desconhecidas são rejeitados antes da troca do mundo vivo. Câmeras e estado do modo de desenvolvedor não são persistidos. Os saves anteriores e os stats antigos sem override continuam usando seus caminhos existentes.

O renderer aceita uma camada adicional de interface, mantendo os HUDs existentes. `PlayerController::viewCamera()` permite compartilhar a câmera ativa com a ponte de gameplay; `camera()` continua expondo a câmera normal. `ChunkWorld::update` recebe um centro visual opcional. Chamadas anteriores mantêm seu comportamento. O SDK nativo não teve alteração de ABI.

## Verificação executada

| Execução final | Resultado |
|---|---|
| Build e CTest Debug | **44/44**, 120,47 s na suíte, zero avisos MSVC |
| Build e CTest Release | **44/44**, 35,54 s na suíte, zero avisos MSVC |
| Executável Release, mundo novo e retomada | Ambas as execuções com código 0 e sem mensagens de erro |
| UTF-8, espaços finais e diff | Arquivos novos verificados; `git diff --check` passou |

Logs: [Debug](../build/developer-debug-final.log), [Release](../build/developer-release-final.log), [criação real](../build/developer-runtime-create.log), [retomada real](../build/developer-runtime-resume.log). Evidências persistidas e hashes dos fontes: [developer-mode-2026-10-18.json](validation/developer-mode-2026-10-18.json).

O teste `developer` cobre combinação/repetição de teclas, bloqueio do console desligado, listagem e ajuda, quantidades/IDs inválidos, coordenadas relativas e globais, edição transacional/undo/redo/air, limites de região, comandos de mods e exceções, remoção durante o próprio callback, orçamento de instruções, Tab, histórico, UTF-8/emoji/clipboard, captura de controles, interface modal, movimento e colisão de câmeras, projeção de interface, origem flutuante, foco, pausa, persistência, versões de atributos inválidas, overflow sem publicação parcial, cancelamento de comandos pendentes e composição do console sobre HUD legado.

O teste `procedural` também verifica os efeitos reais dos atributos no jogador, inimigos do mod base com corpo/saúde/IA criados pelo console e a retomada desses efeitos após save/load. A suíte completa inclui saves/migrações/recuperação, shaders, Studio, equipamentos, física, scripts, mods e extensões nativas anteriores.

O smoke adicional usou um diretório isolado: criou um mundo no executável, preparou um checkpoint com bases registradas e uma base de mod ausente e reabriu-o. O checkpoint seguinte confirmou velocidade 9, vida máxima 150 e retenção do atributo ausente.

## Problemas encontrados e corrigidos

- O módulo de combate também é importado isoladamente em integrações. Seus novos atributos inicialmente dependiam da inicialização do jogador; passaram a ter um contrato importado pelo próprio módulo. A falha apareceu na primeira suíte completa em ambos os presets e o teste `mod_platform` passou nas duas execuções finais.
- Mira e projeções consultavam a câmera normal mesmo com uma câmera de desenvolvimento ativa. Agora compartilham a câmera de visualização; o teste verifica a projeção do alvo no centro da tela.
- Uma câmera livre precisa de streaming próprio e compensação da origem sem deslocar o jogador. O centro visual foi separado do suporte de física, com validação anterior a qualquer recenter.
- Alterações de atributos podiam publicar uma base antes de descobrir overflow de bônus. O cálculo é validado antes da publicação, e o teste confirma a retenção da base anterior.
- Captura de teclado/mouse, Escape, perda de foco e interfaces modais foram integradas para impedir ações involuntárias e retenção do mouse. A interface adicional mantém o HUD anterior.

## Limites atuais

- O mundo continua simulando com o console aberto. O modo não concede invulnerabilidade.
- Edições mantêm o limite existente de 4096 blocos por transação, altura 0–47 e histórico de 16 operações. Teleporte e câmera livre têm alcance local de 100000 unidades.
- A colisão da câmera usa raio com margem, sem varredura volumétrica. Texto usa pontos de código UTF-8; não foi acrescentado suporte a composição IME ou seleção por grafemas.
- Callbacks de mods obedecem ao orçamento do runtime e têm falhas reportadas, mas efeitos arbitrários de scripts não têm rollback geral. A inicialização de uma entidade pode falhar depois de sua criação; isso aparece explicitamente no console.
- A validação usou janelas ocultas, eventos automatizados e leitura de pixels. Não foi realizado um percurso manual usando teclado e mouse físicos nem validação em outra GPU/plataforma.

Não há falhas pendentes nas execuções finais registradas. As alterações anteriores do projeto foram mantidas, sem commit ou publicação.
