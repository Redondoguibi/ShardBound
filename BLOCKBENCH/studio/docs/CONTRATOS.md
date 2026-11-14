# Contratos Studio, versão 1

## Projeto editável

Extensão `.sbstudio`, JSON de projeto Blockbench com `meta.model_format = shardbound_studio` e propriedade `shardbound_studio`. Esta propriedade usa `schema = shardbound.studio.project`, `version = 1`. Todas as texturas e sons necessários à autoria são incorporados. Projetos com versão desconhecida são recusados antes de abrir uma nova aba.

`models` declara objetos independentes por ID, tipo, UUIDs das pastas raiz e bindings. Uma pasta raiz não pode pertencer a dois modelos nem estar dentro de outra raiz de modelo. `context` identifica a raiz, seis pastas corporais e a geometria do player de referência. `effects`, `sounds`, `materials`, `skinVariant`, `sampleRate`, `unitsPerMeter` e `projectileVisual` completam a autoria.

Os eventos vivem em `animations[].animators.effects.keyframes[]`, canal `shardbound_events`, `data_points[].studio_event`. O tempo é o tempo do keyframe nativo. Mover/copiar keyframes mantém o formato; o ID público do evento combina UUID do keyframe e ID do ponto para distinguir cópias. Ao mover apenas um evento pelo campo Tempo do evento nas propriedades do keyframe, o Studio separa/agrupa os pontos sem mover outros eventos coincidentes.

## Pacote exportado

Todos os auxiliares têm `schema`, `version`, `generator` e `project`. A versão de schema é independente da versão do plugin. JSON Schemas acompanham a distribuição em `schemas/`; a validação no editor também confere referências, dimensões, limites e dados nativos.

| Arquivo | Schema | Conteúdo |
| --- | --- | --- |
| `studio.manifest.json` | `shardbound.studio.package` | Editor, escala, modelos, arquivos, vínculos, skin e flecha compartilhada |
| `studio.effects.json` | `shardbound.studio.effects` | Definições completas, sprites e regras visuais |
| `studio.events.json` | `shardbound.studio.events` | Clips, eventos ordenados, arquivos/duração de áudio, locators |
| `studio.player-animations.json` | `shardbound.studio.player_animations` | Mapeamento da referência, canais P/Q/S das seis partes e raiz, tempos, loop |

`models/<namespace>/<path>.glb`, `effects/<namespace>/<path>.png` e `audio/<namespace>/<path>.<ext>` resolvem os arquivos exportados. IDs não aceitam segmentos `.`/`..`, caminhos absolutos ou barras invertidas. Os bytes de áudio originais são preservados. A exportação reserva uma pasta nova e escreve arquivos com exclusividade.

As unidades do editor são declaradas por `unitsPerMeter` (padrão 16, editável). GLB usa uma raiz com escala `1/unitsPerMeter`. Posições auxiliares, tamanhos, velocidades, acelerações, distância de luz, largura e offsets continuam em unidades do editor e devem receber a mesma conversão no importador. Rotações dos sprites são em graus; quaternions amostrados são `[x,y,z,w]` e posições/escalas são vetores de três componentes. Tempos são segundos; FPS/frame também são declarados nos eventos.

## Hierarquia e animação

Grupos/pastas são nós rígidos; cubos/malhas são filhos desses nós. Não há `skins`, pesos ou deformação por armature. Pivôs e hierarquia são avaliados pelo próprio Blockbench 5.2.2. Nós recebem `extras.studio_uuid`; ancestrais do contexto recebem `studio_role = reference_transform`. Esses extras identificam os nós; o contrato principal de efeitos/eventos permanece nos auxiliares.

O exportador amostra **a interpolação nativa** de posição, rotação e escala, incluindo linear, step, Bézier e Catmull-Rom, em `sampleRate` FPS (1–240), preservando também os tempos dos keyframes. Step inclui amostras dos dois lados da mudança, respeitando a tolerância nativa de 1/1200 s e a precisão Float32 do GLB. GLB contém tracks lineares amostradas; não se afirma representação analítica exata de curvas entre amostras. Valores Molang dinâmicos, relógios/delays customizados, IK/alvos removidos e canais fora do subconjunto declarado devem ser corrigidos/convertidos em keyframes numéricos antes da exportação. Blending/transições de gameplay ficam para o runtime.

Player próprio mantém suas animações no GLB de player. Em equipamento, o GLB contém a geometria do equipamento e os ancestrais de transformação necessários. `studio.player-animations.json` guarda o mapeamento dos UUIDs corporais, canais completos das seis partes e movimento da raiz. O consumidor futuro deve vincular os nós `reference_transform` ao player vivo **uma vez**, e não aplicar em duplicidade os canais também disponíveis no GLB. `bindings` declara explicitamente os grupos da arma/armadura e a região corporal.

Locators são origens auxiliares, não ossos. Eventos podem apontar a qualquer pasta/marcador existente; `context.groups` resolve attachments corporais que não tenham geometria no GLB do equipamento. O contexto é neutro e não é uma skin redistribuída.

## Regras de efeitos

