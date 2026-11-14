# Revisão de integração — 2026-10-19

## Escopo e resultado da revisão

Revisão dos fluxos de controles/configurações, troca e fechamento de mundos, geração congelada e perfis de mods, apoio físico e relação com a locomoção. Foram encontrados cinco bugs de severidade média, reproduzidos em testes antes das correções. Não é uma auditoria completa de todos os módulos do repositório.

## Bugs corrigidos

| Bug | Localização da correção | Disparo e impacto | Evidência |
| --- | --- | --- | --- |
| Remapear teclado/mouse removia o botão do gamepad | `src/game/FrontEnd.cpp`, captura de bindings | Trocar Atacar para R substituía toda a lista, inclusive o botão West. O problema persistia depois de reiniciar. | `gamepad remapping`: botão e tecla funcionam; configuração é salva e recarregada. |
| Restrição de movimento vazava entre mundos | `src/core/Application.cpp`, `clearSession` | `keyboard_movement_only(true)` de um mod continuava ativo após descarregar o mundo, bloqueando o analógico em sessões seguintes. | `keyboard session isolation`: o eixo é bloqueado durante o mundo e liberado ao fechar. |
| Ambiente visual vazava entre mundos | `src/core/Application.cpp`, `clearSession` | Céu, luz ambiente e neblina continuavam configurados pelo mundo anterior, mesmo depois de descarregá-lo. | `environment session isolation`: compara pixels reais do fundo após o fechamento com o ambiente padrão. |
| Falha na geração atual invalidava a seleção de um mundo congelado | `src/game/FrontEnd.cpp`, `refreshProfile` e aplicação de perfil | Um `world.json` alterado e inválido limpava a seleção interna; aplicar o perfil acusava falsamente a ausência de um pacote selecionado. | `frozen world profile`: bloqueia criar mundo novo, aplica o perfil com backup e reabre o mundo antigo com sua definição preservada. |
| Apoio de corpo girado usava a área sem rotação | `src/physics/PhysicsWorld.cpp`, `grounded` | Raios lançados a partir das meias-extensões originais podiam atingir uma plataforma fora do corpo e permitir apoio/salto no ar; também podiam deixar de detectar suporte real. | `rotated collider support`: rejeita plataforma ao lado e reconhece uma sob a extremidade girada. |

Confiança alta nos cinco achados: cada condição falhou no teste antes da correção correspondente. Os quatro primeiros estão em `build/readiness-review-before.log`; o quinto, em `build/readiness-review-rotation-before.log`.

## Contratos preservados

O remapeamento mantém os botões de gamepad existentes na ação, sem trocar a versão ou o formato das configurações. As opções persistentes de volume, sensibilidade, escala e bindings continuam separadas do estado temporário de um mundo.

O descarregamento restaura ambiente, luzes pontuais e modo de movimento; mantém a limpeza de shaders já existente. Cada mod do próximo mundo pode configurar novamente a própria apresentação.

A seleção de pacotes é calculada independentemente da preparação de novos geradores. Aplicar um perfil usa diretamente os manifestos selecionados e mantém validação de dependências, compatibilidade, recursos necessários ao terreno e backup prévio. Uma geração atual inválida continua impedindo criar mundos novos; a definição congelada de um mundo existente permanece autoritativa.

A consulta de chão usa o volume físico orientado, com distância de 0,06 unidade e limiar vertical de normal maior que 0,6. Inicia 0,02 acima para evitar a degeneração de contatos exatamente coincidentes. Paredes são descartadas antes de escolher o contato mais próximo. A assinatura pública e os formatos dos saves não mudaram.

## Cobertura

O novo alvo `readiness_review` executa cinco casos independentes e informa cada falha, mesmo quando outro caso já falhou. Usa a mesma `Application`, menu, configurações, entrada, física e renderer do executável. Todos os arquivos e mundos de teste ficam em diretórios próprios dentro do build.

O teste de controle injeta os eventos no `InputSystem`; não equivale a um ensaio com gamepad físico. A revisão de ambiente usa readback do D3D11. Os testes existentes de movimento continuam cobrindo salto contra paredes, três passos de simulação, degraus, rampas, colisão contínua e compatibilidade do gameplay antigo.

Não foram alterados os assets de conteúdo, a ABI nativa ou as alterações anteriores de Blockbench. Não houve teste em Windows limpo, outras GPUs físicas, sessão manual prolongada ou publicação da CI. A proteção por backup e a definição congelada existentes permitiram corrigir a aplicação de perfis sem migrar ou regenerar saves.

## Validação executada

| Verificação | Resultado |
| --- | --- |
| Build MSVC Release e suíte completa | 47/47; 42,48 s de testes |
| Build MSVC Debug e suíte completa | 47/47; 107,61 s de testes |
| Regressões finais direcionadas Release | readiness, readiness_review, movement e gameplay: 4/4 |
| WARP | readiness_review e movement: 2/2; 3,11 s |
| Pacote atualizado com PATH restrito | Validador, criação e retomada: saída 0 |
| Execuções do pacote na Intel UHD Graphics 630 | 119 quadros de gameplay e 116 amostras de GPU em cada processo |
| ZIP extraído | Hashes SHA-256 aprovados |
| Compilador e whitespace | Nenhum warning/erro do compilador nos builds finais; diff conferido com CRLF permitido |

O pacote atualizado é `dist/ShardBound-win64-2026-10-19-review1.zip`; o ZIP anterior foi preservado. Evidências e hashes estão em [validation/player-readiness-review-2026-10-19.json](validation/player-readiness-review-2026-10-19.json). Os resultados valem para os testes e caminhos cobertos; não demonstram ausência de bugs em todo o jogo.
