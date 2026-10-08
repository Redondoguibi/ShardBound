# Qualidade da experiência jogável — revisão de apresentação

Direção solicitada pelo autor em 2026-10-07: elevar a experiência do protótipo
para uma base que possa evoluir até uma publicação comercial na Steam.
Não representa conclusão do jogo ou autorização de publicação.

Decisões de 22 continuam vigentes: WASD, mundo aberto, crítico de queda 1,7×,
waystones gratuitas após 6s sem dano, morte com perda de 15% do ouro, sem
campanha/lore/final obrigatórios. Assets e interface próprios.

## Entrega desta revisão

- Renderização: luz de sol quente, preenchimento frio, correção de saída de cor,
  névoa e sombras direcionais filtradas. Parâmetros de ambiente expostos ao
  conteúdo; iluminação anterior preservada para pacotes sem configuração.
- Desempenho: agrupar instâncias de meshes/materiais e descartar geometria fora
  da câmera; medir draws/instâncias e verificar pixels reais.
- Mundo: abrigo com telhados, vigas, alicerces, cercas e praça; caminhos,
  vegetação baixa, pedras e bordas de terreno na floresta. Decoração separada
  de colisão, locais de encontros/waystones reconhecíveis, passagem contínua.
- Interface: canvas transparente, barra de vida, identificação de região,
  equipamento, avisos breves, inventário e mapa com hierarquia visual; pausa
  com salvar/carregar/retomar/sair. Escape deixa de encerrar imediatamente.
- Combate: mira pelo cursor mantendo exclusivamente WASD para movimento,
  direção travada durante o golpe, buffer curto de ataque, sinais de impacto,
  números de dano e sons distintos. Regras de dano/itens continuam em scripts.
- Câmera: acompanhamento suavizado e controle da altura
  para não acompanhar abruptamente cada salto.

As APIs de canvas, ambiente, projeção e pausa são genéricas. Application
continua apenas coordenando serviços. O pacote adventure compõe as regras.
A decoração oficial é reconciliada uma vez ao carregar saves anteriores; inventário, posição,
descobertas e encontros persistentes mantêm seus contratos anteriores.

## Critério de aceitação

Comparar capturas de hub, floresta, combate, inventário e mapa. Verificar
legibilidade de personagens, sombras e caminho, ausência de interface cortada,
pausa sem simulação, feedback de ataque e compatibilidade de saves/mods.
Builds e testes são necessários, mas não equivalem à aprovação artística ou
à experiência de uma sessão manual prolongada. A publicação ainda exige
conteúdo, balanceamento, QA, empacotamento e preparação comercial próprios.

## Contratos implementados

| API | Contrato |
| --- | --- |
| `show_canvas(commands)` | Canvas lógico 1280×720 transparente, ajustado à janela com margens proporcionais. Até 2048 comandos `rect`, `diamond`, `text`; cada comando possui x/y/w/h/color (vec3 0–1)/alpha. Texto inclui text (até 1024 bytes UTF-8), size (10–72) e align (left/center/right). |
| `pointer_position()` | Retorna x/y em coordenadas de canvas e valid; false fora do canvas, antes do primeiro movimento ou sem foco. |
| `screen_position(vec3)` | Projeta um ponto do mundo no canvas e retorna x/y/visible. Considera câmera, aspecto e margens. |
| `aim_position()` | Interseção do raio do cursor com o plano horizontal na altura do jogador; null sem ponteiro/jogador ou sem interseção frontal. |
| `pause_game(bool)` | O conteúdo assume o tratamento de Escape. Application congela fixed update e física; update/UI, áudio e fila de save continuam ativos. |
| `quit_game()` | Solicita encerramento pelo loop da aplicação. |
| `set_environment(record)` | sky/sun/ambient (vec3, canais 0–2), fog_start/fog_end (0≤início<fim≤10000), shadows (bool). Habilita iluminação/cor/névoa; pacotes que não chamam mantêm o caminho anterior. |
| `audio_volume(number)` | Volume mestre 0–1, apenas na sessão atual. |
| `on_ready` | Evento de Application após `on_start` e processamento inicial de saves. Adventure abre seu menu inicial aqui. |