`kind` é `particle`, `trail`, `flash` ou `glow`. Definições incluem defaults completos para evolução do schema; a UI apresenta as propriedades aplicáveis ao tipo. `texture` no projeto é UUID; na exportação é caminho PNG. Ausente significa disco radial suave gerado matematicamente, com centro branco opaco, branco até raio 35% e alpha linear até zero na borda. O fallback é uma textura 32×32. Sprites importados usam atlas regular, origem superior esquerda, colunas da esquerda à direita e linhas de cima para baixo; largura/altura devem ser divisíveis pela grade. O frame é `floor(age * fps) % frames`.

Partículas têm posição/velocidade inicial determinísticas com `seed` e índice de nascimento. Formas: ponto, volume de caixa, elipsoide ou cone direcional. Direção/abertura, velocidade/variação, aceleração e rotação são configuráveis. Gravidade/aceleração é **local ao emissor**, explicitamente declarada em `renderContract.gravitySpace`; zero desliga a gravidade. Arrasto linear é integrado analiticamente, sem acumular erro por frame. As curvas interpolam pontos de idade normalizada 0–1 para tamanho, alpha, cor e rotação; sem curva, usam endpoints. Sprites encaram a câmera.

`burst` emite a quantidade inicial uma vez. `start` emite a quantidade inicial e a taxa contínua, até a duração da definição, fim do clip ou `stop`, o que vier primeiro. `instance`, ou o par recurso/alvo quando vazio, relaciona start/stop. Reiniciar a mesma instância encerra a anterior. Partículas já nascidas continuam pela própria vida após parar a emissão. Seguir usa a transformação atual; soltar usa a transformação do alvo no nascimento. Offset é local ao attachment. Um loop reinicia o ciclo visual deterministicamente.

Rastros são faixas trianguladas em espaço de mundo, entre dois offsets do alvo, amostrados ao longo do histórico. Largura, histórico, quantidade de amostras, textura, cor e transparência são editáveis. Curvas de tamanho, alpha e cor modulam a faixa; a cauda perde alpha com a idade. Não são uma imagem simulando um rastro.

Flash é uma sobreposição aditiva real sobre a geometria do alvo/descendentes, com cor e alpha ao longo da duração. Brilho combina sprite aditivo e `PointLight` real com intensidade/alcance. Seguir/soltar e offsets também são respeitados nesses tipos. A influência da luz depende do material de destino; os shaders nativos de pintura do Blockbench têm iluminação própria, portanto não se declara equivalência luminosa ao runtime ainda.

Texturas de modelos usam nearest, material simples roughness 1/metalness 0, OPAQUE ou MASK, corte alpha e opção de faces duplas. O preview aplica o mesmo contrato de recorte/opacidade aos materiais nativos de pintura. VFX são unlit, alpha ou aditivos, depth-test ativo e depth-write desligado; rastros são de duas faces. Brilho é aditivo. Ordenação de transparência e iluminação final deverão ser confrontadas com o ShardBound na fase B.

## Som, tempo e cancelamento

Importação WAV/OGG/MP3 exige decodificação real pelo Chromium/Blockbench. Eventos possuem recurso, frame/tempo, volume 0–1, pitch 0.1–4, repetição e instância. O preview usa reprodução real de áudio; volume multiplica o volume global do Blockbench. Pitch altera frequência/duração, combinado à velocidade da timeline; o posicionamento de retomada considera o pitch. Combinações rejeitadas pelo Chromium produzem uma notificação no editor. Stop cancela as reproduções da instância, inclusive bursts repetidos; cancelamento voluntário não é tratado como falha de áudio.

Eventos são ordenados por tempo, ordem de criação e ID. Reprodução atravessando keyframes dispara uma vez, inclusive quando passa vários frames; repetição trata final e início do clip. Pausar, trocar clip/projeto ou encerrar preview cancela áudio ativo. Scrubbing recalcula VFX sem reproduzir sons repetidos. O som é de monitoração 2D; o attachment é exportado para futura espacialização no jogo. Não há evento que automaticamente cause dano/crie projétil de gameplay.

## Limites explícitos

Até 4096 partículas simultâneas por definição; curvas com 2–64 pontos; timeline de 1–120 FPS e clips até 1000 s (**limites nativos de snap/tempo no Blockbench 5.2.2**); amostragem de exportação independente até 240 FPS e até 2 milhões de amostras de grupos por clip; sprites até 2048×2048 pela criação de tela e até 256×256 células de atlas; áudio até aproximadamente 24 MiB por recurso. Cache de transformações do preview é limitado a 4096 entradas. Exceder limites gera diagnóstico; não reduz silenciosamente o conteúdo.

O runtime do ShardBound consome estes contratos v1 quando o manifesto é declarado em `assets.json → studio`. Consulte [runtime e uso](../../../docs/shardbound-studio/05-RUNTIME-E-USO.md) para importação, reprodução, validação e limites reais. Pacotes existentes com `runtimeIntegration: "not-yet-implemented"` continuam válidos: esse campo histórico é informativo; a compatibilidade é determinada por schema/version.
