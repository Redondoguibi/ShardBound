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

A decoração atual usa o marcador persistente `adventure_scenery_v2` para não
duplicar objetos no load. A v1 foi substituída pela correção de identidade
descrita abaixo. Saves anteriores preservam jogador, equipamento, encontros,
waystones e dados de mods ausentes. As colisões das casas acompanham o volume
retangular visível; troncos e piso físico continuam presentes.

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

## Correção de identidade e funcionamento (2026-10-09)

Direção reafirmada pelo autor: preservar a construção em cubos, manter o mapa
existente e priorizar funcionamento e acabamento dos sistemas atuais. O
personagem fica com o autor; esta correção não acrescenta regiões, armas,
inimigos, progressão ou lore.

O cenário usa piso em cubos regulares de duas unidades, com recorte da última
fileira para coincidir com o limite das regiões. Caminhos e praças usam a mesma
grade; casas possuem poucos cuboids e telhados com três degraus de meia unidade.
Peças comuns permanecem alinhadas aos eixos. Vegetação fina inclinada, pedras
rotacionadas e fachadas com muitos frisos foram removidas. Árvores e waystone
do autor voltam à composição oficial. Isso é apresentação do cenário existente;
não foi criado sistema de mineração/construção ou mundo voxel editável.

A v2 dá nomes próprios às peças decorativas e as agrupa sob um marcador na
origem. A remoção da v1 compara apenas definições/nome padrão, ausência de corpo
físico e assinatura exata de modelo, posição, rotação, escala e tint. Não depende
de IDs ou ordem de entidades, que mudam no load. Peças nomeadas por extensões ou
com aparência modificada não são removidas por essa comparação. Os modelos
originais não são regravados. O esquema do save continua v1.

O marcador só recebe o nome de versão concluída ao terminar a montagem. Uma
montagem interrompida é identificada pelo nome temporário e suas peças próprias
são removidas antes de tentar novamente, sem duplicar cenário. O load também
reatribui a referência do grupo decorativo ao novo mundo restaurado.

`entity_visual(entity)` é uma consulta genérica de modelo/transform/tint,
`physical` e `signature`; não modifica entidades. A assinatura é o ID do modelo
em UTF-8 hexadecimal, seguido dos bits float32 de posição, rotação, escala e
tint, em ordem XYZ. Serve à comparação de apresentação, não à identidade da
entidade. A tabela histórica pode ser reproduzida com
`python tools/record-scenery-v1-signatures.py build/release/user/presentation-saves/scenery_v1.json`
após executar o teste de apresentação com as fontes v1 congeladas em
`tests/fixtures/`.

A montagem foi simplificada para respeitar o orçamento de instruções da
linguagem, sem aumentar seu limite. Abrir inventário, mapa ou pausa cancela
ataques ainda aguardando no buffer; golpes já iniciados mantêm seu ciclo.

O teste de apresentação agora também gera um save real com as fontes v1,
carrega-o pela implementação atual, verifica remoção da decoração antiga,
preservação de ouro/nível/descobertas e de decoração nomeada por extensões,
idempotência, orientação dos blocos, piso físico, travessia do caminho e colisão
com a frente das casas. A aprovação visual final continua pertencendo ao autor.

Validação em 2026-10-09: 30/30 testes CTest em Debug e Release, com capturas
reais de D3D11. Controles/travessia foram exercitados por input e controlador
sintetizados; esta verificação não equivale a uma sessão manual completa.


## Salto na escala do autor — 2026-10-09

A altura do salto usa dois cubos da árvore original do autor, e não dois cubos
do piso decorativo: 2 × 16 × 0,038 = 1,216 unidade de mundo acima do ponto de
saída. A gravidade individual do jogador é aproximadamente 20 unidades/s²,
com impulso 7,19 e duração aproximada de 0,7 segundo. A configuração é reaplicada
ao carregar saves antigos; os demais corpos conservam sua gravidade.
O teste de apresentação usa Espaço e física real após a migração de save,
mede altura/duração e mantém as verificações de travessia e colisão.

Validação do salto: altura medida de 1,21839 unidade e 42 passos a 60 Hz
(0,7 segundo); 30/30 testes CTest passaram tanto em Debug quanto em Release.
