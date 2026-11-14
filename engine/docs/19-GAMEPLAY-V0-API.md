# Gameplay v0 implementado

Esta é a API executável da etapa 11. Os valores do sandbox são parâmetros de
teste, não decisões de balanceamento, escala final, formato de mapa ou rig final.
Jolt e miniaudio ficam privados aos adaptadores C++; EnTT continua privado a World.

O jogo padrão agora é o pacote `adventure`. Este documento preserva o contrato
do sandbox da Etapa 11; salto, câmera configurável, poses e APIs adicionais
estão em [23-PLAYABLE-SLICE.md](23-PLAYABLE-SLICE.md).

## Executar o sandbox

```powershell
./tools/build.ps1 debug
./build/debug/game.exe --mods examples/gameplay
```

`--mods` é relativo à pasta do executável, ou aceita caminho absoluto. O padrão
continua `mods`, com o exemplo mínimo da etapa 10. O sandbox separado cria chão,
player de seis partes e alvo interativo por script. WASD/analógico move; E/botão
norte coleta um token quando perto do alvo; mouse esquerdo/botão oeste causa dano.
Três ataques destroem o alvo. Escape encerra. O inventário é demonstrado por logs,
ainda sem HUD. O modelo estático tem geometria original nas proporções de referência
documentadas; não representa o template final de skins, bones ou animações.

## Entidades e física

| API | Contrato |
|---|---|
| `set_scale(entity, vec3(x,y,z))` | Escala visual positiva. Não redimensiona colisores. |
| `add_box(entity, vec3(x,y,z), dynamic)` | Meias dimensões positivas, em unidades de mundo; substitui corpo anterior. |
| `set_player(entity, speed)` | Seleciona entidade física e velocidade positiva em unidades/segundo. |
| `set_health(entity, maximum)` | Inicializa/reinicializa vida cheia; máximo positivo. |
| `health(entity)` | Retorna vida atual. |
| `damage(entity, amount)` | Dano não negativo, limitado pela vida atual; retorna dano aplicado. |
| `heal(entity, amount)` | Cura não negativa, limitada ao máximo; retorna cura aplicada. |

O controlador provisório aplica velocidade horizontal a um corpo com rotação
travada e preserva a velocidade vertical. Normaliza diagonais. Vida zero ou
Active=false bloqueia seu movimento comandado. A câmera acompanha o player de
cima e por trás. Não há pulo, degraus, rampas avançadas ou character controller
definitivo. Jolt roda no fixed update de 60 Hz; apenas camadas internas estática e
dinâmica existem nesta implementação. Determinismo entre máquinas não é prometido.

Entidades físicas precisam ser raízes da hierarquia. Alterar sua posição no
script teleporta o corpo no próximo passo; após a simulação, a posição física
atualiza Transform. Dimensões e orientação dos boxes são explícitas, independentes
do modelo. Destruir a entidade ou remover seu Transform libera o corpo no próximo
passo. Active controla render e input do player, não remove colisões. Vida zero
não destrói automaticamente: essa política pertence ao gameplay/script.

## Itens, interação e input

| API | Contrato |
|---|---|
| `give_item(entity, item, count)` | Adiciona quantidade inteira positiva; overflow é erro sem alterar saldo. |
| `take_item(entity, item, count)` | Retira a quantidade inteira positiva inteira ou retorna false sem alterações. |
| `item_count(entity, item)` | Retorna saldo, zero quando ausente. |
| `set_interactable(entity, enabled)` | Adiciona/atualiza marcador de interação. |
| `nearest_interactable(actor, radius)` | Entidade ativa mais próxima no raio positivo, excluindo ator; null se ausente. |
| `action_pressed(name)` | Borda de pressionamento no frame; usar em on_update. |
| `action_held(name)` | Estado atual da ação registrada. |

Itens são definições em `@Items`; a API aceita símbolo ou ID qualificado e valida
o registry. Inventário v0 é um conjunto de quantidades por ID, sem equipamentos,
slots, peso ou regras de stack finais. Interação usa distância entre origens em
world space; empates usam o menor ID de runtime. Não faz teste de linha de visão.

## Áudio

`assets.json` aceita declaração explícita:

```json
{"sounds":{"my_mod:hit":{"source":"audio/hit.wav","streaming":false}}}
```

`play_sound("my_mod:hit")` resolve o ID registrado pelo VFS e toca o arquivo.
Efeitos são decodificados; `streaming:true` permite áudio longo. WAV é o formato
validado nesta entrega, sem congelar o formato final. Registro valida existência
e contenção; decodificação valida o áudio no playback. Não há cache de áudio
compilado, spatial audio, mixer público, loop ou AudioEmitterComponent ainda.
Se não houver dispositivo, o runtime registra aviso e mistura silenciosamente
em modo offline para continuar executando. Testes verificam PCM offline, não
qualidade audível do dispositivo físico.

## Validação

O teste `gameplay` carrega o pacote real, executa o script, move e colide o player
com chão/alvo, sintetiza input de interação, confere inventário e som decodificado,
verifica limites de vida e overflow, desenha as seis partes e testa teleporte,
destruição e cancelamento do carregamento. `gameplay_smoke` executa a Application.