`show_ui` permanece compatível. Canvas e ambiente não conhecem definições de
armas, waystones, ouro ou nomes de regiões. A apresentação de adventure fica
em `ui_canvas.sl`, `ui_inventory.sl` e `scenery.sl`; as regras continuam nos módulos existentes.

A câmera suaviza X/Z e a altura no chão; saltos não deslocam abruptamente o
alvo vertical, e deslocamentos acima de dez unidades reposicionam a câmera.
O ataque aceita um clique até 140 ms antes do fim da recuperação. Um clique
no mapa/inventário não pode iniciar ataque ao fechar o painel.

A decoração usa o marcador persistente `adventure_scenery_v1` para não duplicar
objetos no load. Saves anteriores recebem o cenário novo preservando o jogador,
equipamento, encontros, waystones e dados de mods ausentes. O caminho visual
legado é identificado por sua posição oficial exata e desativado. As colisões
existentes das casas e troncos permanecem; pequenos adereços são decorativos.

A gravação atômica mantém o save original ao falhar e tenta novamente, por no
máximo 80 ms, apenas para conflitos transitórios de compartilhamento/bloqueio.
Erros incluem o código do Windows no diagnóstico. Não há exclusão do save
original como fallback nem autosave silencioso.

## Validação desta entrega

Builds MSVC Debug e Release concluídos em 2026-10-08, com 30/30 testes CTest
aprovados em cada configuração. Capturas de GPU foram inspecionadas; as
interações desta revisão foram verificadas por input sintetizado, sem nova
sessão manual prolongada.

`presentation_tests` cobre menu inicial por mouse, Escape, pausa dos timers,
inventário, mapa e viagem sem ataque acidental, projeção em 16:9 e janela
quadrada, direção travada, buffer de ataque, dano visual e save/load sem
duplicação. `renderer_tests` verifica pixels de instâncias coloridas, descarte
fora da câmera e sombras recebidas. `extension_tests` segura um arquivo sem
compartilhamento de exclusão e verifica que o save anterior permanece intacto.

Capturas reproduzíveis: `build/release/presentation_tests.exe --preview`, com
working directory `build/release`. Saída: `user/previews/*.ppm`.
Os contadores de render separam draws, instâncias, triângulos e draws de sombra;
tempo de CPU de render não representa tempo total de frame nem FPS de GPU.

## Revisão do inventário (2026-10-08)

A pedido do autor, o refinamento do personagem fica sob sua responsabilidade.
O estudo editável foi salvo em `assets/core/models/explorer_concept.bbmodel` e
continua aberto no Blockbench. Ele não foi exportado nem integrado ao jogador.

O inventário usa grade 4×3, filtros Todos/Armas/Outros, navegação WASD/Q e páginas
por mouse. Selecionar é separado de equipar. O painel compara a cópia selecionada
com a arma atual usando os dados reais (definição + atributos persistidos):
dano e alcance maiores são verdes; intervalo windup+recovery menor é verde.
UID, nível e raridade distinguem cópias. Indicador de equipado, estado sem arma,
contagens de pilhas e ausência de mods são apresentados explicitamente. Não há
slots decorativos prometendo sistemas de armadura/consumíveis inexistentes.
As ilustrações de arma são símbolos da interface, não previews 3D dos modelos.

`presentation_tests` verifica que selecionar não equipa, que a comparação é
correta, que o botão equipa, que a segunda página alcança a cópia esperada e que
células/categorias vazias não mudam equipamento. O teste da extensão real de
conteúdo também equipa uma cópia na segunda página da grade e confere a diferença
entre atributos persistidos de duas cópias e a indicação de mod ausente.
Fechar o painel com Escape e clicar no mesmo frame não inicia um ataque.
